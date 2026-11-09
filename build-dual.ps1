#requires -Version 7.0
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repository = $PSScriptRoot
$artifacts = Join-Path $repository 'artifacts'
$dualRoot = Join-Path $repository 'Build/Dual'
$releaseRoot = Join-Path $repository 'Build/Release'
$managedX64Root = Join-Path $dualRoot 'x64/managed'
$nativeX64Root = Join-Path $dualRoot 'x64/bin'
$logs = Join-Path $artifacts 'logs'

New-Item -ItemType Directory -Path $logs -Force | Out-Null

Write-Host '[dual 1/6] 构建并回归 x86 基线'
& (Join-Path $repository 'build.ps1')

Write-Host '[dual 2/6] 构建 x64 原生组件'
& (Join-Path $repository 'Native/LocaleEmulatorX64/build-x64.ps1') -Clean

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -version '[16.0,17.0)' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
    throw '未找到同时安装 MSBuild 和 C++ 工具的 Visual Studio 2019。'
}
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'

Write-Host '[dual 3/6] 构建 x64 属性表边界探针'
& $msbuild (Join-Path $repository 'tests/Native/X64AttributeContracts.vcxproj') /t:Rebuild /m /nr:false /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/fl" "/flp:logfile=$(Join-Path $logs 'dual-x64-attribute-contracts-build.log');verbosity=normal;encoding=UTF-8"
if ($LASTEXITCODE -ne 0) {
    throw 'x64 属性表边界探针构建失败。'
}


Write-Host '[dual 4/6] 构建 x64 LEProc 与公共托管库'
& $msbuild (Join-Path $repository 'LEProc/LEProc.csproj') /t:Rebuild /m /nr:false /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=$repository/" "/fl" "/flp:logfile=$(Join-Path $logs 'dual-x64-managed-build.log');verbosity=normal;encoding=UTF-8"
if ($LASTEXITCODE -ne 0) {
    throw 'x64 托管构建失败。'
}

$x64AttributeSmoke = Join-Path $dualRoot 'x64-attribute-smoke'
if (Test-Path -LiteralPath $x64AttributeSmoke) {
    Remove-Item -LiteralPath $x64AttributeSmoke -Recurse -Force
}
New-Item -ItemType Directory -Path $x64AttributeSmoke -Force | Out-Null
$attributeProcess = $null
try {
    foreach ($name in @('LEProc.exe', 'LEProc.exe.config', 'LECommonLibrary.dll', 'LEVersion.xml')) {
        Copy-Item -LiteralPath (Join-Path $managedX64Root $name) -Destination $x64AttributeSmoke -Force
    }
    foreach ($name in @('LoaderDll.dll', 'LocaleEmulator.dll', 'X64AttributeContracts.exe')) {
        Copy-Item -LiteralPath (Join-Path $nativeX64Root $name) -Destination $x64AttributeSmoke -Force
    }
    Copy-Item -LiteralPath (Join-Path $releaseRoot 'NativeProbe.exe') -Destination $x64AttributeSmoke -Force
    $x86AttributeSmoke = Join-Path $x64AttributeSmoke 'x86'
    New-Item -ItemType Directory -Path $x86AttributeSmoke -Force | Out-Null
    foreach ($name in @('LoaderDll.dll', 'LocaleEmulator.dll')) {
        Copy-Item -LiteralPath (Join-Path $releaseRoot $name) -Destination $x86AttributeSmoke -Force
    }
    $attributeTarget = Join-Path $x64AttributeSmoke 'X64AttributeContracts.exe'
    $attributeConfig = '<LEConfig><Profiles><Profile Name="x64 attribute contract" Guid="00000000-0000-0000-0000-0000000000A1" MainMenu="false"><Parameter></Parameter><Location>ja-JP</Location><Timezone>Tokyo Standard Time</Timezone><RunAsAdmin>false</RunAsAdmin><RedirectRegistry>true</RedirectRegistry><IsAdvancedRedirection>false</IsAdvancedRedirection><RunWithSuspend>false</RunWithSuspend></Profile></Profiles></LEConfig>'
    $attributeConfig | Set-Content -LiteralPath ($attributeTarget + '.le.config') -Encoding utf8NoBOM
    $attributeStart = [Diagnostics.ProcessStartInfo]::new((Join-Path $x64AttributeSmoke 'LEProc.exe'))
    $attributeStart.UseShellExecute = $false
    $attributeStart.CreateNoWindow = $true
    $attributeStart.WorkingDirectory = $x64AttributeSmoke
    [void]$attributeStart.ArgumentList.Add('-run')
    [void]$attributeStart.ArgumentList.Add($attributeTarget)
    $attributeProcess = [Diagnostics.Process]::Start($attributeStart)
    if (-not $attributeProcess.WaitForExit(60000)) {
        $attributeProcess.Kill($true)
        $attributeProcess.WaitForExit()
        throw 'x64 属性表边界探针超时。'
    }
    if ($attributeProcess.ExitCode -ne 0) {
        throw "x64 属性表边界探针失败，退出码 $($attributeProcess.ExitCode)。"
    }
} finally {
    if ($null -ne $attributeProcess) {
        if (-not $attributeProcess.HasExited) {
            $attributeProcess.Kill($true)
            $attributeProcess.WaitForExit()
        }
        $attributeProcess.Dispose()
    }
    if (Test-Path -LiteralPath $x64AttributeSmoke) {
        Remove-Item -LiteralPath $x64AttributeSmoke -Recurse -Force
    }
}

[xml]$versionFile = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'LEVersion.xml')
$version = [version]$versionFile.LEVersion.Version
$packageRoot = Join-Path $artifacts 'package-dual'
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

function Copy-RequiredFile {
    param([string]$SourceRoot, [string]$DestinationRoot, [string]$Name)
    $source = Join-Path $SourceRoot $Name
    if (-not (Test-Path -LiteralPath $source)) {
        throw "缺少打包文件：$source"
    }
    if (Test-Path -LiteralPath $source -PathType Container) {
        Copy-Item -LiteralPath $source -Destination $DestinationRoot -Recurse -Force
    } else {
        Copy-Item -LiteralPath $source -Destination $DestinationRoot -Force
    }
}

function Copy-PackageCommon {
    param([string]$DestinationRoot)
    foreach ($name in @(
        'LEGUI.exe', 'LEGUI.exe.config', 'LEInstaller.exe', 'LEInstaller.exe.config',
        'LEContextMenuHandler.dll', 'Lang', 'CommonLang', 'InstallerLang', 'LEVersion.xml')) {
        Copy-RequiredFile $releaseRoot $DestinationRoot $name
    }
    Copy-Item -LiteralPath (Join-Path $repository 'README.md') -Destination $DestinationRoot -Force
    Copy-Item -LiteralPath (Join-Path $repository 'Directory.Build.props') -Destination $DestinationRoot -Force
    $docs = Join-Path $DestinationRoot 'docs'
    New-Item -ItemType Directory -Path $docs -Force | Out-Null
    foreach ($name in @('SOURCES.md', 'MAINTENANCE.md', 'QUALITY.md', 'THIRD_PARTY_NOTICES.md')) {
        Copy-Item -LiteralPath (Join-Path $repository "docs/$name") -Destination $docs -Force
    }
    $native = Join-Path $DestinationRoot 'Native'
    New-Item -ItemType Directory -Path $native -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repository 'Native/Toolchain.props') -Destination $native -Force
    $licenses = Join-Path $DestinationRoot 'licenses'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    foreach ($name in @('COPYING', 'COPYING.LESSER')) {
        Copy-Item -LiteralPath (Join-Path $repository $name) -Destination $licenses -Force
    }
    Copy-Item -LiteralPath (Join-Path $repository 'licenses/MS-PL.txt') -Destination $licenses -Force
    foreach ($name in @('LICENSE-GPL.txt', 'LICENSE-LGPL.txt')) {
        Copy-Item -LiteralPath (Join-Path $repository "Native/$name") -Destination $licenses -Force
    }
}

function Copy-X86Compatibility {
    param([string]$Package, [string]$ManagedRoot, [string]$NativeRoot)
    $compatibility = Join-Path $Package 'x86'
    New-Item -ItemType Directory -Path $compatibility -Force | Out-Null
    foreach ($name in @('LEProc.exe', 'LEProc.exe.config', 'LECommonLibrary.dll')) {
        Copy-RequiredFile $ManagedRoot $compatibility $name
    }
    foreach ($name in @('LoaderDll.dll', 'LocaleEmulator.dll')) {
        Copy-RequiredFile $NativeRoot $compatibility $name
    }
}


function New-Package {
    param(
        [ValidateSet('x86', 'x64')][string]$Architecture,
        [string]$ManagedRoot,
        [string]$NativeRoot,
        [switch]$IncludeX86Compatibility
    )

    $name = "Locale.Emulator.$version-$Architecture"
    $package = Join-Path $packageRoot $name
    New-Item -ItemType Directory -Path $package -Force | Out-Null
    Copy-PackageCommon $package

    foreach ($nameToCopy in @('LEGUI.exe', 'LEGUI.exe.config', 'LEInstaller.exe', 'LEInstaller.exe.config', 'LEContextMenuHandler.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $package $nameToCopy))) {
            throw "公共发行文件缺失：$nameToCopy"
        }
    }
    foreach ($nameToCopy in @('LEProc.exe', 'LEProc.exe.config', 'LECommonLibrary.dll', 'LEVersion.xml')) {
        Copy-RequiredFile $ManagedRoot $package $nameToCopy
    }
    foreach ($nameToCopy in @('LoaderDll.dll', 'LocaleEmulator.dll')) {
        Copy-RequiredFile $NativeRoot $package $nameToCopy
    }
    if ($IncludeX86Compatibility) {
        Copy-X86Compatibility $package $releaseRoot $releaseRoot
    }

    $commit = & git -C $repository rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw '无法读取构建提交。' }
    $buildInfo = [ordered]@{
        version = $version.ToString()
        architecture = $Architecture
        commit = $commit
        built_at = (Get-Date).ToUniversalTime().ToString('o')
        managed_platform = $Architecture
        native_platform = $Architecture
        managed_framework = 'v4.8'
        native_toolset = 'v142'
        windows_sdk = '10.0.19041.0'
    }
    $buildInfo | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'build-info.json') -Encoding utf8NoBOM

    $hashLines = Get-ChildItem -File -Recurse -LiteralPath $package | Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($package, $_.FullName).Replace('\', '/')
        (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $relative
    }
    $hashLines | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS') -Encoding utf8NoBOM
    $archive = Join-Path $packageRoot "$name.zip"
    Compress-Archive -Path (Join-Path $package '*') -DestinationPath $archive -CompressionLevel Optimal -Force

    $smoke = Join-Path $packageRoot "smoke-$Architecture"
    if (Test-Path -LiteralPath $smoke) {
        Remove-Item -LiteralPath $smoke -Recurse -Force
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $smoke
    foreach ($entry in Get-Content -LiteralPath (Join-Path $smoke 'SHA256SUMS')) {
        $expected, $relative = $entry -split '  ', 2
        $actual = (Get-FileHash -LiteralPath (Join-Path $smoke $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            throw "双包校验失败：$Architecture/$relative"
        }
    }
    return [pscustomobject]@{ Architecture = $Architecture; Package = $package; Archive = $archive }
}

Write-Host '[dual 5/6] 装配 x86 与 x64 ZIP'
$x86 = New-Package -Architecture x86 -ManagedRoot $releaseRoot -NativeRoot $releaseRoot
$x64 = New-Package -Architecture x64 -ManagedRoot $managedX64Root -NativeRoot $nativeX64Root -IncludeX86Compatibility

$dumpbin = Join-Path $installation 'VC/Tools/MSVC/14.29.30133/bin/Hostx86/x64/dumpbin.exe'
function Assert-Machine {
    param([string]$Executable, [string]$Expected)
    $headers = & $dumpbin /headers $Executable 2>&1 | Out-String
    if ($Expected -eq 'x86' -and $headers -notmatch '14C machine \(x86\)') {
        throw "x86 PE 校验失败：$Executable"
    }
    if ($Expected -eq 'x64' -and $headers -notmatch '8664 machine \(x64\)') {
        throw "x64 PE 校验失败：$Executable"
    }
}
Assert-Machine (Join-Path $x86.Package 'LEProc.exe') 'x86'
Assert-Machine (Join-Path $x86.Package 'LocaleEmulator.dll') 'x86'
Assert-Machine (Join-Path $x86.Package 'LoaderDll.dll') 'x86'
Assert-Machine (Join-Path $x64.Package 'LEProc.exe') 'x64'
Assert-Machine (Join-Path $x64.Package 'LoaderDll.dll') 'x64'
Assert-Machine (Join-Path $x64.Package 'LocaleEmulator.dll') 'x64'
function Assert-LauncherSelection {
    param([string]$Package)
    $commonAssembly = [Reflection.Assembly]::LoadFrom((Join-Path $Package 'LECommonLibrary.dll'))
    $helperType = $commonAssembly.GetType('LECommonLibrary.GlobalHelper', $true)
    $method = $helperType.GetMethod(
        'GetLauncherPath', [Reflection.BindingFlags]::Static -bor [Reflection.BindingFlags]::Public)
    foreach ($target in @('LEProc.exe', 'x86/LEProc.exe')) {
        $targetPath = Join-Path $Package $target
        $invokeArguments = [object[]]::new(1)
        $invokeArguments[0] = [string]$targetPath
        $selected = [string]$method.Invoke($null, $invokeArguments)
        $expected = if ($target -like 'x86/*') { $targetPath } else { Join-Path $Package 'LEProc.exe' }
        if (-not [StringComparer]::OrdinalIgnoreCase.Equals([IO.Path]::GetFullPath($selected), [IO.Path]::GetFullPath($expected))) {
            throw "Launcher 选择错误：$target -> $selected，期望 $expected"
        }
    }
    Write-Host 'PASS LauncherSelection x86/x64'
}
Assert-LauncherSelection $x64.Package
Assert-Machine (Join-Path $x64.Package 'x86/LEProc.exe') 'x86'
Assert-Machine (Join-Path $x64.Package 'x86/LoaderDll.dll') 'x86'
Assert-Machine (Join-Path $x64.Package 'x86/LocaleEmulator.dll') 'x86'

Write-Host '[dual 6/6] 运行 x64 Loader 启动烟测'
$smokeRoot = Join-Path $packageRoot 'x64-launch-smoke'
if (Test-Path -LiteralPath $smokeRoot) {
    Remove-Item -LiteralPath $smokeRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $smokeRoot -Force | Out-Null
$target = Join-Path $smokeRoot 'cmd.exe'
Copy-Item -LiteralPath (Join-Path $env:windir 'System32/cmd.exe') -Destination $target
$marker = Join-Path $smokeRoot 'started.txt'
$configuration = '<LEConfig><Profiles><Profile Name="x64 smoke" Guid="00000000-0000-0000-0000-000000000064" MainMenu="false"><Parameter>/c echo x64-started&gt;&quot;' + $marker + '&quot;</Parameter><Location>ja-JP</Location><Timezone>Tokyo Standard Time</Timezone><RunAsAdmin>false</RunAsAdmin><RedirectRegistry>true</RedirectRegistry><IsAdvancedRedirection>false</IsAdvancedRedirection><RunWithSuspend>false</RunWithSuspend></Profile></Profiles></LEConfig>'
$configuration | Set-Content -LiteralPath ($target + '.le.config') -Encoding utf8NoBOM
$start = [Diagnostics.ProcessStartInfo]::new((Join-Path $x64.Package 'LEProc.exe'))
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.WorkingDirectory = $x64.Package
[void]$start.ArgumentList.Add('-run')
[void]$start.ArgumentList.Add($target)
$process = [Diagnostics.Process]::Start($start)
try {
    if (-not $process.WaitForExit(60000)) {
        $process.Kill($true)
        $process.WaitForExit()
        throw 'x64 Loader 启动烟测超时。'
    }
    if ($process.ExitCode -ne 0) {
        throw "x64 LEProc 启动烟测失败，退出码 $($process.ExitCode)。"
    }
}
finally {
    if (-not $process.HasExited) {
        $process.Kill($true)
        $process.WaitForExit()
    }
    $process.Dispose()
}
if (-not (Test-Path -LiteralPath $marker)) {
    throw 'x64 Loader 启动烟测未生成目标进程标记。'
}

$x86Target = Join-Path $smokeRoot 'cmd32.exe'
Copy-Item -LiteralPath (Join-Path $env:windir 'SysWOW64/cmd.exe') -Destination $x86Target
$x86Marker = Join-Path $smokeRoot 'started-x86.txt'
$x86Configuration = '<LEConfig><Profiles><Profile Name="x86 smoke" Guid="00000000-0000-0000-0000-000000000032" MainMenu="false"><Parameter>/c echo x86-started&gt;&quot;' + $x86Marker + '&quot;</Parameter><Location>ja-JP</Location><Timezone>Tokyo Standard Time</Timezone><RunAsAdmin>false</RunAsAdmin><RedirectRegistry>true</RedirectRegistry><IsAdvancedRedirection>false</IsAdvancedRedirection><RunWithSuspend>false</RunWithSuspend></Profile></Profiles></LEConfig>'
$x86Configuration | Set-Content -LiteralPath ($x86Target + '.le.config') -Encoding utf8NoBOM
$x86Start = [Diagnostics.ProcessStartInfo]::new((Join-Path $x64.Package 'x86/LEProc.exe'))
$x86Start.UseShellExecute = $false
$x86Start.CreateNoWindow = $true
$x86Start.WorkingDirectory = $x64.Package
[void]$x86Start.ArgumentList.Add('-run')
[void]$x86Start.ArgumentList.Add($x86Target)
$x86Process = [Diagnostics.Process]::Start($x86Start)
try {
    if (-not $x86Process.WaitForExit(60000)) {
        $x86Process.Kill($true)
        $x86Process.WaitForExit()
        throw 'x86 helper 启动烟测超时。'
    }
    if ($x86Process.ExitCode -ne 0) {
        throw "x86 helper 启动烟测失败，退出码 $($x86Process.ExitCode)。"
    }
} finally {
    if (-not $x86Process.HasExited) {
        $x86Process.Kill($true)
        $x86Process.WaitForExit()
    }
    $x86Process.Dispose()
}
if (-not (Test-Path -LiteralPath $x86Marker)) {
    throw 'x64 包中的 x86 helper 未生成目标进程标记。'
}

Write-Host '[dual 6/6] 双包验收完成'
Write-Host "x86: $($x86.Archive)"
Write-Host "x64: $($x64.Archive)"
