#include "stdafx.h"

extern BOOL Initialize(PVOID BaseAddress);

typedef struct
{
    USHORT  Size;
    USHORT  Unknown;
    WCHAR   Version16Name[1];

} IMAGE_VERSION_INFO_DATA_ENTRY, *PIMAGE_VERSION_INFO_DATA_ENTRY;

PVOID SearchLdrInitNtContinue()
{
    return WalkOpCodeT(LdrInitializeThunk, 0x5B,
                WalkOpCodeM(Buffer, OpLength, Ret)
                {
                    if (Buffer[0] == CALL && (PVOID)GetCallDestination(Buffer) == NtContinue)
                    {
                        Ret = Buffer;
                        return STATUS_SUCCESS;
                    }

                    return STATUS_NOT_FOUND;
                }
            );
}

NTSTATUS ReplaceMUIVersionLocaleInfo(PVOID& VersionData, ULONG& VersionSize)
{
    PVOID       TlbBuffer;
    PBYTE       Buffer;
    LONG_PTR    Size;
    PUSHORT     LangID, CharsetID;
    PIMAGE_VERSION_INFO_DATA_ENTRY Entry;

    static WCHAR VarFileInfo[] = L"VarFileInfo";
    static WCHAR Translation[] = L"Translation";

    TlbBuffer = (PBYTE)THREAD_LOCAL_BUFFER::GetTlb(TRUE)->GetBuffer();
    if (TlbBuffer == nullptr)
        return STATUS_NO_MEMORY;

    CopyMemory(TlbBuffer, VersionData, VersionSize);

    Entry = (PIMAGE_VERSION_INFO_DATA_ENTRY)TlbBuffer;
    Size = Entry->Size;

    if (Size < 8 || Size > 0x8000)
        return STATUS_DATA_NOT_ACCEPTED;

    // ansi version
    if (Entry->Version16Name[0] == TAG2('VS'))
        return STATUS_DATA_NOT_ACCEPTED;

    Buffer = (PBYTE)KMP(Entry, Size, VarFileInfo, sizeof(VarFileInfo));
    if (Buffer == nullptr)
        return STATUS_RESOURCE_DATA_NOT_FOUND;

    Size = Entry->Size - PtrOffset(Buffer, Entry) - sizeof(VarFileInfo);
    if (Size <= 0)
        return STATUS_RESOURCE_DATA_NOT_FOUND;

    Buffer += sizeof(VarFileInfo);

    Buffer = (PBYTE)KMP(Buffer, Size, Translation, sizeof(Translation));
    if (Buffer == nullptr)
        return STATUS_RESOURCE_DATA_NOT_FOUND;

    Size = Entry->Size - PtrOffset(Buffer, Entry) - sizeof(Translation);
    if (Size <= 0)
        return STATUS_RESOURCE_DATA_NOT_FOUND;

    Entry = (PIMAGE_VERSION_INFO_DATA_ENTRY)PtrSub(Buffer, 6);

    LangID = (PUSHORT)PtrAdd(Entry, ROUND_DOWN(CONST_STRSIZE(Translation) + 11, 4));
    CharsetID = LangID + 1;

    *LangID = LANGIDFROMLCID(LeGetGlobalData()->GetEnvironment()->LocaleID);

    VersionData = TlbBuffer;

    return STATUS_SUCCESS;
}

NTSTATUS QueryRegKeyFullPath(HANDLE Key, PUNICODE_STRING KeyFullPath)
{
    NTSTATUS                    Status;
    PKEY_NAME_INFORMATION       KeyFullName;
    ULONG_PTR                   MaxLength;
    ULONG                       ReturnedLength;

    KeyFullName = nullptr;
    MaxLength   = 0;

    LOOP_FOREVER
    {
        Status = NtQueryKey(Key, KeyNameInformation, KeyFullName, MaxLength, &ReturnedLength);
        if (Status != STATUS_BUFFER_TOO_SMALL)
            break;

        KeyFullName = (PKEY_NAME_INFORMATION)ReAllocateMemoryP(KeyFullName, ReturnedLength + sizeof(WCHAR));
        if (KeyFullName == nullptr)
        {
            Status = STATUS_NO_MEMORY;
            break;
        }

        MaxLength = ReturnedLength;
    }

    if (NT_SUCCESS(Status))
    {
        PtrAdd(KeyFullName->Name, KeyFullName->NameLength)[0] = 0;
        Status = RtlCreateUnicodeString(KeyFullPath, KeyFullName->Name) ? STATUS_SUCCESS : STATUS_NO_MEMORY;
    }

    FreeMemoryP(KeyFullName);
    return Status;
}

NoInline PVOID FASTCALL LoadSelfAsFirstDll(PVOID ReturnAddress)
{
    PVOID   BaseToFree;
    PLEPEB  LePeb;

    BaseToFree = nullptr;

    LOOP_ONCE
    {
        PVOID DllHandle;

        LePeb = OpenOrCreateLePeb();
        if (LePeb == nullptr)
            break;

        WriteProtectMemory(CurrentProcess, LePeb->LdrLoadDllAddress, LePeb->LdrLoadDllBackup, LePeb->LdrLoadDllBackupSize);

        if(!Initialize(&__ImageBase))
            break;

        // UNICODE_STRING DllPath;

        // RtlInitUnicodeString(&DllPath, LePeb->LeDllFullPath);

        // if (NT_FAILED(LdrLoadDll(nullptr, nullptr, &DllPath, &DllHandle)))
        // {
            // CloseLePeb(LePeb);
            // break;
        // }

        // *(PULONG_PTR)_AddressOfReturnAddress() += PtrOffset(DllHandle, &__ImageBase);

        CloseLePeb(LePeb);

        // BaseToFree = &__ImageBase;
    }

    return BaseToFree;
}

__declspec(safebuffers)
NTSTATUS
NTAPI
LoadFirstDll(
    PWCHAR              PathToFile OPTIONAL,
    PULONG              DllCharacteristics OPTIONAL,
    PCUNICODE_STRING    ModuleFileName,
    PVOID*              DllHandle
)
{
    // 子进程的私有映像不经过 DLL 入口，必须在调用受 GS 保护的初始化前播种。
    __security_init_cookie();

    PVOID BaseToFree;

    // ExceptionBox(L"inject");

    BaseToFree = LoadSelfAsFirstDll(_ReturnAddress());
    // if (BaseToFree != nullptr)
        // Mm::FreeVirtualMemory(BaseToFree);

    return LdrLoadDll(PathToFile, DllCharacteristics, ModuleFileName, DllHandle);
}

NTSTATUS
HPCALL
LeNtQuerySystemInformation(
    HPARGS
    SYSTEM_INFORMATION_CLASS    SystemInformationClass,
    PVOID                       SystemInformation,
    ULONG_PTR                   SystemInformationLength,
    PULONG                      ReturnLength OPTIONAL
)
{
    NTSTATUS Status = 0;
    PLeGlobalData GlobalData = (PLeGlobalData)HpGetFilterContext();

    switch (SystemInformationClass)
    {
        case SystemCurrentTimeZoneInformation:
            if (SystemInformation == nullptr ||
                SystemInformationLength < sizeof(GlobalData->GetEnvironment()->Timezone) ||
                IsWritableMemory(SystemInformation, sizeof(GlobalData->GetEnvironment()->Timezone)) == FALSE)
                break;
            if (ReturnLength != nullptr && IsWritableMemory(ReturnLength, sizeof(*ReturnLength)) == FALSE)
                break;

            CopyMemory(SystemInformation, &GlobalData->GetEnvironment()->Timezone, sizeof(GlobalData->GetEnvironment()->Timezone));

            if (ReturnLength != nullptr)
                *ReturnLength = sizeof(GlobalData->GetEnvironment()->Timezone);

            HpSetFilterAction(BlockSystemCall);

            return STATUS_SUCCESS;

        // GetDynamicTimeZoneInformation (used by .NET's TimeZoneInfo.Local) asks for the dynamic variant:
        // the same zone data, then the registry key name and the DynamicDaylightTimeDisabled flag.
        case SystemDynamicTimeZoneInformation:
        {
            struct
            {
                LE_RUNTIME_TIME_ZONE_INFORMATION    Zone;
                WCHAR                               TimeZoneKeyName[128];
                BOOLEAN                             DynamicDaylightTimeDisabled;
            } *Dynamic = (decltype(Dynamic))SystemInformation;

            PCWSTR Id = GlobalData->GetLePeb()->TimezoneId;
            SIZE_T Length = StrLengthW(Id);

            if (SystemInformation == nullptr ||
                SystemInformationLength < sizeof(*Dynamic) ||
                Length == 0 || Length >= countof(Dynamic->TimeZoneKeyName) ||
                IsWritableMemory(SystemInformation, sizeof(*Dynamic)) == FALSE)
                break;
            if (ReturnLength != nullptr && IsWritableMemory(ReturnLength, sizeof(*ReturnLength)) == FALSE)
                break;

            Dynamic->Zone = GlobalData->GetEnvironment()->Timezone;
            ZeroMemory(Dynamic->TimeZoneKeyName, sizeof(Dynamic->TimeZoneKeyName));
            CopyMemory(Dynamic->TimeZoneKeyName, Id, Length * sizeof(WCHAR));
            Dynamic->DynamicDaylightTimeDisabled = FALSE;

            if (ReturnLength != nullptr)
                *ReturnLength = sizeof(*Dynamic);

            HpSetFilterAction(BlockSystemCall);

            return STATUS_SUCCESS;
        }
    }

    return Status;
}

NTSTATUS
HPCALL
LeNtQueryInformationThread(
    HPARGS
    HANDLE          ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID           ThreadInformation,
    ULONG           ThreadInformationLength,
    PULONG          ReturnLength
)
{
    NTSTATUS Status;

    switch (ThreadInformationClass)
    {
        case ThreadTimes:
            break;

        default:
            return 0;
    }

    HpSetFilterAction(BlockSystemCall);

    Status = HpCallSysCall(NtQueryInformationThread, ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
    FAIL_RETURN(Status);

    PKERNEL_USER_TIMES Times;

    Times = (PKERNEL_USER_TIMES)ThreadInformation;

    //Times->CreateTime.QuadPart = 0;
    Times->KernelTime.QuadPart = 0x26161;
    Times->UserTime.QuadPart = 0x26161;

    AllocConsole();
    PrintConsoleW(L"%I64X, %I64X\n", Times->KernelTime, Times->UserTime);

    return Status;
}

#define LE_CROSS_ARCH_LAUNCH_CONTEXT TAG4('LCAL')
#define LE_CROSS_ARCH_HELPER_TIMEOUT_MS 60000

// The child is a native x64 process, which this x86 Core cannot be mapped into.  In the dual package
// this Core lives in the "x86" folder, and the x64 LEProc sits in its parent: start it in -attach mode
// with the serialized environment so it injects the x64 Loader/Core into the suspended child.
// A failure leaves the child un-emulated and is reported; the caller still lets the child run.
static NTSTATUS LeInjectCrossArchitecture(PLeGlobalData GlobalData, PCLIENT_ID Cid)
{
    WCHAR HelperPath[MAX_NTPATH];
    BYTE Input[2048];
    ULONG InputSize = 0;
    PLEPEB LePeb = GlobalData->GetLePeb();
    ULONG_PTR Length = wcslen(LePeb->LeDllDirPath);
    static const WCHAR Folder[] = L"x86\\";
    static const WCHAR Helper[] = L"LEProc.exe";
    const ULONG_PTR FolderChars = countof(Folder) - 1;

    if (Length <= FolderChars || LePeb->LeDllDirPath[Length - FolderChars - 1] != L'\\')
        return STATUS_NOT_SUPPORTED;
    for (ULONG_PTR Index = 0; Index < FolderChars; ++Index)
    {
        WCHAR Actual = LePeb->LeDllDirPath[Length - FolderChars + Index];
        if (Actual != Folder[Index] && Actual != (WCHAR)(Folder[Index] ^ 0x20))
            return STATUS_NOT_SUPPORTED;
    }
    Length -= FolderChars;
    if (Length + countof(Helper) > countof(HelperPath))
        return STATUS_NAME_TOO_LONG;
    CopyMemory(HelperPath, LePeb->LeDllDirPath, Length * sizeof(WCHAR));
    CopyMemory(HelperPath + Length, Helper, sizeof(Helper));
    if (GetFileAttributesW(HelperPath) == INVALID_FILE_ATTRIBUTES)
    {
        WriteLog(L"cross-arch helper missing %s", HelperPath);
        return STATUS_DLL_NOT_FOUND;
    }

    if (!LeSerializeRuntimeInput(&LePeb->Environment, LePeb->TimezoneId, Input, sizeof(Input), &InputSize))
        return STATUS_BUFFER_OVERFLOW;

    static WCHAR CommandLine[4096];
    int Prefix = wsprintfW(CommandLine, L"\"%s\" -attach %u %u ", HelperPath,
                           (ULONG)(ULONG_PTR)Cid->UniqueProcess, (ULONG)(ULONG_PTR)Cid->UniqueThread);
    if (Prefix <= 0 || (ULONG)Prefix + InputSize * 2 + 1 > countof(CommandLine))
        return STATUS_BUFFER_OVERFLOW;
    LeBytesToHex(Input, InputSize, CommandLine + Prefix);

    STARTUPINFOW StartupInfo = {};
    PROCESS_INFORMATION Created = {};
    StartupInfo.cb = sizeof(StartupInfo);
    BOOL Started;
    {
        // The helper is created through this very hook; it must be neither emulated nor recursed on.
        TEB_ACTIVE_FRAME LaunchFrame(LE_CROSS_ARCH_LAUNCH_CONTEXT);
        LaunchFrame.Push();
        Started = CreateProcessW(HelperPath, CommandLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                 nullptr, nullptr, &StartupInfo, &Created);
        LaunchFrame.Pop();
    }
    if (!Started)
        return ML_NTSTATUS_FROM_WIN32(GetLastError());

    NTSTATUS Status = STATUS_TIMEOUT;
    if (WaitForSingleObject(Created.hProcess, LE_CROSS_ARCH_HELPER_TIMEOUT_MS) == WAIT_OBJECT_0)
    {
        DWORD ExitCode = (DWORD)STATUS_UNSUCCESSFUL;
        GetExitCodeProcess(Created.hProcess, &ExitCode);
        Status = (NTSTATUS)ExitCode;
    }
    else
    {
        TerminateProcess(Created.hProcess, (UINT)STATUS_TIMEOUT);
    }
    WriteLog(L"cross-arch helper pid=%u status=%08X", Created.dwProcessId, Status);
    CloseHandle(Created.hThread);
    CloseHandle(Created.hProcess);
    return Status;
}

NTSTATUS LeGlobalData::InjectSelfToChildProcess(HANDLE Process, PCLIENT_ID Cid)
{
    NTSTATUS    Status;
    PVOID       SelfShadow, LocalSelfShadow, ShadowLoadFirstDll;
    ULONG_PTR   SizeOfImage;
    LONG        Offset;
    PLEPEB      TargetLePeb, LePeb;
    HANDLE      Section;
    BYTE        Backup[LDR_LOAD_DLL_BACKUP_SIZE];

    // Process = CurrentProcess;

    if (this->Wow64 && !Ps::IsWow64Process(Process))
        return STATUS_NOT_SUPPORTED;

    LePeb = GetLePeb();

    SizeOfImage = ImageGetSizeOfImage(&__ImageBase);

    SelfShadow = nullptr;
    Status = AllocVirtualMemoryEx(Process, &SelfShadow, SizeOfImage, PAGE_READWRITE);
    WriteLog(L"AllocVirtualMemoryEx: %p", Status);
    if (NT_FAILED(Status))
        return Status;

    Status = LoadPeImage(LePeb->LeDllFullPath, &LocalSelfShadow, nullptr, 0);
    WriteLog(L"LocalSelfShadow: %p", Status);
    if (NT_FAILED(Status))
    {
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return Status;
    }

    // 子进程尚未映射 KERNEL32；先在临时映像内运行官方熵采集，再传输已初始化的映像。
    auto InitShadowCookie = (VOID (__cdecl*)(VOID))PtrAdd(LocalSelfShadow, PtrOffset(__security_init_cookie, &__ImageBase));
    InitShadowCookie();

    RelocPeImage(LocalSelfShadow, LocalSelfShadow, nullptr, SelfShadow);


    Status = WriteMemory(Process, SelfShadow, LocalSelfShadow, SizeOfImage);
    UnloadPeImage(LocalSelfShadow);

    WriteLog(L"WriteMemory: %p", Status);

    if (NT_FAILED(Status))
    {
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return Status;
    }

    // 私有映像也按 PE 节恢复权限，不能让子进程长期保留整块 RWX 映像。
    Status = ProtectVirtualMemory(SelfShadow, SizeOfImage, PAGE_READONLY, nullptr, Process);
    PIMAGE_NT_HEADERS Headers = ImageNtHeadersFast(&__ImageBase);
    PIMAGE_SECTION_HEADER ImageSection = IMAGE_FIRST_SECTION(Headers);
    for (USHORT Index = 0; NT_SUCCESS(Status) && Index < Headers->FileHeader.NumberOfSections; ++Index, ++ImageSection)
    {
        if (ImageSection->Misc.VirtualSize == 0)
            continue;
        const bool Executable = (ImageSection->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        const bool Writable = (ImageSection->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        if (Executable && Writable)
        {
            Status = STATUS_INVALID_IMAGE_PROTECT;
            break;
        }
        const ULONG Protection = Executable ? PAGE_EXECUTE_READ : (Writable ? PAGE_READWRITE : PAGE_READONLY);
        Status = ProtectVirtualMemory(PtrAdd(SelfShadow, ImageSection->VirtualAddress),
            ImageSection->Misc.VirtualSize, Protection, nullptr, Process);
    }
    if (NT_FAILED(Status))
    {
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return Status;
    }
    Status = NtFlushInstructionCache(Process, SelfShadow, SizeOfImage);
    if (NT_FAILED(Status))
    {
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return Status;
    }

    PVOID ooxxAddress = LePeb->LdrLoadDllAddress;
    BYTE ooxxBuffer[16];

    Status = ReadMemory(Process, ooxxAddress, Backup, LDR_LOAD_DLL_BACKUP_SIZE);
    WriteLog(L"ReadMemory: %p", Status);
    WriteLog(L"ooxxAddress: %p", ooxxAddress);
    if (NT_FAILED(Status))
    {
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return Status;
    }

    ShadowLoadFirstDll = PtrAdd(LoadFirstDll, PtrOffset(SelfShadow, &__ImageBase));
    Offset = PtrOffset(ShadowLoadFirstDll, PtrAdd(ooxxAddress, 5));

    ooxxBuffer[0] = JUMP;
    *(PULONG)&ooxxBuffer[1] = Offset;

    auto Rollback = [&](NTSTATUS Failure) -> NTSTATUS
    {
        NTSTATUS RestoreStatus = WriteProtectMemory(Process, ooxxAddress, Backup, sizeof(Backup));
        Mm::FreeVirtualMemory(SelfShadow, Process);
        return NT_FAILED(RestoreStatus) ? RestoreStatus : Failure;
    };
    Status = WriteProtectMemory(Process, ooxxAddress, ooxxBuffer, 5);
    WriteLog(L"WriteProtectMemory: %p", Status);
    if (NT_FAILED(Status))
        return Rollback(Status);

    TargetLePeb = OpenOrCreateLePeb((ULONG_PTR)Cid->UniqueProcess, TRUE, 0);
    if (TargetLePeb == nullptr)
        return Rollback(STATUS_UNSUCCESSFUL);

    Section = TargetLePeb->Section;
    *TargetLePeb = *LePeb;
    TargetLePeb->Section = Section;
    CopyStruct(TargetLePeb->LdrLoadDllBackup, Backup, LDR_LOAD_DLL_BACKUP_SIZE);
    TargetLePeb->LdrLoadDllBackupSize = LDR_LOAD_DLL_BACKUP_SIZE;
    CloseLePeb(TargetLePeb);
    return STATUS_SUCCESS;

}

NTSTATUS
HPCALL
LeNtCreateUserProcess(
    HPARGS
    PHANDLE                         ProcessHandle,
    PHANDLE                         ThreadHandle,
    ACCESS_MASK                     ProcessDesiredAccess,
    ACCESS_MASK                     ThreadDesiredAccess,
    POBJECT_ATTRIBUTES              ProcessObjectAttributes,
    POBJECT_ATTRIBUTES              ThreadObjectAttributes,
    ULONG                           ProcessFlags,
    ULONG                           ThreadFlags,
    PRTL_USER_PROCESS_PARAMETERS    ProcessParameters,
    PPS_CREATE_INFO                 CreateInfo,
    PPS_ATTRIBUTE_LIST              AttributeList
)
{
    NTSTATUS            Status, Status2;
    PLeGlobalData       GlobalData;
    PPS_ATTRIBUTE_LIST  LocalAttributeList;
    PVOID               LocalAttributeAllocation;
    PPS_ATTRIBUTE       Attribute;
    CLIENT_ID           LocalCid, *Cid;
    ULONG_PTR           ReturnLength;
    LONG                Offset;
    PVOID               SelfShadow;

    LocalAttributeList = nullptr;
    LocalAttributeAllocation = nullptr;
    Attribute = nullptr;
    Cid = nullptr;

    if ((ProcessObjectAttributes != nullptr &&
         IsReadableMemory(ProcessObjectAttributes, sizeof(*ProcessObjectAttributes)) == FALSE) ||
        (ThreadObjectAttributes != nullptr &&
         IsReadableMemory(ThreadObjectAttributes, sizeof(*ThreadObjectAttributes)) == FALSE))
        return 0;

    if (AttributeList != nullptr)
    {
        if (IsReadableMemory(AttributeList, FIELD_OFFSET(PS_ATTRIBUTE_LIST, Attributes)) == FALSE ||
            AttributeList->TotalLength < FIELD_OFFSET(PS_ATTRIBUTE_LIST, Attributes) ||
            IsReadableMemory(AttributeList, AttributeList->TotalLength) == FALSE)
            return 0;
    }

    auto ReleaseLocalAttributes = [&]()
    {
        if (LocalAttributeAllocation != nullptr)
        {
            FreeMemory(LocalAttributeAllocation);
            LocalAttributeAllocation = nullptr;
        }
    };

    HpSetFilterAction(BlockSystemCall);

    if (AttributeList != nullptr)
    {
        ULONG_PTR Count;

        Attribute = AttributeList->Attributes;
        Count = (PPS_ATTRIBUTE)PtrAdd(AttributeList, AttributeList->TotalLength) - Attribute;
        FOR_EACH(Attribute, AttributeList->Attributes, Count)
        {
            if (Attribute->AttributeNumber != PsAttributeClientId)
                continue;

//            if (Attribute->AttributeFlags != PS_ATTRIBUTE_FLAG_THREAD)
//                break;

            Cid = (PCLIENT_ID)Attribute->ValuePtr;
            break;
        }
    }

    if (Cid == nullptr)
    {
        ULONG_PTR ListLength;

        if ((AttributeList != nullptr && AttributeList->TotalLength > MAXULONG_PTR - sizeof(*Attribute)) ||
            (AttributeList == nullptr && sizeof(AttributeList->TotalLength) > MAXULONG_PTR - sizeof(*Attribute)))
            return STATUS_INTEGER_OVERFLOW;
        ListLength = (AttributeList != nullptr ? AttributeList->TotalLength : sizeof(AttributeList->TotalLength)) + sizeof(*Attribute);
        LocalAttributeList = (PPS_ATTRIBUTE_LIST)AllocateMemory(ListLength);
        if (LocalAttributeList == nullptr)
            return STATUS_NO_MEMORY;
        LocalAttributeAllocation = LocalAttributeList;

        if (AttributeList != nullptr)
            CopyMemory(LocalAttributeList, AttributeList, AttributeList->TotalLength);

        LocalAttributeList->TotalLength = ListLength;
        Attribute = (PPS_ATTRIBUTE)PtrAdd(LocalAttributeList, LocalAttributeList->TotalLength) - 1;

        Attribute->AttributeNumber  = PsAttributeClientId;
        Attribute->AttributeFlags   = PS_ATTRIBUTE_FLAG_THREAD;
        Attribute->ValuePtr         = &LocalCid;
        Attribute->Size             = sizeof(LocalCid);
        Attribute->ReturnLength     = &ReturnLength;

        Cid = &LocalCid;

        AttributeList = LocalAttributeList;
    }

    const ACCESS_MASK InternalProcessAccess = PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;
    const ACCESS_MASK InternalThreadAccess = THREAD_SUSPEND_RESUME | THREAD_TERMINATE;
    const ULONG ProcessHandleAttributes = ProcessObjectAttributes == nullptr ? 0 : ProcessObjectAttributes->Attributes & OBJ_INHERIT;
    const ULONG ThreadHandleAttributes = ThreadObjectAttributes == nullptr ? 0 : ThreadObjectAttributes->Attributes & OBJ_INHERIT;
    OBJECT_ATTRIBUTES LocalProcessAttributes, LocalThreadAttributes;
    // Private operation handles must not leak their extra rights through inheritance.
    if (ProcessObjectAttributes != nullptr)
    {
        LocalProcessAttributes = *ProcessObjectAttributes;
        LocalProcessAttributes.Attributes &= ~OBJ_INHERIT;
        ProcessObjectAttributes = &LocalProcessAttributes;
    }
    if (ThreadObjectAttributes != nullptr)
    {
        LocalThreadAttributes = *ThreadObjectAttributes;
        LocalThreadAttributes.Attributes &= ~OBJ_INHERIT;
        ThreadObjectAttributes = &LocalThreadAttributes;
    }

    Status = HpCallSysCall(
                NtCreateUserProcess,
                ProcessHandle,
                ThreadHandle,
                ProcessDesiredAccess | InternalProcessAccess,
                ThreadDesiredAccess | InternalThreadAccess,
                ProcessObjectAttributes,
                ThreadObjectAttributes,
                ProcessFlags,
                ThreadFlags | THREAD_CREATE_FLAGS_CREATE_SUSPENDED,
                ProcessParameters,
                CreateInfo,
                AttributeList
            );

    WriteLog(L"create proc %p", Status);

    if (NT_FAILED(Status))
    {
        ReleaseLocalAttributes();
        return Status;
    }

    HANDLE InternalProcess = *ProcessHandle, InternalThread = *ThreadHandle;
    HANDLE ReturnedProcess = nullptr, ReturnedThread = nullptr;
    *ProcessHandle = nullptr;
    *ThreadHandle = nullptr;

    GlobalData = (PLeGlobalData)HpGetFilterContext();

    if (FindThreadFrame(LE_CROSS_ARCH_LAUNCH_CONTEXT) != nullptr)
        // The cross-architecture helper itself: runs unmodified.
        Status2 = STATUS_SUCCESS;
    else if (Ps::IsWow64Process(CurrentProcess) && !Ps::IsWow64Process(InternalProcess))
    {
        // A native x64 child: the x64 helper injects it. If that cannot be done the child still runs,
        // just without emulation, as before.
        NTSTATUS CrossStatus = LeInjectCrossArchitecture(GlobalData, Cid);
        WriteLog(L"cross-arch child status=%08X", CrossStatus);
        UNREFERENCED_PARAMETER(CrossStatus);
        Status2 = STATUS_SUCCESS;
    }
    else
        Status2 = GlobalData->InjectSelfToChildProcess(InternalProcess, Cid);

    // Creation resolved MAXIMUM_ALLOWED; duplication treats it as no concrete access bits.
    if (NT_SUCCESS(Status2))
        Status2 = NtDuplicateObject(CurrentProcess, InternalProcess, CurrentProcess, &ReturnedProcess,
            ProcessDesiredAccess, ProcessHandleAttributes, FLAG_ON(ProcessDesiredAccess, MAXIMUM_ALLOWED) ? DUPLICATE_SAME_ACCESS : 0);
    if (NT_SUCCESS(Status2))
        Status2 = NtDuplicateObject(CurrentProcess, InternalThread, CurrentProcess, &ReturnedThread,
            ThreadDesiredAccess, ThreadHandleAttributes, FLAG_ON(ThreadDesiredAccess, MAXIMUM_ALLOWED) ? DUPLICATE_SAME_ACCESS : 0);

    if (NT_SUCCESS(Status2) && FLAG_OFF(ThreadFlags, THREAD_CREATE_FLAGS_CREATE_SUSPENDED))
        Status2 = NtResumeThread(InternalThread, nullptr);

    if (NT_FAILED(Status2))
    {
        if (ReturnedThread != nullptr) NtClose(ReturnedThread);
        if (ReturnedProcess != nullptr) NtClose(ReturnedProcess);
        NTSTATUS CleanupStatus = NtTerminateProcess(InternalProcess, Status2);
        if (NT_FAILED(CleanupStatus))
        {
            LARGE_INTEGER NoWait = {};
            if (NtWaitForSingleObject(InternalProcess, FALSE, &NoWait) != STATUS_SUCCESS)
            {
                // Failure paths still own the only thread, which has never been resumed.
                CleanupStatus = NtTerminateThread(InternalThread, Status2);
                if (NT_FAILED(CleanupStatus) &&
                    NtWaitForSingleObject(InternalProcess, FALSE, &NoWait) != STATUS_SUCCESS)
                {
                    // Both termination primitives failed: return the remaining cleanup capability.
                    *ProcessHandle = InternalProcess;
                    *ThreadHandle = InternalThread;
                    ReleaseLocalAttributes();
                    return CleanupStatus;
                }
            }
        }
        NtClose(InternalThread);
        NtClose(InternalProcess);
        ReleaseLocalAttributes();
        return Status2;
    }

    NtClose(InternalThread);
    NtClose(InternalProcess);
    *ProcessHandle = ReturnedProcess;
    *ThreadHandle = ReturnedThread;

    WriteLog(L"inject %p", Status2);

    ReleaseLocalAttributes();
    return Status;
}

NTSTATUS
HPCALL
LeNtInitializeNlsFiles(
    HPARGS
    PVOID*          BaseAddress,
    PLCID           DefaultLocaleId,
    PLARGE_INTEGER  DefaultCasingTableSize
)
{
    NTSTATUS Status;
    PLeGlobalData GlobalData;

    HpSetFilterAction(BlockSystemCall);

    Status = HpCallSysCall(
                NtInitializeNlsFiles,
                BaseAddress,
                DefaultLocaleId,
                DefaultCasingTableSize
            );

    FAIL_RETURN(Status);

    GlobalData = (PLeGlobalData)HpGetFilterContext();

    *DefaultLocaleId = GlobalData->GetEnvironment()->LocaleID;

    return Status;
}

NTSTATUS
HPCALL
LeNtTerminateThread(
    HPARGS
    HANDLE      ThreadHandle,
    NTSTATUS    ExitStatus
)
{
    if (ThreadHandle == CurrentThread || ThreadHandle == nullptr)
    {
        THREAD_LOCAL_BUFFER::ReleaseTlb();
    }

    return 0;
}

NTSTATUS
LeGlobalData::
LookupRegistryRedirectionEntry(
    HANDLE                          KeyHandle,
    PUNICODE_STRING                 ValueName,
    PREGISTRY_REDIRECTION_ENTRY*    RedirectionEntry
)
{
    NTSTATUS                    Status;
    PREGISTRY_REDIRECTION_ENTRY Entry;
    UNICODE_STRING              KeyFullPath;

    // RtlAllocateHeap dead lock
    if (RtlEqualUnicodeString(ValueName, PUSTR(L"ResourcePolicies"), FALSE))
        return STATUS_NOT_FOUND;

    Status = QueryRegKeyFullPath(KeyHandle, &KeyFullPath);
    FAIL_RETURN(Status);

    Status = STATUS_NOT_FOUND;

    //ml::String KeyFullPathString = KeyFullPath;
    ml::String ValueNameString;

    if (ValueName != nullptr)
        ValueNameString = *ValueName;

    FOR_EACH_VEC(Entry, this->RegistryRedirectionEntry)
    {
        if (ValueName != nullptr)
        {
            if (ValueNameString.MatchExpression(Entry->Original.ValueName, TRUE) == FALSE)
                continue;
        }

        if (RtlEqualUnicodeString(&KeyFullPath, Entry->Original.FullPath, TRUE) == FALSE)
            continue;

        //if (KeyFullPathString.MatchExpression(Entry->Original.FullPath, TRUE) == FALSE)
        //    continue;

        *RedirectionEntry = Entry;
        Status = STATUS_SUCCESS;
        break;
    }

    RtlFreeUnicodeString(&KeyFullPath);

    return Status;
}

NTSTATUS
HPCALL
LeNtQueryValueKey(
    HPARGS
    HANDLE                      KeyHandle,
    PUNICODE_STRING             ValueName,
    KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    PVOID                       KeyValueInformation,
    ULONG                       Length,
    PULONG                      ResultLength
)
{
    NTSTATUS                    Status;
    ULONG_PTR                   BufferLength;
    PLeGlobalData               GlobalData;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    PREGISTRY_REDIRECTION_ENTRY Entry;

    // ValueName 结构及其 Buffer 都来自系统调用方，必须在影子映像中分别验证。
    if (IsReadableMemory(ValueName, sizeof(*ValueName)) == FALSE)
        return 0;
    if (NT_FAILED(RtlValidateUnicodeString(0, ValueName)))
        return 0;

    // N12：ValueName 缓冲区由应用提供，重定向表非空时查找路径会解引用；
    // 影子映像内 SEH 不可靠（见 N11），不可读时放行原系统调用，
    // 由内核 ProbeForRead 返回访问冲突，与原生行为一致。
    if (ValueName->Length != 0 && IsReadableMemory(ValueName->Buffer, ValueName->Length) == FALSE)
        return 0;

    GlobalData = (PLeGlobalData)HpGetFilterContext();
    
    WriteLog(L"lookup %s", ValueName->Buffer);

    Status = GlobalData->LookupRegistryRedirectionEntry(KeyHandle, ValueName, &Entry);

    WriteLog(L"lookup %s: %p", ValueName->Buffer, Status);

    FAIL_RETURN(Status);

    if (Entry->Redirected.Data != nullptr)
    {
        if (KeyValueInformation != nullptr && Length != 0 &&
            IsWritableMemory(KeyValueInformation, Length) == FALSE)
            return 0;
        if (ResultLength != nullptr && IsWritableMemory(ResultLength, sizeof(*ResultLength)) == FALSE)
            return 0;
    }

    HpSetFilterAction(BlockSystemCall);

    if (Entry->Redirected.Data != nullptr)
    {
        union
        {
            PVOID                                   Information;
            PKEY_VALUE_BASIC_INFORMATION            BasicInformation;
            PKEY_VALUE_PARTIAL_INFORMATION          PartialInformation;
            PKEY_VALUE_PARTIAL_INFORMATION_ALIGN64  PartialInformationAlign64;
        };

        ULONG_PTR   Alignment, DataLength, DataOffset, RequiredLength, MinimumSize;
        PVOID       LocalBuffer;

        Alignment       = sizeof(ULONG_PTR);
        DataOffset      = 0;
        DataLength      = Entry->Redirected.DataSize;
        RequiredLength  = 0;
        Information     = nullptr;

        switch (KeyValueInformationClass)
        {
            default:
                return STATUS_INVALID_PARAMETER;

            case KeyValueBasicInformation:
                goto REDIRECT_ONLY;

            case KeyValueFullInformationAlign64:
            case KeyValueFullInformation:
            {
                const auto NameBytes = Entry->Redirected.ValueName.GetSize();
                MinimumSize = FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name);
                if (NameBytes > ULONG_MAX - MinimumSize - (sizeof(ULONG64) - 1))
                    return STATUS_INTEGER_OVERFLOW;
                const ULONG NameLength = static_cast<ULONG>(NameBytes);
                const ULONG UnalignedOffset = MinimumSize + NameLength;
                DataOffset = (UnalignedOffset + static_cast<ULONG>(sizeof(ULONG64) - 1)) &
                    ~static_cast<ULONG>(sizeof(ULONG64) - 1);
                if (DataLength > ULONG_MAX - DataOffset)
                    return STATUS_INTEGER_OVERFLOW;
                RequiredLength = DataOffset + DataLength;
                if (ResultLength != nullptr)
                    *ResultLength = static_cast<ULONG>(RequiredLength);
                if (Length < MinimumSize)
                    return STATUS_BUFFER_TOO_SMALL;
                if (KeyValueInformation == nullptr)
                    return STATUS_ACCESS_VIOLATION;

                KEY_VALUE_FULL_INFORMATION Header = {};
                Header.Type = static_cast<ULONG>(Entry->Redirected.DataType);
                Header.NameLength = static_cast<ULONG>(NameLength);
                Header.DataOffset = static_cast<ULONG>(DataOffset);
                Header.DataLength = static_cast<ULONG>(DataLength);
                CopyMemory(KeyValueInformation, &Header, MinimumSize);
                CopyMemory(PtrAdd(KeyValueInformation, MinimumSize), (PWSTR)Entry->Redirected.ValueName,
                    ML_MIN(NameLength, Length - MinimumSize));
                if (Length > DataOffset)
                    CopyMemory(PtrAdd(KeyValueInformation, DataOffset), Entry->Redirected.Data,
                        ML_MIN(DataLength, Length - DataOffset));
                return Length < RequiredLength ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
            }

            case KeyValuePartialInformationAlign64:
                Alignment = sizeof(ULONG64);

                MinimumSize = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION_ALIGN64, Data);
                RequiredLength = MinimumSize + DataLength;
                RequiredLength = ROUND_UP(RequiredLength, Alignment);
                if (KeyValueInformation == nullptr || Length < MinimumSize)
                    break;

                Information = Length < RequiredLength ? AllocStack(RequiredLength) : KeyValueInformation;

                PartialInformationAlign64->Type = Entry->Redirected.DataType;
                PartialInformationAlign64->DataLength = DataLength;

                CopyMemory(PartialInformationAlign64->Data, Entry->Redirected.Data, DataLength);
                break;

            case KeyValuePartialInformation:
                MinimumSize = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
                RequiredLength = MinimumSize + DataLength;
                RequiredLength = ROUND_UP(RequiredLength, Alignment);
                if (KeyValueInformation == nullptr || Length < MinimumSize)
                    break;

                Information = Length < RequiredLength ? AllocStack(RequiredLength) : KeyValueInformation;

                PartialInformation->TitleIndex  = 0;
                PartialInformation->Type        = Entry->Redirected.DataType;
                PartialInformation->DataLength  = DataLength;

                CopyMemory(PartialInformation->Data, Entry->Redirected.Data, DataLength);
                break;
        }

        if (Information != nullptr && Information != KeyValueInformation)
        {
            CopyMemory(KeyValueInformation, Information, ML_MIN(RequiredLength, Length));
        }

        if (ResultLength != nullptr)
            *ResultLength = RequiredLength;

        if (KeyValueInformation == nullptr)
            return STATUS_ACCESS_VIOLATION;

        if (Length < MinimumSize)
            return STATUS_BUFFER_TOO_SMALL;

        if (Length < RequiredLength)
            return STATUS_BUFFER_OVERFLOW;

        return STATUS_SUCCESS;
    }
    else
    {

REDIRECT_ONLY:

        InitializeObjectAttributes(&ObjectAttributes, Entry->Redirected.FullPath, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        Status = NtOpenKey(&KeyHandle, KEY_QUERY_VALUE, &ObjectAttributes);
        FAIL_RETURN(Status);

        Status = HpCallSysCall(
                    NtQueryValueKey,
                    KeyHandle,
                    ValueName,
                    KeyValueInformationClass,
                    KeyValueInformation,
                    Length,
                    ResultLength
                );

        NtClose(KeyHandle);
    }

    return Status;
}

NTSTATUS
HPCALL
LeNtQueryDefaultLocale(
    HPARGS
    BOOLEAN UserProfile,
    PLCID   DefaultLocaleId
)
{
    if (IsWritableMemory(DefaultLocaleId, sizeof(*DefaultLocaleId)) == FALSE)
        return 0;
    HpSetFilterAction(BlockSystemCall);
    *DefaultLocaleId = ((PLeGlobalData)HpGetFilterContext())->GetEnvironment()->LocaleID;
    return STATUS_SUCCESS;
}

NTSTATUS
HPCALL
LeNtQueryDefaultUILanguage(
    HPARGS
    LANGID *DefaultUILanguageId
)
{
    if (IsWritableMemory(DefaultUILanguageId, sizeof(*DefaultUILanguageId)) == FALSE)
        return 0;
    HpSetFilterAction(BlockSystemCall);
    *DefaultUILanguageId = LANGIDFROMLCID(((PLeGlobalData)HpGetFilterContext())->GetEnvironment()->LocaleID);
    return STATUS_SUCCESS;
}

NTSTATUS
HPCALL
LeNtQueryInstallUILanguage(
    HPARGS
    LANGID *InstallUILanguageId
)
{
    if (IsWritableMemory(InstallUILanguageId, sizeof(*InstallUILanguageId)) == FALSE)
        return 0;
    HpSetFilterAction(BlockSystemCall);
    *InstallUILanguageId = LANGIDFROMLCID(((PLeGlobalData)HpGetFilterContext())->GetEnvironment()->LocaleID);
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI LeLdrInitNtContinue(PCONTEXT Context, BOOLEAN TestAlert)
{
    PLeGlobalData GlobalData = LeGetGlobalData();

    CurrentTeb()->CurrentLocale = GlobalData->GetEnvironment()->LocaleID;

    return GlobalData->HookStub.StubLdrInitNtContinue(Context, TestAlert);
}

NTSTATUS
NTAPI
LeLdrResSearchResource(
    PVOID       DllHandle,
    PULONG_PTR  ResourceIdPath,
    ULONG       ResourceIdPathLength,
    ULONG       Flags,
    PVOID*      Resource,
    PULONG      Size,
    PVOID       Reserve1,
    PVOID       Reserve2
)
{
    NTSTATUS Status;
    PLeGlobalData GlobalData = LeGetGlobalData();

    Status = GlobalData->HookStub.StubLdrResSearchResource(DllHandle, ResourceIdPath, ResourceIdPathLength, Flags, Resource, Size, Reserve1, Reserve2);
    FAIL_RETURN(Status);

    if (ResourceIdPathLength != 3)
        return Status;

    if (ResourceIdPath[0] != (ULONG_PTR)RT_VERSION || ResourceIdPath[1] != 1 || ResourceIdPath[2] != 0)
        return Status;

    if (Resource == nullptr)
        return Status;

    ReplaceMUIVersionLocaleInfo(*Resource, *Size);

    return Status;
}

LONG NTAPI LeKnownExceptionFilter(PEXCEPTION_POINTERS ExceptionPointers)
{
    LONG Result = LeGetGlobalData()->RtlKnownExceptionFilter(ExceptionPointers);

    if (Result == EXCEPTION_CONTINUE_SEARCH)
        CreateMiniDump(ExceptionPointers);

    return Result;
}

NTSTATUS NTAPI LeCustomCPToUnicodeN(IN PCPTABLEINFO CustomCP,
    OUT PWCHAR UnicodeString,
    IN ULONG UnicodeSize,
    OUT PULONG ResultSize OPTIONAL,
    IN PCHAR CustomString,
    IN ULONG CustomSize) {
    PLeGlobalData GlobalData = LeGetGlobalData();

    if (CustomCP->CodePage != 65001 && CustomCP->CodePage != GlobalData->GetEnvironment()->AnsiCodePage) {
        PROTECT_SECTION(&GlobalData->HookRoutineData.Ntdll.NtLock) {
            if (CustomCP->CodePage != 65001 && CustomCP->CodePage != GlobalData->GetEnvironment()->AnsiCodePage) {
                RtlInitCodePageTable((PUSHORT)PtrAdd(GlobalData->CodePageMapView,
                    GlobalData->AnsiCodePageOffset), CustomCP);
            }
        }
    }

    return GlobalData->HookStub.StubRtlCustomCPToUnicodeN(CustomCP, UnicodeString,
        UnicodeSize, ResultSize, CustomString, CustomSize); 
}

NTSTATUS LeGlobalData::HookNtdllRoutines(PVOID Ntdll)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    UNICODE_STRING      SubKey;
    PVOID               LdrInitNtContinue;

    LdrInitNtContinue = SearchLdrInitNtContinue();
    if (LdrInitNtContinue == nullptr)
        return STATUS_NOT_SUPPORTED;

	WriteLog(L"LdrInitNtContinue %08X", LdrInitNtContinue);

    ADD_FILTER_(NtCreateUserProcess,        LeNtCreateUserProcess,      this);

    if (IsLeLoader())
        return STATUS_SUCCESS;

    if (this->RegistryRedirectionEntry.GetSize() != 0)
    {
        ADD_FILTER_(NtQueryValueKey, LeNtQueryValueKey, this);
    }

    ADD_FILTER_(NtQuerySystemInformation,   LeNtQuerySystemInformation, this);
    ADD_FILTER_(NtInitializeNlsFiles,       LeNtInitializeNlsFiles,     this);
    ADD_FILTER_(NtQueryDefaultLocale,       LeNtQueryDefaultLocale,     this);
    ADD_FILTER_(NtQueryDefaultUILanguage,   LeNtQueryDefaultUILanguage, this);

    if (LeGetGlobalData()->GetEnvironment()->HookUILanguageMode != 0)
    {
        ADD_FILTER_(NtQueryInstallUILanguage, LeNtQueryInstallUILanguage, this);
        WriteLog(L"NtQueryInstallUILanguage hooked");
    }
    //ADD_FILTER_(NtQueryInformationThread,   LeNtQueryInformationThread, this);
    //ADD_FILTER_(NtTerminateThread,          LeNtTerminateThread,        this);

    Mp::PATCH_MEMORY_DATA p[] =
    {
        Mp::FunctionCallVa(LdrInitNtContinue, LeLdrInitNtContinue, &HookStub.StubLdrInitNtContinue),
        Mp::FunctionJumpVa(::RtlKnownExceptionFilter, LeKnownExceptionFilter, &HookStub.StubRtlKnownExceptionFilter),
        Mp::FunctionJumpVa(::RtlCustomCPToUnicodeN, LeCustomCPToUnicodeN, &HookStub.StubRtlCustomCPToUnicodeN),
    };

    Mp::PatchMemory(p, countof(p));

    RtlInitializeCriticalSectionAndSpinCount(&HookRoutineData.Ntdll.NtLock, 4000);

    return STATUS_SUCCESS;
}

NTSTATUS LeGlobalData::UnHookNtdllRoutines()
{
    Mp::RestoreMemory(HookStub.StubLdrInitNtContinue);

    return 0;
}
