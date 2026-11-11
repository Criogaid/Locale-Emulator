#include "ml.h"

#if ML_KERNEL_MODE


// #include "Source\MyLibraryKernel.cpp"


    ML_NAMESPACE

    LONG_PTR MlInitialize()
    {
        return 0;
    }

    LONG_PTR MlUnInitialize()
    {
        return 0;
    }

    MY_NAMESPACE_END

#else



_ML_C_HEAD_

#if ML_KERNEL_MODE

PVOID AllocateMemory(ULONG_PTR Size, POOL_TYPE PoolType /* = PagedPool */)
{
    return MemoryAllocator::AllocateMemory(Size, PoolType);
}

PVOID AllocateMemoryP(ULONG_PTR Size, POOL_TYPE PoolType)
{
    return AllocateMemory(Size, PoolType);
}

BOOL FreeMemory(PVOID Memory, ULONG Flags)
{
    return Memory != NULL ? MemoryAllocator::FreeMemory(Memory) : FALSE;
}

BOOL FreeMemoryP(PVOID Memory, ULONG Flags)
{
    return FreeMemory(Memory, Flags);
}

#else // user mode

PVOID AllocateMemoryP(ULONG_PTR Size, ULONG Flags)
{
    return MemoryAllocator::AllocateMemory(Size, Flags);
}

PVOID ReAllocateMemoryP(PVOID Memory, ULONG_PTR Size, ULONG Flags)
{
    return MemoryAllocator::ReAllocateMemory(Memory, Size, Flags);
}

PVOID AllocateMemory(ULONG_PTR Size, ULONG Flags)
{
#if USE_CRT_VER
    return malloc(Size);
#elif USE_NT_VER
    return RtlAllocateHeap(CurrentPeb()->ProcessHeap, Flags, Size);
#else
    return HeapAlloc(GetProcessHeap(), Flags, Size);
#endif
}

PVOID ReAllocateMemory(PVOID Memory, ULONG_PTR Size, ULONG Flags)
{
    PVOID Block;

    if (Memory == NULL)
        return AllocateMemory(Size, Flags);

#if USE_CRT_VER
    Block = realloc(Memory, Size);
#elif USE_NT_VER
    Block = RtlReAllocateHeap(CurrentPeb()->ProcessHeap, Flags, Memory, Size);
#else
    Block = HeapReAlloc(GetProcessHeap(), Flags, Memory, Size);
#endif

    if (Block == nullptr)
    {
        FreeMemory(Memory);
        return nullptr;
    }

    return Block;
}

BOOL FreeMemory(PVOID Memory, ULONG Flags)
{
#if USE_CRT_VER
    free(Memory);
    return TRUE;
#elif USE_NT_VER
    return Memory != NULL ? RtlFreeHeap(CurrentPeb()->ProcessHeap, Flags, Memory) : FALSE;
#else
    return Memory != NULL ? HeapFree(GetProcessHeap(), Flags, Memory) : FALSE;
#endif
}

BOOL FreeMemoryP(PVOID Memory, ULONG Flags)
{
    return MemoryAllocator::FreeMemory(Memory, Flags);
}

PVOID AllocateVirtualMemory(ULONG_PTR Size, ULONG Protect, HANDLE ProcessHandle)
{
#if USE_NT_VER
    PVOID BaseAddress = NULL;
    NTSTATUS Status;
    Status = Nt_AllocateMemory(ProcessHandle, &BaseAddress, Size, Protect);
    return NT_SUCCESS(Status) ? BaseAddress : NULL;
#else
    return VirtualAllocEx(ProcessHandle, NULL, Size, MEM_RESERVE | MEM_COMMIT, Protect);
#endif
}

BOOL FreeVirtualMemory(PVOID Memory, HANDLE ProcessHandle)
{
#if USE_NT_VER
    return NT_SUCCESS(Nt_FreeMemory(ProcessHandle, Memory));
#else
    return VirtualFreeEx(ProcessHandle, Memory, 0, MEM_RELEASE);
#endif
}

#endif  // ML_KERNEL_MODE

_ML_C_TAIL_

#pragma warning(disable:4245)
#pragma warning(disable:4750)

_ML_C_HEAD_

/************************************************************************
  strings
************************************************************************/
ULONG
InternalCopyUnicodeString(
    PUNICODE_STRING Unicode,
    PWCHAR          Buffer,
    ULONG_PTR       BufferCount,
    BOOL            IsDirectory = FALSE
)
{
    ULONG_PTR Length;

    Length = MY_MIN(Unicode->Length, BufferCount * sizeof(WCHAR));
    CopyMemory(Buffer, Unicode->Buffer, Length);
    Length /= sizeof(WCHAR);

    if (IsDirectory && Length < BufferCount && Buffer[Length - 1] != '\\')
        Buffer[Length++] = '\\';

    if (Length < BufferCount)
        Buffer[Length] = 0;

    return Length;
}










/************************************************************************
  process api
************************************************************************/







/************************************************************************
  common file io
************************************************************************/



/************************************************************************
  common query api
************************************************************************/

BOOL ReleaseSystemInformation(PVOID Processes)
{
    return FreeMemoryP(Processes);
}

/************************************************************************
  common process
************************************************************************/

/************************************************************************
  common token
************************************************************************/



#if ML_KERNEL_MODE

/************************************************************************
  kernel mode
************************************************************************/








#else   // r3

/************************************************************************
  user mode
************************************************************************/

/************************************************************************
  user token
************************************************************************/

NTSTATUS
Nt_GetTokenInfo(
    HANDLE                  TokenHandle,
    TOKEN_INFORMATION_CLASS TokenInformationClass,
    PVOID                   Information,
    PULONG_PTR              Size
)
{
    NTSTATUS Status;
    PVOID TokenInformation;
    ULONG Length;

    TokenInformation = NULL;
    Length = 0;

    LOOP_FOREVER
    {
        Status = NtQueryInformationToken(TokenHandle, TokenInformationClass, TokenInformation, Length, &Length);
        if (Status != STATUS_BUFFER_TOO_SMALL)
            break;

        TokenInformation = ReAllocateMemory(TokenInformation, Length);
    }

    if (NT_FAILED(Status))
    {
        FreeMemory(TokenInformation);
        TokenInformation = NULL;
    }
    else
    {
        *(PVOID *)Information = TokenInformation;
        if (Size != NULL)
            *Size=  Length;
    }

    return Status;
}

VOID ReleaseTokenInfo(PVOID TokenInfo)
{
    FreeMemory(TokenInfo);
}





/************************************************************************
  debug api
************************************************************************/







/************************************************************************
  list entry
************************************************************************/





































/************************************************************************
  registry api
************************************************************************/

#if !defined(HKEY_CURRENT_USER_LOCAL_SETTINGS)
    #define HKEY_CURRENT_USER_LOCAL_SETTINGS    (( HKEY ) (ULONG_PTR)((LONG)0x80000007) )
#endif









/************************************************************************
  memory
************************************************************************/









/************************************************************************
  process
************************************************************************/
/*
NTSTATUS
Nt_GetProcessUserName(
    PUNICODE_STRING UserName,
    HANDLE          Process
)
{
    HANDLE      Token;
    NTSTATUS    Status;
    PTOKEN_USER User;
    ULONG       Length, DomainLength;
    WCHAR       Name[MAX_NTPATH], Domain[MAX_NTPATH];
    SID_NAME_USE SidType;

    Status = NtOpenProcessToken(Process, TOKEN_QUERY, &Token);
    FAIL_RETURN(Status);

    Status = Nt_GetTokenInfo(Token, TokenUser, &User);
    if (NT_FAILED(Status))
    {
        NtClose(Token);
        return Status;
    }

    Length = countof(Name);
    DomainLength = countof(Domain);
    Status = LookupAccountSidW(NULL, User->User.Sid, Name, &Length, Domain, &DomainLength, &SidType);

    NtClose(Token);
    ReleaseTokenInfo(User);

    if (!Status)
        return STATUS_UNSUCCESSFUL;

    return RtlCreateUnicodeString(UserName, Name) ? STATUS_SUCCESS : STATUS_NO_MEMORY;
}
*/
#if ML_AMD64



#else // x86











#endif // x64


















/************************************************************************
  path
************************************************************************/























ULONG Nt_GetFileAttributes(LPCWSTR FileName)
{
    return Io::QueryFileAttributes(FileName);
}











ULONG_PTR Nt_GetModuleFileName(PVOID ModuleBase, LPWSTR Filename, ULONG_PTR BufferCount)
{
    ULONG_PTR               Length;
    PEB_BASE               *Peb;
    PLDR_DATA_TABLE_ENTRY   LdrModule, FirstLdrModule;

    Peb = Nt_CurrentPeb();
    LdrModule = FIELD_BASE(Peb->Ldr->InLoadOrderModuleList.Flink, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

    FirstLdrModule = LdrModule;

    if (ModuleBase == NULL)
        ModuleBase = Peb->ImageBaseAddress;

    LOOP_FOREVER
    {
        if ((ULONG_PTR)ModuleBase >= (ULONG_PTR)LdrModule->DllBase &&
            (ULONG_PTR)ModuleBase < (ULONG_PTR)LdrModule->DllBase + LdrModule->SizeOfImage)
        {
            break;
        }

        LdrModule = (PLDR_DATA_TABLE_ENTRY)LdrModule->InLoadOrderLinks.Flink;
        if (LdrModule == FirstLdrModule)
            return 0;
    }

    Length = InternalCopyUnicodeString(&LdrModule->FullDllName, Filename, BufferCount);
    if (LdrModule->FullDllName.Length / sizeof(WCHAR) < BufferCount)
        Filename[BufferCount] = 0;
/*
    Length = min(nSize * sizeof(*lpFilename), LdrModule->FullDllName.Length);
    CopyMemory(lpFilename, LdrModule->FullDllName.Buffer, Length);
    Length /= sizeof(*lpFilename);
    if (Length < nSize)
        lpFilename[Length] = 0;
*/
    return Length;
}



/************************************************************************
  misc
************************************************************************/



PVOID FASTCALL Nt_GetProcAddress(PVOID ModuleBase, LPCSTR lpProcName)
{
    return Ldr::GetRoutineAddress(ModuleBase, lpProcName);
}

PVOID Nt_GetModuleHandle(LPCWSTR lpModuleName)
{
    UNICODE_STRING  ModuleFileName;
    PLDR_MODULE     Module;

    RtlInitUnicodeString(&ModuleFileName, lpModuleName);
    Module = Ldr::FindLdrModuleByName(&ModuleFileName);
    return Module == NULL ? NULL : Module->DllBase;
}



#endif  // MY_NT_DDK

_ML_C_TAIL_

#pragma warning(default:4245)
#pragma warning(default:4750)

_ML_C_HEAD_

#if ML_KERNEL_MODE






#else // r3

#pragma comment(lib, "dbghelp.lib")
#include <DbgHelp.h>

NTSTATUS CreateMiniDump(PEXCEPTION_POINTERS ExceptionPointers)
{
    WCHAR           MiniDumpFile[MAX_NTPATH];
    NtFileDisk      File;
    BOOL            Success;
    NTSTATUS        Status;
    PROCESS_TIMES   Times;

    MINIDUMP_EXCEPTION_INFORMATION ExceptionInformation;

    NtQueryInformationProcess(NtCurrentProcess(), ProcessTimes, &Times, sizeof(Times), NULL);

    Nt_GetModuleFileName(NULL, MiniDumpFile, countof(MiniDumpFile));
    PWSTR Extension = findextw(MiniDumpFile);
    const SIZE_T ExtensionCapacity = countof(MiniDumpFile) - (Extension - MiniDumpFile);
    const int ExtensionLength = _snwprintf(Extension, ExtensionCapacity,
            L".%I64X.crash.dmp", Times.CreationTime.QuadPart);
    if (ExtensionLength < 0 || static_cast<SIZE_T>(ExtensionLength) >= ExtensionCapacity)
        return STATUS_NAME_TOO_LONG;

    Status = File.Create(MiniDumpFile);
    FAIL_RETURN(Status);

    ExceptionInformation.ClientPointers     = FALSE;
    ExceptionInformation.ExceptionPointers  = ExceptionPointers;
    ExceptionInformation.ThreadId           = Ps::CurrentTid();

    Success = MiniDumpWriteDump(
                NtCurrentProcess(),
                (ULONG_PTR)Ps::CurrentTeb()->ClientId.UniqueProcess,
                File,
                MiniDumpNormal,
                &ExceptionInformation,
                NULL,
                NULL
              );

    return Success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/************************************************************************
  directory
************************************************************************/



















IF_NOT_EXIST(_wfinddata64_t)
{
    typedef struct
    {
        unsigned    attrib;
        __int64  time_create;    /* -1 for FAT file systems */
        __int64  time_access;    /* -1 for FAT file systems */
        __int64  time_write;
        __int64     size;
        wchar_t     name[260];
    } _wfinddata64_t;
}

















/************************************************************************
  command line
************************************************************************/









#endif  // ML_KERNEL_MODE

_ML_C_TAIL_

_ML_C_HEAD_


PWSTR FASTCALL findextw(PCWChar lpFileName)
{
    WChar ch;
    PWSTR pDot, pSlash, pszFileName;

    pDot = 0;
    pSlash = 0;
    pszFileName = (PWSTR)lpFileName;
    LOOP_ALWAYS
    {
        ch = *pszFileName++;
        if (ch == 0)
            break;
        if (ch == '/' || ch == '\\')
            pSlash = pszFileName - 1;
        else if (ch == '.')
            pDot = pszFileName - 1;
    }

    if (pDot == NULL)
        pDot = pszFileName - 1;
    else if (pDot < pSlash)
        pDot = pszFileName - 1;

    return pDot;
}











PWChar StringUpperW(PWChar String, ULong Length)
{
    PWChar Text;

    if (Length == -1)
        Length = StrLengthW(String);

    for (Text = String; Length != 0; ++Text, --Length)
    {
        *Text = CHAR_UPPER(*Text);
    }

    return String;
}

Long_Ptr FASTCALL StrLengthA(PCChar pString)
{

    Long ch;
    Long_Ptr SizeOfUnit;
    PCChar pBuffer = pString;

    if (pString == NULL)
        return 0;

    SizeOfUnit = sizeof(Int32);
    while ((Long_Ptr)(pBuffer) & 3)
    {
        if (*pBuffer++ == 0)
        {
            --pBuffer;
            goto end_of_calc;
        }
    }

    LOOP_ALWAYS
    {
        Long temp;
        ch = *(PLong)pBuffer;
        pBuffer += SizeOfUnit;

        temp = (0x7EFEFEFF + ch) ^ (ch ^ -1);
        if ((temp & 0x81010100) == 0)
            continue;

        if (LoByte(ch) == 0)
        {
            pBuffer -= SizeOfUnit;
            break;
        }

        if ((ch & 0xFF00) == 0)
        {
            pBuffer -= SizeOfUnit - 1;
            break;
        }

        if ((ch & 0x00FF0000) == 0)
        {
            pBuffer -= SizeOfUnit - 2;
            break;
        }

        if ((ch & 0xFF000000) == 0)
        {
            pBuffer -= SizeOfUnit - 3;
            break;
        }
    }

end_of_calc:
    return pBuffer - pString;

}

Long_Ptr FASTCALL StrLengthW(PCWChar pString)
{
    Long ch;
    PCWChar pBuffer;

    if (pString == NULL)
        return 0;

    pBuffer = pString;
    while ((Int_Ptr)pBuffer & 3)
    {
        if (*pBuffer++ == 0)
        {
            --pBuffer;
            goto end_of_strlen;
        }
    }

    LOOP_ALWAYS
    {
        ch = *(PInt)pBuffer;
        if ((ch & 0xFFFF) == 0)
        {
            break;
        }
        else if ((ch & 0xFFFF0000) == 0)
        {
            pBuffer = (PCWChar)((PByte)pBuffer + sizeof(*pBuffer));
            break;
        }

        pBuffer = (PCWChar)((PByte)pBuffer + sizeof(ch));
    }

end_of_strlen:

    return pBuffer - pString;
}











Long_Ptr FASTCALL StrNICompareW(PCWChar pString1, PCWChar pString2, SizeT LengthToCompare, LongPtr CaseConvertTo)
{
    ULong ch1, ch2;

    if (LengthToCompare == 0)
        return 0;

    if (pString1 == NULL || pString2 == NULL)
        return 1;

    do
    {
        ch1 = *(PInt16)pString1;
        ch2 = *(PInt16)pString2;

        if ((ch1 | ch2) & 0xFF80)
        {
            if (ch1 != ch2)
                break;
        }
        else if (((ch1 ^ ch2) & 0xFFDF) != 0)
        {
            break;
        }


        ++pString1;
        ++pString2;
    } while (--LengthToCompare != 0 && ch1);

    switch (CaseConvertTo)
    {
        case StrCmp_Keep:
            break;

        case StrCmp_ToLower:
            ch1 = CHAR_LOWER(ch1);
            ch2 = CHAR_LOWER(ch2);
            break;

        case StrCmp_ToUpper:
            ch1 = CHAR_UPPER(ch1);
            ch2 = CHAR_UPPER(ch2);
            break;
    }

    // return LengthToCompare == 0 ? 0 : (Long_Ptr)(Int16)(ch1 - ch2);
    return LengthToCompare == 0 ? 0 : (ULong)ch1 < (ULong)ch2 ? -1 : (ULong)ch1 > (ULong)ch2 ? 1 : 0;
}









#define DECL_NUMBER_TABLE32(name) \
        UInt32 name[] = \
        { \
            0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
            0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
            0x03020100, 0x07060504, 0xFFFF0908, 0xFFFFFFFF, 0x0C0B0AFF, 0xFF0F0E0D, \
        }



















/***********************************************************************
 *           WPRINTF_ParseFormatA
 *
 * Parse a format specification. A format specification has the form:
 *
 * [-][#][0][width][.precision]type
 *
 * Return value is the length of the format specification in characters.
 ***********************************************************************/



/***********************************************************************
 *           WPRINTF_ParseFormatW
 *
 * Parse a format specification. A format specification has the form:
 *
 * [-][#][0][width][.precision]type
 *
 * Return value is the length of the format specification in characters.
 */

Int FormatString_ParseFormatW(PCWChar format, FMTSTR_FORMAT *res)
{
    PCWChar p = format;

    res->flags = 0;
    res->width = 0;
    res->precision = 0;
    if (*p == 'I' && p[1] == '6' && p[2] == '4') { res->flags |= FMTSTR_FLAG_I64; p += 3; }
    if (*p == '+') { res->flags |= FMTSTR_FLAG_FORCESIGNED; p++; }
    if (*p == '-') { res->flags |= FMTSTR_FLAG_LEFTALIGN; p++; }
    if (*p == '#') { res->flags |= FMTSTR_FLAG_PREFIX_HEX; p++; }
    if (*p == '0') { res->flags |= FMTSTR_FLAG_ZEROPAD; p++; }
    while ((*p >= '0') && (*p <= '9'))  /* width field */
    {
        res->width = res->width * 10 + *p - '0';
        p++;
    }
    if (*p == '.')  /* precision field */
    {
        p++;
        while ((*p >= '0') && (*p <= '9'))
        {
            res->precision = res->precision * 10 + *p - '0';
            p++;
        }
    }
    if (*p == 'l') { res->flags |= FMTSTR_FLAG_LONG; p++; }
    else if (*p == 'h') { res->flags |= FMTSTR_FLAG_SHORT; p++; }
    else if (*p == 'w') { res->flags |= FMTSTR_FLAG_WIDE; p++; }
    switch((Char)*p)
    {
        case 'c':
            res->type = (res->flags & FMTSTR_FLAG_SHORT) ? FMTSTR_FORMAT_CHAR : FMTSTR_FORMAT_WCHAR;
            break;

        case 'C':
            res->type = (res->flags & FMTSTR_FLAG_LONG) ? FMTSTR_FORMAT_WCHAR : FMTSTR_FORMAT_CHAR;
            break;

        case 'u':
            if (!TEST_BITS(res->flags, FMTSTR_FLAG_FORCESIGNED))
            {
                res->type = FMTSTR_FORMAT_UNSIGNED;
                break;
            }
            NO_BREAK;
        case 'd':
        case 'i':
            res->type = FMTSTR_FORMAT_SIGNED;
            break;

        case 's':
            res->type = ((res->flags & FMTSTR_FLAG_SHORT) && !(res->flags & FMTSTR_FLAG_WIDE)) ? FMTSTR_FORMAT_STRING : FMTSTR_FORMAT_WSTRING;
            break;

        case 'S':
            res->type = (res->flags & (FMTSTR_FLAG_LONG|FMTSTR_FLAG_WIDE)) ? FMTSTR_FORMAT_WSTRING : FMTSTR_FORMAT_STRING;
            break;

        case 'p':
            res->width = 8;
            res->flags |= FMTSTR_FLAG_ZEROPAD;
            /* fall through */
        case 'X':
            res->flags |= FMTSTR_FLAG_UPPER_HEX;
            /* fall through */
        case 'x':
            res->type = FMTSTR_FORMAT_HEXA;
            break;

#if MY_OS_WIN32
        case 'b':
        case 'B':
            res->type = FMTSTR_FORMAT_BINARY;
            break;
#endif

        default:
            res->type = FMTSTR_FORMAT_UNKNOWN;
            p--;  /* print format as normal char */
            break;
    }

    return (Int)(p - format) + 1;
}

inline
UInt
FormatString_GetLen(
    FMTSTR_FORMAT   *format,
    FMTSTR_DATA     *arg,
    PChar            number,
    UInt             maxlen
)
{
    UInt len = 0, UpperFlag;

    if (format->flags & FMTSTR_FLAG_LEFTALIGN)
        format->flags &= ~FMTSTR_FLAG_ZEROPAD;

    if (format->width > maxlen)
        format->width = maxlen;

    switch(format->type)
    {
        case FMTSTR_FORMAT_CHAR:
        case FMTSTR_FORMAT_WCHAR:
            return (format->precision = 1);

        case FMTSTR_FORMAT_STRING:
            if (arg->lpcstr_view)
            {
                len = StrLengthA(arg->lpcstr_view);
                if (format->precision != 0 && format->precision > len)
                    len = format->precision;
/*
                for (len = 0; format->precision == 0 || (len < format->precision); len++)
                {
                    Char ch = arg->lpcstr_view[len];
                    if (ch == 0)
                        break;
                }
*/
            }
            else
            {
                len = format->precision == NULL ? 6 : ML_MIN(6, format->precision);
            }

            if (len > maxlen)
                len = maxlen;

            return (format->precision = len);

        case FMTSTR_FORMAT_WSTRING:
            if (arg->lpcwstr_view)
            {
                len = StrLengthW(arg->lpcwstr_view);
                if (format->precision != 0 && format->precision > len)
                    len = format->precision;
/*
                for (len = 0; !format->precision || (len < format->precision); len++)
                    if (arg->lpcwstr_view[len] == 0)
                        break;
*/
            }
            else
            {
                len = format->precision == NULL ? 6 : ML_MIN(6, format->precision);
            }

            if (len > maxlen)
                len = maxlen;
/*
            if (!arg->lpcwstr_view) arg->lpcwstr_view = NULL;
            for (len = 0; !format->precision || (len < format->precision); len++)
                if (!*(arg->lpcwstr_view + len)) break;
            if (len > maxlen) len = maxlen;
*/
            return (format->precision = len);
/*
        case FMTSTR_FORMAT_SIGNED:
            if ((format->flags & WPRINTF_I64) == 0)
            {
                arg->int64_view = arg->int_view;
            }

            if ((INT64)arg->int64_view < 0)
            {
                *number++ = '-';
                arg->int64_view = -(INT64)arg->int64_view;
                ++len;
            }
        case FMTSTR_FORMAT_UNSIGNED:
            for (UINT64 i = arg->int64_view; i; i /= 10)
            {
                UINT64 v = i % 10;
                ++number;
            }
            for (UINT64 i = arg->int64_view; i; i /= 10)
            {
                UINT64 v = i % 10;
                *--number = v + '0';
                ++len;
            }
            break;
*/
        case FMTSTR_FORMAT_SIGNED:
            if (format->flags & FMTSTR_FLAG_I64)
            {
                if ((Int64)arg->int64_view < 0)
                {
                    *number++ = '-';
                    arg->int64_view = -(Int64)arg->int64_view;
                    ++len;
                }
            }
            else
            {
                if ((Int)arg->int_view < 0)
                {
                    *number++ = '-';
                    arg->int_view = -(Int)arg->int_view;
                    ++len;
                }
            }
            NO_BREAK;

        case FMTSTR_FORMAT_UNSIGNED:
            if (format->flags & FMTSTR_FLAG_I64)
            {
                if (arg->int64_view == 0)
                {
                    *number++ = '0';
                    ++len;
                    break;
                }

                for (UInt64 i = arg->int64_view; i; i /= 10)
                {
                    ++number;
                }
                for (UInt64 i = arg->int64_view; i; i /= 10)
                {
                    UInt64 v = i % 10;
                    *--number = (Char)(v + '0');
                    ++len;
                }
            }
            else
            {
                if (arg->int_view == 0)
                {
                    *number++ = '0';
                    ++len;
                    break;
                }

                for (UInt i = arg->int_view; i; i /= 10)
                {
                    ++number;
                }
                for (UInt i = arg->int_view; i; i /= 10)
                {
                    UInt v = i % 10;
                    *--number = (Char)(v + '0');
                    ++len;
                }
            }
            break;

        case FMTSTR_FORMAT_HEXA:
            UpperFlag = (format->flags & FMTSTR_FLAG_UPPER_HEX) ? 0xDF : 0xFF;

            if (format->flags & FMTSTR_FLAG_I64)
            {
                for (UInt64 i = arg->int64_view; i; i >>= 4)
                    ++number;

                for (UInt64 i = arg->int64_view; i; i >>= 4)
                {
                    UInt64 v = i & 0xF;
                    *--number = (Char)(v < 10 ? v + '0' : (v - 10 + ('a' & UpperFlag)));
                    ++len;
                }
            }
            else
            {
                for (UInt i = arg->int_view; i; i >>= 4)
                    ++number;

                for (UInt i = arg->int_view; i; i >>= 4)
                {
                    UInt v = i & 0xF;
                    *--number = (Char)(v < 10 ? v + '0' : (v - 10 + ('a' & UpperFlag)));
                    ++len;
                }
            }
            break;

#if MY_OS_WIN32

        case FMTSTR_FORMAT_BINARY:
            {
                ULong bit;
                Large_Integer val;

                val.QuadPart = arg->int64_view;
                if ((format->flags & FMTSTR_FLAG_I64) && val.HighPart != 0)
                {
                    _BitScanReverse(&bit, val.HighPart);
                    bit += 32;
                }
                else if (val.LowPart != 0)
                {
                    _BitScanReverse(&bit, val.LowPart);
                }
                else
                {
                    bit = 0;
                }

                number += bit;
                ++bit;
                do
                {
                    *number-- = (Char)(val.LowPart & 1) + '0';
                    ++len;
                } while (--bit);
            }
            break;

#endif

        default:
            return 0;
    }

    if (len > maxlen) len = maxlen;
    if (format->precision < len) format->precision = len;
    if (format->precision > maxlen) format->precision = maxlen;
    if ((format->flags & FMTSTR_FLAG_ZEROPAD) && (format->width > format->precision))
        format->precision = format->width;
    if (format->flags & FMTSTR_FLAG_PREFIX_HEX) len += 2;
    return len;
}

/***********************************************************************
 *           wvnsprintfA   (SHLWAPI.@)
 *
 * Print formatted output to a string, up to a maximum number of chars.
 *
 * PARAMS
 * buffer [O] Destination for output string
 * maxlen [I] Maximum number of characters to write
 * spec   [I] Format string
 *
 * RETURNS
 *  Success: The number of characters written.
 *  Failure: -1.
 */


/***********************************************************************
 *           wvnsprintfW   (SHLWAPI.@)
 *
 * See wvnsprintfA.
 */
Int FormatStringvnW(PWChar pszBuffer, UInt cchLimitIn, PCWChar pszFormat, va_list args)
{
    FMTSTR_FORMAT format;
    PWChar p = pszBuffer;
    UInt i, len, sign;
    Char number[65];
    FMTSTR_DATA argData;

    if (pszBuffer == NULL)
        cchLimitIn = (UInt)-1;

    while (*pszFormat && (cchLimitIn > 1))
    {
        if (*pszFormat != '%')
        {
            if (pszBuffer != NULL)
                *p = *pszFormat;

            ++p;
            ++pszFormat;
            cchLimitIn--;
            continue;
        }

        pszFormat++;

        if (*pszFormat == '%')
        {
            if (pszBuffer != NULL)
                *p = *pszFormat;

            ++p;
            ++pszFormat;
            cchLimitIn--;
            continue;
        }

        pszFormat += FormatString_ParseFormatW( pszFormat, &format );

        switch(format.type)
        {
            case FMTSTR_FORMAT_WCHAR:
                argData.wchar_view = (WChar)va_arg( args, int );
                break;

            case FMTSTR_FORMAT_CHAR:
                argData.char_view = (Char)va_arg( args, int );
                break;

            case FMTSTR_FORMAT_STRING:
                argData.lpcstr_view = va_arg( args, PCChar );
                break;

            case FMTSTR_FORMAT_WSTRING:
                argData.lpcwstr_view = va_arg( args, PCWChar );
                break;

#if MY_OS_WIN32
            case FMTSTR_FORMAT_BINARY:
#endif
            case FMTSTR_FORMAT_HEXA:
            case FMTSTR_FORMAT_SIGNED:
            case FMTSTR_FORMAT_UNSIGNED:
                if (format.flags & FMTSTR_FLAG_I64)
                    argData.int64_view = va_arg( args, Int64 );
                else
                    argData.int_view = va_arg( args, Int );
                break;

            default:
                argData.wchar_view = 0;
                break;
        }

        len = FormatString_GetLen( &format, &argData, number, cchLimitIn - 1 );
        sign = 0;
        if (!(format.flags & FMTSTR_FLAG_LEFTALIGN))
        {
            if (pszBuffer != NULL)
            {
                for (i = format.precision; i < format.width; i++, cchLimitIn--)
                    *p++ = ' ';
            }
            else if (format.precision < format.width)
            {
                UInt l = format.width - format.precision;
                p += l;
                cchLimitIn -= l;
            }
        }

        switch(format.type)
        {
            case FMTSTR_FORMAT_WCHAR:
                if (pszBuffer != NULL)
                    *p = argData.wchar_view;
                ++p;
                break;

            case FMTSTR_FORMAT_CHAR:
                if (pszBuffer != NULL)
                    *p = argData.char_view;
                ++p;
                break;

            case FMTSTR_FORMAT_STRING:
            case FMTSTR_FORMAT_WSTRING:
                if (len == 0)
                    break;

                if (format.type == FMTSTR_FORMAT_STRING)
                {
                    if (argData.lpcstr_view)
                    {
                        PCChar ptr = argData.lpcstr_view;
                        UInt MaxLength = cchLimitIn - (p - pszBuffer);

                        MaxLength = (cchLimitIn - (p - pszBuffer));
                        if (MaxLength > len * 2)
                            MaxLength = len * 2;

#if MY_NT_DDK

                        UNICODE_STRING  Unicode;
                        ANSI_STRING     Ansi;

                        Unicode.Buffer = (PWChar)ptr;
                        Unicode.Length = (UShort)(len * sizeof(*ptr));
                        Unicode.MaximumLength = Unicode.Length;

                        Ansi.Buffer = (PChar)p;
                        Ansi.MaximumLength = Ansi.Length;

                        RtlUnicodeStringToAnsiString(&Ansi, &Unicode, FALSE);
                        len = Ansi.Length;
#else
                        len = MultiByteToWideChar(CP_ACP, 0, ptr, len, p, pszBuffer == NULL ? 0 : MaxLength);
#endif // MY_NT_DDK
                        p += len;
                        break;
                    }
                }
                else if (argData.lpcwstr_view != NULL)
                {
                    if (pszBuffer != NULL)
                        CopyMemory(p, argData.lpcwstr_view, len * sizeof(WChar));
                    p += len;
                    cchLimitIn -= len;
                    break;
                }

                if (pszBuffer != NULL)
                    switch (len)
                    {
                        default:
                            p[5] = ')';
                        case 5:
                            p[4] = 'l';
                        case 4:
                            p[3] = 'l';
                        case 3:
                            p[2] = 'u';
                        case 2:
                            p[1] = 'n';
                        case 1:
                            p[0] = '(';
                            break;
                    }

                len = ML_MIN(6, len);
                p += len;
                break;

            case FMTSTR_FORMAT_HEXA:
                if ((format.flags & FMTSTR_FLAG_PREFIX_HEX) && (cchLimitIn > 3))
                {
                    if (pszBuffer != NULL)
                    {
                        *p++ = '0';
                        *p++ = (format.flags & FMTSTR_FLAG_UPPER_HEX) ? 'X' : 'x';
                    }
                    else
                    {
                        p += 2;
                    }
                    cchLimitIn -= 2;
                    len -= 2;
                }
                /* fall through */
            case FMTSTR_FORMAT_SIGNED:
                /* Transfer the sign now, just in case it will be zero-padded*/
                if (number[0] == '-')
                {
                    if (pszBuffer != NULL)
                        *p = '-';
                    ++p;
                    sign = 1;
                }
                /* fall through */
            case FMTSTR_FORMAT_UNSIGNED:
                if (pszBuffer != NULL)
                {
                    for (i = len; i < format.precision; i++, cchLimitIn--)
                        *p++ = '0';
                }
                else if (len < format.precision)
                {
                    UInt l = format.precision - len;
                    p += l;
                    cchLimitIn -= l;
                }
                /* fall through */
#if MY_OS_WIN32
            case FMTSTR_FORMAT_BINARY:
#endif
                if (pszBuffer != NULL)
                {
                    for (i = sign; i < len; i++)
                        *p++ = (WChar)number[i];
                }
                else if (sign < len)
                {
                    UInt l = len - sign;
                    p += l;
                }

                break;

            case FMTSTR_FORMAT_UNKNOWN:
                continue;
        }
        if (format.flags & FMTSTR_FLAG_LEFTALIGN)
        {
            if (pszBuffer != NULL)
            {
                for (i = format.precision; i < format.width; i++, cchLimitIn--)
                    *p++ = ' ';
            }
            else if (format.precision < format.width)
            {
                UInt l = format.width - format.precision;
                p += l;
                cchLimitIn -= l;
            }
        }
        cchLimitIn -= len;
    }

    if (pszBuffer != NULL)
        *p = 0;

    return (cchLimitIn >= 1) ? (Int)(p - pszBuffer) : -1;
}









_ML_C_TAIL_

_ML_C_HEAD_

#if ML_USER_MODE


ULONG_PTR PrintConsoleW(PCWSTR Format, ...)
{
    return PrintConsole(Format);
}

ULONG_PTR PrintConsole(PCWSTR Format, ...)
{
    BOOL        Success, IsConsole;
    ULONG       Length;
    WCHAR       Buffer[0xF00 / 2];
    va_list     Args;
    HANDLE      StdOutput;

    va_start(Args, Format);
    Length = _vsnwprintf(Buffer, countof(Buffer) - 1, Format, Args);
    if (Length == -1)
        return Length;

    StdOutput = CurrentPeb()->ProcessParameters->StandardOutput;

    IsConsole = TRUE;

    LOOP_ONCE
    {
        IO_STATUS_BLOCK             IoStatus;
        FILE_FS_DEVICE_INFORMATION  VolumeInfo;

        if (PtrAnd(StdOutput, 0x10000003) == (HANDLE)3)
            break;

        IoStatus.Status = NtQueryVolumeInformationFile(
                            StdOutput,
                            &IoStatus,
                            &VolumeInfo,
                            sizeof(VolumeInfo),
                            FileFsDeviceInformation
                        );
        if (NT_FAILED(IoStatus.Status))
            break;

        IsConsole = VolumeInfo.DeviceType == FILE_DEVICE_CONSOLE;
    }

    // if (PtrAnd(StdHandle, (1 << (bitsof(ULONG_PTR) - 4)) | 3) == (HANDLE)3)
    if (IsConsole)
    {
        Success = WriteConsoleW(
                    StdOutput,
                    Buffer,
                    Length,
                    &Length,
                    NULL
                 );
    }
    else
    {
        LARGE_INTEGER BytesWritten;

        Success = NT_SUCCESS(NtFileDisk::Write(StdOutput, Buffer, Length * sizeof(WCHAR), &BytesWritten));
        Length = Success ? ((ULONG_PTR)BytesWritten.QuadPart / sizeof(WCHAR)) : 0;
    }

    return Success ? Length : 0;
}







#endif // ML_USER_MODE

_ML_C_TAIL_
#if !ML_DISABLE_THIRD_LIB

#if !ML_DISABLE_THIRD_LIB_UCL

#define NRV2E

#include <ucl/ucl_conf.h>
#include <ucl/ucl.h>
#include <ucl/n2_99.c>
#include <ucl/alloc.c>

_ML_C_HEAD_




#if MY_OS_WIN32

#if ML_AMD64



#else // x86



#endif // x64

#endif // MY_OS_WIN32

_ML_C_TAIL_

#endif // !ML_DISABLE_THIRD_LIB_UCL

#endif // !ML_DISABLE_THIRD_LIB

#if MY_OS_WIN32

#if !MY_X64

#define SHA224_DIGEST_SIZE ( 224 / 8)
#define SHA256_DIGEST_SIZE ( 256 / 8)
#define SHA384_DIGEST_SIZE ( 384 / 8)
#define SHA512_DIGEST_SIZE ( 512 / 8)

#define SHA256_BLOCK_SIZE  ( 512 / 8)
#define SHA512_BLOCK_SIZE  (1024 / 8)
#define SHA384_BLOCK_SIZE  SHA512_BLOCK_SIZE
#define SHA224_BLOCK_SIZE  SHA256_BLOCK_SIZE

#pragma warning(push, 0)




#pragma warning(pop)

#endif // MY_X64
#endif // MY_OS_WIN32




#pragma warning(disable:4750)

_ML_C_HEAD_

#if ML_KERNEL_MODE

PVOID GetImageBaseAddress(PVOID ImageAddress)
{
    ULONG                           Size, BufferSize;
    NTSTATUS                        Status;
    PRTL_PROCESS_MODULE_INFORMATION Module;
    PRTL_PROCESS_MODULES            ModuleInformation;

    Size                = 0;
    BufferSize          = 0;
    ModuleInformation   = nullptr;

    LOOP_FOREVER
    {
        Status = ZwQuerySystemInformation(SystemModuleInformation, ModuleInformation, BufferSize, &Size);

        if (NT_SUCCESS(Status))
            break;

        if (Status != STATUS_INFO_LENGTH_MISMATCH)
            return nullptr;

        ModuleInformation = (PRTL_PROCESS_MODULES)AllocStack(Size - BufferSize);
        BufferSize = Size;
    }

    Module = ModuleInformation->Modules;
    for (ULONG Count = ModuleInformation->NumberOfModules; Count; --Count)
    {
        ULONG_PTR ImageBase, EndOfImage;

        ImageBase   = (ULONG_PTR)Module->ImageBase;
        EndOfImage  = ImageBase + Module->ImageSize;
        if (IN_RANGE(ImageBase, (ULONG_PTR)ImageAddress, EndOfImage))
            return (PVOID)ImageBase;

        ++Module;
    }

    return nullptr;
}

#else // user mode

PVOID GetImageBaseAddress(PVOID ImageAddress)
{
    PLDR_MODULE LdrModule;

    LdrModule = Ldr::FindLdrModuleByHandle(ImageAddress);

    return LdrModule == nullptr ? nullptr : LdrModule->DllBase;
}

#endif // ML_KERNEL_MODE

BOOL ValidateDataDirectory(PIMAGE_DATA_DIRECTORY DataDirectory, ULONG_PTR SizeOfImage)
{
    if (DataDirectory->Size == 0)
        return FALSE;

    if ((ULONG64)DataDirectory->VirtualAddress + DataDirectory->Size > ULONG_MAX)
        return FALSE;

    return DataDirectory->VirtualAddress <= SizeOfImage && DataDirectory->VirtualAddress + DataDirectory->Size <= SizeOfImage;
}

BOOL IsValidImage(PVOID ImageBase, ULONG_PTR Flags)
{
    PVOID                       Base, End, EndOfImage;
    ULONG_PTR                   Size, SizeOfImage;
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS           NtHeader;
    PIMAGE_NT_HEADERS64         NtHeader64;
    PIMAGE_DATA_DIRECTORY       DataDirectory, Directory;
    PIMAGE_IMPORT_DESCRIPTOR    ImportDescriptor;

    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return FALSE;

    NtHeader = (PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase + DosHeader->e_lfanew);
    if (NtHeader->Signature != IMAGE_NT_SIGNATURE)
        return FALSE;

    if (Flags == 0)
        return TRUE;

    switch (NtHeader->OptionalHeader.Magic)
    {
        case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
            DataDirectory = NtHeader->OptionalHeader.DataDirectory;
            SizeOfImage = NtHeader->OptionalHeader.SizeOfImage;
            break;

        case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
            NtHeader64 = (PIMAGE_NT_HEADERS64)NtHeader;
            DataDirectory = NtHeader64->OptionalHeader.DataDirectory;
            SizeOfImage = NtHeader64->OptionalHeader.SizeOfImage;
            break;

        default:
            return FALSE;
    }

    if (FLAG_ON(Flags, IMAGE_VALID_IMPORT_ADDRESS_TABLE))
    {
        if (!ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT], SizeOfImage))
            return FALSE;

        ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((ULONG_PTR)ImageBase + DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        if (ImportDescriptor->Name == 0)
            return FALSE;
    }

    if (FLAG_ON(Flags, IMAGE_VALID_EXPORT_ADDRESS_TABLE))
    {
        PIMAGE_EXPORT_DIRECTORY ExportDirectory;

        if (!ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT], SizeOfImage))
            return FALSE;

        ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)ImageBase + DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
        if (ExportDirectory->AddressOfFunctions >= SizeOfImage)
            return FALSE;
    }

    if (FLAG_ON(Flags, IMAGE_VALID_RELOC))
    {
        if (!ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC], SizeOfImage))
            return FALSE;
    }

    if (FLAG_ON(Flags, IMAGE_VALID_RESOURCE))
    {
        if (!ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE], SizeOfImage))
            return FALSE;
    }

    return TRUE;
}









ULONG_PTR IATLookupRoutineRVAByHashNoFix(PVOID ImageBase, ULONG_PTR Hash)
{
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS           NtHeaders;
    PIMAGE_IMPORT_DESCRIPTOR    ImportDescriptor;
    PIMAGE_THUNK_DATA           OriginalThunk, FirstThunk;

    if (IsValidImage(ImageBase, IMAGE_VALID_IMPORT_ADDRESS_TABLE) == FALSE)
        return IMAGE_INVALID_RVA;

    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase + DosHeader->e_lfanew);

    ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((ULONG_PTR)ImageBase + NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (; ImportDescriptor->Name != NULL && ImportDescriptor->FirstThunk != NULL; ++ImportDescriptor)
    {
        OriginalThunk = (PIMAGE_THUNK_DATA)ImageBase;
        if (ImportDescriptor->OriginalFirstThunk)
        {
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->OriginalFirstThunk);
        }
        else
        {
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->FirstThunk);
        }

        FirstThunk = (PIMAGE_THUNK_DATA)PtrAdd(ImageBase, ImportDescriptor->FirstThunk);

        for (; OriginalThunk->u1.AddressOfData != NULL; ++OriginalThunk, ++FirstThunk)
        {
            LONG_PTR FunctionName;

            FunctionName = OriginalThunk->u1.AddressOfData;
            if (FunctionName < 0)
                continue;

            FunctionName += (LONG_PTR)PtrAdd(ImageBase, 2);

            if ((HashAPI((PCSTR)FunctionName) ^ Hash) == 0)
            {
                return PtrOffset(&FirstThunk->u1.Function, ImageBase);
            }
        }
    }

    return IMAGE_INVALID_RVA;
}





PVOID EATLookupRoutineByHashNoFix(PVOID ImageBase, ULONG_PTR Hash)
{
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS           NtHeaders;
    PIMAGE_EXPORT_DIRECTORY     ExportDirectory;
    ULONG_PTR                   NumberOfNames;
    PULONG_PTR                  AddressOfFuntions;
    PCSTR                      *AddressOfNames;
    PUSHORT                     AddressOfNameOrdinals;

    if (Hash != 0 && !IsValidImage(ImageBase, IMAGE_VALID_EXPORT_ADDRESS_TABLE))
        return nullptr;

    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase + DosHeader->e_lfanew);

    switch (NtHeaders->OptionalHeader.Magic)
    {
        case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
            break;

        case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
            return EATLookupRoutineByHashNoFix64(ImageBase, Hash);
    }

    if (Hash == 0)
    {
        return &NtHeaders->OptionalHeader.AddressOfEntryPoint;
    }

    ExportDirectory = nullptr;
    ExportDirectory = PtrAdd(ExportDirectory, ImageBase);
    ExportDirectory = PtrAdd(ExportDirectory, NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    NumberOfNames           = ExportDirectory->NumberOfNames;
    AddressOfFuntions       = PtrAdd((PULONG_PTR)ImageBase, ExportDirectory->AddressOfFunctions);
    AddressOfNames          = PtrAdd((PCSTR *)ImageBase, ExportDirectory->AddressOfNames);
    AddressOfNameOrdinals   = PtrAdd((PUSHORT)ImageBase, ExportDirectory->AddressOfNameOrdinals);

    do
    {
        if (!(HashAPI(PtrAdd((PCSTR)(ImageBase), *AddressOfNames)) ^ Hash))
        {
            return AddressOfFuntions + *AddressOfNameOrdinals;
        }

        ++AddressOfNameOrdinals;
        ++AddressOfNames;
    } while (--NumberOfNames);

    return nullptr;
}

PVOID FASTCALL EATLookupRoutineByHashPNoFix(PVOID ImageBase, ULONG_PTR Hash)
{
    PVOID Pointer;

    Pointer = EATLookupRoutineByHashNoFix(ImageBase, Hash);
    if (Pointer != nullptr)
        Pointer = PtrAdd(ImageBase, *(PULONG)Pointer);

    return Pointer;
}

/************************************************************************
  x64 ver
************************************************************************/

PVOID EATLookupRoutineByHashNoFix64(PVOID ImageBase, ULONG_PTR Hash)
{
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS64         NtHeaders;
    PIMAGE_EXPORT_DIRECTORY     ExportDirectory;
    ULONG_PTR                   NumberOfNames;
    PULONG                      AddressOfFuntions;
    PULONG                      AddressOfNames;
    PUSHORT                     AddressOfNameOrdinals;

    if (Hash != 0 && !IsValidImage(ImageBase, IMAGE_VALID_EXPORT_ADDRESS_TABLE))
        return nullptr;

    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = (PIMAGE_NT_HEADERS64)((ULONG_PTR)ImageBase + DosHeader->e_lfanew);

    if (Hash == 0)
    {
        return &NtHeaders->OptionalHeader.AddressOfEntryPoint;
    }

    ExportDirectory = nullptr;
    ExportDirectory = PtrAdd(ExportDirectory, ImageBase);
    ExportDirectory = PtrAdd(ExportDirectory, NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    NumberOfNames           = ExportDirectory->NumberOfNames;
    AddressOfFuntions       = PtrAdd((PULONG)ImageBase, (ULONG_PTR)ExportDirectory->AddressOfFunctions);
    AddressOfNames          = PtrAdd((PULONG)ImageBase, (ULONG_PTR)ExportDirectory->AddressOfNames);
    AddressOfNameOrdinals   = PtrAdd((PUSHORT)ImageBase, (ULONG_PTR)ExportDirectory->AddressOfNameOrdinals);

    do
    {
        if (!(HashAPI(PtrAdd((PCSTR)(ImageBase), *AddressOfNames)) ^ Hash))
        {
            return AddressOfFuntions + *AddressOfNameOrdinals;
        }

        ++AddressOfNameOrdinals;
        ++AddressOfNames;
    } while (--NumberOfNames);

    return nullptr;
}



#if CPP_DEFINED

_ML_CPP_HEAD_

PVOID LookupImportTable(PVOID ImageBase, PCSTR DllName, ULONG Hash)
{
    PCSTR TargetDllName;
    PVOID IATVA;

    IATVA = IMAGE_INVALID_VA;
    TargetDllName = nullptr;

    WalkImportTableT(ImageBase,
        WalkIATCallbackM(Data)
        {
            if (DllName != nullptr)
            {
                if (TargetDllName != nullptr)
                {
                    if (TargetDllName != Data->DllName)
                        return STATUS_NOT_FOUND;
                }
                else if (_stricmp(Data->DllName, DllName) != 0)
                {
                    return STATUS_VALIDATE_CONTINUE;
                }
                else
                {
                    TargetDllName = Data->DllName;
                }
            }

            if (Data->FunctionName == nullptr)
                return STATUS_SUCCESS;

            if (HashAPI(Data->FunctionName) == Hash)
            {
                IATVA = Data->ThunkData;
                return STATUS_NO_MORE_MATCHES;
            }

            return STATUS_SUCCESS;
        },
        0
    );

    return IATVA;
}

PVOID LookupImportTable(PVOID ImageBase, PCSTR DllName, PCSTR RoutineName)
{
    return LookupImportTable(ImageBase, DllName, HashAPI(RoutineName));
}

PVOID LookupExportTable(PVOID ImageBase, ULONG Hash)
{
    ULONG RoutineRVA = (ULONG)IMAGE_INVALID_RVA;

    WalkExportTableT(ImageBase,
        WalkEATCallbackM(Data)
        {
            if (Data->FunctionName == nullptr)
                return STATUS_NO_MORE_MATCHES;

            if (HashAPI(Data->FunctionName) == Hash)
            {
                RoutineRVA = *Data->AddressOfFunction;
                return STATUS_NO_MORE_MATCHES;
            }

            return STATUS_SUCCESS;
        },0
    );

    return RoutineRVA == IMAGE_INVALID_RVA ? IMAGE_INVALID_VA : PtrAdd(ImageBase, RoutineRVA);
}

PVOID LookupExportTable(PVOID ImageBase, PCSTR RoutineName)
{
    return LookupExportTable(ImageBase, HashAPI(RoutineName));
}

_ML_CPP_TAIL_

#endif // cpp

#if ML_USER_MODE

typedef struct LOAD_MEM_DLL_INFO : public TEB_ACTIVE_FRAME
{
    ULONG           Flags;
    PVOID           MappedBase;
    PVOID           MemDllBase;
    SIZE_T          DllBufferSize;
    SIZE_T          ViewSize;
    UNICODE_STRING  Lz32Path;

    union
    {
        HANDLE DllFileHandle;
        HANDLE SectionHandle;
    };

    UNICODE_STRING  MemDllFullPath;

    API_POINTER(NtQueryAttributesFile)  NtQueryAttributesFile;
    API_POINTER(NtOpenFile)             NtOpenFile;
    API_POINTER(NtCreateSection)        NtCreateSection;
    API_POINTER(NtMapViewOfSection)     NtMapViewOfSection;
    API_POINTER(NtClose)                NtClose;
    API_POINTER(NtQuerySection)         NtQuerySection;
    API_POINTER(LdrLoadDll)             LdrLoadDll;

} LOAD_MEM_DLL_INFO, *PLOAD_MEM_DLL_INFO;

















#endif // ML_USER_MODE

_ML_C_TAIL_

#pragma warning(default:4750)

#if ML_SUPPORT_LAMBDA


ML_NAMESPACE
ML_NAMESPACE_BEGIN(DebugHelper);

static API_POINTER(RtlAllocateHeap)    StubRtlAllocateHeap;
static API_POINTER(RtlReAllocateHeap)  StubRtlReAllocateHeap;
static API_POINTER(RtlFreeHeap)        StubRtlFreeHeap;
static API_POINTER(RtlSizeHeap)        StubRtlSizeHeap;











ML_NAMESPACE_END_(DebugHelper);
ML_NAMESPACE_END; // ml





#endif // ML_SUPPORT_LAMBDA

// class

#if CPP_DEFINED


#pragma warning(disable:4750 4995)

#if CPP_DEFINED

NtFileDisk::NtFileDisk()
{
    m_FileHandle = nullptr;
}

NtFileDisk::NtFileDisk(const NtFileDisk &file)
{
    *this = file;
}

NtFileDisk::operator HANDLE() const
{
    return m_FileHandle;
}

NtFileDisk& NtFileDisk::operator=(const NtFileDisk &file)
{
    if (this == &file)
        return *this;

    Close();

    ZwDuplicateObject(
        NtCurrentProcess(),
        file,
        NtCurrentProcess(),
        &m_FileHandle,
        0,
        0,
        DUPLICATE_SAME_ACCESS
    );

    return *this;
}

NtFileDisk& NtFileDisk::operator=(HANDLE Handle)
{
    if (m_FileHandle == Handle)
        return *this;

    Close();

    ZwDuplicateObject(
        NtCurrentProcess(),
        Handle,
        NtCurrentProcess(),
        &m_FileHandle,
        0,
        0,
        DUPLICATE_SAME_ACCESS
    );

    return *this;
}

NTSTATUS NtFileDisk::QueryFullNtPath(PCWSTR FileName, PUNICODE_STRING NtFilePath, ULONG_PTR Flags)
{
#if ML_KERNEL_MODE

    return RtlCreateUnicodeString(NtFilePath, FileName) ? STATUS_SUCCESS : STATUS_OBJECT_PATH_NOT_FOUND;

#else // r3

    WCHAR ExpandBuffer[MAX_NTPATH];
    UNICODE_STRING Path, Expand;

    static UNICODE_STRING DosDevicesPrefix = RTL_CONSTANT_STRING(L"\\??\\");

    if (FLAG_ON(Flags, NFD_EXPAND_ENVVAR))
    {
        RtlInitUnicodeString(&Path, FileName);

        RtlInitEmptyString(&Expand, ExpandBuffer, sizeof(ExpandBuffer));
        RtlExpandEnvironmentStrings_U(nullptr, &Path, &Expand, nullptr);
    }
    else
    {
        RtlInitUnicodeString(&Expand, (PWSTR)FileName);
    }

    if (
        Expand.Length >= DosDevicesPrefix.Length &&
        RtlCompareMemory(Expand.Buffer, DosDevicesPrefix.Buffer, DosDevicesPrefix.Length) == DosDevicesPrefix.Length
       )
    {
        return RtlDuplicateUnicodeString(RTL_DUPSTR_ADD_NULL, &Expand, NtFilePath);
    }

    return RtlDosPathNameToNtPathName_U(Expand.Buffer, NtFilePath, nullptr, nullptr) ? STATUS_SUCCESS : STATUS_OBJECT_PATH_NOT_FOUND;

#endif // MY_NT_DDK
}

NTSTATUS
NtFileDisk::
CreateFileInternalWithFullPath(
    PHANDLE         FileHandle,
    PUNICODE_STRING FileName,
    ULONG_PTR       Flags,
    ULONG_PTR       ShareMode,
    ULONG_PTR       Access,
    ULONG_PTR       CreationDisposition,
    ULONG_PTR       Attributes,
    ULONG_PTR       CreateOptions /* = 0 */
)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    IO_STATUS_BLOCK     IoStatus;

    if (!FLAG_ON(Attributes, 0x40000000))
    {
        CreateOptions |= FILE_SYNCHRONOUS_IO_NONALERT;
    }

    CLEAR_FLAG(Attributes, 0x40000000);

/*
    if (FLAG_ON(Attributes, FILE_ATTRIBUTE_DIRECTORY))
    {
        CLEAR_FLAG(Attributes, FILE_ATTRIBUTE_DIRECTORY);
        SET_FLAG(CreateOptions, FILE_DIRECTORY_FILE);
    }

    if (FLAG_ON(Attributes, FILE_FLAG_OPEN_REPARSE_POINT))
    {
        CLEAR_FLAG(Attributes, FILE_FLAG_OPEN_REPARSE_POINT);
        SET_FLAG(CreateOptions, FILE_OPEN_REPARSE_POINT);
    }

    if (FLAG_ON(Attributes, FILE_FLAG_BACKUP_SEMANTICS))
    {
        CLEAR_FLAG(Attributes, FILE_FLAG_BACKUP_SEMANTICS);
        SET_FLAG(CreateOptions, FILE_OPEN_FOR_BACKUP_INTENT);
    }
*/
/*
    if(Access & GENERIC_ALL)
    {
        CreateOptions |= FILE_OPEN_FOR_BACKUP_INTENT; // | FILE_OPEN_REMOTE_INSTANCE;
    }
    else
    {
        if(Access & GENERIC_READ)
            CreateOptions |= FILE_OPEN_FOR_BACKUP_INTENT;

//            if(Access & GENERIC_WRITE)
//                CreateOptions |= FILE_OPEN_REMOTE_INSTANCE;
    }
*/
    InitializeObjectAttributes(&ObjectAttributes, FileName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);

    static API_POINTER(ZwCreateFile) XCreateFile;

    if (XCreateFile == nullptr) {

        PLDR_MODULE Nt = FindLdrModuleByName(&USTR(L"ntdll.dll"));

        XCreateFile = (API_POINTER(ZwCreateFile))LookupExportTable(Nt->DllBase, NTDLL_NtCreateFile);
    }

    Status = XCreateFile(
                FileHandle,
                Access | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                &ObjectAttributes,
                &IoStatus,
                nullptr,
                Attributes,
                ShareMode,
                CreationDisposition,
                CreateOptions,
                nullptr,
                0
             );

    return Status;
}

NTSTATUS
NtFileDisk::
CreateFileInternal(
    PHANDLE     FileHandle,
    LPCWSTR     FileName,
    ULONG_PTR   Flags,
    ULONG_PTR   ShareMode,
    ULONG_PTR   Access,
    ULONG_PTR   CreationDisposition,
    ULONG_PTR   Attributes,
    ULONG_PTR   CreateOptions /* = 0 */
)
{
    NTSTATUS            Status;
    UNICODE_STRING      NtFilePath;

    if (FLAG_ON(Flags, NFD_NOT_RESOLVE_PATH))
    {
        RtlInitUnicodeString(&NtFilePath, FileName);
    }
    else
    {
        Status = QueryFullNtPath(FileName, &NtFilePath, Flags);
        if (!NT_SUCCESS(Status))
            return Status;
    }

    Status = CreateFileInternalWithFullPath(
                FileHandle,
                &NtFilePath,
                Flags,
                ShareMode,
                Access,
                CreationDisposition,
                Attributes,
                CreateOptions
             );

    if (!FLAG_ON(Flags, NFD_NOT_RESOLVE_PATH))
        RtlFreeUnicodeString(&NtFilePath);

    return Status;
}

NTSTATUS NtFileDisk::GetSizeInternal(HANDLE FileHandle, PLARGE_INTEGER pFileSize)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    FILE_STANDARD_INFORMATION FileStandard;
    API_POINTER(ZwQueryInformationFile) XQueryInformationFile;

#if ML_KERNEL_MODE

    XQueryInformationFile = ZwQueryInformationFile;

#else

    XQueryInformationFile = NtQueryInformationFile;

#endif

    if (pFileSize == nullptr)
        return STATUS_INVALID_PARAMETER;

    pFileSize->QuadPart = 0;

    Status = XQueryInformationFile(
                FileHandle,
                &IoStatus,
                &FileStandard,
                sizeof(FileStandard),
                FileStandardInformation
              );

    if (!NT_SUCCESS(Status))
        return Status;

    pFileSize->QuadPart = FileStandard.EndOfFile.QuadPart;

    return Status;
}

NTSTATUS
NtFileDisk::
SeekInternal(
    HANDLE          FileHandle,
    LARGE_INTEGER   Offset,
    ULONG_PTR           MoveMethod,
    PLARGE_INTEGER  NewPosition /* = NULL */
)
{
    NTSTATUS                    Status;
    IO_STATUS_BLOCK             IoStatus;
    FILE_POSITION_INFORMATION   FilePosition;
    FILE_STANDARD_INFORMATION   FileStandard;
    API_POINTER(ZwQueryInformationFile) XQueryInformationFile;
    API_POINTER(ZwSetInformationFile)   XSetInformationFile;

#if ML_KERNEL_MODE

    XQueryInformationFile = ZwQueryInformationFile;
    XSetInformationFile = ZwSetInformationFile;

#else

    XQueryInformationFile = NtQueryInformationFile;
    XSetInformationFile = NtSetInformationFile;

#endif


    switch(MoveMethod)
    {
        case FILE_CURRENT:
            XQueryInformationFile(
                FileHandle,
                &IoStatus,
                &FilePosition,
                sizeof(FilePosition),
                FilePositionInformation);

            FilePosition.CurrentByteOffset.QuadPart += Offset.QuadPart;
            break;

        case FILE_END:
            XQueryInformationFile(
                FileHandle,
                &IoStatus,
                &FileStandard,
                sizeof(FileStandard),
                FileStandardInformation);

            FilePosition.CurrentByteOffset.QuadPart = FileStandard.EndOfFile.QuadPart + Offset.QuadPart;
            break;

        case FILE_BEGIN:
            FilePosition.CurrentByteOffset.QuadPart = Offset.QuadPart;
            break;

        default:
            return STATUS_INVALID_PARAMETER_2;
    }

    if(FilePosition.CurrentByteOffset.QuadPart < 0)
        return STATUS_INVALID_PARAMETER_1;

    Status = XSetInformationFile(
                FileHandle,
                &IoStatus,
                &FilePosition,
                sizeof(FILE_POSITION_INFORMATION),
                FilePositionInformation);

    if (!NT_SUCCESS(Status))
        return Status;

    if (NewPosition != nullptr)
    {
        NewPosition->QuadPart = FilePosition.CurrentByteOffset.QuadPart;
    }

    return Status;
}

NTSTATUS
NtFileDisk::
ReadInternal(
    HANDLE          FileHandle,
    PVOID           Buffer,
    ULONG_PTR           Size,
    PLARGE_INTEGER  BytesRead /* = NULL */,
    PLARGE_INTEGER  Offset     /* = NULL */
)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IoStatus;
    API_POINTER(ZwReadFile) XReadFile;

    if (BytesRead != nullptr)
        BytesRead->QuadPart = 0;

#if ML_KERNEL_MODE

    XReadFile = ZwReadFile;

#else

    XReadFile = NtReadFile;

#endif

    Status = XReadFile(
                FileHandle,
                nullptr,
                nullptr,
                nullptr,
                &IoStatus,
                Buffer,
                Size,
                Offset,
                nullptr
             );
/*
    if (Status == STATUS_PENDING)
    {
        Status = NtWaitForSingleObject(FileHandle, FALSE, NULL);
        if (NT_SUCCESS(Status))
            Status = IoStatus.Status;
    }
*/
    if (!NT_SUCCESS(Status) || Status == STATUS_PENDING)
        return Status;

    if (BytesRead != nullptr)
        BytesRead->QuadPart = IoStatus.Information;

    return Status;
}

NTSTATUS
NtFileDisk::
WriteInternal(
    HANDLE          FileHandle,
    PVOID           Buffer,
    ULONG_PTR           Size,
    PLARGE_INTEGER  pBytesWritten  /* = NULL */,
    PLARGE_INTEGER  Offset         /* = NULL */
)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IoStatus;

    API_POINTER(ZwWriteFile)    XWriteFile;

    if (pBytesWritten != nullptr)
        pBytesWritten->QuadPart = 0;

#if ML_KERNEL_MODE

    XWriteFile = ZwWriteFile;

#else

    XWriteFile = NtWriteFile;

#endif

    Status = XWriteFile(
                FileHandle,
                nullptr,
                nullptr,
                nullptr,
                &IoStatus,
                Buffer,
                Size,
                Offset,
                nullptr
             );
/*
    if (Status == STATUS_PENDING)
    {
        Status = NtWaitForSingleObject(FileHandle, FALSE, NULL);
        if (NT_SUCCESS(Status))
            Status = IoStatus.Status;
    }
*/
    if (!NT_SUCCESS(Status))
        return Status;

    if (pBytesWritten != nullptr)
        pBytesWritten->QuadPart = IoStatus.Information;

    return Status;
}

NTSTATUS NtFileDisk::DeleteInternal(HANDLE FileHandle)
{
    NTSTATUS Status;
    IO_STATUS_BLOCK  IoStatus;
    FILE_DISPOSITION_INFORMATION FileInformation;
    API_POINTER(ZwSetInformationFile)   XSetInformationFile;

#if ML_KERNEL_MODE

    XSetInformationFile = ZwSetInformationFile;

#else

    XSetInformationFile = NtSetInformationFile;

#endif

    FileInformation.DeleteFile = TRUE;
    Status = XSetInformationFile(
                 FileHandle,
                 &IoStatus,
                 &FileInformation,
                 sizeof(FileInformation),
                 FileDispositionInformation);

    return Status;
}

NTSTATUS NtFileDisk::SetEndOfFileInternal(HANDLE FileHandle, LARGE_INTEGER EndPosition)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IoStatus;

    union
    {
        FILE_END_OF_FILE_INFORMATION    EndOfFile;
        FILE_ALLOCATION_INFORMATION     FileAllocation;
    };

    API_POINTER(ZwSetInformationFile)   XSetInformationFile;

#if ML_KERNEL_MODE

    XSetInformationFile = ZwSetInformationFile;

#else

    XSetInformationFile = NtSetInformationFile;

#endif


    EndOfFile.EndOfFile.QuadPart = EndPosition.QuadPart;

    Status = XSetInformationFile(
                FileHandle,
                &IoStatus,
                &EndOfFile,
                sizeof(EndOfFile),
                FileEndOfFileInformation
             );

    if (!NT_SUCCESS(Status))
        return Status;

    FileAllocation.AllocationSize.QuadPart = EndPosition.QuadPart;
    Status = XSetInformationFile(
                FileHandle,
                &IoStatus,
                &FileAllocation,
                sizeof(FileAllocation),
                FileAllocationInformation
             );

    return Status;
}

NTSTATUS
NtFileDisk::
QuerySymbolicTargetInternal(
    HANDLE                  FileHandle,
    PREPARSE_DATA_BUFFER    ReparseBuffer,
    ULONG_PTR               BufferSize,
    PULONG_PTR              BytesReturned /* = NULL */
)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IoStatus;

    API_POINTER(ZwDeviceIoControlFile) IoControlRoutine;

#if ML_KERNEL_MODE

    IoControlRoutine = ZwFsControlFile;

#else

    IoControlRoutine = NtFsControlFile;

#endif

    Status = IoControlRoutine(
                FileHandle,
                nullptr,
                nullptr,
                nullptr,
                &IoStatus,
                FSCTL_GET_REPARSE_POINT,
                nullptr,
                0,
                ReparseBuffer,
                (ULONG_PTR)BufferSize
             );

    if (BytesReturned != nullptr)
        *BytesReturned = IoStatus.Information;

    return Status;
}

NTSTATUS
NtFileDisk::
QuerySymbolicTargetInternal(
    HANDLE                  FileHandle,
    PWSTR                   PrintNameBuffer,
    PULONG_PTR              PrintNameBufferCount,
    PWSTR                   SubstituteNameBuffer       /* = NULL */,
    PULONG_PTR              SubstituteNameBufferCount  /* = NULL */
)
{
    PWSTR                   PrintName, SubstituteName;
    ULONG_PTR               PrintLength, SubstituteLength;
    ULONG_PTR               ReparseBufferSize, Returned;
    NTSTATUS                Status;
    PREPARSE_DATA_BUFFER    ReparseBuffer;

    ReparseBufferSize = sizeof(*ReparseBuffer) + MAX_NTPATH * sizeof(WCHAR);
    ReparseBuffer = (PREPARSE_DATA_BUFFER)AllocStack(ReparseBufferSize);

    LOOP_FOREVER
    {
        Status = QuerySymbolicTargetInternal(FileHandle, ReparseBuffer, ReparseBufferSize, &Returned);
        if (NT_SUCCESS(Status))
            break;
        if (Status != STATUS_BUFFER_TOO_SMALL)
            return Status;

        ReparseBufferSize += MAX_NTPATH * sizeof(WCHAR);
        ReparseBuffer = (PREPARSE_DATA_BUFFER)AllocStack(ReparseBufferSize);
    }

    if (PrintNameBuffer == nullptr || PrintNameBufferCount == nullptr)
    {
        if (PrintNameBufferCount != nullptr)
        {
            *PrintNameBufferCount = ReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength / sizeof(WCHAR);
        }
    }
    else
    {
        PrintName   = ReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer + ReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(WCHAR);
        PrintLength = ReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength;

        PrintLength = MY_MIN(PrintLength, *PrintNameBufferCount);
        CopyMemory(PrintNameBuffer, PrintName, PrintLength);
        PrintLength /= sizeof(WCHAR);
        if (PrintLength < *PrintNameBufferCount)
            PrintNameBuffer[PrintLength] = 0;

        *PrintNameBufferCount = PrintLength;
    }

    if (SubstituteNameBuffer == nullptr || SubstituteNameBufferCount == nullptr)
    {
        if (SubstituteNameBufferCount != nullptr)
        {
            *SubstituteNameBufferCount = ReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
        }
    }
    else
    {
        SubstituteName  = ReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer + ReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR);
        SubstituteLength= ReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;

        SubstituteLength = MY_MIN(SubstituteLength, *SubstituteNameBufferCount);
        CopyMemory(SubstituteNameBuffer, SubstituteName, SubstituteLength);
        SubstituteLength /= sizeof(WCHAR);
        if (SubstituteLength < *SubstituteNameBufferCount)
            SubstituteNameBuffer[SubstituteLength] = 0;

        *SubstituteNameBufferCount = SubstituteLength;
    }

    return Status;
}

NTSTATUS NtFileDisk::Close()
{
    NTSTATUS Status;

    if (m_FileHandle == nullptr)
        return STATUS_SUCCESS;

#if ML_KERNEL_MODE

    Status = ZwClose(m_FileHandle);

#else

    Status = NtClose(m_FileHandle);

#endif

    if (!NT_SUCCESS(Status))
        return Status;

    m_FileHandle = nullptr;

    return Status;
}

NTSTATUS
NtFileDisk::
Create(
    LPCWSTR FileName,
    ULONG_PTR   Flags               /* = 0 */,
    ULONG_PTR   ShareMode           /* = FILE_SHARE_READ */,
    ULONG_PTR   Access              /* = GENERIC_WRITE */,
    ULONG_PTR   CreationDisposition /* = FILE_OVERWRITE_IF */,
    ULONG_PTR   Attributes          /* = FILE_ATTRIBUTE_NORMAL */,
    ULONG_PTR   CreateOptions       /* = 0 */
)
{
    NTSTATUS Status;

    Close();

    Status = CreateFileInternal(&m_FileHandle, FileName, Flags, ShareMode, Access, CreationDisposition, Attributes, CreateOptions);
    if (!NT_SUCCESS(Status))
        return Status;

    if (CreationDisposition != FILE_OVERWRITE_IF)
    {
        Status = GetSizeInternal(m_FileHandle, &m_FileSize);
        if (!NT_SUCCESS(Status))
        {
            Close();
            return Status;
        }
    }
    else
    {
        m_FileSize.QuadPart = 0;
    }

    m_Position.QuadPart = 0;

    return Status;
}

NTSTATUS
NtFileDisk::
OpenDevice(
    PCWSTR      FileName,
    ULONG_PTR   Flags               /* = 0 */,
    ULONG_PTR   ShareMode           /* = FILE_SHARE_READ */,
    ULONG_PTR   Access              /* = GENERIC_READ */,
    ULONG_PTR   CreationDisposition /* = FILE_OPEN */,
    ULONG_PTR   Attributes          /* = FILE_ATTRIBUTE_NORMAL */,
    ULONG_PTR   CreateOptions       /* = 0 */
)
{
    NTSTATUS        Status;
    UNICODE_STRING  DeviceName;

    Close();

    RtlInitUnicodeString(&DeviceName, FileName);

    Status = CreateFileInternalWithFullPath(&m_FileHandle, &DeviceName, Flags, ShareMode, Access, CreationDisposition, Attributes, CreateOptions);
    if (!NT_SUCCESS(Status))
        return Status;

    return Status;
}

NTSTATUS
NtFileDisk::
DeviceIoControl(
    ULONG_PTR   IoControlCode,
    PVOID       InputBuffer,
    ULONG_PTR   InputBufferLength,
    PVOID       OutputBuffer,
    ULONG_PTR   OutputBufferLength,
    PULONG_PTR  BytesReturned /* = NULL */
)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IoStatus;
    TYPE_OF(ZwDeviceIoControlFile)* IoControlRoutine;

#if ML_KERNEL_MODE

    IoControlRoutine = DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM ? ZwFsControlFile : ZwDeviceIoControlFile;

#else

    IoControlRoutine = DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM ? NtFsControlFile : NtDeviceIoControlFile;

#endif

    Status = IoControlRoutine(
                m_FileHandle,
                nullptr,
                nullptr,
                nullptr,
                &IoStatus,
                IoControlCode,
                InputBuffer,
                InputBufferLength,
                OutputBuffer,
                OutputBufferLength
             );
    if (!NT_SUCCESS(Status))
        return Status;

    if (BytesReturned != nullptr)
        *BytesReturned = IoStatus.Information;

    return Status;
}

NTSTATUS NtFileDisk::GetSize(PLARGE_INTEGER FileSize)
{
    if (FileSize == nullptr)
        return STATUS_INVALID_PARAMETER;

    FileSize->QuadPart = m_FileSize.QuadPart;

    return STATUS_SUCCESS;
}

NTSTATUS NtFileDisk::GetPosition(PLARGE_INTEGER Position)
{
    if (Position == nullptr)
        return STATUS_INVALID_PARAMETER;

    Position->QuadPart = m_Position.QuadPart;

    return STATUS_SUCCESS;
}

NTSTATUS
NtFileDisk::
Seek(
    LARGE_INTEGER   Offset,
    ULONG_PTR       MoveMethod,
    PLARGE_INTEGER  NewPosition /* = NULL */
)
{
#if 1
    LARGE_INTEGER NewOffset = m_Position;

    switch(MoveMethod)
    {
        case FILE_CURRENT:
            NewOffset.QuadPart += Offset.QuadPart;
            break;

        case FILE_END:
            NewOffset.QuadPart = m_FileSize.QuadPart + Offset.QuadPart;
            break;

        case FILE_BEGIN:
            NewOffset.QuadPart = Offset.QuadPart;
            break;

        default:
            return STATUS_INVALID_PARAMETER_2;
    }

    if (NewOffset.QuadPart < 0)
    {
        return STATUS_INVALID_PARAMETER_1;
    }

    m_Position = NewOffset;
    if (m_Position.QuadPart > m_FileSize.QuadPart)
        m_FileSize.QuadPart = m_Position.QuadPart;

    if (NewPosition != nullptr)
        NewPosition->QuadPart = m_Position.QuadPart;

    return STATUS_SUCCESS;
#else
    NTSTATUS Status;
    LARGE_INTEGER CurrentByteOffset;

    Status = SeekInternal(m_FileHandle, Offset, MoveMethod, &CurrentByteOffset);

    if (!NT_SUCCESS(Status))
        return Status;

    m_Position.QuadPart = CurrentByteOffset.QuadPart;
    if (m_Position.QuadPart > m_FileSize.QuadPart)
        m_FileSize.QuadPart = m_Position.QuadPart;

    if (NewPosition != nullptr)
        NewPosition->QuadPart = CurrentByteOffset.QuadPart;

    return Status;
#endif
}

NTSTATUS
NtFileDisk::
Read(
    PVOID           Buffer,
    ULONG_PTR       Size        /* = 0 */,
    PLARGE_INTEGER  BytesRead   /* = NULL */
)
{
    NTSTATUS        Status;
    LARGE_INTEGER   LocalBytesRead;

    if (Size == 0)
        Size = m_FileSize.LowPart;

    Status = ReadInternal(m_FileHandle, Buffer, Size, &LocalBytesRead, &m_Position);
    if (!NT_SUCCESS(Status) || Status == STATUS_PENDING)
        return Status;

    m_Position.QuadPart += LocalBytesRead.QuadPart;

    if (BytesRead != nullptr)
        BytesRead->QuadPart = LocalBytesRead.QuadPart;

    return Status;
}

NTSTATUS
NtFileDisk::
Write(
    PVOID           Buffer,
    ULONG_PTR           Size,
    PLARGE_INTEGER  pBytesWritten /* = NULL */
)
{
    NTSTATUS        Status;
    LARGE_INTEGER   BytesWritten;

    Status = WriteInternal(m_FileHandle, Buffer, Size, &BytesWritten, &m_Position);
    if (!NT_SUCCESS(Status) || Status == STATUS_PENDING)
        return Status;

    if (pBytesWritten != nullptr)
        pBytesWritten->QuadPart = BytesWritten.QuadPart;

    if (BytesWritten.QuadPart + m_Position.QuadPart > m_FileSize.QuadPart)
    {
        m_FileSize.QuadPart = BytesWritten.QuadPart + m_Position.QuadPart;
    }

    m_Position.QuadPart += BytesWritten.QuadPart;

    return Status;
}

NTSTATUS
NtFileDisk::
Print(
    PLARGE_INTEGER  BytesWritten,
    PWSTR           Format,
    ...
)
{
    ULONG_PTR   Length;
    WCHAR   Buffer[0xFF0 / 2];
    va_list Arg;

    va_start(Arg, Format);
    //Length = _vsnwprintf(Buffer, countof(Buffer) - 1, Format, Arg);
    Length = FormatStringvnW(Buffer, countof(Buffer) - 1, Format, Arg);
    if (Length == -1)
        return STATUS_BUFFER_TOO_SMALL;

    return Write(Buffer, Length * sizeof(WCHAR), BytesWritten);
}

NTSTATUS NtFileDisk::MapView(PVOID *BaseAddress, HANDLE ProcessHandle)
{
    NTSTATUS    Status;
    HANDLE      SectionHandle;
    ULONG_PTR   ViewSize;

    Status = NtCreateSection(
                &SectionHandle,
                GENERIC_READ,
                NULL,
                NULL,
                PAGE_READONLY,
                SEC_IMAGE,
                *this
             );
    FAIL_RETURN(Status);

    ViewSize = 0;
    Status = NtMapViewOfSection(
                SectionHandle,
                ProcessHandle,
                BaseAddress,
                0,
                0,
                NULL,
                &ViewSize,
                ViewShare,
                0,
                PAGE_READONLY
             );

    NtClose(SectionHandle);

    return Status;
}

NTSTATUS NtFileDisk::UnMapView(PVOID BaseAddress, HANDLE ProcessHandle)
{
    if (BaseAddress == nullptr)
        return STATUS_INVALID_PARAMETER;

#if ML_USER_MODE

    return NtUnmapViewOfSection(ProcessHandle, BaseAddress);

#else

    return STATUS_NOT_IMPLEMENTED;

#endif
}


NTSTATUS NtFileDisk::SetEndOfFile()
{
    NTSTATUS Status;

    Status = SetEndOfFileInternal(m_FileHandle, m_Position);
    if (NT_SUCCESS(Status))
    {
        m_FileSize.QuadPart = m_Position.QuadPart;
    }

    return Status;
}

NTSTATUS NtFileDisk::SetEndOfFile(LARGE_INTEGER EndPosition)
{
    NTSTATUS Status;

    Status = SetEndOfFileInternal(m_FileHandle, EndPosition);
    if (NT_SUCCESS(Status))
    {
        m_FileSize.QuadPart = EndPosition.QuadPart;
        m_Position.QuadPart = ML_MIN(m_Position.QuadPart, EndPosition.QuadPart);
    }

    return Status;
}

/************************************************************************
  static ver
************************************************************************/

NTSTATUS NtFileDisk::SetEndOfFile(HANDLE FileHandle)
{
    NTSTATUS                    Status;
    FILE_POSITION_INFORMATION   FilePosition;
    IO_STATUS_BLOCK             IoStatus;
    API_POINTER(ZwQueryInformationFile) XQueryInformationFile;

#if ML_KERNEL_MODE

    XQueryInformationFile = ZwQueryInformationFile;

#else

    XQueryInformationFile = NtQueryInformationFile;

#endif

    Status = XQueryInformationFile(
                FileHandle,
                &IoStatus,
                &FilePosition,
                sizeof(FilePosition),
                FilePositionInformation
            );
    if (!NT_SUCCESS(Status))
        return Status;

    return SetEndOfFileInternal(FileHandle, FilePosition.CurrentByteOffset);
}

#endif // CPP_DEFINED

#pragma warning(default:4750 4995)
//#include "Hook.cpp"
//#include "HookHelper.cpp"
/*
  ___________________________________________________
  Opcode Length Disassembler.
  Coded By Ms-Rem ( Ms-Rem@yandex.ru ) ICQ 286370715
  ---------------------------------------------------
  12.08.2005 - fixed many bugs...
  09.08.2005 - fixed bug with 0F BA opcode.
  07.08.2005 - added SSE, SSE2, SSE3 and 3Dnow instruction support.
  06.08.2005 - fixed bug with F6 and F7 opcodes.
  29.07.2005 - fixed bug with OP_X64_WORD opcodes.
*/


#define OP_X64_NONE           0x00
#define OP_X64_MODRM          0x01
#define OP_X64_DATA_I8        0x02
#define OP_X64_DATA_I16       0x04
#define OP_X64_DATA_I32       0x08
#define OP_X64_DATA_PRE66_67  0x10
#define OP_X64_WORD           0x20
#define OP_X64_REL32          0x40

ForceInline PUCHAR GetOpCodeFlags()
{
    static UCHAR OpcodeFlags[256] =
    {
        OP_X64_MODRM,                      // 00
        OP_X64_MODRM,                      // 01
        OP_X64_MODRM,                      // 02
        OP_X64_MODRM,                      // 03
        OP_X64_DATA_I8,                    // 04
        OP_X64_DATA_PRE66_67,              // 05
        OP_X64_NONE,                       // 06
        OP_X64_NONE,                       // 07
        OP_X64_MODRM,                      // 08
        OP_X64_MODRM,                      // 09
        OP_X64_MODRM,                      // 0A
        OP_X64_MODRM,                      // 0B
        OP_X64_DATA_I8,                    // 0C
        OP_X64_DATA_PRE66_67,              // 0D
        OP_X64_NONE,                       // 0E
        OP_X64_NONE,                       // 0F
        OP_X64_MODRM,                      // 10
        OP_X64_MODRM,                      // 11
        OP_X64_MODRM,                      // 12
        OP_X64_MODRM,                      // 13
        OP_X64_DATA_I8,                    // 14
        OP_X64_DATA_PRE66_67,              // 15
        OP_X64_NONE,                       // 16
        OP_X64_NONE,                       // 17
        OP_X64_MODRM,                      // 18
        OP_X64_MODRM,                      // 19
        OP_X64_MODRM,                      // 1A
        OP_X64_MODRM,                      // 1B
        OP_X64_DATA_I8,                    // 1C
        OP_X64_DATA_PRE66_67,              // 1D
        OP_X64_NONE,                       // 1E
        OP_X64_NONE,                       // 1F
        OP_X64_MODRM,                      // 20
        OP_X64_MODRM,                      // 21
        OP_X64_MODRM,                      // 22
        OP_X64_MODRM,                      // 23
        OP_X64_DATA_I8,                    // 24
        OP_X64_DATA_PRE66_67,              // 25
        OP_X64_NONE,                       // 26
        OP_X64_NONE,                       // 27
        OP_X64_MODRM,                      // 28
        OP_X64_MODRM,                      // 29
        OP_X64_MODRM,                      // 2A
        OP_X64_MODRM,                      // 2B
        OP_X64_DATA_I8,                    // 2C
        OP_X64_DATA_PRE66_67,              // 2D
        OP_X64_NONE,                       // 2E
        OP_X64_NONE,                       // 2F
        OP_X64_MODRM,                      // 30
        OP_X64_MODRM,                      // 31
        OP_X64_MODRM,                      // 32
        OP_X64_MODRM,                      // 33
        OP_X64_DATA_I8,                    // 34
        OP_X64_DATA_PRE66_67,              // 35
        OP_X64_NONE,                       // 36
        OP_X64_NONE,                       // 37
        OP_X64_MODRM,                      // 38
        OP_X64_MODRM,                      // 39
        OP_X64_MODRM,                      // 3A
        OP_X64_MODRM,                      // 3B
        OP_X64_DATA_I8,                    // 3C
        OP_X64_DATA_PRE66_67,              // 3D
        OP_X64_NONE,                       // 3E
        OP_X64_NONE,                       // 3F
        OP_X64_NONE,                       // 40
        OP_X64_NONE,                       // 41
        OP_X64_NONE,                       // 42
        OP_X64_NONE,                       // 43
        OP_X64_NONE,                       // 44
        OP_X64_NONE,                       // 45
        OP_X64_NONE,                       // 46
        OP_X64_NONE,                       // 47
        OP_X64_NONE,                       // 48
        OP_X64_NONE,                       // 49
        OP_X64_NONE,                       // 4A
        OP_X64_NONE,                       // 4B
        OP_X64_NONE,                       // 4C
        OP_X64_NONE,                       // 4D
        OP_X64_NONE,                       // 4E
        OP_X64_NONE,                       // 4F
        OP_X64_NONE,                       // 50
        OP_X64_NONE,                       // 51
        OP_X64_NONE,                       // 52
        OP_X64_NONE,                       // 53
        OP_X64_NONE,                       // 54
        OP_X64_NONE,                       // 55
        OP_X64_NONE,                       // 56
        OP_X64_NONE,                       // 57
        OP_X64_NONE,                       // 58
        OP_X64_NONE,                       // 59
        OP_X64_NONE,                       // 5A
        OP_X64_NONE,                       // 5B
        OP_X64_NONE,                       // 5C
        OP_X64_NONE,                       // 5D
        OP_X64_NONE,                       // 5E
        OP_X64_NONE,                       // 5F
        OP_X64_NONE,                       // 60
        OP_X64_NONE,                       // 61
        OP_X64_MODRM,                      // 62
        OP_X64_MODRM,                      // 63
        OP_X64_NONE,                       // 64
        OP_X64_NONE,                       // 65
        OP_X64_NONE,                       // 66
        OP_X64_NONE,                       // 67
        OP_X64_DATA_PRE66_67,              // 68
        OP_X64_MODRM | OP_X64_DATA_PRE66_67,   // 69
        OP_X64_DATA_I8,                    // 6A
        OP_X64_MODRM | OP_X64_DATA_I8,         // 6B
        OP_X64_NONE,                       // 6C
        OP_X64_NONE,                       // 6D
        OP_X64_NONE,                       // 6E
        OP_X64_NONE,                       // 6F
        OP_X64_DATA_I8,                    // 70
        OP_X64_DATA_I8,                    // 71
        OP_X64_DATA_I8,                    // 72
        OP_X64_DATA_I8,                    // 73
        OP_X64_DATA_I8,                    // 74
        OP_X64_DATA_I8,                    // 75
        OP_X64_DATA_I8,                    // 76
        OP_X64_DATA_I8,                    // 77
        OP_X64_DATA_I8,                    // 78
        OP_X64_DATA_I8,                    // 79
        OP_X64_DATA_I8,                    // 7A
        OP_X64_DATA_I8,                    // 7B
        OP_X64_DATA_I8,                    // 7C
        OP_X64_DATA_I8,                    // 7D
        OP_X64_DATA_I8,                    // 7E
        OP_X64_DATA_I8,                    // 7F
        OP_X64_MODRM | OP_X64_DATA_I8,         // 80
        OP_X64_MODRM | OP_X64_DATA_PRE66_67,   // 81
        OP_X64_MODRM | OP_X64_DATA_I8,         // 82
        OP_X64_MODRM | OP_X64_DATA_I8,         // 83
        OP_X64_MODRM,                      // 84
        OP_X64_MODRM,                      // 85
        OP_X64_MODRM,                      // 86
        OP_X64_MODRM,                      // 87
        OP_X64_MODRM,                      // 88
        OP_X64_MODRM,                      // 89
        OP_X64_MODRM,                      // 8A
        OP_X64_MODRM,                      // 8B
        OP_X64_MODRM,                      // 8C
        OP_X64_MODRM,                      // 8D
        OP_X64_MODRM,                      // 8E
        OP_X64_MODRM,                      // 8F
        OP_X64_NONE,                       // 90
        OP_X64_NONE,                       // 91
        OP_X64_NONE,                       // 92
        OP_X64_NONE,                       // 93
        OP_X64_NONE,                       // 94
        OP_X64_NONE,                       // 95
        OP_X64_NONE,                       // 96
        OP_X64_NONE,                       // 97
        OP_X64_NONE,                       // 98
        OP_X64_NONE,                       // 99
        OP_X64_DATA_I16 | OP_X64_DATA_PRE66_67,// 9A
        OP_X64_NONE,                       // 9B
        OP_X64_NONE,                       // 9C
        OP_X64_NONE,                       // 9D
        OP_X64_NONE,                       // 9E
        OP_X64_NONE,                       // 9F
        OP_X64_DATA_PRE66_67,              // A0
        OP_X64_DATA_PRE66_67,              // A1
        OP_X64_DATA_PRE66_67,              // A2
        OP_X64_DATA_PRE66_67,              // A3
        OP_X64_NONE,                       // A4
        OP_X64_NONE,                       // A5
        OP_X64_NONE,                       // A6
        OP_X64_NONE,                       // A7
        OP_X64_DATA_I8,                    // A8
        OP_X64_DATA_PRE66_67,              // A9
        OP_X64_NONE,                       // AA
        OP_X64_NONE,                       // AB
        OP_X64_NONE,                       // AC
        OP_X64_NONE,                       // AD
        OP_X64_NONE,                       // AE
        OP_X64_NONE,                       // AF
        OP_X64_DATA_I8,                    // B0
        OP_X64_DATA_I8,                    // B1
        OP_X64_DATA_I8,                    // B2
        OP_X64_DATA_I8,                    // B3
        OP_X64_DATA_I8,                    // B4
        OP_X64_DATA_I8,                    // B5
        OP_X64_DATA_I8,                    // B6
        OP_X64_DATA_I8,                    // B7
        OP_X64_DATA_PRE66_67,              // B8
        OP_X64_DATA_PRE66_67,              // B9
        OP_X64_DATA_PRE66_67,              // BA
        OP_X64_DATA_PRE66_67,              // BB
        OP_X64_DATA_PRE66_67,              // BC
        OP_X64_DATA_PRE66_67,              // BD
        OP_X64_DATA_PRE66_67,              // BE
        OP_X64_DATA_PRE66_67,              // BF
        OP_X64_MODRM | OP_X64_DATA_I8,         // C0
        OP_X64_MODRM | OP_X64_DATA_I8,         // C1
        OP_X64_DATA_I16,                   // C2
        OP_X64_NONE,                       // C3
        OP_X64_MODRM,                      // C4
        OP_X64_MODRM,                      // C5
        OP_X64_MODRM   | OP_X64_DATA_I8,       // C6
        OP_X64_MODRM   | OP_X64_DATA_PRE66_67, // C7
        OP_X64_DATA_I8 | OP_X64_DATA_I16,      // C8
        OP_X64_NONE,                       // C9
        OP_X64_DATA_I16,                   // CA
        OP_X64_NONE,                       // CB
        OP_X64_NONE,                       // CC
        OP_X64_DATA_I8,                    // CD
        OP_X64_NONE,                       // CE
        OP_X64_NONE,                       // CF
        OP_X64_MODRM,                      // D0
        OP_X64_MODRM,                      // D1
        OP_X64_MODRM,                      // D2
        OP_X64_MODRM,                      // D3
        OP_X64_DATA_I8,                    // D4
        OP_X64_DATA_I8,                    // D5
        OP_X64_NONE,                       // D6
        OP_X64_NONE,                       // D7
        OP_X64_WORD,                       // D8
        OP_X64_WORD,                       // D9
        OP_X64_WORD,                       // DA
        OP_X64_WORD,                       // DB
        OP_X64_WORD,                       // DC
        OP_X64_WORD,                       // DD
        OP_X64_WORD,                       // DE
        OP_X64_WORD,                       // DF
        OP_X64_DATA_I8,                    // E0
        OP_X64_DATA_I8,                    // E1
        OP_X64_DATA_I8,                    // E2
        OP_X64_DATA_I8,                    // E3
        OP_X64_DATA_I8,                    // E4
        OP_X64_DATA_I8,                    // E5
        OP_X64_DATA_I8,                    // E6
        OP_X64_DATA_I8,                    // E7
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // E8
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // E9
        OP_X64_DATA_I16 | OP_X64_DATA_PRE66_67,// EA
        OP_X64_DATA_I8,                    // EB
        OP_X64_NONE,                       // EC
        OP_X64_NONE,                       // ED
        OP_X64_NONE,                       // EE
        OP_X64_NONE,                       // EF
        OP_X64_NONE,                       // F0
        OP_X64_NONE,                       // F1
        OP_X64_NONE,                       // F2
        OP_X64_NONE,                       // F3
        OP_X64_NONE,                       // F4
        OP_X64_NONE,                       // F5
        OP_X64_MODRM,                      // F6
        OP_X64_MODRM,                      // F7
        OP_X64_NONE,                       // F8
        OP_X64_NONE,                       // F9
        OP_X64_NONE,                       // FA
        OP_X64_NONE,                       // FB
        OP_X64_NONE,                       // FC
        OP_X64_NONE,                       // FD
        OP_X64_MODRM,                      // FE
        OP_X64_MODRM | OP_X64_REL32            // FF
    };

    return OpcodeFlags;
}

ForceInline PUCHAR GetOpCodeFlagsExt()
{
    static UCHAR OpcodeFlagsExt[256] =
    {
        OP_X64_MODRM,                      // 00
        OP_X64_MODRM,                      // 01
        OP_X64_MODRM,                      // 02
        OP_X64_MODRM,                      // 03
        OP_X64_NONE,                       // 04
        OP_X64_NONE,                       // 05
        OP_X64_NONE,                       // 06
        OP_X64_NONE,                       // 07
        OP_X64_NONE,                       // 08
        OP_X64_NONE,                       // 09
        OP_X64_NONE,                       // 0A
        OP_X64_NONE,                       // 0B
        OP_X64_NONE,                       // 0C
        OP_X64_MODRM,                      // 0D
        OP_X64_NONE,                       // 0E
        OP_X64_MODRM | OP_X64_DATA_I8,         // 0F
        OP_X64_MODRM,                      // 10
        OP_X64_MODRM,                      // 11
        OP_X64_MODRM,                      // 12
        OP_X64_MODRM,                      // 13
        OP_X64_MODRM,                      // 14
        OP_X64_MODRM,                      // 15
        OP_X64_MODRM,                      // 16
        OP_X64_MODRM,                      // 17
        OP_X64_MODRM,                      // 18
        OP_X64_NONE,                       // 19
        OP_X64_NONE,                       // 1A
        OP_X64_NONE,                       // 1B
        OP_X64_NONE,                       // 1C
        OP_X64_NONE,                       // 1D
        OP_X64_NONE,                       // 1E
        OP_X64_NONE,                       // 1F
        OP_X64_MODRM,                      // 20
        OP_X64_MODRM,                      // 21
        OP_X64_MODRM,                      // 22
        OP_X64_MODRM,                      // 23
        OP_X64_MODRM,                      // 24
        OP_X64_NONE,                       // 25
        OP_X64_MODRM,                      // 26
        OP_X64_NONE,                       // 27
        OP_X64_MODRM,                      // 28
        OP_X64_MODRM,                      // 29
        OP_X64_MODRM,                      // 2A
        OP_X64_MODRM,                      // 2B
        OP_X64_MODRM,                      // 2C
        OP_X64_MODRM,                      // 2D
        OP_X64_MODRM,                      // 2E
        OP_X64_MODRM,                      // 2F
        OP_X64_NONE,                       // 30
        OP_X64_NONE,                       // 31
        OP_X64_NONE,                       // 32
        OP_X64_NONE,                       // 33
        OP_X64_NONE,                       // 34
        OP_X64_NONE,                       // 35
        OP_X64_NONE,                       // 36
        OP_X64_NONE,                       // 37
        OP_X64_NONE,                       // 38
        OP_X64_NONE,                       // 39
        OP_X64_NONE,                       // 3A
        OP_X64_NONE,                       // 3B
        OP_X64_NONE,                       // 3C
        OP_X64_NONE,                       // 3D
        OP_X64_NONE,                       // 3E
        OP_X64_NONE,                       // 3F
        OP_X64_MODRM,                      // 40
        OP_X64_MODRM,                      // 41
        OP_X64_MODRM,                      // 42
        OP_X64_MODRM,                      // 43
        OP_X64_MODRM,                      // 44
        OP_X64_MODRM,                      // 45
        OP_X64_MODRM,                      // 46
        OP_X64_MODRM,                      // 47
        OP_X64_MODRM,                      // 48
        OP_X64_MODRM,                      // 49
        OP_X64_MODRM,                      // 4A
        OP_X64_MODRM,                      // 4B
        OP_X64_MODRM,                      // 4C
        OP_X64_MODRM,                      // 4D
        OP_X64_MODRM,                      // 4E
        OP_X64_MODRM,                      // 4F
        OP_X64_MODRM,                      // 50
        OP_X64_MODRM,                      // 51
        OP_X64_MODRM,                      // 52
        OP_X64_MODRM,                      // 53
        OP_X64_MODRM,                      // 54
        OP_X64_MODRM,                      // 55
        OP_X64_MODRM,                      // 56
        OP_X64_MODRM,                      // 57
        OP_X64_MODRM,                      // 58
        OP_X64_MODRM,                      // 59
        OP_X64_MODRM,                      // 5A
        OP_X64_MODRM,                      // 5B
        OP_X64_MODRM,                      // 5C
        OP_X64_MODRM,                      // 5D
        OP_X64_MODRM,                      // 5E
        OP_X64_MODRM,                      // 5F
        OP_X64_MODRM,                      // 60
        OP_X64_MODRM,                      // 61
        OP_X64_MODRM,                      // 62
        OP_X64_MODRM,                      // 63
        OP_X64_MODRM,                      // 64
        OP_X64_MODRM,                      // 65
        OP_X64_MODRM,                      // 66
        OP_X64_MODRM,                      // 67
        OP_X64_MODRM,                      // 68
        OP_X64_MODRM,                      // 69
        OP_X64_MODRM,                      // 6A
        OP_X64_MODRM,                      // 6B
        OP_X64_MODRM,                      // 6C
        OP_X64_MODRM,                      // 6D
        OP_X64_MODRM,                      // 6E
        OP_X64_MODRM,                      // 6F
        OP_X64_MODRM | OP_X64_DATA_I8,         // 70
        OP_X64_MODRM | OP_X64_DATA_I8,         // 71
        OP_X64_MODRM | OP_X64_DATA_I8,         // 72
        OP_X64_MODRM | OP_X64_DATA_I8,         // 73
        OP_X64_MODRM,                      // 74
        OP_X64_MODRM,                      // 75
        OP_X64_MODRM,                      // 76
        OP_X64_NONE,                       // 77
        OP_X64_NONE,                       // 78
        OP_X64_NONE,                       // 79
        OP_X64_NONE,                       // 7A
        OP_X64_NONE,                       // 7B
        OP_X64_MODRM,                      // 7C
        OP_X64_MODRM,                      // 7D
        OP_X64_MODRM,                      // 7E
        OP_X64_MODRM,                      // 7F
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 80
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 81
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 82
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 83
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 84
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 85
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 86
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 87
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 88
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 89
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8A
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8B
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8C
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8D
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8E
        OP_X64_DATA_PRE66_67 | OP_X64_REL32,   // 8F
        OP_X64_MODRM,                      // 90
        OP_X64_MODRM,                      // 91
        OP_X64_MODRM,                      // 92
        OP_X64_MODRM,                      // 93
        OP_X64_MODRM,                      // 94
        OP_X64_MODRM,                      // 95
        OP_X64_MODRM,                      // 96
        OP_X64_MODRM,                      // 97
        OP_X64_MODRM,                      // 98
        OP_X64_MODRM,                      // 99
        OP_X64_MODRM,                      // 9A
        OP_X64_MODRM,                      // 9B
        OP_X64_MODRM,                      // 9C
        OP_X64_MODRM,                      // 9D
        OP_X64_MODRM,                      // 9E
        OP_X64_MODRM,                      // 9F
        OP_X64_NONE,                       // A0
        OP_X64_NONE,                       // A1
        OP_X64_NONE,                       // A2
        OP_X64_MODRM,                      // A3
        OP_X64_MODRM | OP_X64_DATA_I8,         // A4
        OP_X64_MODRM,                      // A5
        OP_X64_NONE,                       // A6
        OP_X64_NONE,                       // A7
        OP_X64_NONE,                       // A8
        OP_X64_NONE,                       // A9
        OP_X64_NONE,                       // AA
        OP_X64_MODRM,                      // AB
        OP_X64_MODRM | OP_X64_DATA_I8,         // AC
        OP_X64_MODRM,                      // AD
        OP_X64_MODRM,                      // AE
        OP_X64_MODRM,                      // AF
        OP_X64_MODRM,                      // B0
        OP_X64_MODRM,                      // B1
        OP_X64_MODRM,                      // B2
        OP_X64_MODRM,                      // B3
        OP_X64_MODRM,                      // B4
        OP_X64_MODRM,                      // B5
        OP_X64_MODRM,                      // B6
        OP_X64_MODRM,                      // B7
        OP_X64_NONE,                       // B8
        OP_X64_NONE,                       // B9
        OP_X64_MODRM | OP_X64_DATA_I8,         // BA
        OP_X64_MODRM,                      // BB
        OP_X64_MODRM,                      // BC
        OP_X64_MODRM,                      // BD
        OP_X64_MODRM,                      // BE
        OP_X64_MODRM,                      // BF
        OP_X64_MODRM,                      // C0
        OP_X64_MODRM,                      // C1
        OP_X64_MODRM | OP_X64_DATA_I8,         // C2
        OP_X64_MODRM,                      // C3
        OP_X64_MODRM | OP_X64_DATA_I8,         // C4
        OP_X64_MODRM | OP_X64_DATA_I8,         // C5
        OP_X64_MODRM | OP_X64_DATA_I8,         // C6
        OP_X64_MODRM,                      // C7
        OP_X64_NONE,                       // C8
        OP_X64_NONE,                       // C9
        OP_X64_NONE,                       // CA
        OP_X64_NONE,                       // CB
        OP_X64_NONE,                       // CC
        OP_X64_NONE,                       // CD
        OP_X64_NONE,                       // CE
        OP_X64_NONE,                       // CF
        OP_X64_MODRM,                      // D0
        OP_X64_MODRM,                      // D1
        OP_X64_MODRM,                      // D2
        OP_X64_MODRM,                      // D3
        OP_X64_MODRM,                      // D4
        OP_X64_MODRM,                      // D5
        OP_X64_MODRM,                      // D6
        OP_X64_MODRM,                      // D7
        OP_X64_MODRM,                      // D8
        OP_X64_MODRM,                      // D9
        OP_X64_MODRM,                      // DA
        OP_X64_MODRM,                      // DB
        OP_X64_MODRM,                      // DC
        OP_X64_MODRM,                      // DD
        OP_X64_MODRM,                      // DE
        OP_X64_MODRM,                      // DF
        OP_X64_MODRM,                      // E0
        OP_X64_MODRM,                      // E1
        OP_X64_MODRM,                      // E2
        OP_X64_MODRM,                      // E3
        OP_X64_MODRM,                      // E4
        OP_X64_MODRM,                      // E5
        OP_X64_MODRM,                      // E6
        OP_X64_MODRM,                      // E7
        OP_X64_MODRM,                      // E8
        OP_X64_MODRM,                      // E9
        OP_X64_MODRM,                      // EA
        OP_X64_MODRM,                      // EB
        OP_X64_MODRM,                      // EC
        OP_X64_MODRM,                      // ED
        OP_X64_MODRM,                      // EE
        OP_X64_MODRM,                      // EF
        OP_X64_MODRM,                      // F0
        OP_X64_MODRM,                      // F1
        OP_X64_MODRM,                      // F2
        OP_X64_MODRM,                      // F3
        OP_X64_MODRM,                      // F4
        OP_X64_MODRM,                      // F5
        OP_X64_MODRM,                      // F6
        OP_X64_MODRM,                      // F7
        OP_X64_MODRM,                      // F8
        OP_X64_MODRM,                      // F9
        OP_X64_MODRM,                      // FA
        OP_X64_MODRM,                      // FB
        OP_X64_MODRM,                      // FC
        OP_X64_MODRM,                      // FD
        OP_X64_MODRM,                      // FE
        OP_X64_NONE                        // FF
    };

    return OpcodeFlagsExt;
}

ULONG_PTR FASTCALL LdeGetOpCodeSize64(PVOID Code, PVOID *OpCodePtr)
{
    // OpCode High 5 bits as index, (1 << low 3bits) as flag

    static UCHAR PrefixTable[0x20] =
    {
        0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00
    };

    PUCHAR      Ptr;
    BOOL        PFX66, PFX67;
    BOOL        SibPresent;
    BOOL        Rex;
    ULONG_PTR   Flags;
    ULONG_PTR   Mod, RM, Reg;
    ULONG_PTR   OffsetSize, Imm64;
    ULONG_PTR   Opcode;

    Imm64       = 0;
    OffsetSize  = 0;
    PFX66       = FALSE;
    PFX67       = FALSE;
    Rex         = FALSE;
    Ptr         = (PUCHAR)Code;

//    while ( (*cPtr == 0x2E) || (*cPtr == 0x3E) || (*cPtr == 0x36) ||
//            (*cPtr == 0x26) || (*cPtr == 0x64) || (*cPtr == 0x65) ||
//            (*cPtr == 0xF0) || (*cPtr == 0xF2) || (*cPtr == 0xF3) ||
//            (*cPtr == 0x66) || (*cPtr == 0x67) )
    while (PrefixTable[*Ptr >> 3] & (1 << (*Ptr & 7)))
    {
        PFX66 = *Ptr == 0x66;
        PFX67 = *Ptr == 0x67;
        Ptr++;
        if (Ptr > (PUCHAR)Code + 16)
            return 0;
    }

    // 0x40 ~ 0x4F
    if (((*Ptr) >> 4) == 0x4)
    {
        Rex = *Ptr & 0x0F;
        if (Rex)
            ++Ptr;
    }

    Opcode = *Ptr;
    if (OpCodePtr)
        *OpCodePtr = Ptr;

    if (*Ptr == 0x0F)
    {
        Ptr++;
        Flags = GetOpCodeFlagsExt()[*Ptr];
    }
    else
    {
        ULONG_PTR tmp;

        Flags = GetOpCodeFlags()[Opcode];

        // if (Opcode >= 0xA0 && Opcode <= 0xA3)
        tmp = Opcode >> 2;
        if (tmp == 0x28)
        {
            PFX66 = PFX67;
        }
        else if (Rex && (tmp >> 1) == 0x17)     // 0xB8 ~ 0xBF  mov r64, imm64
        {
            Imm64 = 4;
        }
    }

    Ptr++;
    Ptr += FLAG_ON(Flags, OP_X64_WORD);

    if (Flags & OP_X64_MODRM)
    {
        Mod = *Ptr >> 6;
        Reg = (*Ptr & 0x38) >> 3;
        RM  = *Ptr &  7;
        Ptr++;

        if ((Opcode == 0xF6) && !Reg)
            SET_FLAG(Flags, OP_X64_DATA_I8);

        if ((Opcode == 0xF7) && !Reg)
            SET_FLAG(Flags, OP_X64_DATA_PRE66_67);

        SibPresent = !PFX67 & (RM == 4);
        switch (Mod)
        {
            case 0:
                if (PFX67 && (RM == 6))
                    OffsetSize = 2;
                if (!PFX67 && (RM == 5))
                    OffsetSize = 4;
             break;

            case 1:
                OffsetSize = 1;
                break;

            case 2:
                OffsetSize = PFX67 ? 2 : 4;
                break;

            case 3:
                SibPresent = FALSE;
                break;
        }

        if (SibPresent)
        {
            if (((*Ptr & 7) == 5) && ((!Mod) || (Mod == 2)))
                OffsetSize = 4;

            Ptr++;
        }

        Ptr += OffsetSize;
    }

    Ptr += FLAG_ON(Flags, OP_X64_DATA_I8);
    Ptr += FLAG_ON(Flags, OP_X64_DATA_I16) ? 2 : 0;
    Ptr += FLAG_ON(Flags, OP_X64_DATA_I32) ? 4 : 0;
    Ptr += FLAG_ON(Flags, OP_X64_DATA_PRE66_67) ? (PFX66 ? 2 : 4) : 0;
    Ptr += (Rex & 9) ? Imm64 : 0;   // 0x48 || 0x49

    return PtrOffset(Ptr, Code);
}

ULONG_PTR FASTCALL SizeOfProc(PVOID Proc)
{
    ULONG_PTR   Length;
    PBYTE       pOpcode;
    ULONG_PTR   Result = 0;

    do
    {
        Length = LdeGetOpCodeSize64(Proc, (PVOID *)&pOpcode);
        Result += Length;
        if ((Length == 1) && (*pOpcode == 0xC3))
            break;

        Proc = (PVOID)((ULONG_PTR)Proc + Length);
    } while (Length);

    return Result;
}

BOOL FASTCALL IsRelativeCmd(PBYTE OpCode)
{
    UCHAR Flags;

    if (*OpCode == 0x0F)
    {
        Flags = GetOpCodeFlagsExt()[OpCode[1]];
    }
    else
    {
        Flags = GetOpCodeFlags()[*OpCode];
    }

    return FLAG_ON(Flags, OP_X64_REL32);
}

#define OP_X86_NONE           0x000
#define OP_X86_DATA_I8        0x001
#define OP_X86_DATA_I16       0x002
#define OP_X86_DATA_I32       0x004
#define OP_X86_MODRM          0x008
#define OP_X86_DATA_PRE66_67  0x010
#define OP_X86_PREFIX         0x020
#define OP_X86_REL32          0x040
#define OP_X86_REL8           0x080

/* extended opcode flags (by analyzer) */
#define OP_X86_EXTENDED       0x100

ForceInline PBYTE GetPackedTable()
{
    static BYTE PackedTable[] =
    {
        0x80, 0x84, 0x80, 0x84, 0x80, 0x84, 0x80, 0x84,
        0x80, 0x88, 0x80, 0x88, 0x80, 0x88, 0x80, 0x88,
        0x8c, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
        0x90, 0x94, 0x98, 0x8b, 0x9c, 0x9c, 0x9c, 0x9c,
        0xa0, 0x80, 0x80, 0x80, 0x8b, 0x8b, 0xa4, 0x8b,
        0xa8, 0x8b, 0x84, 0x8b, 0xac, 0xac, 0xa8, 0xa8,
        0xb0, 0xb4, 0xb8, 0xbc, 0x80, 0xc0, 0x80, 0x80,
        0x9c, 0xac, 0xc4, 0x8b, 0xc8, 0x90, 0x8b, 0x90,
        0x80, 0x8b, 0x8b, 0xcc, 0x80, 0x80, 0xd0, 0x8b,
        0x80, 0xd4, 0x80, 0x80, 0x8b, 0x8b, 0x8b, 0x8b,
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x80, 0x80, 0x80, 0x80, 0xd8, 0xdc, 0x8b, 0x80,
        0xe0, 0xe0, 0xe0, 0xe0, 0x80, 0x80, 0x80, 0x80,
        0x8f, 0xcf, 0x8f, 0xdb, 0x80, 0x80, 0xe4, 0x80,
        0xe8, 0xd9, 0x8b, 0x8b, 0x80, 0x80, 0x80, 0x80,
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xdc,
        0x08, 0x08, 0x08, 0x08, 0x01, 0x10, 0x00, 0x00,
        0x01, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x08, 0x08, 0x20, 0x20, 0x20, 0x20,
        0x10, 0x18, 0x01, 0x09, 0x81, 0x81, 0x81, 0x81,
        0x09, 0x18, 0x09, 0x09, 0x00, 0x00, 0x12, 0x00,
        0x10, 0x10, 0x10, 0x10, 0x01, 0x01, 0x01, 0x01,
        0x09, 0x09, 0x02, 0x00, 0x08, 0x08, 0x09, 0x18,
        0x03, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x01, 0x01, 0x00, 0x00, 0x50, 0x50, 0x12, 0x81,
        0x20, 0x00, 0x20, 0x20, 0x00, 0x08, 0x00, 0x09,
        0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
        0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08, 0x00,
        0x50, 0x50, 0x50, 0x50, 0x00, 0x00, 0x09, 0x08,
        0x08, 0x08, 0x09, 0x08
    };

    return PackedTable;
}

ForceInline ULONG_PTR GetOpCodeFlags32(ULONG_PTR OpCode)
{
    return GetPackedTable()[GetPackedTable()[OpCode / 4] + (OpCode % 4)];
}

ULONG_PTR GetOpCodeExtendedFlags32(unsigned char *addr)
{
    ULONG_PTR opcode;

    opcode = addr[0];
    if (opcode == 0x0F)
    {
        opcode = addr[1] | OP_X86_EXTENDED;
    }

    return GetOpCodeFlags32(opcode) | (opcode & OP_X86_EXTENDED);
}

ULONG_PTR FASTCALL LdeGetOpCodeSize32(PVOID Code, PVOID *OpCodePtr)
{
    unsigned char i_mod, i_rm, i_reg;
    ULONG_PTR op1, op2, flags;
    ULONG_PTR pfx66, pfx67;
    ULONG_PTR osize, oflen;
    PBYTE code;

    pfx66 = 0;
    pfx67 = 0;
    osize = 0;
    oflen = 0;

    code = (PBYTE)Code;
    op1 = *code;

    /* skip preffixes */
    while (GetOpCodeFlags32(op1) & OP_X86_PREFIX)
    {
        switch (op1)
        {
            case 0x66:
                pfx66 = 1;
                break;

            case 0x67:
                pfx67 = 1;
                break;
        }

        osize++;
        op1 = *++code;
    }

    /* get opcode size and flags */
    if (OpCodePtr != NULL)
        *OpCodePtr = code;

    ++code;
    osize++;

    if (op1 == 0x0F)
    {
        op2 = (*code | OP_X86_EXTENDED);
        code++;
        osize++;
    }
    else
    {
        op2 = op1;

        /* pfx66 = pfx67 for opcodes A0 - A3 */
        if (op2 >= 0xA0 && op2 <= 0xA3)
        {
                pfx66 = pfx67;
        }
    }

    flags = GetOpCodeFlags32(op2);

    /* process MODRM byte */
    if (flags & OP_X86_MODRM)
    {
        i_mod = (*code >> 6);
        i_reg = (*code & 0x38) >> 3;
        i_rm  = (*code & 7);
        code++;
        osize++;

        /* in F6 and F7 opcodes, immediate value present if i_reg == 0 */
        if (op1 == 0xF6 && i_reg == 0)
        {
            flags |= OP_X86_DATA_I8;
        }
        if (op1 == 0xF7 && i_reg == 0)
        {
            flags |= OP_X86_DATA_PRE66_67;
        }

        switch (i_mod)
        {
            case 0:
                if (pfx67)
                {
                    if (i_rm == 6)
                        oflen = 2;
                }
                else
                {
                    if (i_rm == 5)
                        oflen = 4;
                }
                break;

            case 1:
                oflen = 1;
                break;

            case 2:
                if (pfx67)
                    oflen = 2;
                else
                    oflen = 4;
                break;
        }

        /* process SIB byte */
        if (pfx67 == 0 && i_rm == 4 && i_mod != 3)
        {
            if ( (*code & 7) == 5 && (i_mod != 1) )
            {
                oflen = 4;
            }

            oflen++;
        }

        osize += oflen;
    }

    /* process offset */
    if (flags & OP_X86_DATA_PRE66_67)
    {
        osize += 4 - (pfx66 << 1);
    }
    /* process immediate value */
    osize += (flags & 7);

    return osize;
}

#pragma push_macro("POINTER_SIZE")

#undef POINTER_SIZE
#define POINTER_SIZE sizeof(PVOID)

NTSTATUS
CopyOneOpCode(
    PVOID       Target,
    PVOID       Source,
    PULONG_PTR  TargetOpLength,
    PULONG_PTR  SourceOpLength,
    ULONG_PTR   ForwardSize,
    ULONG_PTR   BackwardSize,
    PVOID       TargetIp
)
{
    ULONG_PTR   Length, OpCode, OpCodeLength, OpOffsetLength, Selector;
    LONG_PTR    OpOffset, *pOpOffset;
    PBYTE       Func, p;
    ULONG_PTR   NextOpAddress;

    enum { OpCodeOffsetShort = 1, OpCodeOffsetLong = POINTER_SIZE };

    TargetIp = TargetIp == IMAGE_INVALID_VA ? Target : TargetIp;

    Func = (PBYTE)Source;
    p    = (PBYTE)Target;

    Length = GetOpCodeSize(Func);

    OpCode          = *Func;
    pOpOffset       = (PLONG_PTR)(Func + 1);
    OpOffsetLength  = OpCodeOffsetShort;
    OpCodeLength    = 2;
    Selector        = -1u;

    if (((OpCode & 0xF0) ^ 0x70) == 0)
//    if (OpCode >= 0x70 && OpCode <= 0x7F)
    {
        OpCode = 0x800F | ((OpCode - 0x70) << 8);
    }
    else if (OpCode == 0xEB)
    {
        OpCode = 0xE9;
        OpCodeLength = 1;
    }
    else if (OpCode == 0xE8 || OpCode == 0xE9)
    {
        OpOffsetLength  = OpCodeOffsetLong;
        OpCodeLength    = 1;
    }
    else
    {
        ULONG _Op;

        OpCode = *(PUSHORT)Func;
        _Op = SWAP2(OpCode);

        if ((_Op & ~0xF) == 0x0F80)
        // if (_Op >= 0x0F80 && _Op <= 0x0F8F)
        {
            OpOffsetLength = OpCodeOffsetLong;
            pOpOffset = (PLONG_PTR)(Func + 2);
        }
        else
        {
DEFAULT_OP_CODE:
            CopyMemory(p, Func, Length);
            p += Length;

            goto EXIT_PROC;
        }
    }

    OpOffset = 0;
    switch (OpOffsetLength)
    {
        case OpCodeOffsetShort:
            OpOffset = *(PCHAR)pOpOffset;
            break;

        case OpCodeOffsetLong:
        default:
            OpOffset = *(PLONG_PTR)pOpOffset;
            break;
    }

//    ULONG_PTR NewOffset = (ULONG_PTR)Func + Length + OpOffset;
//    if (NewOffset >= (ULONG_PTR)Destination && NewOffset < (ULONG_PTR)Destination + BytesToNop + HookOpSize)
//        goto DEFAULT_OP_CODE;

    LONG_PTR NewOffset = Length + OpOffset;

    if (NewOffset < 0)
    {
        if ((ULONG_PTR)-NewOffset < BackwardSize)
            goto DEFAULT_OP_CODE;
    }
    else if ((ULONG_PTR)NewOffset < ForwardSize)
    {
        goto DEFAULT_OP_CODE;
    }

    NextOpAddress = (ULONG_PTR)Func + Length;
    OpOffset = (NextOpAddress + OpOffset) - ((ULONG_PTR)TargetIp + OpCodeLength + OpCodeOffsetLong + (Selector == -1 ? 0 : 2));
    switch (OpCodeLength)
    {
        case 1:
            *p++ = (BYTE)OpCode;
            break;

        case 2:
        default:
            *(PUSHORT)p = (USHORT)OpCode;
            p += 2;
            break;
    }
/*
    if (Selector != -1)
    {
        *(PUSHORT)p = Selector;
        p += 2;
    }
*/
    *(PLONG_PTR)p = OpOffset;
    p += POINTER_SIZE;

//    Length = PtrOffset(p, Destination);

EXIT_PROC:
    if (SourceOpLength != NULL)
        *SourceOpLength = Length;

    if (TargetOpLength != NULL)
        *TargetOpLength = PtrOffset(p, Target);

    return STATUS_SUCCESS;
}

ML_NAMESPACE_BEGIN(Mp);

#if ML_X86

#define SIZE_OF_JUMP_CODE   5

#elif ML_AMD64


#endif // arch

VOID PatchNop(PVOID Address, ULONG_PTR BytesToPatch)
{
    PBYTE Buffer = (PBYTE)Address;

    switch (BytesToPatch)
    {
        case 0:
            return;

        case 1:
            //
            // nop
            //
            Buffer[0] = 0x90;
            break;

        case 2:
            //
            // mov eax, eax
            //
            *(PUSHORT)Buffer = 0xC08B;
            break;

        case 3:
            //
            // lea eax, [eax+0]
            //
            *(PUSHORT)Buffer = 0x408D;
            Buffer[2] = 0x00;
            break;

        case 4:
            //
            // lea esi, [esi]
            //
            *(PULONG)Buffer = 0x0026748D;
            break;

        case 5:
            // 2 + 3
            *Buffer = 0x8B;
            *(PULONG)(Buffer + 1) = 0x408DC0;
            break;

        case 6:
            // lea eax, [eax+0]
            *(PULONG)Buffer = 0x808D;
            *(PUSHORT)(Buffer + 4) = 0;
            break;

        case 7:
            // lea esi, [esi]
            *(PULONG)Buffer = 0x0026B48D;
            *(PULONG)&Buffer[3] = 0;
            break;
    }
}

class MemoryPatchManager
{
protected:
    GrowableArray<PTRAMPOLINE_DATA> TrampolineList;

    NoInline NTSTATUS HandleMemoryPatch(PPATCH_MEMORY_DATA PatchData, PVOID BaseAddress)
    {
        auto&       Patch = PatchData->Memory;
        PVOID       Address;
        ULONG       Protect;
        NTSTATUS    Status;

        if (Patch.Size == 0)
            return STATUS_SUCCESS;

        if (Patch.Address == IMAGE_INVALID_RVA)
            return STATUS_SUCCESS;

        Address = PtrAdd(Patch.Options.VirtualAddress ? nullptr : BaseAddress, Patch.Address);
        Status = ProtectMemory(Address, Patch.Size, PAGE_EXECUTE_READWRITE, &Protect);
        FAIL_RETURN(Status);

        if (Patch.Options.DataIsBuffer)
        {
            CopyMemory(Address, (PVOID)Patch.Data, Patch.Size);
        }
        else
        {
            if (Patch.Options.BackupData)
            {
                PTRAMPOLINE_DATA TrampolineData;

                TrampolineData = AllocateTrampolineData();
                if (TrampolineData == nullptr)
                {
                    Status = ProtectMemory(Address, Patch.Size, Protect, nullptr);
                    return NT_FAILED(Status) ? Status : STATUS_NO_MEMORY;
                }

                TrampolineData->PatchData = *PatchData;
                TrampolineData->PatchData.Memory.Backup = 0;
                TrampolineData->PatchData.Memory.Options.VirtualAddress = TRUE;
                TrampolineData->PatchData.Memory.Address = (ULONG_PTR)Address;
                CopyMemory(&TrampolineData->PatchData.Memory.Backup, Address, Patch.Size);

                *PatchData = TrampolineData->PatchData;

                this->TrampolineList.Add(TrampolineData);
            }

            CopyMemory(&Patch.Backup, Address, Patch.Size);
            CopyMemory(Address, &Patch.Data, Patch.Size);
        }

        FlushInstructionCache(Address, Patch.Size);
        Status = ProtectMemory(Address, Patch.Size, Protect, nullptr);

        return Status;
    }

    NoInline NTSTATUS HandleFunctionPatch(PPATCH_MEMORY_DATA PatchData, PVOID BaseAddress)
    {
        auto&               Function = PatchData->Function;
        BYTE                LocalHookBuffer[TRAMPOLINE_SIZE];
        PVOID               Address;
        PBYTE               Trampoline;
        ULONG               Protect;
        ULONG_PTR           HookOpSize, CopyOpSize;
        NTSTATUS            Status;
        PTRAMPOLINE_DATA    TrampolineData;

        if (Function.Target == nullptr)
            return STATUS_SUCCESS;

        if (Function.Source == IMAGE_INVALID_RVA)
            return STATUS_SUCCESS;

        Address = PtrAdd(Function.Options.VirtualAddress ? nullptr : BaseAddress, Function.Source);

        HookOpSize = GetSizeOfHookOpSize(Function.HookOp);
        if (HookOpSize == ULONG_PTR_MAX)
            return STATUS_BUFFER_TOO_SMALL;

        Status = GetHookAddressAndSize(Address, HookOpSize, &Address, &CopyOpSize);
        FAIL_RETURN(Status);

        if (CopyOpSize > TRAMPOLINE_SIZE)
            RaiseDebugBreak();

        if (Function.Options.NakedTrampoline == FALSE)
        {
            Status = GenerateHookCode(LocalHookBuffer, Address, Function.Target, Function.HookOp, HookOpSize);
            FAIL_RETURN(Status);

            PatchNop(&LocalHookBuffer[HookOpSize], CopyOpSize - HookOpSize);
        }

        TrampolineData = nullptr;
        if (Function.Options.NakedTrampoline != FALSE || (Function.Trampoline != nullptr && Function.Options.DoNotDisassemble == FALSE))
        {
            TrampolineData = AllocateTrampolineData();
            if (TrampolineData == nullptr)
                return STATUS_NO_MEMORY;

            TrampolineData->PatchData = *PatchData;
            TrampolineData->PatchData.Function.NopBytes = CopyOpSize - HookOpSize;
            TrampolineData->PatchData.Function.Source = (ULONG_PTR)Address;
            TrampolineData->PatchData.Function.Options.VirtualAddress = TRUE;

            TrampolineData->OriginSize = CopyOpSize;
            CopyMemory(TrampolineData->OriginalCode, Address, CopyOpSize);

            TrampolineData->JumpBackAddress = PtrAdd(Address, CopyOpSize);
            Trampoline = TrampolineData->Trampoline;

            if (Function.Options.NakedTrampoline == FALSE)
            {
                CopyTrampolineStub(Trampoline, Address, CopyOpSize);
                if (CopyOpSize == HookOpSize && Function.HookOp == OpCall && Function.Options.KeepRawTrampoline == FALSE)
                {
                    TrampolineData->Trampoline[0] = 0xE9;
                }
                else
                {
                    GenerateJumpBack(Trampoline, &TrampolineData->JumpBackAddress);
                }
            }
            else
            {
                GenerateNakedTrampoline(Trampoline, Address, CopyOpSize, TrampolineData);

                Status = GenerateHookCode(LocalHookBuffer, Address, TrampolineData->Trampoline, Function.HookOp, HookOpSize);
                if (NT_FAILED(Status))
                {
                    FreeTrampolineData(TrampolineData);
                    return Status;
                }

                PatchNop(&LocalHookBuffer[HookOpSize], CopyOpSize - HookOpSize);
            }

            if (Trampoline - TrampolineData->Trampoline > TRAMPOLINE_SIZE)
                RaiseDebugBreak();

            Status = ProtectMemory(TrampolineData, sizeof(*TrampolineData), PAGE_EXECUTE_READ, &Protect);
            if (NT_FAILED(Status))
            {
                FreeTrampolineData(TrampolineData);
                return Status;
            }

            FlushInstructionCache(TrampolineData->Trampoline, TRAMPOLINE_SIZE);

            if (Function.Trampoline != nullptr)
                *Function.Trampoline = TrampolineData->Trampoline;

            this->TrampolineList.Add(TrampolineData);
        }

        Status = ProtectMemory(Address, CopyOpSize, PAGE_EXECUTE_READWRITE, &Protect);
        if (NT_FAILED(Status))
        {
            FreeTrampolineData(TrampolineData);
            return Status;
        }

        CopyMemory(Address, LocalHookBuffer, CopyOpSize);
        FlushInstructionCache(Address, CopyOpSize);

        Status = ProtectMemory(Address, CopyOpSize, Protect, &Protect);

        return Status;
    }

    NoInline NTSTATUS GenerateHookCode(PBYTE Buffer, PVOID SourceIp, PVOID Target, ULONG_PTR HookOp, ULONG_PTR HookOpSize)
    {
        ULONG_PTR RegisterIndex;

#if ML_X86

        switch (HookOp)
        {
            case OpCall:
            case OpJump:
                //
                // jmp imm
                //
                *Buffer++ = HookOp == OpCall ? 0xE8 : 0xE9;
                *(PULONG)Buffer = PtrOffset(Target, PtrAdd(SourceIp, HookOpSize));
                break;

            case OpPush:
                //
                // push imm
                // ret
                //
                *Buffer++ = 0x68;
                *(PULONG)Buffer = (ULONG)Target;
                Buffer += POINTER_SIZE;
                *Buffer = 0xC3;
                break;

            default:
                if (HookOp < OpJRax || HookOp > OpJRdi)
                    return STATUS_INVALID_PARAMETER;

                //
                // mov r32, imm
                // jmp r32
                //
                RegisterIndex = HookOp - OpJRax;
                *Buffer++ = 0xB8 + (BYTE)RegisterIndex;
                *(PVOID *)Buffer = Target;
                Buffer += POINTER_SIZE;
                *(PUSHORT)Buffer = (USHORT)(0xE0FF + RegisterIndex);
                break;
        }

#elif ML_AMD64

        switch (HookOp)
        {
            case OpPush:
                //
                // push imm.low
                // mov dword ptr [rsp + 4], imm.high
                // ret
                //
                *Buffer++ = 0x68;
                *(PULONG)Buffer = (ULONG)((ULONG_PTR)Target >> 32);
                Buffer += POINTER_SIZE;

                *Buffer++ = 0xC7;
                *Buffer++ = 0x04;
                *Buffer++ = 0x24;
                *(PULONG)Buffer = (ULONG)(ULONG_PTR)Target;

                *Buffer = 0xC3;
                break;

            default:
                if (HookOp < OpJRax || HookOp > OpJRdi)
                    return STATUS_INVALID_PARAMETER;

                //
                // mov r32, imm
                // jmp r32
                //
                RegisterIndex = HookOp - OpJRax;
                *Buffer++ = 0x48;
                *Buffer++ = 0xB8 + (BYTE)RegisterIndex;
                *(PVOID *)Buffer = Target;
                Buffer += POINTER_SIZE;
                *(PUSHORT)Buffer = (USHORT)(0xE0FF + RegisterIndex);
                break;
        }

#else

        return STATUS_NOT_IMPLEMENTED;

#endif

        return STATUS_SUCCESS;
    }

    NoInline NTSTATUS GenerateNakedTrampoline(PBYTE& Trampoline, PVOID Address, ULONG_PTR CopyOpSize, PTRAMPOLINE_DATA TrampolineData)
    {
        NTSTATUS    Status;
        PVOID*      AddressOfReturnAddress;
        auto&       Function = TrampolineData->PatchData.Function;

        if (Function.Options.ExecuteTrampoline != FALSE)
        {
            Status = CopyTrampolineStub(Trampoline, Address, CopyOpSize);
            FAIL_RETURN(Status);
        }

#if ML_X86

        //
        // push     eax
        // push     esp
        // push     eax
        // push     ecx
        // push     edx
        // push     ebx
        // push     ebp
        // push     esi
        // push     edi
        // pushfd
        //
        // mov      eax, ReturnAddress
        // mov      [esp + 24h], eax
        // mov      eax, CallBack
        // lea      ecx, [esp];      ecx = PTRAMPOLINE_NAKED_CONTEXT
        // call     eax
        //
        // popfd
        // pop      edi
        // pop      esi
        // pop      ebp
        // pop      ebx
        // pop      edx
        // pop      ecx
        // pop      eax
        // pop      esp
        //
        // ret
        //

        if (Function.HookOp != OpCall)
        {
            // push eax
            *Trampoline++ = 0x50;
        }

        *Trampoline++ = 0x54;       // push    esp
        *Trampoline++ = 0x50;       // push    eax
        *Trampoline++ = 0x51;       // push    ecx
        *Trampoline++ = 0x52;       // push    edx
        *Trampoline++ = 0x53;       // push    ebx
        *Trampoline++ = 0x55;       // push    ebp
        *Trampoline++ = 0x56;       // push    esi
        *Trampoline++ = 0x57;       // push    edi
        *Trampoline++ = 0x9C;       // pushfd

        // mov eax, imm
        *Trampoline++ = 0xB8;
        *(PVOID *)Trampoline = PtrAdd(Address, CopyOpSize);
        AddressOfReturnAddress = (PVOID *)Trampoline;
        Trampoline += POINTER_SIZE;

        // mov [esp + 24h], eax
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x44;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x24;

        // mov eax, CallBack
        *Trampoline++ = 0xB8;
        *(PVOID *)Trampoline = Function.Target;
        Trampoline += POINTER_SIZE;

        // lea ecx, [esp]
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x0C;
        *Trampoline++ = 0x24;

        // call eax
        *Trampoline++ = 0xFF;
        *Trampoline++ = 0xD0;

        *Trampoline++ = 0x9D;       // popfd
        *Trampoline++ = 0x5F;       // pop     edi
        *Trampoline++ = 0x5E;       // pop     esi
        *Trampoline++ = 0x5D;       // pop     ebp
        *Trampoline++ = 0x5B;       // pop     ebx
        *Trampoline++ = 0x5A;       // pop     edx
        *Trampoline++ = 0x59;       // pop     ecx
        *Trampoline++ = 0x58;       // pop     eax
        *Trampoline++ = 0x5C;       // pop     esp

        // ret
        *Trampoline++ = 0xC3;

#else

        //
        // SIZE_OF_CONTEXT equ 78h
        // push    rax
        // pushfq
        // lea     rsp, [rsp - SIZE_OF_CONTEXT]
        // mov     [rsp + 00h], rax
        // mov     [rsp + 08h], rcx
        // mov     [rsp + 10h], rdx
        // mov     [rsp + 18h], rbx
        // mov     [rsp + 20h], rbp
        // mov     [rsp + 28h], rsi
        // mov     [rsp + 30h], rdi
        // mov     [rsp + 38h], r8
        // mov     [rsp + 40h], r9
        // mov     [rsp + 48h], r10
        // mov     [rsp + 50h], r11
        // mov     [rsp + 58h], r12
        // mov     [rsp + 60h], r13
        // mov     [rsp + 68h], r14
        // mov     [rsp + 70h], r15
        // mov     rax, ReturnAddress
        // mov     [rsp + SIZE_OF_CONTEXT + 10h], rax
        // mov     rax, CallBack
        // lea     rcx, [rsp];      rcx = PTRAMPOLINE_NAKED_CONTEXT
        // lea     rsp, [rsp - 20h]
        // call    rax
        // lea     rsp, [rsp + 20h]
        // mov     rax, [rsp + 00h]
        // mov     rcx, [rsp + 08h]
        // mov     rdx, [rsp + 10h]
        // mov     rbx, [rsp + 18h]
        // mov     rbp, [rsp + 20h]
        // mov     rsi, [rsp + 28h]
        // mov     rdi, [rsp + 30h]
        // mov     r8 , [rsp + 38h]
        // mov     r9 , [rsp + 40h]
        // mov     r10, [rsp + 48h]
        // mov     r11, [rsp + 50h]
        // mov     r12, [rsp + 58h]
        // mov     r13, [rsp + 60h]
        // mov     r14, [rsp + 68h]
        // mov     r15, [rsp + 70h]
        // lea     rsp, [rsp + SIZE_OF_CONTEXT]
        // popfq
        // ret
        //

        if (Function.HookOp != OpCall)
        {
            // push rax
            *Trampoline++ = 0x50;
        }

        // pushfq
        *Trampoline++ = 0x9C;

        // lea     rsp, [rsp-78h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x88;

        // mov     [rsp + 00h], rax
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x04;
        *Trampoline++ = 0x24;

        // mov     [rsp + 08h], rcx
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x08;

        // mov     [rsp + 10h], rdx
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x54;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x10;

        // mov     [rsp + 18h], rbx
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x5C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x18;

        // mov     [rsp + 20h], rbp
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x6C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x20;

        // mov     [rsp + 28h], rsi
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x74;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x28;

        // mov     [rsp + 30h], rdi
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x7C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x30;

        // mov     [rsp + 38h], r8
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x44;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x38;

        // mov     [rsp + 40h], r9
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x40;

        // mov     [rsp + 48h], r10
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x54;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x48;

        // mov     [rsp + 50h], r11
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x5C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x50;

        // mov     [rsp + 58h], r12
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x58;

        // mov     [rsp + 60h], r13
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x6C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x60;

        // mov     [rsp + 68h], r14
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x74;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x68;

        // mov     [rsp + 70h], r15
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x7C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x70;

        // mov     rax, ReturnAddress
        *Trampoline++ = 0x48;
        *Trampoline++ = 0xB8;
        *(PVOID *)Trampoline = PtrAdd(Address, CopyOpSize);
        AddressOfReturnAddress = (PVOID *)Trampoline;
        Trampoline += POINTER_SIZE;

        // mov     [rsp + SIZE_OF_CONTEXT + 10], rax
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x89;
        *Trampoline++ = 0x84;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x80;
        *Trampoline++ = 0x00;
        *Trampoline++ = 0x00;
        *Trampoline++ = 0x00;

        // mov     rax, CallBack
        *Trampoline++ = 0x48;
        *Trampoline++ = 0xB8;
        *(PVOID *)Trampoline = Function.Target;
        Trampoline += POINTER_SIZE;

        // lea     rcx, [rsp];      rcx = PTRAMPOLINE_NAKED_CONTEXT
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x0C;
        *Trampoline++ = 0x24;

        // lea     rsp, [rsp - 20h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0xE0;

        // call    rax
        *Trampoline++ = 0xFF;
        *Trampoline++ = 0xD0;

        // lea     rsp, [rsp + 20h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x20;

        // mov     rax, [rsp + 00h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x04;
        *Trampoline++ = 0x24;

        // mov     rcx, [rsp + 08h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x08;

        // mov     rdx, [rsp + 10h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x54;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x10;

        // mov     rbx, [rsp + 18h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x5C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x18;

        // mov     rbp, [rsp + 20h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x6C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x20;

        // mov     rsi, [rsp + 28h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x74;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x28;

        // mov     rdi, [rsp + 30h]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x7C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x30;

        // mov     r8 , [rsp + 38h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x44;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x38;

        // mov     r9 , [rsp + 40h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x40;

        // mov     r10, [rsp + 48h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x54;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x48;

        // mov     r11, [rsp + 50h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x5C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x50;

        // mov     r12, [rsp + 58h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x58;

        // mov     r13, [rsp + 60h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x6C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x60;

        // mov     r14, [rsp + 68h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x74;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x68;

        // mov     r15, [rsp + 70h]
        *Trampoline++ = 0x4C;
        *Trampoline++ = 0x8B;
        *Trampoline++ = 0x7C;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x70;

        // lea     rsp, [rsp + SIZE_OF_CONTEXT]
        *Trampoline++ = 0x48;
        *Trampoline++ = 0x8D;
        *Trampoline++ = 0x64;
        *Trampoline++ = 0x24;
        *Trampoline++ = 0x78;

        // popfq
        *Trampoline++ = 0x9D;

        // ret
        *Trampoline++ = 0xC3;

//#error NOT COMPLETE

#endif // arch

        if (Function.Options.ExecuteTrampoline == FALSE)
        {
            *AddressOfReturnAddress = Trampoline;

            Status = CopyTrampolineStub(Trampoline, Address, CopyOpSize);
            FAIL_RETURN(Status);

            Status = GenerateJumpBack(Trampoline, &TrampolineData->JumpBackAddress);
            FAIL_RETURN(Status);
        }

        return STATUS_SUCCESS;
    }

    NoInline NTSTATUS GenerateJumpBack(PBYTE& Trampoline, PVOID AddressOfJumpAddress)
    {
        // jmp [imm32]
        *Trampoline++ = 0xFF;
        *Trampoline++ = 0x25;

        *(PVOID *)Trampoline = AddressOfJumpAddress;
        Trampoline += 2 + 4;

        return STATUS_SUCCESS;
    }

    NoInline NTSTATUS CopyTrampolineStub(PBYTE& Trampoline, PVOID Address, ULONG_PTR CopyOpSize)
    {
        ULONG_PTR SourceOpLength, ForwardSize, BackwardSize;
        PBYTE Source = (PBYTE)Address;

        ForwardSize  = CopyOpSize;
        BackwardSize = 0;

        for (LONG_PTR Bytes = CopyOpSize; Bytes > 0; Bytes -= SourceOpLength)
        {
            ULONG_PTR TargetOpLength;

            if (Source[0] == 0xC2 || Source[0] == 0xC3)
                return STATUS_BUFFER_TOO_SMALL;

            CopyOneOpCode(Trampoline, Source, &TargetOpLength, &SourceOpLength, ForwardSize, BackwardSize);

            BackwardSize+= SourceOpLength;
            ForwardSize -= SourceOpLength;
            Trampoline  += TargetOpLength;
            Source      += SourceOpLength;
        }

        return STATUS_SUCCESS;
    }

    NoInline NTSTATUS GetHookAddressAndSize(PVOID Address, ULONG_PTR HookOpSize, PVOID *FinalAddress, PULONG_PTR Size)
    {
        ULONG_PTR   Length, OpSize;
        PBYTE       Buffer;

        OpSize = 0;
        Buffer = (PBYTE)Address;
        *FinalAddress = Address;

#if ML_X86 || ML_AMD64

        while (OpSize < HookOpSize)
        {
            if (Buffer[0] == 0xC2 || Buffer[0] == 0xC3)
                return STATUS_BUFFER_TOO_SMALL;

            Length  = GetOpCodeSize(Buffer);

            // jmp short const
            if (Buffer[0] == 0xEB)
            {
                if (OpSize == 0)
                {
                    if (Buffer[1] != 0)
                    {
                        Buffer += *(PCHAR)&Buffer[1] + Length;
                        *FinalAddress = Buffer;
                        continue;
                    }
                }
                else if (OpSize < HookOpSize - Length)
                {
                    return STATUS_BUFFER_TOO_SMALL;
                }
            }
            else if (Buffer[0] == 0xFF && Buffer[1] == 0x25)
            {
                // jmp [rimm]

#if ML_X86
                if (OpSize != 0 && OpSize + Length < HookOpSize)
                    return STATUS_BUFFER_TOO_SMALL;

                if (OpSize == 0 && HookOpSize > Length)
                {
                    Buffer = **(PBYTE **)&Buffer[2];
                    *FinalAddress = Buffer;
                    continue;
                }
#elif ML_AMD64
                if (OpSize != 0)
                    return STATUS_BUFFER_TOO_SMALL;

                Buffer = *(PBYTE *)(Buffer + Length + *(PLONG)&Buffer[2]);
                *FinalAddress = Buffer;
                continue;
#endif // arch

            }

            Buffer += Length;
            OpSize += Length;
        }

#else

#error "not support"

#endif // arch

        *Size = OpSize;

        return STATUS_SUCCESS;
    }

    NoInline ULONG_PTR GetSizeOfHookOpSize(ULONG_PTR HookOp)
    {

#if ML_AMD64

        switch (HookOp)
        {
            case OpPush:
                // 68 00 00 00 80       push    80000000h
                // C7 04 24 00 00 00 80 mov     dword ptr [rsp], 80000000h
                // C3                   ret
                return 0xD;

            //case OpJumpIndirect:
            //    //
            //    // FF 25 00 00 00 00 jmp [rimm]
            //    // 00 00 00 00 00 00 00 00
            //    //
            //    break;
            //    //return 0xE;

            default:
                if (HookOp >= OpJRax && HookOp <= OpJRdi)
                {
                    //
                    // 48 B8 00 00 00 80 00 00 00 80    mov     rax, 8000000080000000h
                    // FF E0                            jmp     rax
                    //
                    return 0xC;
                }
        }

#elif ML_X86

        switch (HookOp)
        {
            case OpPush:
                //
                // 68 00000000  push const
                // C3           ret
                //
                return 1 + POINTER_SIZE + 1;

            case OpCall:
            case OpJump:
                //
                // E8 00000000  call    const
                // E9 00000000  jmp     const
                //
                return 1 + POINTER_SIZE;

            default:
                if (HookOp >= OpJRax && HookOp <= OpJRdi)
                {
                    //
                    // b8 88888888      mov     r32, const
                    // ffe0             jmp     r32
                    //
                    return 1 + POINTER_SIZE + 2;
                }
        }

#endif

        return ULONG_PTR_MAX;
    }

    ForceInline
    NTSTATUS
    ProtectMemory(
        PVOID       BaseAddress,
        ULONG_PTR   Size,
        ULONG       NewProtect,
        PULONG      OldProtect
    )
    {
        return Mm::ProtectMemory(CurrentProcess, BaseAddress, Size, NewProtect, OldProtect);
    }

    ForceInline
    NTSTATUS
    FlushInstructionCache(
        PVOID       BaseAddress,
        ULONG_PTR   NumberOfBytesToFlush
    )
    {
        return NtFlushInstructionCache(CurrentProcess, BaseAddress, NumberOfBytesToFlush);
    }

    PTRAMPOLINE_DATA AllocateTrampolineData()
    {
        // 独立分配，避免改写一个跳板时同时改变其他跳板所在堆页的权限。
        return (PTRAMPOLINE_DATA)::AllocateVirtualMemory(sizeof(TRAMPOLINE_DATA), PAGE_READWRITE);
    }

    VOID FreeTrampolineData(PTRAMPOLINE_DATA TrampolineData)
    {
        if (TrampolineData != nullptr)
            ::FreeVirtualMemory(TrampolineData);
    }

    NTSTATUS RemoveTrampolineData(PTRAMPOLINE_DATA TrampolineData)
    {
        NTSTATUS Status;
        ULONG_PTR Index;

        Index = this->TrampolineList.IndexOf(TrampolineData);
        if (Index == this->TrampolineList.kInvalidIndex)
            return STATUS_NOT_FOUND;

        this->TrampolineList.Remove(Index);
        FreeTrampolineData(TrampolineData);

        return STATUS_SUCCESS;
    }

    DECL_NORETURN VOID RaiseDebugBreak()
    {
        __debugbreak();
    }

public:
    NoInline NTSTATUS PatchMemory(PPATCH_MEMORY_DATA PatchData, ULONG_PTR PatchCount, PVOID BaseAddress)
    {
        NTSTATUS Status;

        FOR_EACH(PatchData, PatchData, PatchCount)
        {
            switch (PatchData->PatchType)
            {
                case PatchMemoryTypes::MemoryPatch:
                    Status = HandleMemoryPatch(PatchData, BaseAddress);
                    break;

                case PatchMemoryTypes::FunctionPatch:
                    Status = HandleFunctionPatch(PatchData, BaseAddress);
                    break;

                default:
                    Status = STATUS_NOT_IMPLEMENTED;
                    break;
            }

            FAIL_RETURN(Status);
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS RestoreMemory(PTRAMPOLINE_DATA TrampolineData)
    {
        NTSTATUS Status;
        PATCH_MEMORY_DATA PatchData = MemoryPatchVa(&TrampolineData->OriginalCode, TrampolineData->OriginSize, TrampolineData->PatchData.Function.Source);
        Status = HandleMemoryPatch(&PatchData, nullptr);
        NT_SUCCESS(Status) && RemoveTrampolineData(TrampolineData);

        return Status;
    }

public:
    static NoInline NTSTATUS CreateInstance(MemoryPatchManager **Manager)
    {
        ml::MlInitialize();

        *Manager = (MemoryPatchManager *)AllocateMemory(sizeof(**Manager));
        if (*Manager == nullptr)
            return STATUS_NO_MEMORY;

        ZeroMemory(*Manager, sizeof(**Manager));
        new (*Manager) MemoryPatchManager;

        return STATUS_SUCCESS;
    }

    static NoInline NTSTATUS DestroyInstance(MemoryPatchManager *Manager)
    {
        if (Manager != nullptr)
        {
            Manager->~MemoryPatchManager();
            FreeMemory(Manager);
        }

        return STATUS_SUCCESS;
    }
};

ForceInline MemoryPatchManager*& MemoryPatchManagerInstance()
{
    static MemoryPatchManager *Manager;
    return Manager;
}

NTSTATUS MP_CALL PatchMemory(PPATCH_MEMORY_DATA PatchData, ULONG_PTR PatchCount, PVOID BaseAddress)
{
    NTSTATUS Status;

    if (MemoryPatchManagerInstance() == nullptr)
    {
        Status = MemoryPatchManager::CreateInstance(&MemoryPatchManagerInstance());
        FAIL_RETURN(Status);
    }

    return MemoryPatchManagerInstance()->PatchMemory(PatchData, PatchCount, BaseAddress);
}

NTSTATUS MP_CALL RestoreMemory(PTRAMPOLINE_DATA TrampolineData)
{
    if (MemoryPatchManagerInstance() == nullptr)
        return STATUS_FLT_NOT_INITIALIZED;

    return MemoryPatchManagerInstance()->RestoreMemory(TrampolineData);
}

ML_NAMESPACE_END_(Mp);

#pragma pop_macro("POINTER_SIZE")

ML_NAMESPACE

ML_NAMESPACE_BEGIN(Native)


ML_NAMESPACE_BEGIN(Ldr)

/************************************************************************
  UserMode
************************************************************************/

#if ML_USER_MODE

PLDR_MODULE FindLdrModuleByName(PUNICODE_STRING ModuleName)
{
    PLDR_MODULE Ldr;
    PLIST_ENTRY LdrLink, NextLink;
    PVOID       LoaderLockCookie;
    NTSTATUS    Status;

    Status = LdrLockLoaderLock(0, NULL, &LoaderLockCookie);

    LdrLink = &Ps::CurrentPeb()->Ldr->InLoadOrderModuleList;
    NextLink = LdrLink->Flink;

    if (ModuleName == NULL)
    {
        if (NT_SUCCESS(Status))
            LdrUnlockLoaderLock(0, LoaderLockCookie);

        return FIELD_BASE(NextLink, LDR_MODULE, InLoadOrderLinks);
    }

    while (NextLink != LdrLink)
    {
        Ldr = FIELD_BASE(NextLink, LDR_MODULE, InLoadOrderLinks);

        if (RtlEqualUnicodeString(ModuleName, &Ldr->BaseDllName, TRUE))
        {
            if (NT_SUCCESS(Status))
                LdrUnlockLoaderLock(0, LoaderLockCookie);

            return Ldr;
        }

        NextLink = NextLink->Flink;
    }

    if (NT_SUCCESS(Status))
        LdrUnlockLoaderLock(0, LoaderLockCookie);

    return NULL;
}

PLDR_MODULE FindLdrModuleByHandle(PVOID BaseAddress)
{
    PLDR_MODULE Ldr;
    PLIST_ENTRY LdrLink, NextLink;

    if (BaseAddress != NULL)
    {
        NTSTATUS Status;

        Status = LdrFindEntryForAddress(BaseAddress, &Ldr);
        return NT_SUCCESS(Status) ? Ldr : NULL;
    }

    LdrLink = &Nt_CurrentPeb()->Ldr->InLoadOrderModuleList;
    NextLink = LdrLink->Flink;

    return FIELD_BASE(NextLink, LDR_MODULE, InLoadOrderLinks);
}

#endif // r3








/************************************************************************
  load pe image
************************************************************************/

#define LOAD_PE_MEMORY_TAG   TAG4('LPMT')

NTSTATUS
RelocBlock(
    PVOID                   ImageBase,
    LONG64                  ImageBaseOffset,
    PIMAGE_BASE_RELOCATION2 Relocation
)
{
    LONG        SizeOfBlock;
    PVOID       RelocateBase;
    PIMAGE_RELOCATION_ADDRESS_ENTRY TypeOffset;

    TypeOffset      = Relocation->TypeOffset;
    SizeOfBlock     = Relocation->SizeOfBlock;
    RelocateBase    = PtrAdd(ImageBase, Relocation->VirtualAddress);
    SizeOfBlock    -= sizeof(*Relocation) - sizeof(Relocation->TypeOffset);

    for (; SizeOfBlock > 0; ++TypeOffset, SizeOfBlock -= sizeof(*TypeOffset))
    {
        if (*(PUSHORT)TypeOffset == 0)
            continue;

        PLONG_PTR VA = (PLONG_PTR)PtrAdd(RelocateBase, TypeOffset->Offset);
        switch (TypeOffset->Type)
        {
            case IMAGE_REL_BASED_ABSOLUTE:
                break;

            case IMAGE_REL_BASED_HIGH:
                *(PUSHORT)VA += HIWORD((ULONG)ImageBaseOffset);
                break;

            case IMAGE_REL_BASED_LOW:
                *(PUSHORT)VA += LOWORD((ULONG)ImageBaseOffset);
                break;

            case IMAGE_REL_BASED_HIGHLOW:
                *(PLONG)VA += (ULONG)ImageBaseOffset;
                break;

            case IMAGE_REL_BASED_HIGHADJ:
                ++TypeOffset;
                SizeOfBlock -= sizeof(*TypeOffset);

                *(PUSHORT)VA += ((*(PUSHORT)VA << 16) + (ULONG)ImageBaseOffset + *(PSHORT)TypeOffset + 0x8000) >> 16;
                break;

            case IMAGE_REL_BASED_MIPS_JMPADDR:
            case IMAGE_REL_BASED_IA64_IMM64:
                break;

            case IMAGE_REL_BASED_DIR64:
                *(PLONG64)VA += (LONG64)ImageBaseOffset;
                break;

            default:
                break;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
RelocPeImage(
    PVOID   ImageBuffer,
    PVOID64 ImageDefaultBase,
    PVOID   ImageOldBase,
    PVOID   ImageNewBase
)
{
    LONG                            SizeOfBlock;
    PBYTE                           Base, RelocateBase;
    LONG64                          BaseOffset;
    ULONG                           SizeOfRelocation;
    PIMAGE_BASE_RELOCATION2         Relocation, RelocationEnd;
    PIMAGE_RELOCATION_ADDRESS_ENTRY TypeOffset;
    PIMAGE_DATA_DIRECTORY           DataDirectory;
    PIMAGE_NT_HEADERS32             NtHeaders32;
    PIMAGE_NT_HEADERS64             NtHeaders64;

    if (ImageNewBase == nullptr)
        ImageNewBase = ImageBuffer;

    NtHeaders32 = (PIMAGE_NT_HEADERS32)RtlImageNtHeader(ImageBuffer);

    switch (NtHeaders32->OptionalHeader.Magic)
    {
        case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
            DataDirectory = NtHeaders32->OptionalHeader.DataDirectory;
            break;

        case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
            NtHeaders64 = (PIMAGE_NT_HEADERS64)NtHeaders32;
            DataDirectory = NtHeaders64->OptionalHeader.DataDirectory;
            break;

        default:
            return STATUS_INVALID_IMAGE_FORMAT;
    }

    Relocation = (PIMAGE_BASE_RELOCATION2)PtrAdd(ImageBuffer, DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    SizeOfRelocation = DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

    if ((PVOID)Relocation == ImageBuffer || SizeOfRelocation == 0)
        return STATUS_SUCCESS;

    RelocationEnd = PtrAdd(Relocation, SizeOfRelocation);

    Base        = (PBYTE)ImageBuffer;
    BaseOffset  = PtrSub(ImageOldBase == nullptr ? (LONG64)ImageNewBase : (LONG64)ImageOldBase, (LONG64)ImageDefaultBase);

    while (Relocation < RelocationEnd && Relocation->SizeOfBlock != 0)
    {
        RelocBlock(Base, BaseOffset, Relocation);
        Relocation = PtrAdd(Relocation, Relocation->SizeOfBlock);
    }

    return STATUS_SUCCESS;
}

#if ML_KERNEL_MODE

NTSTATUS ProcessStaticImport32(PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor, ULONG_PTR BaseAddress, ULONG_PTR Flags)
{
    return STATUS_NOT_IMPLEMENTED;
}

#else // user mode

NTSTATUS ProcessStaticImport32(PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor, ULONG_PTR BaseAddress, ULONG_PTR Flags)
{
    ULONG_PTR           Length;
    PVOID               DllBase;
    PCSTR               DllName;
    WCHAR               DllPathBuffer[MAX_NTPATH];
    UNICODE_STRING      DllPath;
    NTSTATUS            Status;
    PIMAGE_THUNK_DATA   OriginalThunk, FirstThunk;

    DllPath.Buffer = DllPathBuffer;
    DllPath.MaximumLength = sizeof(DllPathBuffer);
    for (; ImportDescriptor->Name != NULL && ImportDescriptor->FirstThunk != NULL; ++ImportDescriptor)
    {
        DllName = (PCSTR)BaseAddress + ImportDescriptor->Name;
        Nls::AnsiToUnicode(DllPathBuffer, countof(DllPathBuffer), DllName, -1, &Length);
        DllPath.Length = (USHORT)Length;
        Status = LdrLoadDll(NULL, 0, &DllPath, &DllBase);
        if (!NT_SUCCESS(Status))
        {
            if (FLAG_ON(Flags, LOAD_PE_DLL_NOT_FOUND_CONTINUE))
                continue;

            return Status;
        }

        OriginalThunk = (PIMAGE_THUNK_DATA)BaseAddress;
        if (ImportDescriptor->OriginalFirstThunk)
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->OriginalFirstThunk);
        else
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->FirstThunk);

        FirstThunk = (PIMAGE_THUNK_DATA)(BaseAddress + ImportDescriptor->FirstThunk);
        while (OriginalThunk->u1.AddressOfData != NULL)
        {
            LONG_PTR FunctionName;

            FunctionName = OriginalThunk->u1.AddressOfData;
            FunctionName = (FunctionName < 0) ? (USHORT)FunctionName : (FunctionName + BaseAddress + 2);
            *(PVOID *)&FirstThunk->u1.Function = GetRoutineAddress(DllBase, (PCSTR)FunctionName);
            ++OriginalThunk;
            ++FirstThunk;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS ProcessStaticImport64(PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor, ULONG_PTR BaseAddress, ULONG_PTR Flags)
{
    ULONG_PTR           Length;
    PVOID               DllBase;
    PCSTR               DllName;
    WCHAR               DllPathBuffer[MAX_NTPATH];
    UNICODE_STRING      DllPath;
    NTSTATUS            Status;
    PIMAGE_THUNK_DATA64 OriginalThunk, FirstThunk;

    DllPath.Buffer = DllPathBuffer;
    DllPath.MaximumLength = sizeof(DllPathBuffer);
    for (; ImportDescriptor->Name != NULL && ImportDescriptor->FirstThunk != NULL; ++ImportDescriptor)
    {
        DllName = (PCSTR)BaseAddress + ImportDescriptor->Name;
        Nls::AnsiToUnicode(DllPathBuffer, countof(DllPathBuffer), DllName, -1, &Length);
        DllPath.Length = (USHORT)Length;
        Status = LdrLoadDll(NULL, 0, &DllPath, &DllBase);
        if (!NT_SUCCESS(Status))
            return Status;

        OriginalThunk = (PIMAGE_THUNK_DATA64)BaseAddress;
        if (ImportDescriptor->OriginalFirstThunk)
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->OriginalFirstThunk);
        else
            OriginalThunk = PtrAdd(OriginalThunk, ImportDescriptor->FirstThunk);

        FirstThunk = (PIMAGE_THUNK_DATA64)(BaseAddress + ImportDescriptor->FirstThunk);
        while (OriginalThunk->u1.AddressOfData != NULL)
        {
            LONG_PTR FunctionName;

            FunctionName = (LONG_PTR)OriginalThunk->u1.AddressOfData;
            FunctionName = (FunctionName < 0) ? (USHORT)FunctionName : (FunctionName + BaseAddress + 2);
            *(PVOID *)&FirstThunk->u1.Function = GetRoutineAddress(DllBase, FunctionName);
            ++OriginalThunk;
            ++FirstThunk;
        }
    }

    return STATUS_SUCCESS;
}

#endif // ML_KERNEL_MODE

NTSTATUS
LoadPeImageWorker(
    PCWSTR      FullDllPath,
    PVOID*      DllBaseAddress,
    PVOID       OldBaseAddress,
    ULONG_PTR   Flags
)
{
    ULONG_PTR                   SizeOfHeaders, FileAlignment, SectionAlignment, SizeOfImage;
    PVOID64                     ImageBase;
    NTSTATUS                    Status;
    NtFileDisk                  file;
    IMAGE_DOS_HEADER            DosHeader;
    PIMAGE_OPTIONAL_HEADER      OptionalHeader32;
    PIMAGE_OPTIONAL_HEADER64    OptionalHeader64;
    PIMAGE_SECTION_HEADER       SectionHeader, Section;
    PIMAGE_IMPORT_DESCRIPTOR    ImportDescriptor;
    PIMAGE_DATA_DIRECTORY       DataDirectory;
    PBYTE                       Buffer;
    PVOID                       BaseAddress;

    union
    {
        IMAGE_NT_HEADERS            NtHeaders;
        IMAGE_NT_HEADERS64          NtHeaders64;
    };

    *DllBaseAddress = NULL;

    Status = file.Open(FullDllPath, FLAG_ON(Flags, LOAD_PE_NOT_RESOLVE_PATH) ? NFD_NOT_RESOLVE_PATH : 0);

    if (!NT_SUCCESS(Status))
        return Status;

    Status = file.Read(&DosHeader, sizeof(DosHeader));
    if (!NT_SUCCESS(Status))
        return Status;

    if (DosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        return STATUS_INVALID_IMAGE_FORMAT;

    Status = file.Seek(DosHeader.e_lfanew, FILE_BEGIN);
    if (!NT_SUCCESS(Status))
        return Status;

    Status = file.Read(&NtHeaders, sizeof(NtHeaders) - sizeof(NtHeaders.OptionalHeader));
    if (!NT_SUCCESS(Status))
        return Status;

    if (NtHeaders.Signature != IMAGE_NT_SIGNATURE)
        return STATUS_INVALID_IMAGE_FORMAT;

    switch (NtHeaders.FileHeader.Machine)
    {
        case IMAGE_FILE_MACHINE_I386:
            OptionalHeader32 = (PIMAGE_OPTIONAL_HEADER)AllocStack(NtHeaders.FileHeader.SizeOfOptionalHeader);
            OptionalHeader64 = NULL;
            Status = file.Read(OptionalHeader32, NtHeaders.FileHeader.SizeOfOptionalHeader);
            break;

        case IMAGE_FILE_MACHINE_AMD64:
            OptionalHeader32 = NULL;
            OptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)AllocStack(NtHeaders64.FileHeader.SizeOfOptionalHeader);
            Status = file.Read(OptionalHeader64, NtHeaders64.FileHeader.SizeOfOptionalHeader);
            break;

        default:
            return STATUS_INVALID_IMAGE_FORMAT;
    }

    if (!NT_SUCCESS(Status))
        return Status;

    SectionHeader = (PIMAGE_SECTION_HEADER)AllocStack(NtHeaders.FileHeader.NumberOfSections * sizeof(*SectionHeader));
    Status = file.Read(SectionHeader, NtHeaders.FileHeader.NumberOfSections * sizeof(*SectionHeader));
    if (!NT_SUCCESS(Status))
        return Status;

    if (OptionalHeader32 != NULL)
    {
        SizeOfImage         = OptionalHeader32->SizeOfImage;
        SizeOfHeaders       = OptionalHeader32->SizeOfHeaders;
        FileAlignment       = OptionalHeader32->FileAlignment;
        SectionAlignment    = OptionalHeader32->SectionAlignment;
        ImageBase           = (PVOID64)OptionalHeader32->ImageBase;
        DataDirectory       = OptionalHeader32->DataDirectory;
    }
    else
    {
        SizeOfImage         = OptionalHeader64->SizeOfImage;
        SizeOfHeaders       = OptionalHeader64->SizeOfHeaders;
        FileAlignment       = OptionalHeader64->FileAlignment;
        SectionAlignment    = OptionalHeader64->SectionAlignment;
        ImageBase           = (PVOID64)OptionalHeader64->ImageBase;
        DataDirectory       = OptionalHeader64->DataDirectory;
    }

#if ML_KERNEL_MODE

    PMDL                Mdl;
    PHYSICAL_ADDRESS    LowAddress, HighAddress, SkipBytes;

    LowAddress.QuadPart     = 0;
    HighAddress.QuadPart    = -1;
    SkipBytes.QuadPart      = 0;

    BaseAddress = MmAllocateMappingAddress(SizeOfImage, LOAD_PE_MEMORY_TAG);
    if (BaseAddress == NULL)
        return STATUS_NO_MEMORY;

    Mdl = MmAllocatePagesForMdl(LowAddress, HighAddress, SkipBytes, SizeOfImage);
    if (Mdl == NULL)
    {
        MmFreeMappingAddress(BaseAddress, LOAD_PE_MEMORY_TAG);
        return STATUS_NO_MEMORY;
    }

    BaseAddress = MmMapLockedPagesWithReservedMapping(BaseAddress, LOAD_PE_MEMORY_TAG, Mdl, MmCached);
    if (BaseAddress == NULL)
    {
        MmFreePagesFromMdl(Mdl);
        ExFreePool(Mdl);
        MmFreeMappingAddress(BaseAddress, LOAD_PE_MEMORY_TAG);

        return STATUS_NO_MEMORY;
    }

    *(PMDL *)&DosHeader.e_res2 = Mdl;

    *DllBaseAddress = BaseAddress;

    Status = MmProtectMdlSystemAddress(Mdl, PAGE_EXECUTE_READWRITE);
    FAIL_RETURN(Status);

#else // r3

    Status = Mm::AllocVirtualMemory(DllBaseAddress, SizeOfImage, PAGE_EXECUTE_READWRITE, MEM_RESERVE | MEM_COMMIT | (FLAG_ON(Flags, LOAD_PE_TOP_TO_DOWN) ? MEM_TOP_DOWN : 0));
    if (!NT_SUCCESS(Status))
        return Status;

    BaseAddress = *DllBaseAddress;

#endif

    FAIL_RETURN(file.Seek(0ll, FILE_BEGIN));
    FAIL_RETURN(file.Read(BaseAddress, SizeOfHeaders));

    Section = SectionHeader;
    for (ULONG_PTR NumberOfSection = NtHeaders.FileHeader.NumberOfSections; NumberOfSection; ++Section, --NumberOfSection)
    {
        ULONG_PTR PointerToRawData, SizeOfRawData, VirtualAddress;

        SizeOfRawData = Section->SizeOfRawData;
        SizeOfRawData = ROUND_UP(SizeOfRawData, FileAlignment);
        if (SizeOfRawData == 0)
            continue;

        PointerToRawData = Section->PointerToRawData;
        PointerToRawData = ROUND_DOWN(PointerToRawData, FileAlignment);
        Status = file.Seek(PointerToRawData, FILE_BEGIN);
        if (!NT_SUCCESS(Status))
            return Status;

        VirtualAddress = Section->VirtualAddress;
        VirtualAddress = ROUND_DOWN(VirtualAddress, SectionAlignment);
        Buffer = (PBYTE)BaseAddress + VirtualAddress;
        Status = file.Read(Buffer, SizeOfRawData);
        if (!NT_SUCCESS(Status))
            return Status;
    }

    Buffer = (PBYTE)BaseAddress;

    if (!FLAG_ON(Flags, LOAD_PE_IGNORE_RELOC)) LOOP_ONCE
    {
        if (!ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC], SizeOfImage))
            break;

        Status = RelocPeImage(Buffer, ImageBase, OldBaseAddress);
    }

#if !ML_KERNEL_MODE

    if (!FLAG_ON(Flags, LOAD_PE_IGNORE_IAT) &&
        ValidateDataDirectory(&DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT], SizeOfImage))
    {
        ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + Buffer);

        switch (NtHeaders.FileHeader.Machine)
        {
            case IMAGE_FILE_MACHINE_I386:
                Status = ProcessStaticImport32(ImportDescriptor, (ULONG_PTR)Buffer, Flags);
                break;

            case IMAGE_FILE_MACHINE_AMD64:
                Status = ProcessStaticImport64(ImportDescriptor, (ULONG_PTR)Buffer, Flags);
                break;
        }
    }

#else

    Status = STATUS_SUCCESS;

#endif // r3

    return Status;
}

NTSTATUS UnloadPeImage(PVOID DllBase)
{
    if (DllBase == NULL)
        return STATUS_INVALID_ADDRESS;

#if ML_KERNEL_MODE

    PIMAGE_DOS_HEADER   DosHeader;
    PMDL                Mdl;

    DosHeader   = (PIMAGE_DOS_HEADER)DllBase;
    Mdl         = *(PMDL *)&DosHeader->e_res2;

    if (Mdl == NULL)
        return STATUS_UNSUCCESSFUL;

    MmUnmapReservedMapping(DllBase, LOAD_PE_MEMORY_TAG, Mdl);
    MmFreePagesFromMdl(Mdl);
    ExFreePool(Mdl);
    MmFreeMappingAddress(DllBase, LOAD_PE_MEMORY_TAG);

    return STATUS_SUCCESS;

#else // r3

    return Mm::FreeVirtualMemory(DllBase);

#endif // ML_KERNEL_MODE
}

NTSTATUS
LoadPeImage(
    PCWSTR      FullDllPath,
    PVOID*      DllBaseAddress,
    PVOID       OldBaseAddress,
    ULONG_PTR   Flags
)
{
    NTSTATUS Status;

    Status = LoadPeImageWorker(FullDllPath, DllBaseAddress, OldBaseAddress, Flags);
    if (!NT_SUCCESS(Status) && *DllBaseAddress != NULL)
        UnloadPeImage(*DllBaseAddress);

    return Status;
}

PVOID FASTCALL GetRoutineAddress(PVOID ModuleBase, LPCSTR RoutineName)
{

#if ML_USER_MODE

    PVOID       ProcAddress;
    LONG        Ordinal;
    NTSTATUS    Status;
    ANSI_STRING ProcString, *ProcName;

    if ((ULONG_PTR)RoutineName > 0xFFFF)
    {
        RtlInitAnsiString(&ProcString, RoutineName);
        ProcName    = &ProcString;
        Ordinal     = 0;
    }
    else
    {
        ProcName    = NULL;
        Ordinal     = (LONG)(LONG_PTR)RoutineName;
    }

    Status = LdrGetProcedureAddress(ModuleBase, ProcName, (USHORT)Ordinal, &ProcAddress);
    if (!NT_SUCCESS(Status))
        return NULL;

    return ProcAddress == (PVOID)ModuleBase ? NULL : (PVOID)ProcAddress;

#else

    return 0;

#endif

}

#if ML_USER_MODE

PVOID
LoadDll(
    PCWSTR ModuleFileName
)
{
    PVOID           ModuleBase;
    NTSTATUS        Status;
    UNICODE_STRING  ModuleFile;

    RtlInitUnicodeString(&ModuleFile, ModuleFileName);

    Status = LdrLoadDll(NULL, NULL, &ModuleFile, &ModuleBase);

    return NT_SUCCESS(Status) ? ModuleBase : NULL;
}



#endif // r3

ML_NAMESPACE_END_(Ldr);

ML_NAMESPACE_BEGIN(Ps);

using namespace Mm;
using namespace Ldr;





NTSTATUS ProcessIdToHandleEx(PHANDLE ProcessHandle, ULONG_PTR ProcessId, ULONG_PTR Access /* = PROCESS_ALL_ACCESS */)
{
#if ML_USER_MODE

    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    CLIENT_ID           CliendID;

    CliendID.UniqueThread   = nullptr;
    CliendID.UniqueProcess  = (HANDLE)ProcessId;
    InitializeObjectAttributes(&ObjectAttributes, nullptr, 0, nullptr, nullptr);

    Status = NtOpenProcess(ProcessHandle, Access, &ObjectAttributes, &CliendID);

    return Status;

#else // r0

    PEPROCESS   Process;
    NTSTATUS    Status;

    Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &Process);
    FAIL_RETURN(Status);

    Status = ObOpenObjectByPointer(Process, 0, nullptr, Access, *PsProcessType, KernelMode, ProcessHandle);
    ObDereferenceObject(Process);

    return Status;

#endif
}

HANDLE ProcessIdToHandle(ULONG_PTR ProcessId, ULONG_PTR Access /* = PROCESS_ALL_ACCESS */)
{
    NTSTATUS    Status;
    HANDLE      ProcessHandle;

    Status = ProcessIdToHandleEx(&ProcessHandle, ProcessId, Access);
    return NT_SUCCESS(Status) ? ProcessHandle : nullptr;
}





ULONG_PTR GetSessionId(HANDLE Process)
{
    NTSTATUS Status;
    PROCESS_SESSION_INFORMATION Session;

#if ML_KERNEL_MODE

    Status = ZwQueryInformationProcess(Process, ProcessSessionInformation, &Session, sizeof(Session), NULL);

#else

    Status = NtQueryInformationProcess(Process, ProcessSessionInformation, &Session, sizeof(Session), NULL);

#endif

    return NT_FAILED(Status) ? INVALID_SESSION_ID : Session.SessionId;
}

ULONG_PTR GetSessionId(ULONG_PTR ProcessId)
{
    ULONG_PTR   SessionId;
    HANDLE      Process;

    Process = PidToHandle(ProcessId);
    if (Process == NULL)
        return INVALID_SESSION_ID;

    SessionId = GetSessionId(Process);

#if ML_KERNEL_MODE

    ZwClose(Process);

#else

    NtClose(Process);

#endif

    return SessionId;
}

BOOL IsWow64Process(HANDLE Process)
{
    PVOID       Wow64Information;
    NTSTATUS    Status;

    API_POINTER(ZwQueryInformationProcess)  XQueryInformationProcess;

#if ML_KERNEL_MODE

    XQueryInformationProcess = ZwQueryInformationProcess;

#else

    XQueryInformationProcess = NtQueryInformationProcess;

#endif

    Status = XQueryInformationProcess(
                Process,
                ProcessWow64Information,
                &Wow64Information,
                sizeof(Wow64Information),
                NULL
             );

    return NT_SUCCESS(Status) && Wow64Information != NULL;
}

PSYSTEM_PROCESS_INFORMATION QuerySystemProcesses()
{
    ULONG                       Size, Length;
    NTSTATUS                    Status;
    PSYSTEM_PROCESS_INFORMATION ProcessInfo;

    ProcessInfo = NULL;
    Size = 0;

    LOOP_FOREVER
    {
#if ML_KERNEL_MODE

        Status = ZwQuerySystemInformation(SystemProcessInformation, ProcessInfo, Size, &Length);
        if (Status != STATUS_INFO_LENGTH_MISMATCH)
            break;

        FreeMemoryP(ProcessInfo);
        ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)AllocateMemoryP(Length);

#else
        Status = NtQuerySystemInformation(SystemProcessInformation, ProcessInfo, Size, &Length);
        if (Status != STATUS_INFO_LENGTH_MISMATCH)
            break;

        ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)ReAllocateMemoryP(ProcessInfo, Length);

#endif

        if (ProcessInfo == NULL)
            return NULL;

        Size = Length;
    }

    if (!NT_SUCCESS(Status))
    {
        FreeMemoryP(ProcessInfo);
        ProcessInfo = NULL;
    }

    return ProcessInfo;
}

#if ML_X86





#endif // x86



#if ML_USER_MODE

PTEB_ACTIVE_FRAME
FindThreadFrame(
    ULONG_PTR Context
)
{
    PTEB_ACTIVE_FRAME Frame;

    Frame = RtlGetFrame();
    while (Frame != NULL && Frame->Context != Context)
        Frame = Frame->Previous;

    return Frame;
}



NTSTATUS
CreateProcess(
    PCWSTR                  ApplicationName,
    PWSTR                   CommandLine,
    PCWSTR                  CurrentDirectory,
    ULONG                   CreationFlags,
    LPSTARTUPINFOW          StartupInfo,
    LPPROCESS_INFORMATION   ProcessInformation,
    LPSECURITY_ATTRIBUTES   ProcessAttributes,
    LPSECURITY_ATTRIBUTES   ThreadAttributes,
    PVOID                   Environment,
    HANDLE                  Token,
    BOOL                    InheritHandles
)
{
    BOOL                Result;
    STARTUPINFOW        DefaultStartupInfo;
    PROCESS_INFORMATION DefaultProcessInformation;

    if (StartupInfo == NULL)
    {
        ZeroMemory(&DefaultStartupInfo, sizeof(DefaultStartupInfo));
        DefaultStartupInfo.cb = sizeof(DefaultStartupInfo);
        StartupInfo = &DefaultStartupInfo;
    }

    if (ProcessInformation == NULL)
        ProcessInformation = &DefaultProcessInformation;

    auto createProcessInternal = reinterpret_cast<API_POINTER(CreateProcessInternalW)>(
        LookupExportTable(GetKernel32Handle(), KERNEL32_CreateProcessInternalW));
    if (createProcessInternal == nullptr)
        return STATUS_PROCEDURE_NOT_FOUND;

    Result = createProcessInternal(
                    Token,
                    ApplicationName,
                    CommandLine,
                    ProcessAttributes,
                    ThreadAttributes,
                    InheritHandles,
                    CreationFlags,
                    Environment,
                    CurrentDirectory,
                    StartupInfo,
                    ProcessInformation,
                    NULL
             );

    if (!Result)
        return ML_NTSTATUS_FROM_WIN32(RtlGetLastWin32Error());

    if (ProcessInformation == &DefaultProcessInformation)
    {
        NtClose(DefaultProcessInformation.hProcess);
        NtClose(DefaultProcessInformation.hThread);
    }

    return STATUS_SUCCESS;
}

#if !ML_AMD64





#endif // AMD64





#endif // r3

ML_NAMESPACE_END_(Ps);

ML_NAMESPACE_BEGIN(Mm)

NTSTATUS
AllocVirtualMemory(
    PVOID*      BaseAddress,
    ULONG_PTR   Size,
    ULONG       Protect,        /* = PAGE_EXECUTE_READWRITE */
    ULONG       AllocationType, /* = MEM_RESERVE | MEM_COMMIT */
    HANDLE      ProcessHandle   /* = Ps::CurrentProcess */
)
{
    return NtAllocateVirtualMemory(ProcessHandle, BaseAddress, 0, &Size, AllocationType, Protect);
}

NTSTATUS
AllocVirtualMemoryEx(
    HANDLE      ProcessHandle,
    PVOID*      BaseAddress,
    ULONG_PTR   Size,
    ULONG       Protect,
    ULONG       AllocationType
)
{
    return AllocVirtualMemory(BaseAddress, Size, Protect, AllocationType, ProcessHandle);
}

NTSTATUS
FreeVirtualMemory(
    PVOID   BaseAddress,
    HANDLE  ProcessHandle,  /* = Ps::CurrentProcess */
    ULONG   FreeType        /* = MEM_RELEASE */
)
{
    SIZE_T Size = 0;

    return NtFreeVirtualMemory(ProcessHandle, &BaseAddress, &Size, MEM_RELEASE);
}

#if ML_USER_MODE

NTSTATUS
ProtectVirtualMemory(
    PVOID       BaseAddress,
    ULONG_PTR   Size,
    ULONG       NewProtect,
    PULONG      OldProtect,
    HANDLE      ProcessHandle   /* = Ps::CurrentProcess */
)
{
    ULONG _OldProtect;
    NTSTATUS Status;

    Status = NtProtectVirtualMemory(ProcessHandle, &BaseAddress, &Size, NewProtect, &_OldProtect);
    if (NT_SUCCESS(Status) && OldProtect != NULL)
        *OldProtect = _OldProtect;

    return Status;
}

NTSTATUS
ProtectMemory(
    HANDLE      ProcessHandle,
    PVOID       BaseAddress,
    ULONG_PTR   Size,
    ULONG       NewProtect,
    PULONG      OldProtect
)
{
    return ProtectVirtualMemory(BaseAddress, Size, NewProtect, OldProtect, ProcessHandle);
}

NTSTATUS
ReadMemory(
    HANDLE      ProcessHandle,
    PVOID       BaseAddress,
    PVOID       Buffer,
    ULONG_PTR   Size,
    PULONG_PTR  BytesRead /* = NULL */
)
{
    return NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, Size, BytesRead);
}

NTSTATUS
WriteMemory(
    HANDLE      ProcessHandle,
    PVOID       BaseAddress,
    PVOID       Buffer,
    ULONG_PTR   Size,
    PULONG_PTR  BytesWritten /* = NULL */
)
{
    return NtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, Size, BytesWritten);
}

NTSTATUS
WriteProtectMemory(
    HANDLE      ProcessHandle,
    PVOID       BaseAddress,
    PVOID       Buffer,
    ULONG_PTR   Size,
    PULONG_PTR  BytesWritten /* = NULL */
)
{
    ULONG       Protect;
    NTSTATUS    Status;

    //Status = ProtectVirtualMemory(BaseAddress, Size, PAGE_WRITECOPY, &Protect, ProcessHandle);
    //FAIL_RETURN(Status);

    Status = ProtectVirtualMemory(BaseAddress, Size, PAGE_EXECUTE_READWRITE, &Protect, ProcessHandle);
    FAIL_RETURN(Status);

    Status = WriteMemory(ProcessHandle, BaseAddress, Buffer, Size, BytesWritten);

    if (Protect != PAGE_EXECUTE_READWRITE)
        ProtectVirtualMemory(BaseAddress, Size, Protect, NULL, ProcessHandle);

    return Status;
}



#endif // r3

ML_NAMESPACE_END_(Mm)

ML_NAMESPACE_BEGIN(Rtl);





#if ML_USER_MODE













#endif // r3


BOOL
DoesNameContainWildCards(
    IN PUNICODE_STRING Name
)

/*++

Routine Description:

    This routine simply scans the input Name string looking for any Nt
    wild card characters.

Arguments:

    Name - The string to check.

Return Value:

    BOOLEAN - TRUE if one or more wild card characters was found.

--*/
{
    PWSTR p;

    //
    //  Check each character in the name to see if it's a wildcard
    //  character.
    //

#if ML_USER_MODE

    static const UCHAR LEGAL_ANSI_CHARACTER_ARRAY[128] =
    {
        0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x17, 0x07, 0x18, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x18, 0x16, 0x16, 0x17, 0x07, 0x00,
        0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x04, 0x16, 0x18, 0x16, 0x18, 0x18,
        0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
        0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x16, 0x00, 0x16, 0x17, 0x17,
        0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
        0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x10, 0x17, 0x17, 0x17
    };

#endif // r3

    if( Name->Length ) {
        for( p = Name->Buffer + (Name->Length / sizeof(WCHAR)) - 1;
             p >= Name->Buffer && *p != L'\\' ;
             p-- ) {

            //
            //  check for a wild card character
            //

            if (FsRtlIsUnicodeCharacterWild( *p )) {

                //
                //  Tell caller that this name contains wild cards
                //

                return TRUE;
            }
        }
    }

    //
    //  No wildcard characters were found, so return to our caller
    //

    return FALSE;
}

#define MATCHES_ARRAY_SIZE 16

//
//  Local support routine prototypes
//

BOOL
IsNameInExpressionPrivate (
    IN PUNICODE_STRING Expression,
    IN PUNICODE_STRING Name,
    IN BOOL IgnoreCase,
    IN PWCH UpcaseTable
)

/*++

Routine Description:

    This routine compares a Dbcs name and an expression and tells the caller
    if the name is in the language defined by the expression.  The input name
    cannot contain wildcards, while the expression may contain wildcards.

    Expression wild cards are evaluated as shown in the nondeterministic
    finite automatons below.  Note that ~* and ~? are DOS_STAR and DOS_QM.


             ~* is DOS_STAR, ~? is DOS_QM, and ~. is DOS_DOT


                                       S
                                    <-----<
                                 X  |     |  e       Y
             X * Y ==       (0)----->-(1)->-----(2)-----(3)


                                      S-.
                                    <-----<
                                 X  |     |  e       Y
             X ~* Y ==      (0)----->-(1)->-----(2)-----(3)



                                X     S     S     Y
             X ?? Y ==      (0)---(1)---(2)---(3)---(4)



                                X     .        .      Y
             X ~.~. Y ==    (0)---(1)----(2)------(3)---(4)
                                   |      |________|
                                   |           ^   |
                                   |_______________|
                                      ^EOF or .^


                                X     S-.     S-.     Y
             X ~?~? Y ==    (0)---(1)-----(2)-----(3)---(4)
                                   |      |________|
                                   |           ^   |
                                   |_______________|
                                      ^EOF or .^



         where S is any single character

               S-. is any single character except the final .

               e is a null character transition

               EOF is the end of the name string

    In words:

        * matches 0 or more characters.

        ? matches exactly 1 character.

        DOS_STAR matches 0 or more characters until encountering and matching
            the final . in the name.

        DOS_QM matches any single character, or upon encountering a period or
            end of name string, advances the expression to the end of the
            set of contiguous DOS_QMs.

        DOS_DOT matches either a . or zero characters beyond name string.

Arguments:

    Expression - Supplies the input expression to check against
        (Caller must already upcase if passing CaseInsensitive TRUE.)

    Name - Supplies the input name to check for.

    CaseInsensitive - TRUE if Name should be Upcased before comparing.

Return Value:

    BOOLEAN - TRUE if Name is an element in the set of strings denoted
        by the input Expression and FALSE otherwise.

--*/

{
    USHORT NameOffset;
    USHORT ExprOffset;

    ULONG SrcCount;
    ULONG DestCount;
    ULONG PreviousDestCount;
    ULONG MatchesCount;

    WCHAR NameChar = 0, ExprChar;

    USHORT LocalBuffer[MATCHES_ARRAY_SIZE * 2];

    USHORT *AuxBuffer = NULL;
    USHORT *PreviousMatches;
    USHORT *CurrentMatches;

    USHORT MaxState;
    USHORT CurrentState;

    BOOL NameFinished = FALSE;

    //
    //  The idea behind the algorithm is pretty simple.  We keep track of
    //  all possible locations in the regular expression that are matching
    //  the name.  If when the name has been exhausted one of the locations
    //  in the expression is also just exhausted, the name is in the language
    //  defined by the regular expression.
    //

    //
    //  If one string is empty return FALSE.  If both are empty return TRUE.
    //

    if ( (Name->Length == 0) || (Expression->Length == 0) ) {

        return (BOOL)(!(Name->Length + Expression->Length));
    }

    //
    //  Special case by far the most common wild card search of *
    //

    if ((Expression->Length == 2) && (Expression->Buffer[0] == L'*')) {

        return TRUE;
    }

    //
    //  Also special case expressions of the form *X.  With this and the prior
    //  case we have covered virtually all normal queries.
    //

    if (Expression->Buffer[0] == L'*') {

        UNICODE_STRING LocalExpression;

        LocalExpression = *Expression;

        LocalExpression.Buffer += 1;
        LocalExpression.Length -= 2;

        //
        //  Only special case an expression with a single *
        //

        if ( !DoesNameContainWildCards( &LocalExpression ) ) {

            ULONG StartingNameOffset;

            if (Name->Length < (USHORT)(Expression->Length - sizeof(WCHAR))) {

                return FALSE;
            }

            StartingNameOffset = ( Name->Length -
                                   LocalExpression.Length ) / sizeof(WCHAR);

            //
            //  Do a simple memory compare if case sensitive, otherwise
            //  we have got to check this one character at a time.
            //

            if ( !IgnoreCase ) {

                return (BOOL) RtlEqualMemory( LocalExpression.Buffer,
                                                 Name->Buffer + StartingNameOffset,
                                                 LocalExpression.Length );

            } else {

                for ( ExprOffset = 0;
                      ExprOffset < (USHORT)(LocalExpression.Length / sizeof(WCHAR));
                      ExprOffset += 1 ) {

                    NameChar = Name->Buffer[StartingNameOffset + ExprOffset];
                    NameChar = UpcaseTable[NameChar];

                    ExprChar = LocalExpression.Buffer[ExprOffset];

                    if ( NameChar != ExprChar ) {

                        return FALSE;
                    }
                }

                return TRUE;
            }
        }
    }

    //
    //  Walk through the name string, picking off characters.  We go one
    //  character beyond the end because some wild cards are able to match
    //  zero characters beyond the end of the string.
    //
    //  With each new name character we determine a new set of states that
    //  match the name so far.  We use two arrays that we swap back and forth
    //  for this purpose.  One array lists the possible expression states for
    //  all name characters up to but not including the current one, and other
    //  array is used to build up the list of states considering the current
    //  name character as well.  The arrays are then switched and the process
    //  repeated.
    //
    //  There is not a one-to-one correspondence between state number and
    //  offset into the expression.  This is evident from the NFAs in the
    //  initial comment to this function.  State numbering is not continuous.
    //  This allows a simple conversion between state number and expression
    //  offset.  Each character in the expression can represent one or two
    //  states.  * and DOS_STAR generate two states: ExprOffset*2 and
    //  ExprOffset*2 + 1.  All other expression characters can produce only
    //  a single state.  Thus ExprOffset = State/2.
    //
    //
    //  Here is a short description of the variables involved:
    //
    //  NameOffset  - The offset of the current name char being processed.
    //
    //  ExprOffset  - The offset of the current expression char being processed.
    //
    //  SrcCount    - Prior match being investigated with current name char
    //
    //  DestCount   - Next location to put a matching assuming current name char
    //
    //  NameFinished - Allows one more iteration through the Matches array
    //                 after the name is exhausted (to come *s for example)
    //
    //  PreviousDestCount - This is used to prevent entry duplication, see coment
    //
    //  PreviousMatches   - Holds the previous set of matches (the Src array)
    //
    //  CurrentMatches    - Holds the current set of matches (the Dest array)
    //
    //  AuxBuffer, LocalBuffer - the storage for the Matches arrays
    //

    //
    //  Set up the initial variables
    //

    PreviousMatches = &LocalBuffer[0];
    CurrentMatches = &LocalBuffer[MATCHES_ARRAY_SIZE];

    PreviousMatches[0] = 0;
    MatchesCount = 1;

    NameOffset = 0;

    MaxState = (USHORT)(Expression->Length * 2);

    while ( !NameFinished ) {

        if ( NameOffset < Name->Length ) {

            NameChar = Name->Buffer[NameOffset / sizeof(WCHAR)];

            NameOffset += sizeof(WCHAR);;

        } else {

            NameFinished = TRUE;

            //
            //  if we have already exhausted the expression, cool.  Don't
            //  continue.
            //

            if ( PreviousMatches[MatchesCount-1] == MaxState ) {

                break;
            }
        }


        //
        //  Now, for each of the previous stored expression matches, see what
        //  we can do with this name character.
        //

        SrcCount = 0;
        DestCount = 0;
        PreviousDestCount = 0;

        while ( SrcCount < MatchesCount ) {

            USHORT Length;

            //
            //  We have to carry on our expression analysis as far as possible
            //  for each character of name, so we loop here until the
            //  expression stops matching.  A clue here is that expression
            //  cases that can match zero or more characters end with a
            //  continue, while those that can accept only a single character
            //  end with a break.
            //

            ExprOffset = (USHORT)((PreviousMatches[SrcCount++] + 1) / 2);


            Length = 0;

            LOOP_FOREVER
            {

                if ( ExprOffset == Expression->Length ) {

                    break;
                }

                //
                //  The first time through the loop we don't want
                //  to increment ExprOffset.
                //

                ExprOffset = (USHORT)(ExprOffset + Length);
                Length = sizeof(WCHAR);

                CurrentState = (USHORT)(ExprOffset * 2);

                if ( ExprOffset == Expression->Length ) {

                    CurrentMatches[DestCount++] = MaxState;
                    break;
                }

                ExprChar = Expression->Buffer[ExprOffset / sizeof(WCHAR)];

                //
                //  Before we get started, we have to check for something
                //  really gross.  We may be about to exhaust the local
                //  space for ExpressionMatches[][], so we have to allocate
                //  some pool if this is the case.  Yuk!
                //

                if ( (DestCount >= MATCHES_ARRAY_SIZE - 2) &&
                     (AuxBuffer == NULL) ) {

                    ULONG ExpressionChars;

                    ExpressionChars = Expression->Length / sizeof(WCHAR);

                    AuxBuffer = (PUSHORT)AllocateMemoryP((ExpressionChars + 1) * sizeof(USHORT) * 2 * 2);

                    RtlCopyMemory( AuxBuffer,
                                   CurrentMatches,
                                   MATCHES_ARRAY_SIZE * sizeof(USHORT) );

                    CurrentMatches = AuxBuffer;

                    RtlCopyMemory( AuxBuffer + (ExpressionChars+1)*2,
                                   PreviousMatches,
                                   MATCHES_ARRAY_SIZE * sizeof(USHORT) );

                    PreviousMatches = AuxBuffer + (ExpressionChars+1)*2;
                }

                //
                //  * matches any character zero or more times.
                //

                if (ExprChar == L'*') {

                    CurrentMatches[DestCount++] = CurrentState;
                    CurrentMatches[DestCount++] = CurrentState + 3;
                    continue;
                }

                //
                //  DOS_STAR matches any character except . zero or more times.
                //

                if (ExprChar == DOS_STAR) {

                    BOOL ICanEatADot = FALSE;

                    //
                    //  If we are at a period, determine if we are allowed to
                    //  consume it, ie. make sure it is not the last one.
                    //

                    if ( !NameFinished && (NameChar == '.') ) {

                        USHORT Offset;

                        for ( Offset = NameOffset;
                              Offset < Name->Length;
                              Offset = (USHORT)(Offset + Length) ) {

                            if (Name->Buffer[Offset / sizeof(WCHAR)] == L'.') {

                                ICanEatADot = TRUE;
                                break;
                            }
                        }
                    }

                    if (NameFinished || (NameChar != L'.') || ICanEatADot) {

                        CurrentMatches[DestCount++] = CurrentState;
                        CurrentMatches[DestCount++] = CurrentState + 3;
                        continue;

                    } else {

                        //
                        //  We are at a period.  We can only match zero
                        //  characters (ie. the epsilon transition).
                        //

                        CurrentMatches[DestCount++] = CurrentState + 3;
                        continue;
                    }
                }

                //
                //  The following expression characters all match by consuming
                //  a character, thus force the expression, and thus state
                //  forward.
                //

                CurrentState += (USHORT)(sizeof(WCHAR) * 2);

                //
                //  DOS_QM is the most complicated.  If the name is finished,
                //  we can match zero characters.  If this name is a '.', we
                //  don't match, but look at the next expression.  Otherwise
                //  we match a single character.
                //

                if ( ExprChar == DOS_QM ) {

                    if ( NameFinished || (NameChar == L'.') ) {

                        continue;
                    }

                    CurrentMatches[DestCount++] = CurrentState;
                    break;
                }

                //
                //  A DOS_DOT can match either a period, or zero characters
                //  beyond the end of name.
                //

                if (ExprChar == DOS_DOT) {

                    if ( NameFinished ) {

                        continue;
                    }

                    if (NameChar == L'.') {

                        CurrentMatches[DestCount++] = CurrentState;
                        break;
                    }
                }

                //
                //  From this point on a name character is required to even
                //  continue, let alone make a match.
                //

                if ( NameFinished ) {

                    break;
                }

                //
                //  If this expression was a '?' we can match it once.
                //

                if (ExprChar == L'?') {

                    CurrentMatches[DestCount++] = CurrentState;
                    break;
                }

                //
                //  Finally, check if the expression char matches the name char
                //

                if (ExprChar == (WCHAR)(IgnoreCase ?
                                        UpcaseTable[NameChar] : NameChar)) {

                    CurrentMatches[DestCount++] = CurrentState;
                    break;
                }

                //
                //  The expression didn't match so go look at the next
                //  previous match.
                //

                break;
            }


            //
            //  Prevent duplication in the destination array.
            //
            //  Each of the arrays is monotonically increasing and non-
            //  duplicating, thus we skip over any source element in the src
            //  array if we just added the same element to the destination
            //  array.  This guarantees non-duplication in the dest. array.
            //

            while ((SrcCount < MatchesCount) &&
                   (PreviousDestCount < DestCount)) {

                while ((SrcCount < MatchesCount) &&
                       (PreviousMatches[SrcCount] <
                        CurrentMatches[PreviousDestCount])) {

                    SrcCount += 1;
                }

                PreviousDestCount += 1;
            }
        }

        //
        //  If we found no matches in the just finished iteration, it's time
        //  to bail.
        //

        if ( DestCount == 0 )
        {
            FreeMemoryP(AuxBuffer);
            return FALSE;
        }

        //
        //  Swap the meaning the two arrays
        //

        {
            USHORT *Tmp;

            Tmp = PreviousMatches;

            PreviousMatches = CurrentMatches;

            CurrentMatches = Tmp;
        }

        MatchesCount = DestCount;
    }


    CurrentState = PreviousMatches[MatchesCount-1];

    FreeMemoryP(AuxBuffer);


    return (BOOL)(CurrentState == MaxState);
}

BOOL
IsNameInExpression(
    IN PUNICODE_STRING  Expression,
    IN PUNICODE_STRING  Name,
    IN BOOL             IgnoreCase,
    IN PWSTR            UpcaseTable OPTIONAL
)
{
    BOOL Result = FALSE;
    UNICODE_STRING LocalExpression, LocalName;

    //
    //  If we weren't given an upcase table, we have to upcase the names
    //  ourselves.
    //

    if (IgnoreCase && UpcaseTable == NULL)
    {

        NTSTATUS Status;

        Status = RtlUpcaseUnicodeString(&LocalName, Name, TRUE);
        if (NT_FAILED(Status))
            return FALSE;

        Status = RtlUpcaseUnicodeString(&LocalExpression, Expression, TRUE);
        if (NT_FAILED(Status))
        {
            RtlFreeUnicodeString(&LocalName);
            return FALSE;
        }

        Name = &LocalName;
        Expression = &LocalExpression;

        IgnoreCase = FALSE;

    }
    else
    {
        LocalName.Buffer = NULL;
        LocalExpression.Buffer = NULL;
    }

    //
    //  Now call the main routine, remembering to free the upcased string
    //  if we allocated one.
    //

    SEH_TRY
    {
        Result = IsNameInExpressionPrivate(Expression, Name, IgnoreCase, UpcaseTable);
    }
    SEH_FINALLY
    {
        if (LocalName.Buffer != NULL)
            RtlFreeUnicodeString(&LocalName);

        if (LocalExpression.Buffer != NULL)
            RtlFreeUnicodeString(&LocalExpression);
    }

    return Result;
}



ML_NAMESPACE_END_(Rtl);

ML_NAMESPACE_BEGIN(Nls);

NTSTATUS
FASTCALL
AnsiToUnicode(
    PWCHAR      UnicodeBuffer,
    ULONG_PTR   BufferCount,
    LPCSTR      AnsiString,
    LONG_PTR    AnsiLength,
    PULONG_PTR  BytesInUnicode
)
{
    ULONG       Length;
    NTSTATUS    Status;

    if (AnsiLength == -1)
        AnsiLength = StrLengthA(AnsiString);

    Status = RtlMultiByteToUnicodeN(
                UnicodeBuffer,
                (ULONG)(BufferCount != -1 ? (BufferCount * sizeof(WCHAR)) : -1),
                &Length,
                (PCHAR)AnsiString,
                (ULONG)AnsiLength
             );
    if (!NT_SUCCESS(Status))
        return Status;

    if (Length / sizeof(WCHAR) < BufferCount)
        UnicodeBuffer[Length / sizeof(WCHAR)] = 0;

    if (BytesInUnicode != NULL)
        *BytesInUnicode = Length;

    return Status;
}

NTSTATUS
UnicodeToAnsi(
    PSTR        AnsiBuffer,
    ULONG_PTR   BufferCount,
    PCWSTR      UnicodeString,
    LONG_PTR    UnicodeLength,
    PULONG_PTR  BytesInAnsi
)
{
    ULONG       Length;
    NTSTATUS    Status;

    if (UnicodeLength == -1)
        UnicodeLength = StrLengthW(UnicodeString);

    Status = RtlUnicodeToMultiByteN(
                AnsiBuffer,
                (ULONG)BufferCount,
                &Length,
                (PWCHAR)UnicodeString,
                (ULONG)(UnicodeLength * sizeof(WCHAR))
             );

    if (!NT_SUCCESS(Status))
        return Status;

    if (Length < BufferCount)
        AnsiBuffer[Length] = 0;

    if (BytesInAnsi != NULL)
        *BytesInAnsi = Length;

    return Status;
}





ML_NAMESPACE_END_(Nls);

ML_NAMESPACE_BEGIN(Str)

/************************************************************************
  UNICODE_STRING helper class
************************************************************************/

UnicodeString::UnicodeString()
{
    RtlInitEmptyUnicodeString(this, NULL, 0);
}



NTSTATUS UnicodeString::Create(PCWSTR Buffer, ULONG_PTR Length)
{
    return RtlSetUnicodeString(this, Buffer, Length);
}

NTSTATUS UnicodeString::Create(PCUNICODE_STRING Buffer)
{
    return Create(Buffer->Buffer, Buffer->Length / sizeof(WCHAR));
}

BOOL UnicodeString::operator==(PCWSTR Buffer) const
{
    UNICODE_STRING String;

    RtlInitUnicodeString(&String, Buffer);

    return RtlEqualUnicodeString(this, &String, FALSE);
}

BOOL UnicodeString::operator==(PCUNICODE_STRING String) const
{
    return RtlEqualUnicodeString(this, String, FALSE);
}

BOOL UnicodeString::operator==(const UnicodeString &String) const
{
    return RtlEqualUnicodeString(this, &String, FALSE);
}

UnicodeString& UnicodeString::operator=(PCWSTR Buffer)
{
    if (*this == Buffer)
        return *this;

    Create(Buffer);

    return *this;
}

UnicodeString& UnicodeString::operator=(PCUNICODE_STRING Buffer)
{
    if (*this == Buffer)
        return *this;

    Create(Buffer);

    return *this;
}

/************************************************************************
  UNICODE_STRING helper class end
************************************************************************/

ML_NAMESPACE_END_(Str);

ML_NAMESPACE_BEGIN(Reg)

PVOID AllocateKeyInfo(ULONG_PTR Size)
{
    return AllocateMemoryP(Size);
}

VOID FreeKeyInfo(PVOID Info)
{
    FreeMemoryP(Info);
}

NTSTATUS OpenPredefinedKeyHandle(PHANDLE KeyHandle, HANDLE PredefinedKey, ACCESS_MASK DesiredAccess)
{
    ULONG_PTR           Length;
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    UNICODE_STRING      KeyPath;
    PWSTR               LocalKeyBuffer;

    API_POINTER(ZwOpenKey)  XOpenKey;

    static WCHAR LocalMachineSuffix[] = L"_Classes";
    static WCHAR LocalSettingsSuffix[] = L"\\Local Settings";

    if ((LONG)(LONG_PTR)PredefinedKey >= 0)
        return STATUS_INVALID_HANDLE;

#if ML_KERNEL_MODE

    XOpenKey = ZwOpenKey;

#else

    XOpenKey = NtOpenKey;

#endif


#pragma push_macro("KEY_INDEX")
#define KEY_INDEX(_Key) ((ULONG_PTR)(_Key) & ~(1 << (bitsof(LONG) - 1)))

    Status = STATUS_INVALID_HANDLE;
    switch (KEY_INDEX(PredefinedKey))
    {
        case KEY_INDEX(HKEY_CLASSES_ROOT):
            Status = RtlFormatCurrentUserKeyPath(&KeyPath);
            FAIL_BREAK(Status);

            Length = KeyPath.Length;
            LocalKeyBuffer = (PWSTR)AllocStack(KeyPath.Length + sizeof(LocalMachineSuffix));
            CopyMemory(LocalKeyBuffer, KeyPath.Buffer, Length);
            CopyMemory(PtrAdd(LocalKeyBuffer, Length), LocalMachineSuffix, sizeof(LocalMachineSuffix));

            RtlFreeUnicodeString(&KeyPath);

            KeyPath.Buffer          = LocalKeyBuffer;
            KeyPath.Length          = (USHORT)(Length + CONST_STRLEN(LocalMachineSuffix) * sizeof(WCHAR));
            KeyPath.MaximumLength   = KeyPath.Length;

            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);

            if (NT_SUCCESS(Status))
                break;

            NO_BREAK;

        case KEY_INDEX(HKEY_MACHINE_CLASS):
            KeyPath = USTR(L"\\Registry\\Machine\\Software\\Classes");
            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);
            break;

#if ML_USER_MODE

        case KEY_INDEX(HKEY_CURRENT_USER):
            Status = RtlOpenCurrentUser(DesiredAccess, KeyHandle);
            break;

#endif // r3

        case KEY_INDEX(HKEY_LOCAL_MACHINE):
            KeyPath = USTR(L"\\Registry\\Machine");
            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);
            break;

        case KEY_INDEX(HKEY_USERS):
            KeyPath = USTR(L"\\Registry\\User");
            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);
            break;

        case KEY_INDEX(HKEY_PERFORMANCE_DATA):
        case KEY_INDEX(HKEY_PERFORMANCE_TEXT):
        case KEY_INDEX(HKEY_PERFORMANCE_NLSTEXT):
            *KeyHandle = (HANDLE)PredefinedKey;
            Status = STATUS_SUCCESS;
            break;

        case KEY_INDEX(HKEY_CURRENT_CONFIG):
            KeyPath = USTR(L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current");
            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);
            break;

        case KEY_INDEX(HKEY_CURRENT_USER_LOCAL_SETTINGS):
            Status = RtlFormatCurrentUserKeyPath(&KeyPath);
            FAIL_BREAK(Status);

            Length = KeyPath.Length;
            LocalKeyBuffer = (PWSTR)AllocStack(KeyPath.Length + sizeof(LocalMachineSuffix) + sizeof(LocalSettingsSuffix));
            CopyMemory(LocalKeyBuffer, KeyPath.Buffer, Length);
            CopyMemory(PtrAdd(LocalKeyBuffer, Length), LocalMachineSuffix, CONST_STRLEN(LocalMachineSuffix) * sizeof(WCHAR));
            Length += CONST_STRLEN(LocalMachineSuffix) * sizeof(WCHAR);
            CopyMemory(PtrAdd(LocalKeyBuffer, Length), LocalSettingsSuffix, sizeof(LocalSettingsSuffix));
            Length += CONST_STRLEN(LocalSettingsSuffix) * sizeof(WCHAR);

            RtlFreeUnicodeString(&KeyPath);

            KeyPath.Buffer          = LocalKeyBuffer;
            KeyPath.Length          = (USHORT)Length;
            KeyPath.MaximumLength   = KeyPath.Length;

            InitializeObjectAttributes(&ObjectAttributes, &KeyPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
            Status = XOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);
            break;

        case KEY_INDEX(HKEY_DYN_DATA):
            Status = STATUS_NOT_IMPLEMENTED;
            break;
    }

#pragma pop_macro("KEY_INDEX")

    return Status;
}

ForceInline NTSTATUS CloseKeyHandle(HANDLE KeyHandle)
{
    if (KeyHandle == nullptr)
        return STATUS_INVALID_HANDLE;

#if ML_KERNEL_MODE
    return ZwClose(KeyHandle);
#else
    return NtClose(KeyHandle);
#endif
}



NTSTATUS
OpenKey(
    PHANDLE     KeyHandle,
    HANDLE      hKey,
    ACCESS_MASK DesiredAccess,
    PCWSTR      SubKey
)
{
    BOOL                Retry;
    NTSTATUS            Status;
    HANDLE              RootKey;
    UNICODE_STRING      SubKeyString;
    OBJECT_ATTRIBUTES   ObjectAttributes;

    API_POINTER(ZwOpenKey)  XOpenKey;

#if ML_KERNEL_MODE

    XOpenKey = ZwOpenKey;

#else

    XOpenKey = NtOpenKey;

#endif

    RtlInitUnicodeString(&SubKeyString, SubKey);
    Retry = FALSE;

    LOOP_FOREVER
    {
        Status = OpenPredefinedKeyHandle(&RootKey, hKey, DesiredAccess);
        FAIL_RETURN(Status);

        InitializeObjectAttributes(&ObjectAttributes, &SubKeyString, OBJ_CASE_INSENSITIVE, RootKey, nullptr);
        Status = NtOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);

        CloseKeyHandle(RootKey);

        if (Retry == FALSE && Status == STATUS_OBJECT_NAME_NOT_FOUND && hKey == HKEY_CLASSES_ROOT)
        {
            Retry = TRUE;
            hKey = HKEY_MACHINE_CLASS;
            continue;
        }

        break;
    }

    return Status;
}

NTSTATUS
GetKeyValue(
    HANDLE                          hKey,
    PCWSTR                          SubKey,
    PCWSTR                          ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION* Value
)
{
    NTSTATUS    Status;
    ULONG       Length;
    PKEY_VALUE_PARTIAL_INFORMATION Buffer;

    Buffer = NULL;
    Length = 0;

    LOOP_FOREVER
    {
        Status = GetKeyValue(hKey, SubKey, ValueName, KeyValuePartialInformation, Buffer, Length, &Length, 0);
        if (Status != STATUS_BUFFER_TOO_SMALL)
            break;

        FreeKeyInfo(Buffer);
        Buffer = (PKEY_VALUE_PARTIAL_INFORMATION)AllocateKeyInfo(Length + sizeof(WCHAR));
        if (Buffer == NULL)
            return STATUS_NO_MEMORY;

        *(PWSTR)PtrAdd(Buffer, Length) = 0;
    }

    if (NT_FAILED(Status))
    {
        FreeKeyInfo(Buffer);
    }
    else
    {
        *Value = Buffer;
    }

    return Status;
}

NTSTATUS
GetKeyValue(
    HANDLE                      hKey,
    PCWSTR                      SubKey,
    PCWSTR                      ValueName,
    KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    PVOID                       KeyValueInformation,
    ULONG                       Length,
    PULONG                      ResultLength,
    ULONG                       Flags
)
{
    ULONG               ReturnLength;
    HANDLE              KeyHandle;
    NTSTATUS            Status;
    UNICODE_STRING      SubKeyString, ValueNameString;

    API_POINTER(ZwCreateKey)        XCreateKey;
    API_POINTER(ZwOpenKey)          XOpenKey;
    API_POINTER(ZwQueryValueKey)    XQueryValueKey;

#if ML_KERNEL_MODE

    XCreateKey      = ZwCreateKey;
    XOpenKey        = ZwOpenKey;
    XQueryValueKey  = ZwQueryValueKey;

#else

    XCreateKey      = NtCreateKey;
    XOpenKey        = NtOpenKey;
    XQueryValueKey  = NtQueryValueKey;

#endif

    Status = OpenKey(&KeyHandle, hKey, KEY_QUERY_VALUE, SubKey);
    FAIL_RETURN(Status);

    ReturnLength = 0;
    RtlInitUnicodeString(&ValueNameString, ValueName);
    Status = XQueryValueKey(
                KeyHandle,
                &ValueNameString,
                KeyValueInformationClass,
                KeyValueInformation,
                Length,
                &ReturnLength
             );

    if (KeyHandle != (HANDLE)hKey)
        CloseKeyHandle(KeyHandle);

    if ((NT_SUCCESS(Status) || Status == STATUS_BUFFER_TOO_SMALL) && ResultLength != NULL)
        *ResultLength = ReturnLength;

    return Status;
}







ML_NAMESPACE_END_(Reg);

ML_NAMESPACE_BEGIN(Exp);

NTSTATUS
ExceptionBox(
    PCWSTR      Text,
    PCWSTR      Title,
    PULONG_PTR  Response,
    ULONG_PTR   Type
)
{
    UNICODE_STRING  HardErrorTitle, HardErrorText;
    ULONG_PTR       HardErrorParameters[3];
    ULONG           LocalResponse;
    NTSTATUS        Status;

    RtlInitUnicodeString(&HardErrorTitle, Title);
    RtlInitUnicodeString(&HardErrorText, Text);

    HardErrorParameters[0] = (ULONG_PTR)&HardErrorText;
    HardErrorParameters[1] = (ULONG_PTR)&HardErrorTitle;
    HardErrorParameters[2] = Type;

#if ML_USER_MODE

    Status = NtRaiseHardError(STATUS_SERVICE_NOTIFICATION, countof(HardErrorParameters), 1 | 2, HardErrorParameters, 0, &LocalResponse);

#elif ML_KERNEL_MODE

    Status = ExRaiseHardError(STATUS_SERVICE_NOTIFICATION, countof(HardErrorParameters), 1 | 2, HardErrorParameters, 0, &LocalResponse);

#endif

    if (NT_SUCCESS(Status) && Response != NULL)
        *Response = LocalResponse;

    return Status;
}

ML_NAMESPACE_END_(Exp);

ML_NAMESPACE_BEGIN(Lpc);

#if ML_USER_MODE

/************************************************************************
  InterProcessLpcServer
************************************************************************/

#define VERIFY_IPC_TYPE(_type) if (this->IpcType != (_type)) return STATUS_OBJECT_TYPE_MISMATCH
#define VERIFY_IPC_TYPE_(_obj, _type) if ((_obj)->IpcType != (_type)) return STATUS_OBJECT_TYPE_MISMATCH

InterProcessLpcServer::InterProcessLpcServer()
{
    Reset();
}



VOID InterProcessLpcServer::Reset()
{
    IpcType = IPCType_None;
    PortHandle = nullptr;

    ZeroMemory(&Connected.ServerView, sizeof(Connected.ServerView));
    ZeroMemory(&Connected.ClientView, sizeof(Connected.ClientView));
}




/*++

  listening

--*/

IPC_STATUS
InterProcessLpcServer::
Create(
    PWSTR Name,
    ULONG MaxConnectionInfoLength,
    ULONG MaxMessageLength
)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    UNICODE_STRING      PortName;

    VERIFY_IPC_TYPE(IPCType_None);

    RtlInitUnicodeString(&PortName, Name);
    InitializeObjectAttributes(&ObjectAttributes, &PortName, 0, nullptr, nullptr);

    Status = NtCreatePort(&LpcPort, &ObjectAttributes, MaxConnectionInfoLength, MaxMessageLength, 0);
    FAIL_RETURN(Status);

    IpcType = IPCType_ServerListen;

    Listening.MaxConnectionInfoLength   = MaxConnectionInfoLength;
    Listening.MaxMessageLength          = MaxMessageLength;
    Listening.MaxSmallDataLength        = MaxMessageLength - sizeof(PORT_MESSAGE) - IPC_MESSAGE::kOverhead;

    return Status;
}








/*++

  listening end

--*/


/*++

  connected

--*/






/*++

  connected end

--*/


/************************************************************************
  InterProcessLpcClient
************************************************************************/

InterProcessLpcClient::InterProcessLpcClient()
{
    Reset();
}



VOID InterProcessLpcClient::Reset()
{
    IpcType = IPCType_None;
    ConnetionPort = nullptr;

    ZeroMemory(&ServerView, sizeof(ServerView));
    ZeroMemory(&ClientView, sizeof(ClientView));
}





#endif // r3

ML_NAMESPACE_END_(Lpc);
#include "MountManager.h"

ML_NAMESPACE_BEGIN(Io);









ULONG_PTR QueryFileAttributes(PCWSTR FileName)
{
    NTSTATUS    Status;
    ULONG_PTR   FileAttributes;

    Status = QueryFileAttributesEx(FileName, &FileAttributes);

    return NT_SUCCESS(Status) ? FileAttributes : INVALID_FILE_ATTRIBUTES;
}

NTSTATUS QueryFileAttributesEx(PCWSTR FileName, PULONG_PTR FileAttributes)
{
#if ML_USER_MODE

    NTSTATUS                Status;
    UNICODE_STRING          NtPath;
    OBJECT_ATTRIBUTES       ObjectAttributes;
    FILE_BASIC_INFORMATION  FileBasic;

    Status = NtFileDisk::QueryFullNtPath(FileName, &NtPath);
    FAIL_RETURN(Status);

    InitializeObjectAttributes(&ObjectAttributes, &NtPath, OBJ_CASE_INSENSITIVE, NULL, 0);
    Status = NtQueryAttributesFile(&ObjectAttributes, &FileBasic);
    RtlFreeUnicodeString(&NtPath);
    FAIL_RETURN(Status);

    *FileAttributes = FileBasic.FileAttributes;

    return Status;

#elif ML_KERNEL_MODE

    return STATUS_NOT_IMPLEMENTED;

#endif

}







NTSTATUS DeleteFile(PCWSTR FileName)
{
    HANDLE      FileHandle;
    NTSTATUS    Status;

    Status = NtFileDisk::Open(
                &FileHandle,
                FileName,
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DELETE,
                FILE_OPEN,
                FILE_ATTRIBUTE_NORMAL,
                FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT
             );
    if (NT_FAILED(Status))
        return Status;

    Status = NtFileDisk::Delete(FileHandle);
    NtFileDisk::Close(FileHandle);

    return Status;
}









NTSTATUS QueryDosDevice(PCWSTR DeviceName, PUNICODE_STRING TargetPath)
{
    ULONG               Length;
    NTSTATUS            Status;
    HANDLE              Directory, Link;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    UNICODE_STRING      Device, Target;
    PWSTR               TargetBuffer;

#if ML_USER_MODE

    Device = USTR(L"\\??");

    InitializeObjectAttributes(&ObjectAttributes, &Device, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = NtOpenDirectoryObject(&Directory, DIRECTORY_QUERY, &ObjectAttributes);
    FAIL_RETURN(Status);

    RtlInitUnicodeString(&Device, DeviceName);
    InitializeObjectAttributes(&ObjectAttributes, &Device, OBJ_CASE_INSENSITIVE, Directory, NULL);
    Status = NtOpenSymbolicLinkObject(&Link, SYMBOLIC_LINK_QUERY, &ObjectAttributes);
    NtClose(Directory);
    FAIL_RETURN(Status);

    RtlInitEmptyUnicodeString(&Target, NULL, 0);

    Status = NtQuerySymbolicLinkObject(Link, &Target, &Length);

    if (Status != STATUS_BUFFER_TOO_SMALL)
    {
        NtClose(Link);
        return Status;
    }

    Length += sizeof(WCHAR);
    TargetBuffer = (PWSTR)AllocStack(Length);
    RtlInitEmptyUnicodeString(&Target, TargetBuffer, Length);

    Status = NtQuerySymbolicLinkObject(Link, &Target, &Length);
    NtClose(Link);

    FAIL_RETURN(Status);

    Length = Target.Length / sizeof(WCHAR);
    if (Target.Buffer[Length - 1] != '\\')
    {
        Target.Buffer[Length++] = '\\';
        Target.Buffer[Length] = 0;
        Target.Length = (USHORT)(Length * sizeof(WCHAR));
    }

    return RtlDuplicateUnicodeString(RTL_DUPSTR_ADD_NULL, &Target, TargetPath);

#else

    Device = USTR(L"\\??");

    InitializeObjectAttributes(&ObjectAttributes, &Device, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = ZwOpenDirectoryObject(&Directory, DIRECTORY_QUERY, &ObjectAttributes);
    FAIL_RETURN(Status);

    RtlInitUnicodeString(&Device, DeviceName);
    InitializeObjectAttributes(&ObjectAttributes, &Device, OBJ_CASE_INSENSITIVE, Directory, NULL);
    Status = ZwOpenSymbolicLinkObject(&Link, SYMBOLIC_LINK_QUERY, &ObjectAttributes);
    ZwClose(Directory);
    FAIL_RETURN(Status);

    RtlInitEmptyUnicodeString(&Target, NULL, 0);

    Status = ZwQuerySymbolicLinkObject(Link, &Target, &Length);

    if (Status != STATUS_BUFFER_TOO_SMALL)
    {
        ZwClose(Link);
        return Status;
    }

    Length += sizeof(WCHAR);
    TargetBuffer = (PWSTR)AllocStack(Length);
    RtlInitEmptyUnicodeString(&Target, TargetBuffer, Length);

    Status = ZwQuerySymbolicLinkObject(Link, &Target, &Length);
    ZwClose(Link);

    FAIL_RETURN(Status);

    Length = Target.Length / sizeof(WCHAR);
    if (Target.Buffer[Length - 1] != '\\')
    {
        Target.Buffer[Length++] = '\\';
        Target.Buffer[Length] = 0;
        Target.Length = (USHORT)(Length * sizeof(WCHAR));
    }

    return RtlDuplicateUnicodeString(RTL_DUPSTR_ADD_NULL, &Target, TargetPath);

#endif
}

NTSTATUS
QueryDosPathFromNtDeviceName(
    PUNICODE_STRING DosPath,
    PUNICODE_STRING NtDeviceName
)
{
    NTSTATUS        Status;
    ULONG_PTR       Length;
    WCHAR           DosLetter[4];
    PWSTR           DosPathBuffer;
    UNICODE_STRING  DosDevice;

    *(PULONG)&DosLetter[0] = TAG2W('A:');
    *(PULONG)&DosLetter[2] = 0;

    for (ULONG_PTR Count = 'Z' - 'A' + 1; Count; ++DosLetter[0], --Count)
    {
        Status = QueryDosDevice(DosLetter, &DosDevice);
        FAIL_CONTINUE(Status);

        if (RtlCompareMemory(NtDeviceName->Buffer, DosDevice.Buffer, DosDevice.Length) !=  DosDevice.Length)
        {
            RtlFreeUnicodeString(&DosDevice);
            continue;
        }

        Length = NtDeviceName->Length - DosDevice.Length;
        DosPathBuffer = (PWSTR)AllocStack(Length + sizeof(DosLetter) + sizeof(WCHAR));
        *(PULONG)DosPathBuffer = *(PULONG)DosLetter;
        DosPathBuffer[2] = '\\';

        CopyMemory(DosPathBuffer + 3, PtrAdd(NtDeviceName->Buffer, DosDevice.Length), Length);
        *PtrAdd(DosPathBuffer, Length + 6) = 0;

        RtlFreeUnicodeString(&DosDevice);

        return RtlCreateUnicodeString(DosPath, DosPathBuffer) ? STATUS_SUCCESS : STATUS_NO_MEMORY;
    }

    return STATUS_OBJECT_PATH_NOT_FOUND;
}



ML_NAMESPACE_END_(Io);

ML_NAMESPACE_BEGIN(Ob)

#if ML_KERNEL_MODE

NTSTATUS
CreateObjectType(
    IN  PUNICODE_STRING             TypeName,
    IN  POBJECT_TYPE_INITIALIZER    ObjectTypeInitializer,
    OUT POBJECT_TYPE*               ObjectType
)
{
    NTSTATUS Status;

    *ObjectType = nullptr;

    Status = ObCreateObjectType(TypeName, ObjectTypeInitializer, nullptr, ObjectType);
    if (Status != STATUS_OBJECT_NAME_COLLISION)
        return Status;

    BYTE ObjectHack[0x100];
    POBJECT_TYPE Type;
    PVOID Object;
    ML_OBJECT_HEADER_DYNAMIC ObjectHeader;

    ZeroMemory(ObjectHack, sizeof(ObjectHack));

    Object = &ObjectHack[countof(ObjectHack) / 2];
    ObjectHeader = Object;

    for (ULONG_PTR TypeIndex = ML_OBJECT_TYPE(ObGetObjectType(*TmResourceManagerObjectType))->Index; TypeIndex != 0x100; ++TypeIndex)
    {
        ObjectHeader->TypeIndex = TypeIndex;
        Type = ObGetObjectType(Object);
        if (Type == nullptr || (ULONG_PTR)Type == 1)
            continue;

        if (RtlEqualUnicodeString(&ML_OBJECT_TYPE(Type)->Name, TypeName, TRUE) == FALSE)
            continue;

        *ObjectType = Type;

        return STATUS_SUCCESS;
    }

    return Status;
}

#endif

ML_NAMESPACE_END_(Ob);

ML_NAMESPACE_END_(Native);

ML_NAMESPACE_END;    // ml






NTSTATUS
Nt_AllocateMemory(
    HANDLE      ProcessHandle,
    PVOID*      BaseAddress,
    ULONG_PTR   Size,
    ULONG       Protect,        /* = PAGE_EXECUTE_READWRITE */
    ULONG       AllocationType  /* = MEM_RESERVE | MEM_COMMIT */
)
{
    return ml::Native::Mm::AllocVirtualMemory(BaseAddress, Size, Protect, AllocationType, ProcessHandle);
}

NTSTATUS
Nt_FreeMemory(
    HANDLE  ProcessHandle,
    PVOID   BaseAddress
)
{
    return ml::Native::Mm::FreeVirtualMemory(BaseAddress, ProcessHandle);
}

#endif // CPP_DEFINED

    ML_NAMESPACE

    LONG_PTR MlInitialize()
    {
        if (MemoryAllocator::GetGlobalHeap() == NULL && MemoryAllocator::CreateGlobalHeap() == nullptr)
            return STATUS_NO_MEMORY;

        return STATUS_SUCCESS;
    }

    LONG_PTR MlUnInitialize()
    {
        MemoryAllocator::DestroyGlobalHeap();

        return 0;
    }

    MY_NAMESPACE_END

#endif
