#ifndef _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_
#define _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_

#include "../LocaleEmulatorPlus/LocaleEmulatorPlus.h"
#include "../../RuntimeEnvironment.h"

#define LEP_API EXTC

#if !LEP_LOADER_DLL
#undef LEP_API
#define LEP_API EXTC __declspec(dllimport)
#endif

LEP_API
NTSTATUS
WINAPI
LeCreateProcess(
    PLE_RUNTIME_INPUT       Environment,
    PCWSTR                  ApplicationName,
    PWSTR                   CommandLine = nullptr,
    PCWSTR                  CurrentDirectory = nullptr,
    ULONG                   CreationFlags = 0,
    LPSTARTUPINFOW          StartupInfo = nullptr,
    LPPROCESS_INFORMATION   ProcessInformation = nullptr,
    LPSECURITY_ATTRIBUTES   ProcessAttributes = nullptr,
    LPSECURITY_ATTRIBUTES   ThreadAttributes = nullptr,
    PVOID                   EnvironmentBlock = nullptr,
    HANDLE                  Token = nullptr
    );

#endif // _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_
