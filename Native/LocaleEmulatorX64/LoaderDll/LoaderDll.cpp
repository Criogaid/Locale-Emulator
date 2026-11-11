
#include "../LocaleEmulatorPlus/support/Support.h"
#include "LoaderDll.h"
#include "Environment.h"

#if LEP_LOADER_DLL


static NTSTATUS LepFillFirstDllBootstrapData(
    PVOID LocalImage,
    PVOID LdrLoadDllAddress,
    PBYTE LdrLoadDllBackup,
    ULONG OriginalProtect,
    ULONG InjectionFlags,
    ULONG PayloadOffset,
    ULONG PayloadSize
)
{
    PLEP_FIRST_DLL_BOOTSTRAP_DATA Data = (PLEP_FIRST_DLL_BOOTSTRAP_DATA)
        LookupExportTable(LocalImage, LEP_FIRST_DLL_BOOTSTRAP_EXPORT);
    if (Data == nullptr)
        return STATUS_ENTRYPOINT_NOT_FOUND;

    ZeroMemory(Data, sizeof(*Data));
    Data->Magic = LEP_FIRST_DLL_BOOTSTRAP_MAGIC;
    Data->InjectionFlags = InjectionFlags;
    Data->BackupSize = LDR_LOAD_DLL_BACKUP_SIZE;
    Data->OriginalProtect = OriginalProtect;
    Data->LdrLoadDllAddress = LdrLoadDllAddress;
    Data->PayloadOffset = PayloadOffset;
    Data->PayloadSize = PayloadSize;
    CopyMemory(Data->Backup, LdrLoadDllBackup, LDR_LOAD_DLL_BACKUP_SIZE);

    return STATUS_SUCCESS;
}

static NTSTATUS LepCreateBootstrapPayload(
    PLE_RUNTIME_INPUT Environment,
    PCWSTR DllPath,
    PLEP_BOOTSTRAP_PAYLOAD* Payload,
    PULONG PayloadSize
)
{
    if (Environment == nullptr || DllPath == nullptr || Payload == nullptr || PayloadSize == nullptr)
        return STATUS_INVALID_PARAMETER;
    LE_RUNTIME_CONFIG Resolved;
    NTSTATUS Status = LepResolveEnvironment(Environment, &Resolved);
    if (NT_FAILED(Status))
        return Status;
    ULONG EnvironmentSize = sizeof(Resolved);
    // Validated by LepResolveEnvironment above, so offset and terminator are in range.
    PCWSTR TimezoneId = (PCWSTR)((const BYTE*)Environment + Environment->TimezoneIdOffset);

    WCHAR DirPath[MAX_NTPATH];
    ULONG_PTR Length = StrLengthW(DllPath);
    if (Length + 1 > countof(DirPath))
    {
        return STATUS_NAME_TOO_LONG;
    }
    CopyMemory(DirPath, DllPath, (Length + 1) * sizeof(WCHAR));
    for (ULONG_PTR Index = Length; Index != 0; --Index)
    {
        if (DirPath[Index - 1] == L'\\')
        {
            DirPath[Index] = 0;
            break;
        }
    }

    ULONG Required = LepBootstrapPayloadSize(EnvironmentSize, DllPath, DirPath, TimezoneId);
    if (Required == 0)
    {
        return STATUS_BUFFER_OVERFLOW;
    }
    PLEP_BOOTSTRAP_PAYLOAD LocalPayload = (PLEP_BOOTSTRAP_PAYLOAD)AllocateMemory(Required);
    if (LocalPayload == nullptr)
    {
        return STATUS_NO_MEMORY;
    }

    Status = LepBuildBootstrapPayload(LocalPayload, Required, &Resolved, EnvironmentSize,
                                      DllPath, DirPath, TimezoneId);
    if (NT_FAILED(Status))
    {
        FreeMemory(LocalPayload);
        return Status;
    }
    *Payload = LocalPayload;
    *PayloadSize = Required;
    return STATUS_SUCCESS;
}




#endif

#define LEP_CORE_DLL_NAME L"LocaleEmulator.dll"


// Inject the core DLL into a freshly created suspended process.  The first
// loader-side LdrLoadDll call enters the
// mapped LoadFirstDll trampoline, which restores the original bytes and runs
// LEP initialization before continuing the loader call.
static NTSTATUS LepCreateProcessWithHook(
    PLE_RUNTIME_INPUT      EnvironmentBlock,
    PCWSTR                 DllPath,
    PCWSTR                 ApplicationName,
    PWSTR                 CommandLine,
    PCWSTR                CurrentDirectory,
    ULONG                 CreationFlags,
    LPSTARTUPINFOW        StartupInfo,
    LPPROCESS_INFORMATION ProcessInformation,
    LPSECURITY_ATTRIBUTES ProcessAttributes,
    LPSECURITY_ATTRIBUTES ThreadAttributes,
    PVOID                 Environment,
    HANDLE                Token
)
{
	PLEP_BOOTSTRAP_PAYLOAD Payload = nullptr;
	ULONG PayloadSize = 0;
	NTSTATUS Status = LepCreateBootstrapPayload(EnvironmentBlock, DllPath, &Payload, &PayloadSize);
	if (NT_FAILED(Status))
		return Status;

	PROCESS_INFORMATION ProcInfo = {};
	PVOID LocalImage = nullptr;
	PVOID RemoteImage = nullptr;
	PVOID LdrLoadDllAddress = nullptr;
	ULONG LdrLoadDllProtect = 0;
	BOOL LdrLoadDllWritable = FALSE;
	BYTE Backup[LDR_LOAD_DLL_BACKUP_SIZE];

	Status = Ps::CreateProcess(ApplicationName, CommandLine, CurrentDirectory,
		CreationFlags | CREATE_SUSPENDED, StartupInfo, &ProcInfo,
		ProcessAttributes, ThreadAttributes, Environment, Token);
	if (NT_FAILED(Status))
	{
		FreeMemory(Payload);
		return Status;
	}

	LdrLoadDllAddress = EATLookupRoutineByHashPNoFix(GetNtdllHandle(), lep::HashExportName("LdrLoadDll"));
	if (LdrLoadDllAddress == nullptr)
	{
		Status = STATUS_ENTRYPOINT_NOT_FOUND;
		goto FAIL_NEW;
	}

	Status = LoadPeImage(DllPath, &LocalImage, nullptr, 0);
	if (NT_FAILED(Status))
		goto FAIL_NEW;
	ULONG_PTR ImageSize = ImageGetSizeOfImage(LocalImage);
	if (ImageSize > LEP_BOOTSTRAP_IMAGE_MAX_SIZE)
	{
		Status = STATUS_BUFFER_OVERFLOW;
		goto FAIL_NEW;
	}
	ULONG_PTR PayloadOffset = ROUND_UP(ImageSize, 16);
	Status = AllocVirtualMemoryEx(ProcInfo.hProcess, &RemoteImage, PayloadOffset + PayloadSize);
	if (NT_FAILED(Status))
		goto FAIL_NEW;

	Status = ReadMemory(ProcInfo.hProcess, LdrLoadDllAddress, Backup, sizeof(Backup));
	if (NT_FAILED(Status))
		goto FAIL_NEW;
	Status = ProtectVirtualMemory(LdrLoadDllAddress, sizeof(Backup), PAGE_EXECUTE_READWRITE,
		&LdrLoadDllProtect, ProcInfo.hProcess);
	if (NT_FAILED(Status))
		goto FAIL_NEW;
	LdrLoadDllWritable = TRUE;

	RelocPeImage(LocalImage, LocalImage, nullptr, RemoteImage);
	Status = LepFillFirstDllBootstrapData(LocalImage, LdrLoadDllAddress, Backup,
		LdrLoadDllProtect, LEP_INJECT_FULL, (ULONG)PayloadOffset, PayloadSize);
	if (NT_FAILED(Status))
		goto FAIL_NEW;
	Status = WriteMemory(ProcInfo.hProcess, RemoteImage, LocalImage, ImageSize);
	if (NT_SUCCESS(Status))
		Status = WriteMemory(ProcInfo.hProcess, PtrAdd(RemoteImage, PayloadOffset), Payload, PayloadSize);
	if (NT_FAILED(Status))
		goto FAIL_NEW;

	PVOID LocalLoadFirstDll = LookupExportTable(LocalImage, "LoadFirstDll");
	if (LocalLoadFirstDll == nullptr)
	{
		Status = STATUS_ENTRYPOINT_NOT_FOUND;
		goto FAIL_NEW;
	}
	PVOID RemoteLoadFirstDll = PtrAdd(RemoteImage, PtrOffset(LocalLoadFirstDll, LocalImage));
	BYTE Jump[16] = {};
#if ML_AMD64
	Jump[0] = 0xFF;
	Jump[1] = 0x25;
	*(PVOID*)&Jump[6] = RemoteLoadFirstDll;
#else
	Jump[0] = JUMP;
	*(LONG*)&Jump[1] = (LONG)PtrOffset(RemoteLoadFirstDll, PtrAdd(LdrLoadDllAddress, 5));
#endif
	Status = WriteMemory(ProcInfo.hProcess, LdrLoadDllAddress, Jump, sizeof(Backup));
	if (NT_FAILED(Status))
		goto FAIL_NEW;

	UnloadPeImage(LocalImage);
	FreeMemory(Payload);
	if (FLAG_OFF(CreationFlags, CREATE_SUSPENDED))
		Status = NtResumeProcess(ProcInfo.hProcess);
	if (NT_FAILED(Status))
		goto FAIL_NEW_NO_LOCAL;

	if (ProcessInformation != nullptr)
		*ProcessInformation = ProcInfo;
	else
	{
		NtClose(ProcInfo.hProcess);
		NtClose(ProcInfo.hThread);
	}
	return STATUS_SUCCESS;

FAIL_NEW:
	if (Payload != nullptr)
		FreeMemory(Payload);
	if (LocalImage != nullptr)
		UnloadPeImage(LocalImage);
FAIL_NEW_NO_LOCAL:
	if (LdrLoadDllWritable)
	{
		ULONG IgnoredProtect;
		ProtectVirtualMemory(LdrLoadDllAddress, sizeof(Backup), LdrLoadDllProtect,
			&IgnoredProtect, ProcInfo.hProcess);
	}
	if (RemoteImage != nullptr)
		Mm::FreeVirtualMemory(RemoteImage, ProcInfo.hProcess);
	NtTerminateProcess(ProcInfo.hProcess, Status);
	NtClose(ProcInfo.hProcess);
	NtClose(ProcInfo.hThread);
	return Status;
}



EXTC
NTSTATUS
NTAPI
LeCreateProcess(
    PLE_RUNTIME_INPUT       EnvironmentBlock,
    PCWSTR                  ApplicationName,
    PWSTR                   CommandLine,
    PCWSTR                  CurrentDirectory,
    ULONG                   CreationFlags,
    LPSTARTUPINFOW          StartupInfo,
    LPPROCESS_INFORMATION   ProcessInformation,
    LPSECURITY_ATTRIBUTES   ProcessAttributes,
    LPSECURITY_ATTRIBUTES   ThreadAttributes,
    PVOID                   Environment,
    HANDLE                  Token
 )
{
    static WCHAR Dll[] = LEP_CORE_DLL_NAME;
    PLDR_MODULE Module = FindLdrModuleByHandle(&__ImageBase);
    if (Module == nullptr || Module->FullDllName.Length < Module->BaseDllName.Length)
        return STATUS_DLL_INIT_FAILED;

    ULONG_PTR Length = Module->FullDllName.Length - Module->BaseDllName.Length;
    PWSTR DllFullPath = (PWSTR)AllocStack(Length + sizeof(Dll));
    CopyMemory(DllFullPath, Module->FullDllName.Buffer, Length);
    CopyStruct(PtrAdd(DllFullPath, Length), Dll, sizeof(Dll));

    return LepCreateProcessWithHook(
        EnvironmentBlock,
        DllFullPath,
        ApplicationName,
        CommandLine,
        CurrentDirectory,
        CreationFlags,
        StartupInfo,
        ProcessInformation,
        ProcessAttributes,
        ThreadAttributes,
        Environment,
        Token);
}
