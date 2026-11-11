#include "stdafx.h"
#include "../TimeZoneResolver.h"

NTSTATUS ResolveRuntimeTimeZone(
    PCWSTR Id,
    PCWSTR StandardNameOverride,
    PCWSTR DaylightNameOverride,
    PLE_RUNTIME_TIME_ZONE_INFORMATION Zone)
{
    LeTimeZone::NameOptions Names = {
        StandardNameOverride, (ULONG)StrLengthW(StandardNameOverride) * sizeof(WCHAR),
        DaylightNameOverride, (ULONG)StrLengthW(DaylightNameOverride) * sizeof(WCHAR) };
    return LeResolveTimeZone(Id, Zone, Names);
}
