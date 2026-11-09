#include "stdafx.h"

namespace
{
    struct RegistryTimeZone
    {
        LONG Bias;
        LONG StandardBias;
        LONG DaylightBias;
        SYSTEMTIME StandardDate;
        SYSTEMTIME DaylightDate;
    };

    BOOL ValidTimezoneId(PCWSTR Id)
    {
        if (Id == nullptr || Id[0] == 0)
            return FALSE;
        for (PCWSTR Current = Id; *Current != 0; ++Current)
        {
            if (*Current == L'\\' || *Current == L'/')
                return FALSE;
        }
        return TRUE;
    }

    NTSTATUS OpenTimeZone(PCWSTR Id, HANDLE* Key)
    {
        static const WCHAR Prefix[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\";
        WCHAR Path[countof(Prefix) + 128];
        UNICODE_STRING Name;
        OBJECT_ATTRIBUTES Attributes;
        ULONG Length = (ULONG)StrLengthW(Id);

        if (Length >= 128)
            return STATUS_NAME_TOO_LONG;
        CopyMemory(Path, Prefix, sizeof(Prefix) - sizeof(WCHAR));
        CopyMemory(Path + countof(Prefix) - 1, Id, (Length + 1) * sizeof(WCHAR));
        RtlInitUnicodeString(&Name, Path);
        InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        return NtOpenKey(Key, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &Attributes);
    }

    NTSTATUS ReadValue(HANDLE Key, PCWSTR Name, ULONG Type, PVOID Data, ULONG Capacity, PULONG Length)
    {
        ULONG Storage[140];
        PKEY_VALUE_PARTIAL_INFORMATION Value = (PKEY_VALUE_PARTIAL_INFORMATION)Storage;
        UNICODE_STRING ValueName;
        ULONG ResultLength = 0;
        ULONG Header = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
        NTSTATUS Status;

        RtlInitUnicodeString(&ValueName, Name);
        Status = NtQueryValueKey(Key, &ValueName, KeyValuePartialInformation,
            Value, sizeof(Storage), &ResultLength);
        if (NT_FAILED(Status))
            return Status;
        if (ResultLength < Header || Value->DataLength > ResultLength - Header)
            return STATUS_INFO_LENGTH_MISMATCH;
        if (Value->Type != Type || Value->DataLength > Capacity)
            return STATUS_OBJECT_TYPE_MISMATCH;
        CopyMemory(Data, Value->Data, Value->DataLength);
        *Length = Value->DataLength;
        return STATUS_SUCCESS;
    }

    NTSTATUS ReadName(HANDLE Key, PCWSTR ValueName, WCHAR (&Name)[32])
    {
        ULONG Length = 0;
        ZeroMemory(Name, sizeof(Name));
        NTSTATUS Status = ReadValue(Key, ValueName, REG_SZ, Name, sizeof(Name), &Length);
        if (NT_FAILED(Status))
            return Status;
        if ((Length & (sizeof(WCHAR) - 1)) != 0)
            return STATUS_INVALID_PARAMETER;
        for (ULONG Index = 0; Index != countof(Name); ++Index)
        {
            if (Name[Index] == 0)
                return STATUS_SUCCESS;
        }
        return STATUS_INVALID_PARAMETER;
    }

    LE_RUNTIME_TIME_FIELDS ConvertTransition(const SYSTEMTIME& Date)
    {
        LE_RUNTIME_TIME_FIELDS Fields = {};
        Fields.Year = (SHORT)Date.wYear;
        Fields.Month = (SHORT)Date.wMonth;
        Fields.Day = (SHORT)Date.wDay;
        Fields.Hour = (SHORT)Date.wHour;
        Fields.Minute = (SHORT)Date.wMinute;
        Fields.Second = (SHORT)Date.wSecond;
        Fields.Milliseconds = (SHORT)Date.wMilliseconds;
        Fields.Weekday = (SHORT)Date.wDayOfWeek;
        return Fields;
    }
}

NTSTATUS ResolveRuntimeTimeZone(PCWSTR Id, PLE_RUNTIME_TIME_ZONE_INFORMATION Zone)
{
    HANDLE Key = nullptr;
    RegistryTimeZone Tzi = {};
    ULONG Length = 0;
    NTSTATUS Status;

    if (!ValidTimezoneId(Id))
        return STATUS_INVALID_PARAMETER;
    Status = OpenTimeZone(Id, &Key);
    if (NT_FAILED(Status))
        return Status;

    Status = ReadValue(Key, L"TZI", REG_BINARY, &Tzi, sizeof(Tzi), &Length);
    if (NT_SUCCESS(Status) && Length != sizeof(Tzi))
        Status = STATUS_INFO_LENGTH_MISMATCH;
    if (NT_SUCCESS(Status))
        Status = ReadName(Key, L"Std", Zone->StandardName);
    if (NT_SUCCESS(Status))
        Status = ReadName(Key, L"Dlt", Zone->DaylightName);
    NtClose(Key);
    if (NT_FAILED(Status))
        return Status;

    Zone->Bias = Tzi.Bias;
    Zone->StandardBias = Tzi.StandardBias;
    Zone->DaylightBias = Tzi.DaylightBias;
    Zone->StandardStart = ConvertTransition(Tzi.StandardDate);
    Zone->DaylightStart = ConvertTransition(Tzi.DaylightDate);
    return STATUS_SUCCESS;
}
