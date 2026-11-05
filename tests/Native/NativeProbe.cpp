#include <Windows.h>
#include "WindowsCommandLine.h"

// 退出码位：1 ACP、2 默认转换、4 显式转换、8 字体、16 工作目录、32 子进程、64 报告写入；128 参数错误。
static HANDLE report;
static DWORD failures;

static void Record(const char* key, DWORD value) {
    char line[128];
    const int length = wsprintfA(line, "%s=%lu\r\n", key, value);
    DWORD written = 0;
    if (!WriteFile(report, line, length, &written, nullptr) || written != static_cast<DWORD>(length)) failures |= 64;
}

static bool ParseCodePage(const WCHAR* text, UINT& value) {
    value = 0;
    if (!*text) return false;
    for (; *text; ++text) {
        if (*text < L'0' || *text > L'9' || value > 6553) return false;
        value = value * 10 + *text - L'0';
    }
    return value != 0 && value <= 65535 && IsValidCodePage(value);
}

static bool Conversion(UINT codePage, const WCHAR* text) {
    char encoded[64] = {};
    WCHAR decoded[32] = {};
    const int size = WideCharToMultiByte(codePage, 0, text, -1, encoded, sizeof(encoded), nullptr, nullptr);
    return size > 0 && MultiByteToWideChar(codePage, 0, encoded, size, decoded, ARRAYSIZE(decoded)) > 0 && lstrcmpW(text, decoded) == 0;
}

struct FontResult { UINT charset; UINT count; UINT matched; };
static int CALLBACK FontCallback(const LOGFONTA* font, const TEXTMETRICA*, DWORD, LPARAM context) {
    FontResult* result = reinterpret_cast<FontResult*>(context);
    ++result->count;
    if (font->lfCharSet == result->charset) ++result->matched;
    return 1;
}

static LRESULT CALLBACK ControlParentProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_MEASUREITEM) {
        reinterpret_cast<MEASUREITEMSTRUCT*>(lParam)->itemHeight = 20;
        return TRUE;
    }
    if (message == WM_COMPAREITEM) {
        const COMPAREITEMSTRUCT* items = reinterpret_cast<const COMPAREITEMSTRUCT*>(lParam);
        return (items->itemData1 > items->itemData2) - (items->itemData1 < items->itemData2);
    }
    if (message == WM_DRAWITEM || message == WM_DELETEITEM) return TRUE;
    return DefWindowProcW(window, message, wParam, lParam);
}

static LRESULT ControlMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT route) {
    if (route == 0) return SendMessageA(window, message, wParam, lParam);
    if (route == 1) return SendMessageW(window, message, wParam, lParam);
    if (route == 2) return CallWindowProcA(reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window, GWLP_WNDPROC)), window, message, wParam, lParam);
    return CallWindowProcW(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)), window, message, wParam, lParam);
}

struct ControlClass { const char* ansi; const WCHAR* wide; };
static const ControlClass controlClasses[] = {
    {"ListBox", L"ListBox"}, {"ComboBox", L"ComboBox"},
    {"NativeProbeSuperclassListA", L"NativeProbeSuperclassListW"},
    {"NativeProbeSuperclassComboA", L"NativeProbeSuperclassComboW"}
};

static bool ControlCase(HWND parent, bool combo, DWORD style, bool strings, bool unicode, UINT route, bool superclass) {
    const DWORD windowStyle = WS_CHILD | style | (combo ? CBS_DROPDOWNLIST : 0);
    const ControlClass& control = controlClasses[(superclass ? 2 : 0) + (combo ? 1 : 0)];
    HWND window = unicode ? CreateWindowExW(0, control.wide, L"", windowStyle, 0, 0, 200, 200, parent, nullptr, GetModuleHandleW(nullptr), nullptr) :
        CreateWindowExA(0, control.ansi, "", windowStyle, 0, 0, 200, 200, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return false;
    const UINT add = combo ? CB_ADDSTRING : LB_ADDSTRING;
    const UINT insert = combo ? CB_INSERTSTRING : LB_INSERTSTRING;
    const UINT get = combo ? CB_GETLBTEXT : LB_GETTEXT;
    const UINT length = combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN;
    const UINT data = combo ? CB_GETITEMDATA : LB_GETITEMDATA;
    const UINT find = combo ? CB_FINDSTRING : LB_FINDSTRING;
    const UINT exact = combo ? CB_FINDSTRINGEXACT : LB_FINDSTRINGEXACT;
    const UINT select = combo ? CB_SELECTSTRING : LB_SELECTSTRING;
    WCHAR wide[] = {0x3042, 0x4E2D, 0};
    char ansi[32] = {};
    const int bytes = WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, sizeof(ansi), nullptr, nullptr);
    bool ok = bytes > 0;
    if (superclass) {
        WCHAR registered[64] = {}, actual[64] = {};
        if (!GetClassNameW(window, registered, ARRAYSIZE(registered)) ||
            !RealGetWindowClassW(window, actual, ARRAYSIZE(actual)) ||
            lstrcmpiW(actual, controlClasses[combo ? 1 : 0].wide) || !lstrcmpiW(registered, actual)) ok = false;
    }
    const bool wideRoute = (route & 1) != 0;
    const LPARAM text = wideRoute ? reinterpret_cast<LPARAM>(wide) : reinterpret_cast<LPARAM>(ansi);
    const LPARAM values[] = {1, 0x12345678, text};
    for (UINT item = 0; item != ARRAYSIZE(values); ++item) {
        const LPARAM input = strings ? text : values[item];
        const LRESULT index = ControlMessage(window, item == 1 ? insert : add, 0, input, route);
        if (index < 0) { ok = false; continue; }
        if (!strings && ControlMessage(window, data, index, 0, route) != input) ok = false;
        const LRESULT size = ControlMessage(window, length, index, 0, route);
        if (strings) {
            WCHAR outputW[64] = {};
            char outputA[128] = {};
            const LRESULT copied = ControlMessage(window, get, index, wideRoute ? reinterpret_cast<LPARAM>(outputW) : reinterpret_cast<LPARAM>(outputA), route);
            if (copied != (wideRoute ? 2 : bytes - 1) || size < copied ||
                (wideRoute ? lstrcmpW(outputW, wide) : lstrcmpA(outputA, ansi))) ok = false;
        } else {
            struct { ULONG_PTR value; DWORD guard; } output = {0, 0xDEADBEEF};
            const LRESULT copied = ControlMessage(window, get, index, reinterpret_cast<LPARAM>(&output), route);
            // USER32 A/W thunks can overestimate length and report TCHAR units for item data.
            if (size < sizeof(DWORD) || (copied != sizeof(DWORD) && copied != sizeof(DWORD) / sizeof(WCHAR)) ||
                output.value != static_cast<ULONG_PTR>(input) || output.guard != 0xDEADBEEF) {
                Record("control_item", item); Record("control_length", static_cast<DWORD>(size));
                Record("control_copied", static_cast<DWORD>(copied)); Record("control_data", static_cast<DWORD>(output.value));
                Record("control_expected", static_cast<DWORD>(input));
                ok = false;
            }
        }
        if (ControlMessage(window, find, static_cast<WPARAM>(-1), input, route) < 0 ||
            ControlMessage(window, exact, static_cast<WPARAM>(-1), input, route) < 0 ||
            ControlMessage(window, select, static_cast<WPARAM>(-1), input, route) < 0) ok = false;
    }
    if (!strings) {
        DWORD output = 0xDEADBEEF;
        if (ControlMessage(window, length, 100, 0, route) != LB_ERR ||
            ControlMessage(window, get, 100, reinterpret_cast<LPARAM>(&output), route) != LB_ERR || output != 0xDEADBEEF) ok = false;
    }
    DestroyWindow(window);
    return ok;
}

static LRESULT CALLBACK StringControlProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == CB_ADDSTRING || message == LB_ADDSTRING) {
        const WCHAR wide[] = {0x3042, 0x4E2D, 0};
        char ansi[32] = {};
        return WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, sizeof(ansi), nullptr, nullptr) > 0 &&
            lstrcmpA(reinterpret_cast<const char*>(lParam), ansi) == 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

static bool CheckControls() {
    WNDCLASSW cls = {};
    cls.lpfnWndProc = ControlParentProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"NativeProbeControlParent";
    if (!RegisterClassW(&cls)) return false;
    HWND parent = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 300, 300, nullptr, nullptr, cls.hInstance, nullptr);
    bool ok = parent != nullptr;
    for (UINT combo = 0; combo != 2; ++combo) {
        WNDCLASSA ansi = {};
        WNDCLASSW wide = {};
        if (!GetClassInfoA(nullptr, controlClasses[combo].ansi, &ansi) ||
            !GetClassInfoW(nullptr, controlClasses[combo].wide, &wide)) {
            ok = false;
            continue;
        }
        ansi.hInstance = wide.hInstance = cls.hInstance;
        ansi.lpszClassName = controlClasses[combo + 2].ansi;
        wide.lpszClassName = controlClasses[combo + 2].wide;
        const ATOM registeredA = RegisterClassA(&ansi);
        const ATOM registeredW = RegisterClassW(&wide);
        if (!registeredA || !registeredW) ok = false;
    }
    UINT cases = 0, nativeTextThunkSkips = 0;
    for (UINT superclass = 0; superclass != 2; ++superclass) {
        for (UINT combo = 0; parent && combo != 2; ++combo) {
            for (UINT kind = 0; kind != 7; ++kind) {
                const bool strings = kind == 0 || kind == 3 || kind == 4;
                const DWORD owner = kind == 0 ? 0 : (combo ? (kind % 2 ? CBS_OWNERDRAWFIXED : CBS_OWNERDRAWVARIABLE) :
                    (kind % 2 ? LBS_OWNERDRAWFIXED : LBS_OWNERDRAWVARIABLE));
                const DWORD style = owner | (strings && owner ? (combo ? CBS_HASSTRINGS : LBS_HASSTRINGS) : 0) |
                    (kind >= 5 ? (combo ? CBS_SORT : LBS_SORT) : 0);
                for (UINT unicode = 0; unicode != 2; ++unicode) {
                    for (UINT route = 0; route != 4; ++route) {
                        // USER32 does not thunk control text through CallWindowProcA for a Unicode superclass.
                        if (superclass && unicode && strings && route == 2) { ++nativeTextThunkSkips; continue; }
                        Record("control_case_started", ++cases);
                        const bool passed = ControlCase(parent, combo != 0, style, strings, unicode != 0, route, superclass != 0);
                        if (!passed) Record("control_case_failed", cases);
                        ok = passed && ok;
                    }
                }
            }
        }
        if (!superclass) Record("control_standard_matrix_ok", ok);
    }
    for (UINT combo = 0; combo != 2; ++combo) {
        UnregisterClassA(controlClasses[combo + 2].ansi, cls.hInstance);
        UnregisterClassW(controlClasses[combo + 2].wide, cls.hInstance);
    }
    if (parent) DestroyWindow(parent);
    UnregisterClassW(cls.lpszClassName, cls.hInstance);
    Record("control_matrix_ok", ok);
    WNDCLASSA custom = {};
    custom.lpfnWndProc = StringControlProc;
    custom.hInstance = cls.hInstance;
    custom.lpszClassName = "NativeProbeStringControl";
    if (!RegisterClassA(&custom)) ok = false;
    else {
        HWND window = CreateWindowExA(0, custom.lpszClassName, "", CBS_OWNERDRAWFIXED | LBS_OWNERDRAWFIXED,
            0, 0, 100, 100, nullptr, nullptr, custom.hInstance, nullptr);
        const WCHAR text[] = {0x3042, 0x4E2D, 0};
        if (!window) ok = false;
        else {
            const bool classBoundary = SendMessageW(window, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text)) == TRUE &&
                SendMessageW(window, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text)) == TRUE;
            Record("control_class_boundary_ok", classBoundary);
            ok = classBoundary && ok;
            DestroyWindow(window);
        }
        UnregisterClassA(custom.lpszClassName, custom.hInstance);
    }
    Record("control_cases", cases);
    Record("control_native_text_thunk_skips", nativeTextThunkSkips);
    Record("controls_ok", ok);
    return ok;
}

int wmain(int argc, WCHAR** argv) {
    UINT expected = 0;
    const bool child = argc == 5 && lstrcmpW(argv[4], L"--child") == 0;
    if (argc < 3 || argc > 5 || (argc == 5 && !child) || !ParseCodePage(argv[2], expected)) return 128;
    CheckWindowsCommandLineQuoting();
    report = CreateFileW(argv[1], GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (report == INVALID_HANDLE_VALUE) return 64;
    Record("pid", GetCurrentProcessId());
    Record("child", child);
    Record("argv_quoting_selfcheck", 1);
    Record("expected_acp", expected);
    Record("acp", GetACP());
    if (GetACP() != expected) failures |= 1;

    // 用固定字节验证非 ASCII 转换，避免只验证两个方向同时出错的往返。
    const char bytes[] = {static_cast<char>(0x82), static_cast<char>(0xA0), 0};
    WCHAR actual[8] = {}, reference[8] = {};
    const int actualSize = MultiByteToWideChar(CP_ACP, 0, bytes, -1, actual, ARRAYSIZE(actual));
    const int referenceSize = MultiByteToWideChar(expected, 0, bytes, -1, reference, ARRAYSIZE(reference));
    char actualBytes[32] = {}, referenceBytes[32] = {};
    const WCHAR japanese[] = {0x3042, 0};
    const int actualByteSize = WideCharToMultiByte(CP_ACP, 0, japanese, -1, actualBytes, sizeof(actualBytes), nullptr, nullptr);
    const int referenceByteSize = WideCharToMultiByte(expected, 0, japanese, -1, referenceBytes, sizeof(referenceBytes), nullptr, nullptr);
    if (!actualSize || actualSize != referenceSize || lstrcmpW(actual, reference) || !actualByteSize || actualByteSize != referenceByteSize || lstrcmpA(actualBytes, referenceBytes)) failures |= 2;
    Record("default_first_unicode", actual[0]);
    Record("default_conversion_ok", !(failures & 2));
    WCHAR sjis[4] = {}, utf8[4] = {}, western[4] = {};
    const char utf8Bytes[] = {static_cast<char>(0xE3), static_cast<char>(0x81), static_cast<char>(0x82), 0};
    const char westernBytes[] = {static_cast<char>(0xE9), 0};
    if (MultiByteToWideChar(932, 0, bytes, -1, sjis, ARRAYSIZE(sjis)) != 2 || sjis[0] != 0x3042 ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Bytes, -1, utf8, ARRAYSIZE(utf8)) != 2 || utf8[0] != 0x3042 ||
        MultiByteToWideChar(1252, 0, westernBytes, -1, western, ARRAYSIZE(western)) != 2 || western[0] != 0xE9 ||
        !Conversion(932, japanese) || !Conversion(CP_UTF8, japanese) || !Conversion(1252, L"\xE9")) failures |= 4;
    Record("explicit_conversion_ok", !(failures & 4));

    CHARSETINFO charset = {};
    FontResult fonts = {};
    if (!TranslateCharsetInfo(reinterpret_cast<DWORD*>(static_cast<ULONG_PTR>(expected)), &charset, TCI_SRCCODEPAGE)) failures |= 8;
    fonts.charset = charset.ciCharset;
    HDC dc = CreateCompatibleDC(nullptr);
    LOGFONTA font = {};
    font.lfCharSet = DEFAULT_CHARSET;
    if (!dc || !EnumFontFamiliesExA(dc, &font, FontCallback, reinterpret_cast<LPARAM>(&fonts), 0) || !fonts.count || !fonts.matched) failures |= 8;
    if (dc) DeleteDC(dc);
    Record("expected_charset", fonts.charset);
    Record("font_count", fonts.count);
    Record("font_matching_charset", fonts.matched);

    WCHAR directory[32768] = {};
    const DWORD directoryLength = GetCurrentDirectoryW(ARRAYSIZE(directory), directory);
    WCHAR expectedDirectory[32768] = {};
    const DWORD expectedLength = argc >= 4 ? GetFullPathNameW(argv[3], ARRAYSIZE(expectedDirectory), expectedDirectory, nullptr) : 0;
    if (!directoryLength || directoryLength >= ARRAYSIZE(directory) || (argc >= 4 && (!expectedLength || expectedLength >= ARRAYSIZE(expectedDirectory) || lstrcmpiW(directory, expectedDirectory)))) failures |= 16;
    Record("working_directory_ok", !(failures & 16));
    if (!child) {
        WCHAR executable[32768] = {};
        const DWORD executableLength = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
        if (!executableLength || executableLength >= ARRAYSIZE(executable) || !directoryLength || directoryLength >= ARRAYSIZE(directory)) {
            failures |= 32;
        } else {
            const std::wstring childPath = std::wstring(argv[1]) + L".child";
            std::wstring command = BuildWindowsCommandLine({executable, childPath.c_str(), argv[2], directory, L"--child"});
            if (command.size() >= 32767) {
                failures |= 32;
            } else {
                STARTUPINFOW startup = {sizeof(startup)};
                PROCESS_INFORMATION process = {};
                if (!CreateProcessW(executable, &command[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
                    Record("child_create_error", GetLastError());
                    failures |= 32;
                } else {
                    const DWORD wait = WaitForSingleObject(process.hProcess, 15000);
                    DWORD exitCode = 0;
                    if (wait != WAIT_OBJECT_0) {
                        TerminateProcess(process.hProcess, 32);
                        WaitForSingleObject(process.hProcess, 5000);
                        failures |= 32;
                    }
                    if (!GetExitCodeProcess(process.hProcess, &exitCode) || exitCode) failures |= 32;
                    Record("child_exit", exitCode);
                    CloseHandle(process.hThread);
                    CloseHandle(process.hProcess);
                }
            }
        }
    }
    if (child && !CheckControls()) failures |= 32;
    Record("failures", failures);
    CloseHandle(report);
    return static_cast<int>(failures);
}
