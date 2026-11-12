#requires -Version 7.0
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repository = $PSScriptRoot
$artifacts = Join-Path $repository 'artifacts'
$logs = Join-Path $artifacts 'logs'
New-Item -ItemType Directory -Path $logs -Force | Out-Null

. (Join-Path $repository 'Native/Toolchain.ps1')
$toolchain = Get-OfficialToolchain
$msbuild = $toolchain.MSBuild

function Invoke-Build {
    param([string]$Project, [string]$Platform, [string]$LogName, [string]$Target)
    & $msbuild (Join-Path $repository $Project) "/t:$Target" /m /nr:false /nologo /v:minimal /p:Configuration=Release "/p:SolutionDir=$repository/" "/p:Platform=$Platform" "/p:WindowsTargetPlatformVersion=$($toolchain.SdkVersion)" "/p:VCToolsVersion=$($toolchain.MsvcVersion)" /fl "/flp:logfile=$(Join-Path $logs $LogName);verbosity=normal;encoding=UTF-8"
    if ($LASTEXITCODE -ne 0) {
        throw "构建失败：$Project，详细日志：$(Join-Path $logs $LogName)"
    }
}

Invoke-Build 'LocaleEmulator.sln' 'Any CPU' 'product-restore.log' 'Restore'
Invoke-Build 'tests/Managed/ManagedRegression.csproj' 'AnyCPU' 'managed-restore.log' 'Restore'
Invoke-Build 'tests/Managed/ShellRegressionHost.csproj' 'AnyCPU' 'shell64-restore.log' 'Restore'
Invoke-Build 'LocaleEmulator.sln' 'Any CPU' 'product-clean.log' 'Clean'
Invoke-Build 'LocaleEmulator.sln' 'Any CPU' 'product-build.log' 'Build'
Invoke-Build 'tests/Managed/ManagedRegression.csproj' 'AnyCPU' 'managed-build.log' 'Build'
Invoke-Build 'tests/Managed/ShellRegressionHost.csproj' 'AnyCPU' 'shell64-build.log' 'Build'
Invoke-Build 'tests/Native/NativeProbe.vcxproj' 'Win32' 'probe-build.log' 'Build'
Invoke-Build 'tests/Native/NativeLaunch.vcxproj' 'Win32' 'native-launch-build.log' 'Build'
Invoke-Build 'tests/Native/NativeContracts.vcxproj' 'Win32' 'native-contracts-build.log' 'Build'
Invoke-Build 'tests/Native/NativeDirectoryContracts.vcxproj' 'Win32' 'native-directory-build.log' 'Build'
Invoke-Build 'tests/Native/LoaderInteropStub.vcxproj' 'Win32' 'loader-interop-build.log' 'Build'
Invoke-Build 'tests/Native/HardeningCheck.vcxproj' 'Win32' 'hardening-check-build.log' 'Build'
Invoke-Build 'tests/Native/HardeningMessages.vcxproj' 'Win32' 'hardening-messages-build.log' 'Build'
Invoke-Build 'tests/Native/AllocationContracts.vcxproj' 'Win32' 'allocation-contracts-build.log' 'Build'

$buildDirectory = Join-Path $repository 'Build/Release'
& (Join-Path $repository 'Scripts/Test.ps1') -BuildDirectory $buildDirectory -ResultsDirectory (Join-Path $artifacts 'tests/source')

[xml]$versionFile = Get-Content -Raw -LiteralPath (Join-Path $buildDirectory 'LEVersion.xml')
$version = [version]$versionFile.LEVersion.Version
$packageName = "Locale.Emulator.$version"
$package = Join-Path $artifacts "package/$packageName"
if (Test-Path -LiteralPath $package) {
    Remove-Item -LiteralPath $package -Recurse -Force
}
New-Item -ItemType Directory -Path $package -Force | Out-Null

$requiredFiles = @(
    'LEProc.exe', 'LEGUI.exe', 'LEInstaller.exe',
    'LEProc.exe.config', 'LEGUI.exe.config', 'LEInstaller.exe.config',
    'LECommonLibrary.dll', 'LEContextMenuHandler.dll',
    'LoaderDll.dll', 'LocaleEmulator.dll', 'LEVersion.xml'
)
foreach ($name in $requiredFiles) {
    Copy-Item -LiteralPath (Join-Path $buildDirectory $name) -Destination $package
}
# Satellite resource assemblies: <culture>\<Assembly>.resources.dll
Get-ChildItem -LiteralPath $buildDirectory -Directory |
    Where-Object { Get-ChildItem -LiteralPath $_.FullName -Filter '*.resources.dll' -File } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $package -Recurse }
Copy-Item -LiteralPath (Join-Path $repository 'README.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $repository 'Directory.Build.props') -Destination $package
$packageDocs = Join-Path $package 'docs'
New-Item -ItemType Directory -Path $packageDocs -Force | Out-Null
foreach ($name in @('SOURCES.md', 'MAINTENANCE.md', 'QUALITY.md', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $repository "docs/$name") -Destination $packageDocs
}
$packageNative = Join-Path $package 'Native'
New-Item -ItemType Directory -Path $packageNative -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repository 'Native/Toolchain.props') -Destination $packageNative
$licenses = Join-Path $package 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
foreach ($name in @('COPYING', 'COPYING.LESSER')) {
    Copy-Item -LiteralPath (Join-Path $repository $name) -Destination (Join-Path $licenses $name)
}
Copy-Item -LiteralPath (Join-Path $repository 'licenses/MS-PL.txt') -Destination $licenses
foreach ($name in @('LICENSE-GPL.txt', 'LICENSE-LGPL.txt')) {
    Copy-Item -LiteralPath (Join-Path $repository "Native/$name") -Destination (Join-Path $licenses $name)
}
$commit = & git -C $repository rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw '无法读取构建提交。' }
$changes = & git -C $repository status --porcelain=v1
if ($LASTEXITCODE -ne 0) { throw '无法读取构建工作区状态。' }
$os = Get-ItemProperty -LiteralPath 'HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion'
[xml]$managedToolchain = Get-Content -Raw -LiteralPath (Join-Path $repository 'Directory.Build.props')
[ordered]@{
    version = $version.ToString()
    commit = $commit
    dirty = -not [string]::IsNullOrWhiteSpace(($changes -join "`n"))
    built_at = (Get-Date).ToUniversalTime().ToString('o')
    host = [ordered]@{ display_version = $os.DisplayVersion; build = $os.CurrentBuild; ubr = $os.UBR }
    msbuild = (Get-Item -LiteralPath $msbuild).VersionInfo.FileVersion
    native_platform = 'Win32'
    managed_framework = $managedToolchain.Project.PropertyGroup.TargetFrameworkVersion
    native_toolset = $toolchain.PlatformToolset
    msvc = $toolchain.MsvcVersion
    windows_sdk = $toolchain.SdkVersion
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'build-info.json') -Encoding utf8NoBOM

$hashes = Get-ChildItem -File -Recurse -LiteralPath $package | Sort-Object FullName | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($package, $_.FullName).Replace('\', '/')
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $relative
}
$hashes | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS') -Encoding utf8NoBOM
$archive = Join-Path $artifacts "$packageName.zip"
# Compress-Archive treats -DestinationPath as a wildcard path and fails for repository paths containing '[',
# so build the archive with .NET directly (entries stay relative to the package root).
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
[IO.Compression.ZipFile]::CreateFromDirectory($package, $archive, [IO.Compression.CompressionLevel]::Optimal, $false)

# 从实际 ZIP 解包后复测，验证发行包没有依赖源码目录的文件。
$unpacked = Join-Path $artifacts 'package-smoke'
if (Test-Path -LiteralPath $unpacked) {
    Remove-Item -LiteralPath $unpacked -Recurse -Force
}
Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
foreach ($entry in Get-Content -LiteralPath (Join-Path $unpacked 'SHA256SUMS')) {
    $expected, $relative = $entry -split '  ', 2
    $actual = (Get-FileHash -LiteralPath (Join-Path $unpacked $relative) -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw "发行包文件校验失败：$relative" }
}
foreach ($markdown in Get-ChildItem -LiteralPath $unpacked -Filter '*.md' -File -Recurse) {
    foreach ($link in [regex]::Matches((Get-Content -Raw -LiteralPath $markdown.FullName), '\]\(([^)]+)\)')) {
        $target = $link.Groups[1].Value
        if ($target -match '^(https?://|#)') { continue }
        $target = ($target -split '#', 2)[0]
        if (-not (Test-Path -LiteralPath (Join-Path $markdown.DirectoryName $target))) {
            throw "发行包文档链接缺少目标：$($markdown.Name) -> $target"
        }
    }
}
& (Join-Path $repository 'Scripts/Test.ps1') -BuildDirectory $unpacked -ResultsDirectory (Join-Path $artifacts 'tests/package')
Write-Host "完整构建、回归及发行包验证通过：$archive"
