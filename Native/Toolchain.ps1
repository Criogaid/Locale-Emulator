# Resolves the installed compiler for Toolchain.props, independently of the Visual Studio host version.
function Get-OfficialToolchain {
    [xml]$props = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'Toolchain.props')
    $platformToolset = [string]$props.Project.PropertyGroup.PlatformToolset
    $sdkTarget = [string]$props.Project.PropertyGroup.WindowsTargetPlatformVersion
    if ([string]::IsNullOrWhiteSpace($platformToolset) -or [string]::IsNullOrWhiteSpace($sdkTarget)) {
        throw 'Native/Toolchain.props 必须定义 PlatformToolset 和 WindowsTargetPlatformVersion。'
    }

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
    $sdkInclude = Join-Path $sdkRoot 'Include'
    if (-not (Test-Path -LiteralPath $sdkInclude -PathType Container)) {
        throw "未找到 Windows SDK：$sdkInclude"
    }
    # A target of 10.0 means the latest installed Windows 10/11 SDK, matching MSBuild's selection.
    $sdk = Get-ChildItem -LiteralPath $sdkInclude -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' -and
            ($_.Name -eq $sdkTarget -or $_.Name.StartsWith("$sdkTarget.", [StringComparison]::Ordinal)) } |
        Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if ($null -eq $sdk) { throw "未找到匹配 $sdkTarget 的 Windows SDK。" }
    $sdkVersion = $sdk.Name
    foreach ($relative in @("Include/$sdkVersion/um/Windows.h", "Include/$sdkVersion/ucrt/corecrt.h",
        "Lib/$sdkVersion/um/x86/kernel32.lib", "Lib/$sdkVersion/um/x64/kernel32.lib",
        "bin/$sdkVersion/x64/makeappx.exe")) {
        if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot $relative) -PathType Leaf)) {
            throw "Windows SDK $sdkVersion 缺少文件：$relative"
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw '未找到 Visual Studio Installer 的 vswhere.exe。'
    }
    $installationsJson = & $vswhere -products '*' -version '[16.0,)' -requires Microsoft.Component.MSBuild -format json -utf8
    if ($LASTEXITCODE -ne 0) { throw '查询 Visual Studio 安装失败。' }
    foreach ($installation in @($installationsJson | ConvertFrom-Json)) {
        $vsRoot = $installation.installationPath
        $versionFile = Join-Path $vsRoot "VC/Auxiliary/Build/Microsoft.VCToolsVersion.$platformToolset.default.txt"
        if (-not (Test-Path -LiteralPath $versionFile -PathType Leaf)) { continue }
        $msvcVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
        if ($msvcVersion -notmatch '^\d+\.\d+\.\d+$') { throw "无效的 MSVC 版本文件：$versionFile" }
        $msvcRoot = Join-Path $vsRoot "VC/Tools/MSVC/$msvcVersion"
        $hostBin = Join-Path $msvcRoot 'bin/Hostx86/x64'
        $msbuild = Join-Path $vsRoot 'MSBuild/Current/Bin/MSBuild.exe'
        foreach ($path in @($msbuild, (Join-Path $hostBin 'cl.exe'), (Join-Path $hostBin 'link.exe'),
            (Join-Path $hostBin 'lib.exe'), (Join-Path $hostBin 'dumpbin.exe'),
            (Join-Path $msvcRoot 'bin/Hostx86/x86/cl.exe'))) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "工具链 $platformToolset 缺少文件：$path" }
        }
        return [pscustomobject]@{
            MSBuild         = $msbuild
            PlatformToolset = $platformToolset
            MsvcVersion     = $msvcVersion
            MsvcRoot        = $msvcRoot
            SdkRoot         = $sdkRoot
            SdkVersion      = $sdkVersion
            HostBin         = $hostBin
        }
    }
    throw "未找到安装了 MSBuild 和 $platformToolset x86/x64 工具的 Visual Studio 2019 或更新版本。"
}
