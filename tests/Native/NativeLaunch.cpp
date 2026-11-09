#include <Windows.h>
#include "../../Native/RuntimeEnvironment.h"
#include "WindowsCommandLine.h"

// Loader 的公共 ABI 只传递固定宽度字段和相对偏移；进程信息仍由 Win32 SDK 定义。
struct ProcessInfo : PROCESS_INFORMATION { void* FirstCall; };
struct EnvironmentFixture
{
    LE_RUNTIME_INPUT Input;
    WCHAR Timezone[32];
    WCHAR Standard[1];
    WCHAR Daylight[1];
};
static_assert(sizeof(LE_RUNTIME_INPUT) == 56, "Runtime input ABI");
using Create = NTSTATUS(WINAPI*)(LE_RUNTIME_INPUT*, PCWSTR, PWSTR, PCWSTR, ULONG, STARTUPINFOW*, ProcessInfo*,
    SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, void*, HANDLE);

static void Print(const char* key, DWORD value)
{
    char line[128];
    DWORD written;
    const int length = wsprintfA(line, "%s=%lu (0x%08lX)\r\n", key, value, value);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, length, &written, nullptr);
}

int wmain(int argc, WCHAR** argv)
{
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

    EnvironmentFixture environment = {};
    const bool japanese = lstrcmpW(argv[4], L"932") == 0;
    environment.Input.Size = sizeof(environment);
    environment.Input.Version = LE_RUNTIME_INPUT_VERSION;
    environment.Input.AnsiCodePage = japanese ? 932u : 936u;
    environment.Input.OemCodePage = environment.Input.AnsiCodePage;
    environment.Input.LocaleID = japanese ? 0x411u : 0x804u;
    environment.Input.FontCharset = japanese ? SHIFTJIS_CHARSET : GB2312_CHARSET;
    environment.Input.RegistryMode = LE_RUNTIME_REGISTRY_MODE_STANDARD;
    environment.Input.TimezoneIdOffset = FIELD_OFFSET(EnvironmentFixture, Timezone);
    environment.Input.TimezoneIdLength = (sizeof(L"Tokyo Standard Time") - sizeof(WCHAR));
    environment.Input.StandardNameOffset = FIELD_OFFSET(EnvironmentFixture, Standard);
    environment.Input.DaylightNameOffset = FIELD_OFFSET(EnvironmentFixture, Daylight);
    CopyMemory(environment.Timezone, L"Tokyo Standard Time", sizeof(L"Tokyo Standard Time"));
    if (!LeValidateRuntimeInput(&environment.Input)) { Print("input_error", ERROR_INVALID_DATA); return 3; }

    STARTUPINFOW startup = {sizeof(startup)};
    ProcessInfo process = {};
    const NTSTATUS status = create(&environment.Input, argv[2], &command[0], argv[5], 0, &startup, &process,
        nullptr, nullptr, nullptr, nullptr);
    Print("create_status", static_cast<DWORD>(status));
    if (status < 0) return 4;
    const DWORD wait = WaitForSingleObject(process.hProcess, 20000);
    DWORD exitCode = 0;
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(process.hProcess, 124);
        WaitForSingleObject(process.hProcess, 5000);
    }
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) exitCode = 125;
    Print("probe_exit", exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
