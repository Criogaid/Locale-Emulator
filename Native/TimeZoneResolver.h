#ifndef LOCALE_EMULATOR_TIME_ZONE_RESOLVER_H
#define LOCALE_EMULATOR_TIME_ZONE_RESOLVER_H

// Shared by the x86 Core and the x64 Loader. Include after RuntimeEnvironment.h and
// the architecture's NT helper layer (NtOpenKey, NtQueryValueKey, StrLengthW,
// StrICompareW, countof, CopyMemory, ZeroMemory, NT_FAILED/NT_SUCCESS).
// Where the resolution runs (x86: in the target process, x64: in the Loader) is an
// architecture decision; the registry parsing and name rules below are not.

namespace LeTimeZone
{
    struct RegistryTimeZone
    {
        LONG Bias;
        LONG StandardBias;
        LONG DaylightBias;
        SYSTEMTIME StandardDate;
        SYSTEMTIME DaylightDate;
    };
    static_assert(sizeof(RegistryTimeZone) == 44, "Registry TZI layout mismatch");

    inline BOOL ValidId(PCWSTR Id)
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

    inline NTSTATUS Open(PCWSTR Id, HANDLE* Key)
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

    inline NTSTATUS ReadValue(HANDLE Key, PCWSTR Name, ULONG Type, PVOID Data, ULONG Capacity, PULONG Length)
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
        if (Value->Type != Type)
            return STATUS_OBJECT_TYPE_MISMATCH;
        if (Value->DataLength > Capacity)
            return STATUS_BUFFER_TOO_SMALL;
        CopyMemory(Data, Value->Data, Value->DataLength);
        *Length = Value->DataLength;
        return STATUS_SUCCESS;
    }

    inline NTSTATUS ReadName(HANDLE Key, PCWSTR ValueName, WCHAR (&Name)[32])
    {
        ULONG Length = 0;
        NTSTATUS Status;

        ZeroMemory(Name, sizeof(Name));
        Status = ReadValue(Key, ValueName, REG_SZ, Name, sizeof(Name), &Length);
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

    inline LE_RUNTIME_TIME_FIELDS ConvertTransition(const SYSTEMTIME& Date)
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

    inline VOID CopyBoundedName(PWSTR Destination, ULONG Capacity, PCWSTR Source, ULONG Bytes)
    {
        ULONG Characters = Bytes / sizeof(WCHAR);
        ULONG Limit = Capacity - 1;

        if (Characters > Limit)
            Characters = Limit;
        if (Characters != 0 && Source[Characters - 1] >= 0xD800 && Source[Characters - 1] <= 0xDBFF)
            --Characters;
        ZeroMemory(Destination, Capacity * sizeof(WCHAR));
        CopyMemory(Destination, Source, Characters * sizeof(WCHAR));
    }

    // Display names come from the registry unless the caller supplies explicit
    // overrides (resolved by the launcher for the emulated locale).
    struct NameOptions
    {
        PCWSTR StandardOverride;
        ULONG StandardOverrideBytes;
        PCWSTR DaylightOverride;
        ULONG DaylightOverrideBytes;
    };
}

inline NTSTATUS LeResolveTimeZone(
    PCWSTR Id,
    PLE_RUNTIME_TIME_ZONE_INFORMATION Zone,
    const LeTimeZone::NameOptions& Options)
{
    HANDLE Key = nullptr;
    LeTimeZone::RegistryTimeZone Tzi = {};
    ULONG Length = 0;
    NTSTATUS Status;

    if (!LeTimeZone::ValidId(Id))
        return STATUS_INVALID_PARAMETER;
    Status = LeTimeZone::Open(Id, &Key);
    if (NT_FAILED(Status))
        return Status;

    Status = LeTimeZone::ReadValue(Key, L"TZI", REG_BINARY, &Tzi, sizeof(Tzi), &Length);
    if (NT_SUCCESS(Status) && Length != sizeof(Tzi))
        Status = STATUS_INFO_LENGTH_MISMATCH;
    if (NT_SUCCESS(Status))
        Status = LeTimeZone::ReadName(Key, L"Std", Zone->StandardName);
    if (NT_SUCCESS(Status))
        Status = LeTimeZone::ReadName(Key, L"Dlt", Zone->DaylightName);
    NtClose(Key);
    if (NT_FAILED(Status))
        return Status;

    Zone->Bias = Tzi.Bias;
    Zone->StandardBias = Tzi.StandardBias;
    Zone->DaylightBias = Tzi.DaylightBias;
    Zone->StandardStart = LeTimeZone::ConvertTransition(Tzi.StandardDate);
    Zone->DaylightStart = LeTimeZone::ConvertTransition(Tzi.DaylightDate);
    if (Options.StandardOverrideBytes != 0)
        LeTimeZone::CopyBoundedName(Zone->StandardName, countof(Zone->StandardName),
            Options.StandardOverride, Options.StandardOverrideBytes);
    if (Options.DaylightOverrideBytes != 0)
        LeTimeZone::CopyBoundedName(Zone->DaylightName, countof(Zone->DaylightName),
            Options.DaylightOverride, Options.DaylightOverrideBytes);
    return STATUS_SUCCESS;
}

#endif // LOCALE_EMULATOR_TIME_ZONE_RESOLVER_H
