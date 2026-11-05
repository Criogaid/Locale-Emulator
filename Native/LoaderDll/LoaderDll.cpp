#include "ml.cpp"
#include "LoaderDll.h"


// 自定义入口必须先初始化官方 GS cookie，再进入任何受保护的导出函数。
extern "C" __declspec(safebuffers) BOOL WINAPI LeLoaderEntry(PVOID, DWORD reason, PVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        __security_init_cookie();

    return TRUE;
}

// The public blob has relative offsets but no byte-count parameter.
static NTSTATUS GetLebLength(PLEB Leb, ULONG_PTR& Length)
{
    const ULONG_PTR Prefix = FIELD_OFFSET(LEB, RegistryReplacement);
    const ULONG_PTR Maximum = MAXULONG_PTR - sizeof(LEPEB);
    Length = Prefix;
    if (Leb == nullptr)
        return STATUS_SUCCESS;
    NTSTATUS Status = ValidateLeb(*Leb);
    FAIL_RETURN(Status);
    if (Leb->NumberOfRegistryRedirectionEntries > (Maximum - Prefix) / sizeof(REGISTRY_REDIRECTION_ENTRY64))
        return STATUS_INTEGER_OVERFLOW;
    const ULONG_PTR Count = static_cast<ULONG_PTR>(Leb->NumberOfRegistryRedirectionEntries);
    Length += Count * sizeof(REGISTRY_REDIRECTION_ENTRY64);
    auto Include = [&](ULONG64 Offset, ULONG64 Size) -> bool
    {
        if (Size == 0)
            return true;
        if (Offset > Maximum || Size > Maximum - Offset)
            return false;
        Length = ML_MAX(Length, static_cast<ULONG_PTR>(Offset + Size));
        return true;
    };
    for (ULONG_PTR Index = 0; Index < Count; ++Index)
    {
        const auto& Entry = Leb->RegistryReplacement[Index];
        if (!Include(Entry.Original.SubKey.Dummy, Entry.Original.SubKey.MaximumLength) ||
            !Include(Entry.Original.ValueName.Dummy, Entry.Original.ValueName.MaximumLength) ||
            !Include((ULONG64)Entry.Original.Data, Entry.Original.DataSize) ||
            !Include(Entry.Redirected.SubKey.Dummy, Entry.Redirected.SubKey.MaximumLength) ||
            !Include(Entry.Redirected.ValueName.Dummy, Entry.Redirected.ValueName.MaximumLength) ||
            !Include((ULONG64)Entry.Redirected.Data, Entry.Redirected.DataSize))
            return STATUS_INTEGER_OVERFLOW;
    }
    return STATUS_SUCCESS;
}

EXTC
NTSTATUS
NTAPI
LeCreateProcess(
	PLEB                    Leb,
	PCWSTR                  ApplicationName,
	PWSTR                   CommandLine,
	PCWSTR                  CurrentDirectory,
	ULONG                   CreationFlags,
	LPSTARTUPINFOW          StartupInfo,
	PML_PROCESS_INFORMATION ProcessInformation,
	LPSECURITY_ATTRIBUTES   ProcessAttributes,
	LPSECURITY_ATTRIBUTES   ThreadAttributes,
	PVOID                   Environment,
	HANDLE                  Token
	)
{
	NTSTATUS                Status;
	PVOID                   LeDllHandle;
	ULONG_PTR               Length;
	PWSTR                   DllFullPath;
	PLDR_MODULE             Module;
	PLEPEB                  LePeb;

	static WCHAR Dll[] = L"LocaleEmulator.dll";

    ULONG_PTR LebLength;
    Status = GetLebLength(Leb, LebLength);
    FAIL_RETURN(Status);

	Module = FindLdrModuleByHandle(nullptr);

	Length = Module->FullDllName.Length - Module->BaseDllName.Length;
	DllFullPath = (PWSTR)AllocStack(Length + sizeof(Dll));
	CopyMemory(DllFullPath, Module->FullDllName.Buffer, Length);
	CopyStruct(PtrAdd(DllFullPath, Length), Dll, sizeof(Dll));

	LePeb = nullptr;

	LOOP_ONCE
	{
		if (Leb == nullptr)
		break;

    ULONG_PTR ExtraSize = LebLength - FIELD_OFFSET(LEB, RegistryReplacement);

	LePeb = OpenOrCreateLePeb((ULONG_PTR)CurrentTeb()->ClientId.UniqueProcess, TRUE, ExtraSize);
	if (LePeb == nullptr)
	{
		Status = STATUS_NONE_MAPPED;
		break;
	}

    CopyMemory(&LePeb->Leb, Leb, LebLength);

	LePeb->LdrLoadDllAddress = LookupExportTable(GetNtdllHandle(), NTDLL_LdrLoadDll);
	LePeb->LdrLoadDllBackupSize = LDR_LOAD_DLL_BACKUP_SIZE;
	CopyMemory(LePeb->LdrLoadDllBackup, LePeb->LdrLoadDllAddress, LDR_LOAD_DLL_BACKUP_SIZE);

	ULONG_PTR Length = (StrLengthW(DllFullPath) + 1) * sizeof(WCHAR);
	CopyMemory(LePeb->LeDllFullPath, DllFullPath, ML_MIN(Length, sizeof(LePeb->LeDllFullPath)));

  Length = Module->FullDllName.Length + sizeof(WCHAR) - Module->BaseDllName.Length;

  Length = ML_MIN(Length, sizeof(LePeb->LeDllFullPath));
  CopyMemory(LePeb->LeDllDirPath, Module->FullDllName.Buffer, Length);
  LePeb->LeDllDirPath[Length / sizeof(WCHAR) - 1] = 0;
	}

    if (Leb != nullptr && LePeb == nullptr)
        return Status;

	UNICODE_STRING DllFullPathString;
	TEB_ACTIVE_FRAME frame(LE_LOADER_PROCESS);

	frame.Data = (ULONG_PTR)LePeb;
	frame.Push();

	RtlInitUnicodeString(&DllFullPathString, DllFullPath);

	Status = LdrLoadDll(nullptr, nullptr, &DllFullPathString, &LeDllHandle);
	CloseLePeb(LePeb);

	FAIL_RETURN(Status);

	Status = Ps::CreateProcess(
		ApplicationName,
		CommandLine,
		CurrentDirectory,
		CreationFlags,
		StartupInfo,
		ProcessInformation,
		ProcessAttributes,
		ThreadAttributes,
		Environment,
		Token
		);

	return Status;
}
