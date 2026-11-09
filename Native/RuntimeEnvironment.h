#ifndef LOCALE_EMULATOR_RUNTIME_ENVIRONMENT_H
#define LOCALE_EMULATOR_RUNTIME_ENVIRONMENT_H

#include <Windows.h>

#define LE_RUNTIME_INPUT_VERSION 2u
#define LE_RUNTIME_CONFIG_VERSION 1u
#define LE_RUNTIME_MAX_SIZE (1024u * 1024u)
#define LE_RUNTIME_MAX_STRING_BYTES (32768u)

#define LE_RUNTIME_REGISTRY_MODE_NONE 0u
#define LE_RUNTIME_REGISTRY_MODE_STANDARD 1u
#define LE_RUNTIME_REGISTRY_MODE_ADVANCED 2u
#define LE_RUNTIME_UI_LANGUAGE_MODE_NONE 0u
#define LE_RUNTIME_UI_LANGUAGE_MODE_STANDARD 1u
#define LE_RUNTIME_UI_LANGUAGE_MODE_ADVANCED 2u

#pragma pack(push, 4)

typedef struct _LE_RUNTIME_INPUT
{
    ULONG Size;
    ULONG Version;
    ULONG AnsiCodePage;
    ULONG OemCodePage;
    ULONG LocaleID;
    ULONG FontCharset;
    ULONG RegistryMode;
    ULONG HookUILanguageMode;
    ULONG TimezoneIdOffset;
    ULONG TimezoneIdLength;
    ULONG StandardNameOffset;
    ULONG StandardNameLength;
    ULONG DaylightNameOffset;
    ULONG DaylightNameLength;
} LE_RUNTIME_INPUT, *PLE_RUNTIME_INPUT;

typedef struct _LE_RUNTIME_TIME_FIELDS
{
    SHORT Year;
    SHORT Month;
    SHORT Day;
    SHORT Hour;
    SHORT Minute;
    SHORT Second;
    SHORT Milliseconds;
    SHORT Weekday;
} LE_RUNTIME_TIME_FIELDS, *PLE_RUNTIME_TIME_FIELDS;

typedef struct _LE_RUNTIME_TIME_ZONE_INFORMATION
{
    LONG Bias;
    WCHAR StandardName[32];
    LE_RUNTIME_TIME_FIELDS StandardStart;
    LONG StandardBias;
    WCHAR DaylightName[32];
    LE_RUNTIME_TIME_FIELDS DaylightStart;
    LONG DaylightBias;
} LE_RUNTIME_TIME_ZONE_INFORMATION, *PLE_RUNTIME_TIME_ZONE_INFORMATION;

// Fixed representation used after the Loader validates and resolves input.
typedef struct _LE_RUNTIME_CONFIG
{
    ULONG Size;
    ULONG Version;
    ULONG AnsiCodePage;
    ULONG OemCodePage;
    ULONG LocaleID;
    ULONG FontCharset;
    ULONG RegistryMode;
    ULONG HookUILanguageMode;
    LE_RUNTIME_TIME_ZONE_INFORMATION Timezone;
} LE_RUNTIME_CONFIG, *PLE_RUNTIME_CONFIG;

typedef const LE_RUNTIME_CONFIG* PCLE_RUNTIME_CONFIG;

#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(LE_RUNTIME_INPUT) == 56, "LE runtime input ABI size mismatch");
static_assert(sizeof(LE_RUNTIME_TIME_FIELDS) == 16, "LE runtime time fields ABI size mismatch");
static_assert(sizeof(LE_RUNTIME_TIME_ZONE_INFORMATION) == 172, "LE runtime time zone ABI size mismatch");
static_assert(sizeof(LE_RUNTIME_CONFIG) == 204, "LE runtime config ABI size mismatch");
#endif

static __inline BOOL LeRuntimeRangeValid(ULONG Offset, ULONG Length, ULONG TotalSize)
{
    if (Length == 0 && Offset == 0)
        return TRUE;
    return Offset >= sizeof(LE_RUNTIME_INPUT) &&
        Offset <= TotalSize &&
        Length <= TotalSize - Offset;
}

static __inline BOOL LeRuntimeStringValid(
    const BYTE* Base,
    ULONG Offset,
    ULONG Length,
    ULONG TotalSize,
    BOOL AllowEmpty)
{
    const WCHAR* Value;

    if ((Length & (sizeof(WCHAR) - 1)) != 0 || Length > LE_RUNTIME_MAX_STRING_BYTES ||
        (!AllowEmpty && Length == 0) ||
        !LeRuntimeRangeValid(Offset, Length + sizeof(WCHAR), TotalSize))
    {
        return FALSE;
    }

    Value = (const WCHAR*)(Base + Offset);
    if (Value[Length / sizeof(WCHAR)] != 0)
        return FALSE;

    for (ULONG Index = 0; Index < Length / sizeof(WCHAR); ++Index)
    {
        if (Value[Index] == 0)
            return FALSE;
    }
    return TRUE;
}

static __inline BOOL LeValidateRuntimeInput(const void* Buffer)
{
    const BYTE* Base = (const BYTE*)Buffer;
    const LE_RUNTIME_INPUT* Input;

    if (Buffer == NULL)
        return FALSE;

    Input = (const LE_RUNTIME_INPUT*)Buffer;
    if (Input->Size < sizeof(*Input) || Input->Size > LE_RUNTIME_MAX_SIZE ||
        Input->Version != LE_RUNTIME_INPUT_VERSION ||
        Input->AnsiCodePage > 0xFFFFu || Input->OemCodePage > 0xFFFFu ||
        Input->FontCharset > MAXBYTE ||
        Input->RegistryMode > LE_RUNTIME_REGISTRY_MODE_ADVANCED ||
        Input->HookUILanguageMode > LE_RUNTIME_UI_LANGUAGE_MODE_ADVANCED)
    {
        return FALSE;
    }

    if (!LeRuntimeStringValid(Base, Input->TimezoneIdOffset, Input->TimezoneIdLength, Input->Size, FALSE) ||
        !LeRuntimeStringValid(Base, Input->StandardNameOffset, Input->StandardNameLength, Input->Size, TRUE) ||
        !LeRuntimeStringValid(Base, Input->DaylightNameOffset, Input->DaylightNameLength, Input->Size, TRUE))
    {
        return FALSE;
    }

    return TRUE;
}

static __inline BOOL LeValidateRuntimeConfig(PCLE_RUNTIME_CONFIG Config)
{
    if (Config == NULL || Config->Size != sizeof(*Config) || Config->Version != LE_RUNTIME_CONFIG_VERSION ||
        Config->AnsiCodePage > 0xFFFFu || Config->OemCodePage > 0xFFFFu ||
        Config->FontCharset > MAXBYTE ||
        Config->RegistryMode > LE_RUNTIME_REGISTRY_MODE_ADVANCED ||
        Config->HookUILanguageMode > LE_RUNTIME_UI_LANGUAGE_MODE_ADVANCED)
    {
        return FALSE;
    }

    for (ULONG Index = 0; Index < ARRAYSIZE(Config->Timezone.StandardName); ++Index)
    {
        if (Config->Timezone.StandardName[Index] == 0)
            break;
        if (Index + 1 == ARRAYSIZE(Config->Timezone.StandardName))
            return FALSE;
    }
    for (ULONG Index = 0; Index < ARRAYSIZE(Config->Timezone.DaylightName); ++Index)
    {
        if (Config->Timezone.DaylightName[Index] == 0)
            break;
        if (Index + 1 == ARRAYSIZE(Config->Timezone.DaylightName))
            return FALSE;
    }
    return TRUE;
}

#endif // LOCALE_EMULATOR_RUNTIME_ENVIRONMENT_H
