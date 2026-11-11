#include "stdafx.h"

ML_OVERLOAD_NEW

#pragma comment(linker, "/ENTRY:LeDllEntry")
#pragma comment(linker, "/EXPORT:GetFileAttributesA=_FuckStupid2DJGame@4")

EXTC ULONG NTAPI FuckStupid2DJGame(PCSTR FileName)
{
    return FILE_ATTRIBUTE_NORMAL;
/*
    PVOID Rubbish;
    PLDR_MODULE Garbage;

    Garbage = FindLdrModuleByHandle(nullptr);
    Rubbish = _ReturnAddress();

    if (PtrOffset(Rubbish, Garbage->EntryPoint) < 100 && StrCompareA(FileName, "2djgame.txt") == 0)
        return FILE_ATTRIBUTE_NORMAL;

    return Io::QueryFileAttributes(ml::String::Decode(FileName, StrLengthA(FileName), CP_ACP));
*/
}

PLeGlobalData g_GlobalData;

ForceInline VOID LeSetGlobalData(PLeGlobalData GlobalData)
{
    g_GlobalData = GlobalData;
}

ForceInline VOID LeReleaseGlobalData()
{
    SafeDeleteT(g_GlobalData);
}

BOOL NTAPI DelayInitDllEntry(PVOID BaseAddress, ULONG Reason, PVOID Reserved)
{
    BOOL Success;
    PLDR_MODULE Module = FindLdrModuleByHandle(BaseAddress);
    PIMAGE_NT_HEADERS NtHeaders;

    NtHeaders = PtrAdd((PIMAGE_NT_HEADERS)BaseAddress, ((PIMAGE_DOS_HEADER)BaseAddress)->e_lfanew);
    Module->EntryPoint = PtrAdd(BaseAddress, NtHeaders->OptionalHeader.AddressOfEntryPoint);

    Success = ((API_POINTER(DelayInitDllEntry))Module->EntryPoint)(BaseAddress, Reason, Reserved);

    if (Reason == DLL_PROCESS_ATTACH && !Success)
        return Success;

    switch (Reason)
    {
        case DLL_PROCESS_ATTACH:
            if (!Success)
                return Success;
        case DLL_PROCESS_DETACH:
            break;

        default:
            return Success;
    }

    LeGetGlobalData()->HookModule(BaseAddress, &Module->BaseDllName, Reason == DLL_PROCESS_ATTACH);

    return Success;
}

NTSTATUS GetNlsFile(PUNICODE_STRING NlsFile, ULONG NlsIndex, PCWSTR SubKey)
{
    BOOL        Success;
    WCHAR       NlsIndexBuffer[16];
    NTSTATUS    Status;
    PKEY_VALUE_PARTIAL_INFORMATION FileName;

    _snwprintf(NlsIndexBuffer, countof(NlsIndexBuffer), L"%d", NlsIndex);

    Status = GetKeyValue(REGKEY_ROOT, SubKey, NlsIndexBuffer, &FileName);
    FAIL_RETURN(Status);

    Success = RtlCreateUnicodeString(NlsFile, (PWSTR)FileName->Data);
    FreeMemoryP(FileName);

    return Success ? STATUS_SUCCESS : STATUS_NO_MEMORY;
}

NTSTATUS GetLangFile(PUNICODE_STRING LangFile, ULONG LangIndex, PCWSTR SubKey)
{
    BOOL        Success;
    WCHAR       LangIndexBuffer[16];
    NTSTATUS    Status;
    PKEY_VALUE_PARTIAL_INFORMATION FileName;

    _snwprintf(LangIndexBuffer, countof(LangIndexBuffer), L"%04x", LangIndex);

    Status = GetKeyValue(REGKEY_ROOT, SubKey, LangIndexBuffer, &FileName);
    FAIL_RETURN(Status);

    Success = RtlCreateUnicodeString(LangFile, (PWSTR)FileName->Data);
    FreeMemoryP(FileName);

    return Success ? STATUS_SUCCESS : STATUS_NO_MEMORY;
}

NTSTATUS ReadFileInSystemDirectory(NtFileMemory &File, PUNICODE_STRING Path)
{
    PWSTR       Buffer;
    ULONG_PTR   Length;
    NTSTATUS    Status;

    Length = sizeof(ROOTDIR_SYSTEM32) + Path->Length + sizeof(WCHAR);
    Buffer = (PWSTR)AllocateMemoryP(Length);
    if (Buffer == nullptr)
        return STATUS_NO_MEMORY;

    Length = CONST_STRLEN(ROOTDIR_SYSTEM32);
    CopyMemory(Buffer, ROOTDIR_SYSTEM32, Length * sizeof(WCHAR));
    CopyMemory(Buffer + Length, Path->Buffer, Path->Length);
    Buffer[Length + Path->Length / sizeof(WCHAR)] = 0;

    Status = File.Open(Buffer, NFD_NOT_RESOLVE_PATH);

    FreeMemoryP(Buffer);

    return Status;
}

NTSTATUS LeGlobalData::Initialize()
{
    BOOL            IsLoader;
    PLEPEB          LePeb;
    PLDR_MODULE     Ntdll;
    PPEB_BASE       Peb;
    NTSTATUS        Status;
    NLSTABLEINFO    NlsTableInfo;
    UNICODE_STRING  SystemDirectory, NlsFileName, OemNlsFileName, LangFileName, Win32U;
    PKEY_VALUE_PARTIAL_INFORMATION IndexValue;

    IsLoader = IsLeLoader();

    Wow64 = Ps::IsWow64Process();
    Ntdll = GetNtdllLdrModule();

    LOOP_ONCE
    {
        LePeb = OpenOrCreateLePeb();
        if (LePeb == nullptr)
        {
            PLDR_MODULE Ledll;
            PVOID ReloadedNtdll;

            LePeb = GetLePeb();
            InitDefaultEnvironment(&LePeb->Environment, LePeb->TimezoneId, countof(LePeb->TimezoneId));
            Ledll = FindLdrModuleByHandle(&__ImageBase);
            if (Ledll != nullptr)
            {
                PUNICODE_STRING FullDllName = &Ledll->FullDllName;
                CopyMemory(LePeb->LeDllFullPath, FullDllName->Buffer, FullDllName->Length + sizeof(WCHAR));
                CopyMemory(LePeb->LeDllDirPath, FullDllName->Buffer,
                    FullDllName->Length + sizeof(WCHAR) - Ledll->BaseDllName.Length);
                LePeb->LeDllDirPath[(FullDllName->Length - Ledll->BaseDllName.Length) / sizeof(WCHAR)] = 0;
            }

            Status = LoadPeImage(Ntdll->FullDllName.Buffer, &ReloadedNtdll, nullptr, LOAD_PE_IGNORE_RELOC);
            if (NT_SUCCESS(Status))
            {
                PVOID LdrLoadDllAddress = LookupExportTable(ReloadedNtdll, NTDLL_LdrLoadDll);
                LePeb->LdrLoadDllAddress = PtrAdd(LdrLoadDllAddress, PtrOffset(Ntdll->DllBase, ReloadedNtdll));
                CopyMemory(LePeb->LdrLoadDllBackup, LdrLoadDllAddress, LDR_LOAD_DLL_BACKUP_SIZE);
                LePeb->LdrLoadDllBackupSize = LDR_LOAD_DLL_BACKUP_SIZE;
                UnloadPeImage(ReloadedNtdll);
            }
        }
        else
        {
            if (LePeb->Environment.Size != sizeof(LePeb->Environment) ||
                LePeb->Environment.Version != LE_RUNTIME_CONFIG_VERSION ||
                LePeb->Environment.AnsiCodePage > 0xFFFFu ||
                LePeb->Environment.OemCodePage > 0xFFFFu ||
                LePeb->Environment.FontCharset > 0xFFu ||
                LePeb->Environment.RegistryMode > LE_RUNTIME_REGISTRY_MODE_ADVANCED ||
                LePeb->Environment.HookUILanguageMode > LE_RUNTIME_UI_LANGUAGE_MODE_ADVANCED)
            {
                Status = STATUS_INVALID_PARAMETER;
            }
            else
            {
                Status = ResolveRuntimeTimeZone(LePeb->TimezoneId, LePeb->StandardNameOverride,
                    LePeb->DaylightNameOverride, &LePeb->Environment.Timezone);
                if (NT_SUCCESS(Status) && !LeValidateRuntimeConfig(&LePeb->Environment))
                    Status = STATUS_INVALID_PARAMETER;
            }
            if (NT_SUCCESS(Status))
            {
                *GetLePeb() = *LePeb;
                GetLePeb()->Section = nullptr;
            }
            NtClose(LePeb->Section);
            CloseLePeb(LePeb);
            FAIL_RETURN(Status);
        }

        Status = this->InitRegistryRedirectionMode(GetEnvironment()->RegistryMode);
        FAIL_RETURN(Status);
        if (IsLoader)
            break;


        if (IsLoader)
            break;

        Status = this->TextMetricCache.Initialize();
        FAIL_RETURN(Status);

        PVOID           NlsBaseAddress;
        LCID            DefaultLocaleID;
        LARGE_INTEGER   DefaultCasingTableSize;

        Status = NtInitializeNlsFiles(&NlsBaseAddress, &DefaultLocaleID, &DefaultCasingTableSize);
        FAIL_RETURN(Status);

        this->GetLePeb()->OriginalLocaleID = DefaultLocaleID;

        NtUnmapViewOfSection(CurrentProcess, NlsBaseAddress);

        WriteLog(L"init leb %s", GetLePeb()->LeDllFullPath);

        SystemDirectory = Ntdll->FullDllName;
        SystemDirectory.Length -= Ntdll->BaseDllName.Length;

        Status = RtlDuplicateUnicodeString(RTL_DUPSTR_ADD_NULL, &SystemDirectory, &this->SystemDirectory);
        FAIL_RETURN(Status);

        ml::String Win32U_Path(SystemDirectory);
        Win32U_Path += L"\\win32u.dll";
        this->HasWin32U = Nt_GetFileAttributes(Win32U_Path.GetBuffer()) != INVALID_FILE_ATTRIBUTES;
        WriteLog(L"win32u: %s, has:%d", Win32U_Path.GetBuffer(), this->HasWin32U);

        RtlInitEmptyString(&NlsFileName, nullptr, 0);
        RtlInitEmptyString(&OemNlsFileName, nullptr, 0);
        RtlInitEmptyString(&LangFileName, nullptr, 0);

        SCOPE_EXIT
        {
            RtlFreeUnicodeString(&NlsFileName);
            RtlFreeUnicodeString(&OemNlsFileName);
            RtlFreeUnicodeString(&LangFileName);
        }
        SCOPE_EXIT_END;

        Status = GetNlsFile(&NlsFileName, GetEnvironment()->AnsiCodePage, REGPATH_CODEPAGE);
        FAIL_RETURN(Status);

        Status = GetNlsFile(&OemNlsFileName, GetEnvironment()->OemCodePage, REGPATH_CODEPAGE);
        FAIL_RETURN(Status);

        // Windows 10 1803 removed all keys under "HKLM\SYSTEM\CurrentControlSet\Control\Nls\Language".
        // Since the value of all keys are "l_intl.nls" since Windows XP, 
        // we suppose that MS is not going to change it in the near future.
        // 
        //Status = GetLangFile(&LangFileName, GetEnvironment()->LocaleID, REGPATH_LANGUAGE);
        //FAIL_RETURN(Status);
        RtlCreateUnicodeString(&LangFileName, L"l_intl.nls");

        NtFileMemory AnsiFile, OemFile, LangFile;

        Status = ReadFileInSystemDirectory(AnsiFile, &NlsFileName);
        FAIL_RETURN(Status);

        Status = ReadFileInSystemDirectory(OemFile, &OemNlsFileName);
        FAIL_RETURN(Status);

        Status = ReadFileInSystemDirectory(LangFile, &LangFileName);
        FAIL_RETURN(Status);

        AnsiCodePageOffset      = 0;
        OemCodePageOffset       = ROUND_UP(AnsiFile.GetSize32(), 16);
        UnicodeCaseTableOffset  = OemCodePageOffset + ROUND_UP(OemFile.GetSize32(), 16);

        Status = AllocVirtualMemory(&CodePageMapView, UnicodeCaseTableOffset + LangFile.GetSize32(), PAGE_READWRITE, MEM_COMMIT | MEM_TOP_DOWN);
        FAIL_RETURN(Status);

        CopyMemory(PtrAdd(CodePageMapView, AnsiCodePageOffset),     AnsiFile.GetBuffer(),   AnsiFile.GetSize32());
        CopyMemory(PtrAdd(CodePageMapView, OemCodePageOffset),      OemFile.GetBuffer(),    OemFile.GetSize32());
        CopyMemory(PtrAdd(CodePageMapView, UnicodeCaseTableOffset), LangFile.GetBuffer(),   LangFile.GetSize32());

        ProtectVirtualMemory(CodePageMapView, UnicodeCaseTableOffset + LangFile.GetSize32(), PAGE_READONLY);

        RtlInitNlsTables(
            (PUSHORT)PtrAdd(CodePageMapView, AnsiCodePageOffset),
            (PUSHORT)PtrAdd(CodePageMapView, OemCodePageOffset),
            (PUSHORT)PtrAdd(CodePageMapView, UnicodeCaseTableOffset),
            &NlsTableInfo
        );

        RtlResetRtlTranslations(&NlsTableInfo);

        WriteLog(L"reset nls");

        Peb = CurrentPeb();

        Peb->AnsiCodePageData       = (PUSHORT)PtrAdd(CodePageMapView, AnsiCodePageOffset);
        Peb->OemCodePageData        = (PUSHORT)PtrAdd(CodePageMapView, OemCodePageOffset);
        Peb->UnicodeCaseTableData   = (PUSHORT)PtrAdd(CodePageMapView, UnicodeCaseTableOffset);

        // LdrInitShimEngineDynamic(&__ImageBase);

        LdrRegisterDllNotification(0,
            [] (ULONG NotificationReason, PCLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
            {
                return ((PLeGlobalData)Context)->DllNotification(NotificationReason, NotificationData);
            },
            this,
            &DllNotificationCookie
        );
    }

    Status = InstallHookPort();
    WriteLog(L"inst hp: %08X", Status);
    FAIL_RETURN(Status);

    HookNtdllRoutines(Ntdll->DllBase);

    WriteLog(L"hook ntdll");

    if (IsLoader)
        return Status;

    PLDR_MODULE Kernel32Ldr;

    Kernel32Ldr = GetKernel32Ldr();
    if (Kernel32Ldr != nullptr)
    {
        if (FLAG_ON(Kernel32Ldr->Flags, LDRP_PROCESS_ATTACH_CALLED))
            HookKernel32Routines(Kernel32Ldr->DllBase);
        else
            Kernel32Ldr->EntryPoint = DelayInitDllEntry;
    }

    WriteLog(L"init %p", Status);

    return Status;
}

NTSTATUS LeGlobalData::InitRegistryRedirection(PREGISTRY_REDIRECTION_CONFIG Config, ULONG_PTR Count)
{
    NTSTATUS Status;
    PREGISTRY_REDIRECTION_ENTRY Entry;

    if (Config == nullptr && Count != 0)
        return STATUS_INVALID_PARAMETER;
    if (Count == 0)
        return STATUS_SUCCESS;

    REGISTRY_REDIRECTION_ENTRY LocalEntry;
    FOR_EACH(Config, Config, Count)
    {
        ULONG_PTR LastIndex;
        HANDLE OriginalKey = nullptr, RedirectedKey = nullptr;
        UNICODE_STRING KeyFullPath;

        Status = Reg::OpenKey(&OriginalKey, Config->Original.Root, KEY_QUERY_VALUE, Config->Original.SubKey);
        FAIL_CONTINUE(Status);
        if (Config->Redirected.Root != nullptr)
        {
            Status = Reg::OpenKey(&RedirectedKey, Config->Redirected.Root, KEY_QUERY_VALUE, Config->Redirected.SubKey);
            if (NT_FAILED(Status))
            {
                Reg::CloseKeyHandle(OriginalKey);
                continue;
            }
        }

        this->RegistryRedirectionEntry.Add(LocalEntry);
        LastIndex = this->RegistryRedirectionEntry.GetSize() - 1;
        Entry = &this->RegistryRedirectionEntry[LastIndex];
        Status = QueryRegKeyFullPath(OriginalKey, &KeyFullPath);
        Reg::CloseKeyHandle(OriginalKey);
        if (NT_FAILED(Status))
        {
            Reg::CloseKeyHandle(RedirectedKey);
            this->RegistryRedirectionEntry.Remove(LastIndex);
            continue;
        }
        Entry->Original.FullPath = KeyFullPath;
        RtlFreeUnicodeString(&KeyFullPath);

        if (RedirectedKey != nullptr)
        {
            Status = QueryRegKeyFullPath(RedirectedKey, &KeyFullPath);
            Reg::CloseKeyHandle(RedirectedKey);
            if (NT_FAILED(Status))
            {
                this->RegistryRedirectionEntry.Remove(LastIndex);
                continue;
            }
            Entry->Redirected.FullPath = KeyFullPath;
            RtlFreeUnicodeString(&KeyFullPath);
        }

        Entry->Original.Root = Config->Original.Root;
        Entry->Original.SubKey = Config->Original.SubKey;
        Entry->Original.ValueName = Config->Original.ValueName;
        Entry->Original.DataType = Config->Original.DataType;
        Entry->Original.Data = nullptr;
        Entry->Original.DataSize = 0;
        Entry->Redirected.Root = Config->Redirected.Root;
        Entry->Redirected.SubKey = Config->Redirected.SubKey;
        Entry->Redirected.ValueName = Config->Redirected.ValueName;
        Entry->Redirected.DataType = Config->Redirected.DataType;
        Entry->Redirected.Data = nullptr;
        Entry->Redirected.DataSize = 0;

        if (Config->Redirected.Data != nullptr && Config->Redirected.DataSize != 0)
        {
            Entry->Redirected.DataSize = Config->Redirected.DataSize;
            Entry->Redirected.Data = AllocateMemoryP(Entry->Redirected.DataSize);
            if (Entry->Redirected.Data == nullptr)
            {
                this->RegistryRedirectionEntry.Remove(LastIndex);
                continue;
            }
            CopyMemory(Entry->Redirected.Data, Config->Redirected.Data, Entry->Redirected.DataSize);
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS LeGlobalData::InitRegistryRedirectionMode(ULONG Mode)
{
    NTSTATUS Status;
    if (Mode > LE_RUNTIME_REGISTRY_MODE_ADVANCED)
        return STATUS_INVALID_PARAMETER;
    if (Mode == LE_RUNTIME_REGISTRY_MODE_NONE)
        return STATUS_SUCCESS;

    WCHAR Acp[16] = {}, Oemcp[16] = {}, Language[16] = {};
    const ULONG AcpLength = (ULONG)(_snwprintf(Acp, countof(Acp), L"%u", GetEnvironment()->AnsiCodePage) + 1) * sizeof(WCHAR);
    const ULONG OemcpLength = (ULONG)(_snwprintf(Oemcp, countof(Oemcp), L"%u", GetEnvironment()->OemCodePage) + 1) * sizeof(WCHAR);
    const ULONG LanguageLength = (ULONG)(_snwprintf(Language, countof(Language), L"%04X", GetEnvironment()->LocaleID & 0xFFFFu) + 1) * sizeof(WCHAR);
    const ULONG Hklm = (ULONG)(ULONG_PTR)HKEY_LOCAL_MACHINE;

    REGISTRY_REDIRECTION_CONFIG Entries[8] = {};
    auto SetEntry = [](REGISTRY_REDIRECTION_CONFIG& Entry, HKEY Root, PCWSTR SubKey, PCWSTR ValueName,
                        PVOID Data, ULONG_PTR DataSize, ULONG DataType)
    {
        Entry.Original.Root = Root;
        Entry.Original.SubKey = SubKey;
        Entry.Original.ValueName = ValueName;
        Entry.Original.DataType = DataType;
        Entry.Original.Data = nullptr;
        Entry.Original.DataSize = 0;
        Entry.Redirected.Root = Root;
        Entry.Redirected.SubKey = SubKey;
        Entry.Redirected.ValueName = ValueName;
        Entry.Redirected.DataType = DataType;
        Entry.Redirected.Data = Data;
        Entry.Redirected.DataSize = DataSize;
    };
    SetEntry(Entries[0], (HKEY)(ULONG_PTR)Hklm, REGPATH_LANGUAGE, REGKEY_INSTALL_LANGUAGE, Language, LanguageLength, REG_SZ);
    SetEntry(Entries[1], (HKEY)(ULONG_PTR)Hklm, REGPATH_LANGUAGE, REGKEY_DEFAULT_LANGUAGE, Language, LanguageLength, REG_SZ);
    SetEntry(Entries[2], (HKEY)(ULONG_PTR)Hklm, REGPATH_CODEPAGE, REGKEY_OEMCP, Oemcp, OemcpLength, REG_SZ);
    SetEntry(Entries[3], (HKEY)(ULONG_PTR)Hklm, REGPATH_CODEPAGE, REGKEY_ACP, Acp, AcpLength, REG_SZ);

    Status = InitRegistryRedirection(Entries, 4);
    return Status;
}

NTSTATUS LeGlobalData::InitAdvancedRegistryRedirection()
{
    if (AdvancedRegistryInitialized)
        return STATUS_SUCCESS;

    PLDR_MODULE Kernel32 = GetKernel32Ldr();
    if (Kernel32 == nullptr)
        return STATUS_DLL_NOT_FOUND;
    typedef INT(WINAPI* LCID_TO_LOCALE_NAME)(LCID, LPWSTR, INT, DWORD);
    auto Convert = (LCID_TO_LOCALE_NAME)LookupExportTable(Kernel32->DllBase, KERNEL32_LCIDToLocaleName);
    if (Convert == nullptr)
        return STATUS_PROCEDURE_NOT_FOUND;

    WCHAR LocaleHex[9] = {}, LocaleName[LOCALE_NAME_MAX_LENGTH] = {};
    static const WCHAR Hex[] = L"0123456789ABCDEF";
    for (ULONG Index = 0; Index != 8; ++Index)
        LocaleHex[Index] = Hex[(GetEnvironment()->LocaleID >> ((7 - Index) * 4)) & 0xF];
    const INT LocaleNameLength = Convert(GetEnvironment()->LocaleID, LocaleName, countof(LocaleName), 0);
    if (LocaleNameLength <= 1)
        return STATUS_INVALID_PARAMETER;
    const ULONG LocaleNameBytes = (ULONG)LocaleNameLength * sizeof(WCHAR);
    const ULONG PreferredBytes = LocaleNameBytes + sizeof(WCHAR);
    const HKEY Hkcu = HKEY_CURRENT_USER;

    REGISTRY_REDIRECTION_CONFIG Entries[4] = {};
    auto SetEntry = [](REGISTRY_REDIRECTION_CONFIG& Entry, HKEY Root, PCWSTR SubKey, PCWSTR ValueName,
                        PVOID Data, ULONG_PTR DataSize, ULONG DataType)
    {
        Entry.Original.Root = Root;
        Entry.Original.SubKey = SubKey;
        Entry.Original.ValueName = ValueName;
        Entry.Original.DataType = DataType;
        Entry.Original.Data = nullptr;
        Entry.Original.DataSize = 0;
        Entry.Redirected.Root = Root;
        Entry.Redirected.SubKey = SubKey;
        Entry.Redirected.ValueName = ValueName;
        Entry.Redirected.DataType = DataType;
        Entry.Redirected.Data = Data;
        Entry.Redirected.DataSize = DataSize;
    };
    SetEntry(Entries[0], Hkcu, L"Control Panel\\International", L"Locale", LocaleHex, sizeof(LocaleHex), REG_SZ);
    SetEntry(Entries[1], Hkcu, L"Control Panel\\International", L"LocaleName", LocaleName, LocaleNameBytes, REG_SZ);
    SetEntry(Entries[2], Hkcu, L"Control Panel\\Desktop", L"PreferredUILanguages", LocaleName, PreferredBytes, REG_MULTI_SZ);
    SetEntry(Entries[3], Hkcu, L"Control Panel\\Desktop\\MuiCached", L"MachinePreferredUILanguages", LocaleName, PreferredBytes, REG_MULTI_SZ);

    NTSTATUS Status = InitRegistryRedirection(Entries, countof(Entries));
    if (NT_SUCCESS(Status))
        AdvancedRegistryInitialized = TRUE;
    return Status;
}

typedef struct
{
    UNICODE_STRING DllName;
    TYPE_OF(&LeGlobalData::HookNtdllRoutines)     HookRoutine;
    TYPE_OF(&LeGlobalData::UnHookNtdllRoutines)   UnHookRoutine;

} DLL_HOOK_ENTRY, *PDLL_HOOK_ENTRY;

PDLL_HOOK_ENTRY LookupDllHookEntry(PCUNICODE_STRING BaseDllName)
{
    static DLL_HOOK_ENTRY DllHookEntries[] =
    {
        { RTL_CONSTANT_STRING(L"USER32.dll"),       &LeGlobalData::HookUser32Routines,      &LeGlobalData::UnHookUser32Routines },
        { RTL_CONSTANT_STRING(L"GDI32.dll"),        &LeGlobalData::HookGdi32Routines,       &LeGlobalData::UnHookGdi32Routines },
        { RTL_CONSTANT_STRING(L"KERNEL32.dll"),     &LeGlobalData::HookKernel32Routines,    &LeGlobalData::UnHookKernel32Routines },
    };

    PDLL_HOOK_ENTRY Entry;

    FOR_EACH_ARRAY(Entry, DllHookEntries)
    {
        if (!RtlEqualUnicodeString(BaseDllName, &Entry->DllName, TRUE))
            continue;

        return Entry;
    }

    return nullptr;
}

VOID LeGlobalData::HookModule(PVOID DllBase, PCUNICODE_STRING DllName, BOOL DllLoad)
{
    PDLL_HOOK_ENTRY Entry;

    Entry = LookupDllHookEntry(DllName);
    if (Entry == nullptr)
        return;

    NTSTATUS Status = DllLoad ? (this->*Entry->HookRoutine)(DllBase) : (this->*Entry->UnHookRoutine)();

    UNREFERENCED_PARAMETER(Status);
    WriteLog(L"hook %s: %p", Entry->DllName.Buffer, Status);
}

VOID LeGlobalData::DllNotification(ULONG NotificationReason, PCLDR_DLL_NOTIFICATION_DATA NotificationData)
{
    NTSTATUS            Status;
    PVOID               DllBase;
    ULONG_PTR           Length;
    PLDR_MODULE         Module;
    UNICODE_STRING      DllPath;
    PCUNICODE_STRING    DllName;

    switch (NotificationReason)
    {
        case LDR_DLL_NOTIFICATION_REASON_LOADED:
            DllName = NotificationData->Loaded.BaseDllName;
            DllBase = NotificationData->Loaded.DllBase;
            DllPath = *NotificationData->Loaded.FullDllName;
            DllPath.Length -= DllName->Length;
            break;

        case LDR_DLL_NOTIFICATION_REASON_UNLOADED:
            DllName = NotificationData->Unloaded.BaseDllName;
            DllBase = NotificationData->Unloaded.DllBase;
            DllPath = *NotificationData->Unloaded.FullDllName;
            DllPath.Length -= DllName->Length;
            break;

        default:
            return;
    }

    //if (!RtlEqualUnicodeString(&SystemDirectory, &DllPath, TRUE))
    //    return;

    if (LookupDllHookEntry(DllName) == nullptr)
        return;

    Module = FindLdrModuleByHandle(DllBase);
    if (!FLAG_ON(Module->Flags, LDRP_PROCESS_ATTACH_CALLED))
    {
        Module->EntryPoint = DelayInitDllEntry;
    }
    else
    {
        HookModule(DllBase, DllName, NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED);
    }
}

NTSTATUS LeGlobalData::UnInitialize()
{
    if (DllNotificationCookie != nullptr)
    {
        LdrUnregisterDllNotification(DllNotificationCookie);
        DllNotificationCookie = nullptr;
    }

    UnHookGdi32Routines();
    UnHookUser32Routines();
    UnHookKernel32Routines();
    UnHookNtdllRoutines();

    UnInstallHookPort();

    RtlFreeUnicodeString(&SystemDirectory);

    return 0;
}

inline BOOL UnInitialize(PVOID BaseAddress)
{
    LeReleaseGlobalData();
    ml::MlUnInitialize();
    return FALSE;
}

BOOL Initialize(PVOID BaseAddress)
{
    NTSTATUS            Status;
    PLeGlobalData       GlobalData;

    ml::MlInitialize();

    // Managed (.NET) targets reach the first LdrLoadDll call only after kernel32 has finished its process attach.
    // Aborting in that case left them silently un-emulated. The rest of the initialization copes with an
    // already-attached kernel32, as the x64 core does, so it must not be rejected here.

    LdrDisableThreadCalloutsForDll(BaseAddress);

    GlobalData = new (std::nothrow) LeGlobalData;
    if (GlobalData == nullptr)
        return FALSE;

    LeSetGlobalData(GlobalData);

    Status = GlobalData->Initialize();
    if (NT_FAILED(Status))
        return FALSE;

    WriteLog(L"init ret");

    return TRUE;
}

__declspec(noinline) BOOL NTAPI DllMain(PVOID BaseAddress, ULONG Reason, PVOID Reserved)
{
    switch (Reason)
    {
        case DLL_PROCESS_ATTACH:
            return Initialize(BaseAddress) || UnInitialize(BaseAddress);

        case DLL_PROCESS_DETACH:
            UnInitialize(BaseAddress);
            break;
    }

    return TRUE;
}


// 自定义入口必须先初始化官方 GS cookie，再进入受保护的初始化函数。
EXTC __declspec(safebuffers) BOOL NTAPI LeDllEntry(PVOID BaseAddress, ULONG Reason, PVOID Reserved)
{
    if (Reason == DLL_PROCESS_ATTACH)
        __security_init_cookie();

    return DllMain(BaseAddress, Reason, Reserved);
}
