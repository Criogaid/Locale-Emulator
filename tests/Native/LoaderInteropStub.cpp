#include <windows.h>
#include <cwchar>

#pragma comment(linker, "/EXPORT:LeCreateProcess=_LeCreateProcess@44")

namespace
{
    constexpr LONG FailureStatus = static_cast<LONG>(0xC0000001u);
    constexpr LONG InvalidParameterStatus = static_cast<LONG>(0xC000000Du);
}

// 仅替代托管 P/Invoke 边界；不创建进程，也不模拟注入或未公开的 NTDLL 行为。
extern "C" LONG WINAPI LeCreateProcess(void* leb, LPCWSTR application, LPWSTR commandLine,
    LPCWSTR directory, DWORD flags, STARTUPINFOW* startup, PROCESS_INFORMATION* information,
    void* processAttributes, void* threadAttributes, void* environment, HANDLE token)
{
    if (!information) return InvalidParameterStatus;
    ZeroMemory(information, sizeof(*information));
    if (!leb || !startup || startup->cb != sizeof(*startup) || flags != 0 ||
        processAttributes || threadAttributes || environment || token ||
        !application || !commandLine || !directory ||
        std::wcscmp(commandLine, L"mutable command 日本語") != 0 ||
        std::wcscmp(directory, L"working directory 中文") != 0)
        return InvalidParameterStatus;

    const bool failure = std::wcscmp(application, L"failure-日本語") == 0;
    const bool waitFailure = std::wcscmp(application, L"waitfailure-日本語") == 0;
    if (!failure && !waitFailure && std::wcscmp(application, L"success-日本語") != 0)
        return InvalidParameterStatus;

    commandLine[0] = L'界';
    // 替身明确保证失败输出为零；不假设 Win32 失败时会返回可关闭的句柄。
    if (failure) return FailureStatus;

    HANDLE process = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    HANDLE thread = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    if (!process || !thread)
    {
        if (process) CloseHandle(process);
        if (thread) CloseHandle(thread);
        return FailureStatus;
    }
    if (waitFailure)
    {
        HANDLE restricted = nullptr;
        const BOOL duplicated = DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(),
            &restricted, EVENT_MODIFY_STATE, FALSE, 0);
        CloseHandle(process);
        if (!duplicated)
        {
            CloseHandle(thread);
            return FailureStatus;
        }
        // 有效的事件句柄缺少 SYNCHRONIZE，等待必须报 ERROR_ACCESS_DENIED。
        process = restricted;
    }
    information->hProcess = process;
    information->hThread = thread;
    return 0;
}
