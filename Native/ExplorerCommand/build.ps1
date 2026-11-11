param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\Build\Dual\x64\explorer-command')
)

$ErrorActionPreference = 'Stop'

# Same VS2019 / SDK 10.0.19041.0 toolchain as the x64 Core build.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw '未找到 Visual Studio Installer，无法定位 VS2019 官方工具链。'
}
$vsRoot = (& $vswhere -latest -products '*' -version '[16.0,17.0)' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($vsRoot)) {
    throw '未找到同时安装 MSBuild 和 C++ 工具的 Visual Studio 2019。'
}
$msvcRoot = Join-Path $vsRoot 'VC\Tools\MSVC\14.29.30133'
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = '10.0.19041.0'
$hostBin = Join-Path $msvcRoot 'bin\Hostx86\x64'
$env:Path = "$hostBin;$env:Path"

New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$object = Join-Path $OutputDirectory 'ExplorerCommand.obj'
$dll = Join-Path $OutputDirectory 'LEExplorerCommand.dll'

function Invoke-Native {
    param([string]$File, [string[]]$Arguments)
    & $File @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "命令失败（$LASTEXITCODE）：$File $($Arguments -join ' ')"
    }
}

Invoke-Native (Join-Path $hostBin 'cl.exe') @(
    '/nologo', '/utf-8', '/c', '/O2', '/MT', '/GS', '/guard:cf', '/GR-', '/EHs-c-', '/W4', '/WX',
    '/Zc:wchar_t', '/DUNICODE', '/D_UNICODE', '/DWIN32_LEAN_AND_MEAN', '/DNDEBUG',
    "/I$(Join-Path $msvcRoot 'include')",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\ucrt")",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\shared")",
    "/I$(Join-Path $sdkRoot "Include\$sdkVersion\um")",
    "/Fo$object", (Join-Path $PSScriptRoot 'ExplorerCommand.cpp'))

Invoke-Native (Join-Path $hostBin 'link.exe') @(
    '/nologo', '/dll', '/machine:x64', '/debug:none', '/opt:ref', '/opt:icf',
    '/dynamicbase', '/highentropyva', '/nxcompat', '/guard:cf', '/manifest:no',
    "/def:$(Join-Path $PSScriptRoot 'ExplorerCommand.def')", "/out:$dll",
    "/libpath:$(Join-Path $sdkRoot "Lib\$sdkVersion\um\x64")",
    "/libpath:$(Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64")",
    "/libpath:$(Join-Path $msvcRoot 'lib\x64')",
    $object, 'kernel32.lib', 'user32.lib', 'ole32.lib', 'shell32.lib', 'shlwapi.lib', 'xmllite.lib')

Write-Host "LEExplorerCommand.dll 已生成：$dll"

# Validate the sparse-package manifest against the real Appx schema. Registration cannot be exercised
# in every environment (it needs Developer Mode or a signed package), but schema errors such as a
# missing capability are caught here. /nv skips payload checks: the files live in the external location.
$makeappx = Join-Path $sdkRoot "bin\$sdkVersion\x64\makeappx.exe"
if (-not (Test-Path -LiteralPath $makeappx)) {
    throw "未找到 makeappx.exe：$makeappx"
}
$validation = Join-Path $OutputDirectory 'manifest-validation'
Remove-Item -LiteralPath $validation -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $validation | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'AppxManifest.xml') -Destination $validation
    Invoke-Native $makeappx @('pack', '/nv', '/o', '/d', $validation, '/p', (Join-Path $OutputDirectory 'manifest-validation.msix'))
}
finally {
    Remove-Item -LiteralPath $validation, (Join-Path $OutputDirectory 'manifest-validation.msix') -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'AppxManifest.xml 通过 Appx 架构校验。'
