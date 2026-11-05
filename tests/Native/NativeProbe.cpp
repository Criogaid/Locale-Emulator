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
    Record("failures", failures);
    CloseHandle(report);
    return static_cast<int>(failures);
}
