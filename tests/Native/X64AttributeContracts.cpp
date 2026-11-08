#include <Windows.h>
#include <cstdio>

using NativeCreate = LONG(NTAPI*)(
    HANDLE*, HANDLE*, ACCESS_MASK, ACCESS_MASK, void*, void*, ULONG, ULONG,
    void*, void*, void*);

static int failures;

static void Check(bool condition, const char* name)
{
    if (!condition)
    {
        std::printf("FAIL %s error=%lu\n", name, GetLastError());
        ++failures;
    }
}

int wmain()
{
    static_assert(sizeof(void*) == 8, "x64 attribute contract requires a 64-bit test host");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

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
