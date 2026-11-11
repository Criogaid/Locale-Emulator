#requires -Version 7.0
<#
.SYNOPSIS
  Registers or removes the Windows 11 first-level "Run with Locale Emulator" menu for .exe files.
.DESCRIPTION
  Registers the sparse package from Native/ExplorerCommand/AppxManifest.xml for the current user.
  The product directory must contain LEProc.exe and LEExplorerCommand.dll (x64 package).
  Registering a package that is not signed requires Windows Developer Mode.
  Inside a release package this script sits next to AppxManifest.xml and the product files.
#>
[CmdletBinding()]
param(
    [ValidateSet('Register', 'Unregister')][string]$Action = 'Register',
    [string]$ProductDirectory,
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'
$packageName = 'LocaleEmulator.ModernMenu'

if ($Action -eq 'Unregister') {
    $existing = Get-AppxPackage -Name $packageName
    if ($existing) {
        $existing | Remove-AppxPackage
    }
    Write-Host "已移除 $packageName。"
    return
}

if (-not $ProductDirectory) {
    $ProductDirectory = $PSScriptRoot
}
if (-not $ManifestPath) {
    $ManifestPath = @(
        (Join-Path $PSScriptRoot 'AppxManifest.xml'),
        (Join-Path $PSScriptRoot '..\Native\ExplorerCommand\AppxManifest.xml')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $ManifestPath) {
    throw '未找到 AppxManifest.xml，请用 -ManifestPath 指定。'
}
$ProductDirectory = (Resolve-Path -LiteralPath $ProductDirectory).Path
foreach ($name in 'LEProc.exe', 'LEExplorerCommand.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $ProductDirectory $name))) {
        throw "产品目录缺少 $name：$ProductDirectory"
    }
}

# The registered manifest folder holds the manifest and logos only; keep it stable per user.
$registrationDirectory = Join-Path $env:LOCALAPPDATA 'LocaleEmulator\ModernMenu'
$assetsDirectory = Join-Path $registrationDirectory 'Assets'
New-Item -ItemType Directory -Force $assetsDirectory | Out-Null
Copy-Item -LiteralPath $ManifestPath -Destination $registrationDirectory -Force

Add-Type -AssemblyName System.Drawing
$icon = [System.Drawing.Icon]::ExtractAssociatedIcon((Join-Path $ProductDirectory 'LEProc.exe'))
$bitmap = [System.Drawing.Bitmap]::new($icon.ToBitmap(), 150, 150)
try {
    $bitmap.Save((Join-Path $assetsDirectory 'Logo.png'), [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $bitmap.Dispose()
    $icon.Dispose()
}

Add-AppxPackage -Register (Join-Path $registrationDirectory 'AppxManifest.xml') -ExternalLocation $ProductDirectory
Write-Host "已为当前用户注册 $packageName，外部位置：$ProductDirectory"
Write-Host '重新启动 Explorer 或重新登录后，.exe 的一级右键菜单会出现“Run with Locale Emulator”。'
