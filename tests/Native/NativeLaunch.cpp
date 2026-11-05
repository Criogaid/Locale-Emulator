#include <Windows.h>
#include "WindowsCommandLine.h"

// 与 Native/LocaleEmulator/LocaleEmulator.h 的 Win32 LEB 前缀保持一致，注册表项数量为零。
struct TimeFields { SHORT Year, Month, Day, Hour, Minute, Second, Milliseconds, Weekday; };
struct TimeZone { LONG Bias; WCHAR StandardName[32]; TimeFields StandardStart; LONG StandardBias; WCHAR DaylightName[32]; TimeFields DaylightStart; LONG DaylightBias; };
struct Environment { ULONG Ansi, Oem, Locale, Charset, HookUI; WCHAR Face[32]; TimeZone Zone; ULONG64 RegistryCount; };
struct ProcessInfo : PROCESS_INFORMATION { void* FirstCall; };
static_assert(sizeof(Environment) == 264, "Win32 LEB ABI");
using Create = LONG (WINAPI*)(Environment*, PCWSTR, PWSTR, PCWSTR, ULONG, STARTUPINFOW*, ProcessInfo*, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, void*, HANDLE);
static void Print(const char* key, DWORD value) {
    char line[128]; DWORD written;
    const int length = wsprintfA(line, "%s=%lu (0x%08lX)\r\n", key, value, value);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, length, &written, nullptr);
}
int wmain(int argc, WCHAR** argv) {
    if (argc != 6 || (lstrcmpW(argv[4], L"932") && lstrcmpW(argv[4], L"936"))) return 128;
    CheckWindowsCommandLineQuoting();
    Print("argv_quoting_selfcheck", 1);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const std::wstring dll = std::wstring(argv[1]) + L"\\LoaderDll.dll";
    std::wstring command = BuildWindowsCommandLine({argv[2], argv[3], argv[4], argv[5]});
    if (dll.size() >= 32767 || command.size() >= 32767) return 128;
    HMODULE module = LoadLibraryW(dll.c_str());
    if (!module) { Print("load_error", GetLastError()); return 1; }
    auto create = reinterpret_cast<Create>(GetProcAddress(module, "LeCreateProcess"));
    if (!create) { Print("export_error", GetLastError()); return 2; }
    Environment env = {};
    const bool japanese = lstrcmpW(argv[4], L"932") == 0;
    env.Ansi = env.Oem = japanese ? 932 : 936;
    env.Locale = japanese ? 0x411 : 0x804;
    env.Charset = japanese ? SHIFTJIS_CHARSET : GB2312_CHARSET;
    env.Zone.Bias = japanese ? -540 : -480;
    STARTUPINFOW startup = {sizeof(startup)};
    ProcessInfo process = {};
    const LONG status = create(&env, argv[2], &command[0], argv[5], 0, &startup, &process, nullptr, nullptr, nullptr, nullptr);
    Print("create_status", status);
    if (status < 0) return 3;
    const DWORD wait = WaitForSingleObject(process.hProcess, 20000);
    DWORD exitCode = 0;
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, 124); WaitForSingleObject(process.hProcess, 5000); }
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) exitCode = 125;
    Print("probe_exit", exitCode);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
