#include "Environment.h"
#include "../../TimeZoneResolver.h"

NTSTATUS LepResolveEnvironment(const void* Input, PLE_RUNTIME_CONFIG Output)
{
    const BYTE* Base = (const BYTE*)Input;
    const LE_RUNTIME_INPUT* Environment;
    LE_RUNTIME_CONFIG Resolved = {};
    NTSTATUS Status;

    if (Output == nullptr || !LeValidateRuntimeInput(Input))
        return STATUS_INVALID_PARAMETER;

    Environment = (const LE_RUNTIME_INPUT*)Input;
    Resolved.Size = sizeof(Resolved);
    Resolved.Version = LE_RUNTIME_CONFIG_VERSION;
    Resolved.AnsiCodePage = Environment->AnsiCodePage;
    Resolved.OemCodePage = Environment->OemCodePage;
    Resolved.LocaleID = Environment->LocaleID;
    Resolved.FontCharset = Environment->FontCharset;
    Resolved.RegistryMode = Environment->RegistryMode;
    Resolved.HookUILanguageMode = Environment->HookUILanguageMode;

    LeTimeZone::NameOptions Names = {
        (PCWSTR)(Base + Environment->StandardNameOffset), Environment->StandardNameLength,
        (PCWSTR)(Base + Environment->DaylightNameOffset), Environment->DaylightNameLength };
    Status = LeResolveTimeZone((PCWSTR)(Base + Environment->TimezoneIdOffset), &Resolved.Timezone, Names);
    if (NT_FAILED(Status))
        return Status;

    if (!LeValidateRuntimeConfig(&Resolved))
        return STATUS_INVALID_PARAMETER;
    *Output = Resolved;
    return STATUS_SUCCESS;
}
