#include <Windows.h>
#include <winternl.h>
#include <cstdio>

using NativeCreate = LONG(NTAPI*)(
    HANDLE*, HANDLE*, ACCESS_MASK, ACCESS_MASK, void*, void*, ULONG, ULONG,
    void*, void*, void*);
using NtQuerySystemInformationFn = LONG(NTAPI*)(ULONG, void*, ULONG, ULONG*);
using NtQueryDefaultLocaleFn = LONG(NTAPI*)(BOOLEAN, ULONG*);
using NtQueryDefaultLanguageFn = LONG(NTAPI*)(USHORT*);
constexpr ULONG SystemCurrentTimeZoneInformationClass = 44;

static int failures;

static void Check(bool condition, const char* name)
{
    if (!condition)
    {
        std::printf("FAIL %s error=%lu\n", name, GetLastError());
        ++failures;
    }
}

static void InvalidOutputContracts()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto querySystem = reinterpret_cast<NtQuerySystemInformationFn>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
    auto queryLocale = reinterpret_cast<NtQueryDefaultLocaleFn>(
        GetProcAddress(ntdll, "NtQueryDefaultLocale"));
    auto queryDefaultLanguage = reinterpret_cast<NtQueryDefaultLanguageFn>(
        GetProcAddress(ntdll, "NtQueryDefaultUILanguage"));
    auto queryInstallLanguage = reinterpret_cast<NtQueryDefaultLanguageFn>(
        GetProcAddress(ntdll, "NtQueryInstallUILanguage"));
    Check(querySystem != nullptr, "NtQuerySystemInformation export");
    Check(queryLocale != nullptr, "NtQueryDefaultLocale export");
    Check(queryDefaultLanguage != nullptr, "NtQueryDefaultUILanguage export");
    Check(queryInstallLanguage != nullptr, "NtQueryInstallUILanguage export");
    if (querySystem == nullptr || queryLocale == nullptr ||
        queryDefaultLanguage == nullptr || queryInstallLanguage == nullptr)
        return;

    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    const SIZE_T pageSize = system.dwPageSize;
    auto memory = static_cast<BYTE*>(VirtualAlloc(nullptr, pageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(memory != nullptr, "invalid output contract allocation");
    if (memory == nullptr)
        return;
    DWORD previous = 0;
    Check(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != FALSE,
        "invalid output contract guard page");
    BYTE* invalid = memory + pageSize - 1;
    const ULONG timeZoneBytes = 256;
    LONG status = querySystem(static_cast<ULONG>(SystemCurrentTimeZoneInformationClass),
        invalid, timeZoneBytes, nullptr);
    Check(status < 0, "x64 timezone invalid output rejected");
    status = querySystem(static_cast<ULONG>(SystemCurrentTimeZoneInformationClass),
        memory, timeZoneBytes, reinterpret_cast<ULONG*>(invalid));
    Check(status < 0, "x64 timezone invalid return length rejected");
    status = queryLocale(FALSE, reinterpret_cast<ULONG*>(invalid));
    Check(status < 0, "x64 default locale invalid output rejected");
    status = queryDefaultLanguage(reinterpret_cast<USHORT*>(invalid));
    Check(status < 0, "x64 default UI language invalid output rejected");
    status = queryInstallLanguage(reinterpret_cast<USHORT*>(invalid));
    Check(status < 0, "x64 install UI language invalid output rejected");
    VirtualFree(memory, 0, MEM_RELEASE);
}


int wmain()
{
    static_assert(sizeof(void*) == 8, "x64 attribute contract requires a 64-bit test host");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    InvalidOutputContracts();

    auto create = reinterpret_cast<NativeCreate>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateUserProcess"));
    Check(create != nullptr, "NtCreateUserProcess export");
    if (create == nullptr)
        return 1;

    constexpr SIZE_T AttributeBytes = 1024 * 1024;
    auto list = static_cast<BYTE*>(VirtualAlloc(nullptr, AttributeBytes,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(list != nullptr, "large attribute list allocation");
    if (list == nullptr)
        return 1;

    ZeroMemory(list, AttributeBytes);
    *reinterpret_cast<ULONG_PTR*>(list) = AttributeBytes;

    HANDLE process = nullptr;
    HANDLE thread = nullptr;
    LONG status = create(&process, &thread, 0, 0, nullptr, nullptr, 0, 0,
        nullptr, nullptr, list);
    std::printf("large-attribute-list bytes=%zu status=%08lx\n", AttributeBytes, status);
    Check(status < 0, "large attribute list rejected without crash");

    if (process != nullptr)
    {
        TerminateProcess(process, 99);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }
    if (thread != nullptr)
        CloseHandle(thread);
    VirtualFree(list, 0, MEM_RELEASE);

    std::printf("x64 attribute contract failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
