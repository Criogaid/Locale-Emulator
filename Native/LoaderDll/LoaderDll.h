#ifndef _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_
#define _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_

#include "../LocaleEmulator/LocaleEmulator.h"

typedef struct LE_PROCESS_INFORMATION : public PROCESS_INFORMATION
{
    PVOID FirstCallLdrLoadDll;
} LE_PROCESS_INFORMATION, *PLE_PROCESS_INFORMATION;
#include "../RuntimeEnvironment.h"

#ifndef LE_API
#define LE_API EXTC
 #endif
#if !LE_LOADER_DLL
#undef LE_API
#define LE_API EXTC __declspec(dllimport)
#endif

LE_API
NTSTATUS
WINAPI
LeCreateProcess(
    PLE_RUNTIME_INPUT       Environment,
    PCWSTR                  ApplicationName,
    PWSTR                   CommandLine = nullptr,
    PCWSTR                  CurrentDirectory = nullptr,
    ULONG                   CreationFlags = 0,
    LPSTARTUPINFOW          StartupInfo = nullptr,
    PLE_PROCESS_INFORMATION ProcessInformation = nullptr,
    LPSECURITY_ATTRIBUTES   ProcessAttributes = nullptr,
    LPSECURITY_ATTRIBUTES   ThreadAttributes = nullptr,
    PVOID                   EnvironmentBlock = nullptr,
    HANDLE                  Token = nullptr
    );

#endif // _LOADERDLL_H_586bc656_348b_4b12_ba74_d39366b67f23_
