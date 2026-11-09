#pragma once

#include "../LocaleEmulatorPlus/support/Support.h"
#include "../../RuntimeEnvironment.h"

// Loader validates the public block before this function is called.  The
// resolver converts it once into the fixed representation consumed by Core.
NTSTATUS LepResolveEnvironment(const void* Input, PLE_RUNTIME_CONFIG Output);
