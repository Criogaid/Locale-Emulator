#include "../../Native/LocaleEmulator/stdafx.h"

namespace {
PLeGlobalData testGlobalData;
PLeGlobalData TestGetGlobalData() { return testGlobalData; }
}

// 仅替换本 EXE 中的访问器，不定义产品的全局状态符号或改变 DLL ABI。
#define LeGetGlobalData TestGetGlobalData
#include "../../Native/LocaleEmulator/ml.cpp"
#include "../../Native/LocaleEmulator/Utility/Utility.cpp"
#include "../../Native/LocaleEmulator/Hooks/Gdi32Hook.cpp"
#include "../../Native/LocaleEmulator/Hooks/User32Hook.cpp"
#undef LeGetGlobalData
#include <stdio.h>
#include <vector>

static __declspec(noinline) int __stdcall Original(int value)
{
    volatile int input = value;
    return input * 3 + 7;
}
static decltype(&Original) OriginalStub;
__declspec(noinline) int __stdcall Replacement(int value)
{
    return OriginalStub(value) + 1;
}
static LRESULT CALLBACK LineA(HWND, UINT, WPARAM, LPARAM buffer) {
    USHORT capacity = *(USHORT*)buffer; USHORT count = capacity < 4 ? capacity : 4;
    CopyMemory((void*)buffer, "ABCD", count); return count;
}
static LRESULT NTAPI LineW(HWND, UINT, WPARAM, LPARAM buffer, ULONG_PTR, DWORD, BOOL) {
    USHORT capacity = *(USHORT*)buffer; USHORT count = capacity < 4 ? capacity : 4;
    CopyMemory((void*)buffer, L"ABCD", count * sizeof(WCHAR)); return count;
}
static bool CheckLineThunks() {
    // 模拟状态保留至进程退出；其析构依赖未启动的产品钩子和锁。
    testGlobalData = new LeGlobalData;
    testGlobalData->HookStub.StubNtUserMessageCall = LineW;
    const USHORT capacities[] = {0,1,2,32767,32768,65535};
    for (USHORT capacity : capacities) {
        const size_t ansiSize = capacity > sizeof(USHORT) ? capacity : sizeof(USHORT);
        const size_t wideSize = capacity != 0 ? capacity : 1;
        std::vector<BYTE> ansi(ansiSize, 0x5A);
        std::vector<WCHAR> wide(wideSize, 0x5A);
        *(USHORT*)ansi.data() = capacity; *(USHORT*)wide.data() = capacity;
        std::vector<BYTE> expectedA = ansi;
        std::vector<WCHAR> expectedW = wide;
        const USHORT count = capacity < 4 ? capacity : 4;
        CopyMemory(expectedA.data(), "ABCD", count);
        CopyMemory(expectedW.data(), L"ABCD", count * sizeof(WCHAR));
        if (UserfnINCNTOUTSTRING(LineA, nullptr, EM_GETLINE, 0, (LPARAM)wide.data()) != count ||
            memcmp(wide.data(), expectedW.data(), wide.size() * sizeof(WCHAR))) return false;
        if (NtUserfnINCNTOUTSTRING(nullptr, EM_GETLINE, 0, (LPARAM)ansi.data(), 0, 0, 0) != count ||
            memcmp(ansi.data(), expectedA.data(), ansi.size())) return false;
    }
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    PBYTE pages = static_cast<PBYTE>(VirtualAlloc(nullptr, static_cast<SIZE_T>(system.dwPageSize) * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages) return false;
    DWORD previous;
    const BOOL guarded = VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &previous);
    if (!guarded) {
        VirtualFree(pages, 0, MEM_RELEASE);
        return false;
    }
    PBYTE parameter = pages + system.dwPageSize - sizeof(USHORT) * 2;
    *reinterpret_cast<USHORT*>(parameter) = 4;
    if (UserfnINCNTOUTSTRING(LineA, nullptr, EM_GETLINE, 0, (LPARAM)parameter) != 0) {
        VirtualFree(pages, 0, MEM_RELEASE);
        return false;
    }
    *reinterpret_cast<USHORT*>(parameter) = 8;
    if (NtUserfnINCNTOUTSTRING(nullptr, EM_GETLINE, 0, (LPARAM)parameter, 0, 0, 0) != 0) {
        VirtualFree(pages, 0, MEM_RELEASE);
        return false;
    }
    VirtualFree(pages, 0, MEM_RELEASE);
    return true;
}

static bool CheckBoundedFormatting()
{
    WCHAR section[MAX_NTPATH];
    const ULONG_PTR length = GetLePebSectionName(section, countof(section), 0x2A);
    const WCHAR expected[] = L"Local\\LOCALE_ENUMLATOR_PROCESS_ENVIRONMENT_BLOCK_SECTION_0000002A";
    if (length != countof(expected) - 1 || wcscmp(section, expected) != 0) return false;
    WCHAR buffer[4] = {L'?', L'?', L'?', L'!'};
    if (_snwprintf(buffer, 3, L"%s", L"ABCDE") != -1 ||
        buffer[0] != L'A' || buffer[1] != L'B' || buffer[2] != L'C' || buffer[3] != L'!') return false;
    if (_snwprintf(buffer, 3, L"%s", L"ABC") != 3 || buffer[3] != L'!') return false;
    if (_snwprintf(buffer, countof(buffer), L"%s", L"ABC") != 3 || buffer[3] != 0) return false;
    WCHAR name[] = {L'A', L'B', L'X'};
    UNICODE_STRING counted = {2 * sizeof(WCHAR), sizeof(name), name};
    if (_snwprintf(buffer, countof(buffer), L"%wZ", &counted) != 2 ||
        buffer[0] != L'A' || buffer[1] != L'B' || buffer[2] != 0) return false;
    return GetLePebSectionName(buffer, countof(buffer), 0x2A) >= countof(buffer);
}

int __cdecl main()
{
    if (Original(4) != 19) return 1;
    Mp::PATCH_MEMORY_DATA patch[] = { Mp::FunctionJumpVa(Original, Replacement, &OriginalStub) };
    if (NT_FAILED(Mp::PatchMemory(patch, countof(patch)))) return 2;
    MEMORY_BASIC_INFORMATION original = {}, trampoline = {};
    VirtualQuery(Original, &original, sizeof(original));
    VirtualQuery(OriginalStub, &trampoline, sizeof(trampoline));
    if (Original(4) != 20 || OriginalStub(4) != 19) return 3;
    if (original.Protect != PAGE_EXECUTE_READ || trampoline.Protect != PAGE_EXECUTE_READ) return 4;
    const void* allocation = trampoline.AllocationBase;
    if (NT_FAILED(Mp::RestoreMemory(OriginalStub))) return 5;
    if (Original(4) != 19 || OriginalStub != nullptr) return 6;
    VirtualQuery(allocation, &trampoline, sizeof(trampoline));
    if (trampoline.State != MEM_FREE) return 7;
    ml::String text = L"AB";
    if (!text.MatchExpression(L"A*")) return 8;
    WCHAR* large = new WCHAR[32769];
    for (int i = 0; i < 32768; ++i) large[i] = L'*';
    large[32768] = 0;
    ml::String expression = large;
    delete[] large;
    if (text.MatchExpression(expression)) return 9;
    if (expression.MatchExpression(L"*")) return 10;
    struct RuntimeInputFixture
    {
        LE_RUNTIME_INPUT Input;
        WCHAR Timezone[20];
        WCHAR Standard[1];
        WCHAR Daylight[1];
    } fixture = {};
    CopyMemory(fixture.Timezone, L"Tokyo Standard Time", sizeof(L"Tokyo Standard Time"));
    fixture.Input.Size = sizeof(fixture);
    fixture.Input.Version = LE_RUNTIME_INPUT_VERSION;
    fixture.Input.AnsiCodePage = 932;
    fixture.Input.OemCodePage = 936;
    fixture.Input.FontCharset = 128;
    fixture.Input.TimezoneIdOffset = FIELD_OFFSET(RuntimeInputFixture, Timezone);
    fixture.Input.TimezoneIdLength = sizeof(L"Tokyo Standard Time") - sizeof(WCHAR);
    fixture.Input.StandardNameOffset = FIELD_OFFSET(RuntimeInputFixture, Standard);
    fixture.Input.DaylightNameOffset = FIELD_OFFSET(RuntimeInputFixture, Daylight);
    if (!LeValidateRuntimeInput(&fixture.Input)) return 11;
    fixture.Input.FontCharset = 256;
    if (LeValidateRuntimeInput(&fixture.Input)) return 12;
    fixture.Input.FontCharset = 128; fixture.Input.AnsiCodePage = 65536;
    if (LeValidateRuntimeInput(&fixture.Input)) return 13;
    LARGE_UNICODE_STRING source = {}, destination = {};
    source.Length = 65536; source.MaximumLength = 65536; source.UnicodeBuffer = (PWSTR)1;
    if (LargeStringDuplicate(&source, &destination) != nullptr) return 14;
    source.Ansi = TRUE;
    if (LargeStringAnsiToUnicode(&source, &destination) != nullptr) return 15;
    if (WCharToMByte(L"A", MAXULONG_PTR - 1) != nullptr || MByteToWChar("A", MAXULONG_PTR - 1) != nullptr) return 16;
    SYSTEM_INFO system; GetSystemInfo(&system);
    PBYTE pages = (PBYTE)VirtualAlloc(nullptr, system.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD previous; if (!pages || !VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &previous)) return 17;
    PWSTR wide = (PWSTR)(pages + system.dwPageSize - sizeof(WCHAR)); *wide = L'A';
    PSTR ansi = WCharToMByte(wide, 1);
    if (!ansi || ansi[0] != 'A' || ansi[1] != 0) return 18;
    FreeString(ansi);
    PSTR narrow = (PSTR)(pages + system.dwPageSize - 1); *narrow = 'B';
    PWSTR converted = MByteToWChar(narrow, 1);
    if (!converted || converted[0] != L'B' || converted[1] != 0) return 19;
    FreeString(converted); VirtualFree(pages, 0, MEM_RELEASE);
    TEXTMETRICW metricW = {}; TEXTMETRICA metricA = {};
    metricW.tmFirstChar = L'A'; metricW.tmLastChar = 0xFFFF; metricW.tmDefaultChar = L'?'; metricW.tmBreakChar = L' '; metricW.tmStruckOut = 1;
    ConvertUnicodeTextMetricToAnsi(&metricA, &metricW);
    if (metricA.tmFirstChar != 'A' || metricA.tmLastChar != 255 || metricA.tmDefaultChar != '?' || metricA.tmBreakChar != ' ') return 20;
    if (!CheckLineThunks()) return 21;
    if (!CheckBoundedFormatting()) return 22;
    puts("bounded_format_count_terminator_truncation_and_nt_unicode=ok");
    puts("getline_thunk_word_bounds_and_no_terminator=ok");
    puts("leb_bounds=ok large_string_bounds=ok explicit_length_guard_pages=ok textmetric_first_char=ok");
    puts("patch=ok trampoline=RX source=RX restore=ok freed=ok unicode_length=ok");
    return 0;
}
