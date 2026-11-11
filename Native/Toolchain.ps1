# Locates the official VS2019 / Windows SDK toolchain used by the x64 native builds.
# The SDK version comes from Toolchain.props (the same file the MSBuild projects use); the exact MSVC
# version is pinned here so every x64 build script resolves the same compiler.
function Get-OfficialToolchain {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw '未找到 Visual Studio Installer，无法定位 VS2019 官方工具链。'
    }
    $vsRoot = (& $vswhere -latest -products '*' -version '[16.0,17.0)' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($vsRoot)) {
        throw '未找到同时安装 MSBuild 和 C++ 工具的 Visual Studio 2019。'
    }

    [xml]$props = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'Toolchain.props')
    $sdkVersion = $props.Project.PropertyGroup.WindowsTargetPlatformVersion
    if ([string]::IsNullOrWhiteSpace($sdkVersion)) {
        throw 'Native/Toolchain.props 缺少 WindowsTargetPlatformVersion。'
    }

    $msvcRoot = Join-Path $vsRoot 'VC\Tools\MSVC\14.29.30133'
    [pscustomobject]@{
        MsvcRoot   = $msvcRoot
        SdkRoot    = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
        SdkVersion = $sdkVersion
        HostBin    = Join-Path $msvcRoot 'bin\Hostx86\x64'
    }
}
