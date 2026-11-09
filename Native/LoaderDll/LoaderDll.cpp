#include "ml.cpp"
#include "LoaderDll.h"

// 自定义入口必须先初始化官方 GS cookie，再进入任何受保护的导出函数。
extern "C" __declspec(safebuffers) BOOL WINAPI LeLoaderEntry(PVOID, DWORD reason, PVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        __security_init_cookie();
    return TRUE;
}

static NTSTATUS PrepareRuntimeEnvironment(PLE_RUNTIME_INPUT Input, PLEPEB LePeb)
{
    const BYTE* Base = (const BYTE*)Input;
    ULONG TimezoneCharacters;

    if (Input == nullptr || LePeb == nullptr || !LeValidateRuntimeInput(Input))
        return STATUS_INVALID_PARAMETER;
    if (Input->RegistryEntriesCount != 0)
        return STATUS_NOT_SUPPORTED;

    TimezoneCharacters = Input->TimezoneIdLength / sizeof(WCHAR);
    if (TimezoneCharacters >= countof(LePeb->TimezoneId))
        return STATUS_NAME_TOO_LONG;

    ZeroMemory(&LePeb->Environment, sizeof(LePeb->Environment));
    LePeb->Environment.Size = sizeof(LePeb->Environment);
    LePeb->Environment.Version = LE_RUNTIME_CONFIG_VERSION;
    LePeb->Environment.AnsiCodePage = Input->AnsiCodePage;
    LePeb->Environment.OemCodePage = Input->OemCodePage;
    LePeb->Environment.LocaleID = Input->LocaleID;
    LePeb->Environment.FontCharset = Input->FontCharset;
    LePeb->Environment.RegistryMode = Input->RegistryMode;
    LePeb->Environment.HookUILanguageMode = Input->HookUILanguageMode;
    CopyMemory(LePeb->TimezoneId, Base + Input->TimezoneIdOffset,
        (TimezoneCharacters + 1) * sizeof(WCHAR));
    return STATUS_SUCCESS;
}

EXTC
NTSTATUS
NTAPI
LeCreateProcess(
    PLE_RUNTIME_INPUT       Environment,
    PCWSTR                  ApplicationName,
    PWSTR                   CommandLine,
    PCWSTR                  CurrentDirectory,
    ULONG                   CreationFlags,
    LPSTARTUPINFOW          StartupInfo,
    PLE_PROCESS_INFORMATION ProcessInformation,
    LPSECURITY_ATTRIBUTES   ProcessAttributes,
    LPSECURITY_ATTRIBUTES   ThreadAttributes,
    PVOID                   EnvironmentBlock,
    HANDLE                  Token
    )
{
    NTSTATUS Status;
    PVOID LeDllHandle;
    ULONG_PTR Length;
    PWSTR DllFullPath;
    PLDR_MODULE Module;
    PLEPEB LePeb = nullptr;
    static WCHAR Dll[] = L"LocaleEmulator.dll";

    if (Environment == nullptr || !LeValidateRuntimeInput(Environment))
        return STATUS_INVALID_PARAMETER;

    Module = FindLdrModuleByHandle(nullptr);
    if (Module == nullptr || Module->FullDllName.Length < Module->BaseDllName.Length)
        return STATUS_DLL_INIT_FAILED;

    Length = Module->FullDllName.Length - Module->BaseDllName.Length;
    DllFullPath = (PWSTR)AllocStack(Length + sizeof(Dll));
    CopyMemory(DllFullPath, Module->FullDllName.Buffer, Length);
    CopyStruct(PtrAdd(DllFullPath, Length), Dll, sizeof(Dll));

    LePeb = OpenOrCreateLePeb((ULONG_PTR)CurrentTeb()->ClientId.UniqueProcess, TRUE, 0);
    if (LePeb == nullptr)
        return STATUS_NONE_MAPPED;

    Status = PrepareRuntimeEnvironment(Environment, LePeb);
    if (NT_FAILED(Status))
    {
        CloseLePeb(LePeb);
        return Status;
    }

    LePeb->LdrLoadDllAddress = LookupExportTable(GetNtdllHandle(), NTDLL_LdrLoadDll);
    if (LePeb->LdrLoadDllAddress == nullptr)
    {
        CloseLePeb(LePeb);
        return STATUS_ENTRYPOINT_NOT_FOUND;
    }
    LePeb->LdrLoadDllBackupSize = LDR_LOAD_DLL_BACKUP_SIZE;
    CopyMemory(LePeb->LdrLoadDllBackup, LePeb->LdrLoadDllAddress, LDR_LOAD_DLL_BACKUP_SIZE);

    Length = (StrLengthW(DllFullPath) + 1) * sizeof(WCHAR);
    if (Length > sizeof(LePeb->LeDllFullPath))
    {
        CloseLePeb(LePeb);
        return STATUS_NAME_TOO_LONG;
    }
    CopyMemory(LePeb->LeDllFullPath, DllFullPath, Length);

    Length = Module->FullDllName.Length + sizeof(WCHAR) - Module->BaseDllName.Length;
    if (Length > sizeof(LePeb->LeDllDirPath))
    {
        CloseLePeb(LePeb);
        return STATUS_NAME_TOO_LONG;
    }
    CopyMemory(LePeb->LeDllDirPath, Module->FullDllName.Buffer, Length);
    LePeb->LeDllDirPath[Length / sizeof(WCHAR) - 1] = 0;

    UNICODE_STRING DllFullPathString;
    TEB_ACTIVE_FRAME frame(LE_LOADER_PROCESS);
    frame.Data = (ULONG_PTR)LePeb;
    frame.Push();

    RtlInitUnicodeString(&DllFullPathString, DllFullPath);
    Status = LdrLoadDll(nullptr, nullptr, &DllFullPathString, &LeDllHandle);
    CloseLePeb(LePeb);
    if (NT_FAILED(Status))
        return Status;

    return Ps::CreateProcess(
        ApplicationName,
        CommandLine,
        CurrentDirectory,
        CreationFlags,
        StartupInfo,
        ProcessInformation,
        ProcessAttributes,
        ThreadAttributes,
        EnvironmentBlock,
        Token);
}
