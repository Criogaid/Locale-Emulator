param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$nativeRoot = Join-Path $repoRoot 'Native\LocaleEmulatorX64'
$coreRoot = Join-Path $nativeRoot 'LocaleEmulatorPlus'
$loaderRoot = Join-Path $nativeRoot 'LoaderDll'
$compatRoot = Join-Path $nativeRoot 'compat'
$dependencyRoot = Join-Path $nativeRoot 'dep\libs'
$outputRoot = Join-Path $repoRoot 'Build\Dual\x64'
$objectRoot = Join-Path $outputRoot 'obj'
$libraryRoot = Join-Path $outputRoot 'lib'
$binaryRoot = Join-Path $outputRoot 'bin'

if ($Clean -and (Test-Path $outputRoot)) {
    Remove-Item -Recurse -Force $outputRoot
}
New-Item -ItemType Directory -Force $objectRoot, $libraryRoot, $binaryRoot | Out-Null

# Toolchain discovery is shared with the other x64 native build scripts.
. (Join-Path $PSScriptRoot '../Toolchain.ps1')
$toolchain = Get-OfficialToolchain
$msvcRoot = $toolchain.MsvcRoot
$sdkRoot = $toolchain.SdkRoot
$sdkVersion = $toolchain.SdkVersion
$hostBin = $toolchain.HostBin
$compiler = Join-Path $hostBin 'cl.exe'
$librarian = Join-Path $hostBin 'lib.exe'
$linker = Join-Path $hostBin 'link.exe'

$required = @(
    $compiler,
    $librarian,
    $linker,
    (Join-Path $compatRoot 'ntnls.h'),
    (Join-Path $compatRoot 'mountmgr.h'),
    (Join-Path $dependencyRoot 'kernel32_lep_x64.def'),
    (Join-Path $dependencyRoot 'lep_ntdll_x64.def'),
    (Join-Path $dependencyRoot 'lep_k32_x64.def')
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missing.Count -ne 0) {
    throw "缺少 x64 构建依赖：`n$($missing -join "`n")"
}

$env:Path = "$hostBin;$env:Path"

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$File,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & $File @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "命令失败（$LASTEXITCODE）：$File $($Arguments -join ' ')"
    }
}

$includeArguments = @(
    "/I$compatRoot",
    "/I$coreRoot",
    "/I$loaderRoot",
    "/I$(Join-Path $msvcRoot 'include')",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\ucrt")",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\shared")",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\um")",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\km")"
)
$commonCompileArguments = @(
    '/nologo', '/utf-8', '/c', '/O2', '/Ob1', '/GF', '/Gy', '/GR-',
    '/EHs-c-', '/GS', '/W4', '/WX', '/DWIN64', '/D_WIN64', '/DNDEBUG',
    '/DENABLE_LOG=0', '/DLEP_DIAG_INIT=0', '/DLEP_DIAG_INPUT=0',
    '/D_NO_CRT_STDIO_INLINE', '/Zc:wchar_t'
) + $includeArguments

function Get-ObjectName {
    param([Parameter(Mandatory = $true)][string]$SourcePath)

    $relative = $SourcePath.Substring($coreRoot.Length + 1)
    return ($relative.Replace('\', '_').Replace('/', '_') -replace '\.cpp$', '.obj')
}

function Compile-Source {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [string[]]$ExtraArguments = @(),
        [string]$ObjectName
    )

    if ([string]::IsNullOrEmpty($ObjectName)) {
        $ObjectName = Get-ObjectName $SourcePath
    }
    $objectPath = Join-Path $objectRoot $ObjectName
    $arguments = $commonCompileArguments + $ExtraArguments + @("/Fo$objectPath", $SourcePath)
    Invoke-Native $compiler $arguments | Out-Null
    return $objectPath
}

Write-Host '[x64 1/5] 生成官方 import libraries'
foreach ($definition in @('lep_ntdll_x64', 'lep_k32_x64', 'kernel32_lep_x64')) {
    Invoke-Native $librarian @(
        '/nologo', '/machine:x64',
        "/def:$(Join-Path $dependencyRoot "$definition.def")",
        "/out:$(Join-Path $libraryRoot "$definition.lib")"
    )
}

Write-Host '[x64 2/5] 编译 native support'
$supportObjects = @()
foreach ($directory in @('runtime', 'native', 'pe', 'patch')) {
    $supportSources = Get-ChildItem (Join-Path $coreRoot "support\$directory") -Filter '*.cpp' -File | Sort-Object FullName
    foreach ($source in $supportSources) {
        $supportObjects += Compile-Source $source.FullName -ObjectName "Support_${directory}_$($source.BaseName).obj"
    }
}

$delayObject = Compile-Source (Join-Path $dependencyRoot 'LepDelayLoad_x86_x64.cpp') -ObjectName 'LepDelayLoad.obj'
$crtObject = Compile-Source (Join-Path $dependencyRoot 'LepCrtShim_x64.cpp') -ExtraArguments @('/Oi-') -ObjectName 'LepCrtShim.obj'

Write-Host '[x64 3/5] 编译 LocaleEmulator core'
$coreSources = @(
    'HandleTable.cpp',
    'HookPort.cpp',
    'Hooks\Gdi32Hook.cpp',
    'Hooks\Kernel32Hook.cpp',
    'Hooks\NtdllHook.cpp',
    'Hooks\User32Hook.cpp',
    'Hooks\User32Environment.cpp',
    'LocaleEmulatorPlus.cpp',
    'stdafx.cpp',
    'Utility\Utility.cpp'
)
$coreObjects = @()
foreach ($relativeSource in $coreSources) {
    $sourcePath = Join-Path $coreRoot $relativeSource
    $coreObjects += Compile-Source $sourcePath
}

$libraryArguments = @(
    "/libpath:$(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")",
    "/libpath:$(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64")",
    "/libpath:$(Join-Path $msvcRoot 'lib\x64')",
    "/libpath:$libraryRoot"
)
$systemLibraries = @(
    'kernel32_lep_x64.lib', 'lep_ntdll_x64.lib', 'lep_k32_x64.lib',
    'ntdll.lib', 'user32.lib', 'gdi32.lib', 'dbghelp.lib',
    'libcmt.lib', 'oldnames.lib', 'libvcruntime.lib'
)
$coreLinkArguments = @(
    '/nologo', '/dll', '/nodefaultlib', '/debug:none', '/opt:ref',
    '/manifest:no', '/machine:x64', '/subsystem:windows', '/dynamicbase',
    '/nxcompat', '/entry:DllMain', "/out:$(Join-Path $binaryRoot 'LocaleEmulator.dll')",
    "/implib:$(Join-Path $libraryRoot 'LocaleEmulator.lib')", '/export:LoadFirstDll',
    '/delayload:KERNEL32_LEP.dll', '/delayload:USER32.dll',
    '/delayload:GDI32.dll', '/delayload:DBGHELP.dll'
) + $libraryArguments + ($supportObjects + $coreObjects + @($delayObject, $crtObject)) + $systemLibraries

Write-Host '[x64 4/5] 链接 LocaleEmulator.dll'
Invoke-Native $linker $coreLinkArguments

Write-Host '[x64 5/5] 编译并链接 LoaderDll.dll'
$loaderObjects = @(
    (Compile-Source (Join-Path $loaderRoot 'LoaderDll.cpp') -ExtraArguments @('/DLEP_LOADER_DLL=1', '/guard:cf') -ObjectName 'LoaderDll.obj'),
    (Compile-Source (Join-Path $loaderRoot 'Environment.cpp') -ExtraArguments @('/DLEP_LOADER_DLL=1', '/guard:cf') -ObjectName 'Environment.obj')
)
$loaderLinkArguments = @(
    '/nologo', '/dll', '/noentry', '/nodefaultlib', '/debug:none', '/opt:ref',
    '/manifest:no', '/machine:x64', '/guard:cf', "/out:$(Join-Path $binaryRoot 'LoaderDll.dll')",
    "/implib:$(Join-Path $libraryRoot 'LoaderDll.lib')",
    "/def:$(Join-Path $loaderRoot 'LoaderDll.def')",
    '/delayload:KERNEL32_LEP.dll'
) + $libraryArguments + ($supportObjects + $loaderObjects + @($delayObject, $crtObject)) + $systemLibraries
Invoke-Native $linker $loaderLinkArguments

Write-Host "x64 原生产物已生成：$binaryRoot"
