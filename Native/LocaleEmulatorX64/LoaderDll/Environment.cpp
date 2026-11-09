#include "Environment.h"

namespace
{
    bool StringTerminated(PCWSTR String, ULONG Capacity)
    {
        for (ULONG Index = 0; Index != Capacity; ++Index)
            if (String[Index] == 0)
                return true;
        return false;
    }

    struct RegistryTimeZone
    {
        LONG Bias;
        LONG StandardBias;
        LONG DaylightBias;
        SYSTEMTIME StandardDate;
        SYSTEMTIME DaylightDate;
    };
    static_assert(sizeof(RegistryTimeZone) == 44, "Registry TZI layout mismatch");

    bool ValidTimezoneId(PCWSTR Id, ULONG Capacity)
    {
        if (Id[0] == 0)
            return false;
        for (ULONG Index = 0; Index != Capacity; ++Index)
        {
            if (Id[Index] == 0)
                return true;
            if (Id[Index] == L'\\' || Id[Index] == L'/')
                return false;
        }
        return false;
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
        auto Value = (PKEY_VALUE_PARTIAL_INFORMATION)Storage;
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

    NTSTATUS ReadName(HANDLE Key, PCWSTR ValueName, WCHAR (&Name)[32])
    {
        ULONG Length = 0;
        NTSTATUS Status;

        ZeroMemory(Name, sizeof(Name));
        Status = ReadValue(Key, ValueName, REG_SZ, Name, sizeof(Name), &Length);
        if (NT_FAILED(Status))
            return Status;
        if ((Length & (sizeof(WCHAR) - 1)) != 0 || !StringTerminated(Name, countof(Name)))
            return STATUS_INVALID_PARAMETER;
        return STATUS_SUCCESS;
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

    void NormalizeTokyoName(PLE_RUNTIME_TIME_ZONE_INFORMATION Zone)
    {
        static const WCHAR Name[] = L"东京标准时";
        ZeroMemory(Zone->StandardName, sizeof(Zone->StandardName));
        ZeroMemory(Zone->DaylightName, sizeof(Zone->DaylightName));
        CopyMemory(Zone->StandardName, Name, sizeof(Name));
        CopyMemory(Zone->DaylightName, Name, sizeof(Name));
    }

    void CopyBoundedName(PWSTR Destination, ULONG Capacity, PCWSTR Source, ULONG Bytes)
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

    NTSTATUS ResolveTimeZone(
        PCWSTR Id,
        PLE_RUNTIME_TIME_ZONE_INFORMATION Zone,
        PCWSTR StandardNameOverride,
        ULONG StandardNameBytes,
        PCWSTR DaylightNameOverride,
        ULONG DaylightNameBytes)
    {
        HANDLE Key = nullptr;
        RegistryTimeZone Tzi = {};
        ULONG Length = 0;
        NTSTATUS Status = OpenTimeZone(Id, &Key);
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
        if (StrICompareW(Id, L"Tokyo Standard Time") == 0)
            NormalizeTokyoName(Zone);
        if (StandardNameBytes != 0)
            CopyBoundedName(Zone->StandardName, countof(Zone->StandardName), StandardNameOverride, StandardNameBytes);
        if (DaylightNameBytes != 0)
            CopyBoundedName(Zone->DaylightName, countof(Zone->DaylightName), DaylightNameOverride, DaylightNameBytes);
        return STATUS_SUCCESS;
    }
}

NTSTATUS LepResolveEnvironment(const void* Input, PLE_RUNTIME_CONFIG Output)
{
    const BYTE* Base = (const BYTE*)Input;
    const LE_RUNTIME_INPUT* Environment;
    LE_RUNTIME_CONFIG Resolved = {};
    NTSTATUS Status;

    if (Output == nullptr || !LeValidateRuntimeInput(Input))
        return STATUS_INVALID_PARAMETER;

    Environment = (const LE_RUNTIME_INPUT*)Input;
    if (!ValidTimezoneId((PCWSTR)(Base + Environment->TimezoneIdOffset),
        Environment->TimezoneIdLength / sizeof(WCHAR) + 1))
        return STATUS_INVALID_PARAMETER;
    Resolved.Size = sizeof(Resolved);
    Resolved.Version = LE_RUNTIME_CONFIG_VERSION;
    Resolved.AnsiCodePage = Environment->AnsiCodePage;
    Resolved.OemCodePage = Environment->OemCodePage;
    Resolved.LocaleID = Environment->LocaleID;
    Resolved.FontCharset = Environment->FontCharset;
    Resolved.RegistryMode = Environment->RegistryMode;
    Resolved.HookUILanguageMode = Environment->HookUILanguageMode;

    Status = ResolveTimeZone(
        (PCWSTR)(Base + Environment->TimezoneIdOffset),
        &Resolved.Timezone,
        (PCWSTR)(Base + Environment->StandardNameOffset),
        Environment->StandardNameLength,
        (PCWSTR)(Base + Environment->DaylightNameOffset),
        Environment->DaylightNameLength);
    if (NT_FAILED(Status))
        return Status;

    if (!LeValidateRuntimeConfig(&Resolved))
        return STATUS_INVALID_PARAMETER;
    *Output = Resolved;
    return STATUS_SUCCESS;
}
