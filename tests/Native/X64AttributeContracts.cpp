#include <Windows.h>
#include <cstdio>
#include <string>
#include "WindowsCommandLine.h"

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

static void CrossArchitectureChild()
{
    WCHAR module[32768] = {};
    DWORD moduleLength = GetModuleFileNameW(nullptr, module, ARRAYSIZE(module));
    Check(moduleLength != 0 && moduleLength < ARRAYSIZE(module), "x64 module path");
    if (moduleLength == 0 || moduleLength >= ARRAYSIZE(module))
        return;

    WCHAR* separator = wcsrchr(module, L'\\');
    Check(separator != nullptr, "x64 module directory");
    if (separator == nullptr)
        return;

    const std::wstring directory(module, separator - module);
    const std::wstring probe = directory + L"\\NativeProbe.exe";
    const std::wstring report = directory + L"\\cross-architecture-report.txt";
    std::wstring command = BuildWindowsCommandLine({
        probe.c_str(), report.c_str(), L"932", directory.c_str(), L"--child"});
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    BOOL created = CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, 0, nullptr,
        directory.c_str(), &startup, &process);
    Check(created != FALSE, "x64 creates x86 child");
    if (!created)
        return;

    DWORD wait = WaitForSingleObject(process.hProcess, 30000);
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(process.hProcess, 32);
        WaitForSingleObject(process.hProcess, 5000);
        Check(false, "x86 child completes");
    }
    DWORD exitCode = 1;
    Check(GetExitCodeProcess(process.hProcess, &exitCode) != FALSE && exitCode == 0,
        "x86 child is locale-emulated");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    HANDLE reportFile = CreateFileW(report.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(reportFile != INVALID_HANDLE_VALUE, "x86 child report");
    if (reportFile != INVALID_HANDLE_VALUE)
    {
        char contents[4096] = {};
        DWORD read = 0;
        Check(ReadFile(reportFile, contents, sizeof(contents) - 1, &read, nullptr) != FALSE,
            "x86 child report read");
        contents[read] = 0;
        Check(std::string(contents).find("acp=932") != std::string::npos,
            "x86 child code page");
        Check(std::string(contents).find("failures=0") != std::string::npos,
            "x86 child regression");
        CloseHandle(reportFile);
    }
    DeleteFileW(report.c_str());
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

    CrossArchitectureChild();
    std::printf("x64 attribute contract failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
