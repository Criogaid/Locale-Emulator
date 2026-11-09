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
static WNDPROC messageProbePrevious;
static LPMSG messageProbeInvalid;
static bool messageProbeCalled;

static LRESULT CALLBACK MessageProbeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CHAR && messageProbeInvalid != nullptr)
    {
        messageProbeCalled = true;
        PeekMessageA(messageProbeInvalid, nullptr, 0, 0, PM_NOREMOVE);
    }
    return messageProbePrevious != nullptr ?
        CallWindowProcA(messageProbePrevious, window, message, wParam, lParam) :
        DefWindowProcA(window, message, wParam, lParam);
}

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


static void MessagePointerContracts()
{
    HWND edit = CreateWindowExA(0, "EDIT", "abc", WS_POPUP, 0, 0, 0, 0,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(edit != nullptr, "x64 message contract edit control");
    if (edit == nullptr)
        return;

    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    const SIZE_T pageSize = system.dwPageSize;
    auto memory = static_cast<BYTE*>(VirtualAlloc(nullptr, pageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(memory != nullptr, "x64 message contract allocation");
    if (memory != nullptr)
    {
        DWORD previous = 0;
        Check(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != FALSE,
            "x64 message contract guard page");
        BYTE* invalid = memory + pageSize - 1;
        messageProbeInvalid = reinterpret_cast<LPMSG>(invalid);
        messageProbeCalled = false;
        messageProbePrevious = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(edit, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(MessageProbeWindowProc)));
        Check(messageProbePrevious != nullptr, "x64 message contract subclass");
        if (messageProbePrevious != nullptr)
        {
            SendMessageW(edit, WM_CHAR, static_cast<WPARAM>(0x3042), 0);
            Check(messageProbeCalled, "x64 message contract pending peek");
            SetWindowLongPtrA(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(messageProbePrevious));
        }
        messageProbeInvalid = nullptr;
        messageProbePrevious = nullptr;
        SendMessageA(edit, EM_GETSEL, reinterpret_cast<WPARAM>(invalid), 0);
        SendMessageA(edit, EM_GETSEL, 0, reinterpret_cast<LPARAM>(invalid));
        SendMessageA(edit, WM_GETTEXT, 2, reinterpret_cast<LPARAM>(invalid));
        SendMessageA(edit, EM_GETLINE, 0, reinterpret_cast<LPARAM>(invalid));
        SendMessageA(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(invalid));
        VirtualFree(memory, 0, MEM_RELEASE);
    }
    DestroyWindow(edit);
}


int wmain()
{
    static_assert(sizeof(void*) == 8, "x64 attribute contract requires a 64-bit test host");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    MessagePointerContracts();
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
