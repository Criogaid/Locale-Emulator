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
        BYTE ansi[32]; WCHAR wide[32]; FillMemory(ansi,sizeof(ansi),0x5A); FillMemory(wide,sizeof(wide),0x5A);
        *(USHORT*)ansi = capacity; *(USHORT*)wide = capacity;
        BYTE expectedA[32]; WCHAR expectedW[32]; CopyMemory(expectedA,ansi,sizeof(ansi));CopyMemory(expectedW,wide,sizeof(wide));
        const USHORT count = capacity < 4 ? capacity : 4;
        CopyMemory(expectedA,"ABCD",count);CopyMemory(expectedW,L"ABCD",count * sizeof(WCHAR));
        if(UserfnINCNTOUTSTRING(LineA,nullptr,EM_GETLINE,0,(LPARAM)wide)!=count || memcmp(wide,expectedW,sizeof(wide))) return false;
        if(NtUserfnINCNTOUTSTRING(nullptr,EM_GETLINE,0,(LPARAM)ansi,0,0,0)!=count || memcmp(ansi,expectedA,sizeof(ansi))) return false;
    }
    return true;
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
    LEB environment = {}; environment.AnsiCodePage = 932; environment.OemCodePage = 936; environment.DefaultCharset = 128;
    if (NT_FAILED(ValidateLeb(environment))) return 11;
    environment.DefaultCharset = 256;
    if (ValidateLeb(environment) != STATUS_INVALID_PARAMETER) return 12;
    environment.DefaultCharset = 128; environment.AnsiCodePage = 65536;
    if (ValidateLeb(environment) != STATUS_INVALID_PARAMETER) return 13;
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
    puts("getline_thunk_word_bounds_and_no_terminator=ok");
    puts("leb_bounds=ok large_string_bounds=ok explicit_length_guard_pages=ok textmetric_first_char=ok");
    puts("patch=ok trampoline=RX source=RX restore=ok freed=ok unicode_length=ok");
    return 0;
}
