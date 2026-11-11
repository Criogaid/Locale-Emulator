#requires -Version 5.1
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
# Holds the registered manifest and logo only; the product files stay in the product directory.
$registrationDirectory = Join-Path $env:LOCALAPPDATA 'LocaleEmulator\ModernMenu'

if ($Action -eq 'Unregister') {
    $existing = Get-AppxPackage -Name $packageName
    if ($existing) {
        $existing | Remove-AppxPackage
    }
    Remove-Item -LiteralPath $registrationDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Removed $packageName."
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
    throw 'AppxManifest.xml was not found; pass -ManifestPath.'
}
$ProductDirectory = (Resolve-Path -LiteralPath $ProductDirectory).Path
foreach ($name in 'LEProc.exe', 'LEExplorerCommand.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $ProductDirectory $name))) {
        throw "The product directory is missing ${name}: $ProductDirectory"
    }
}

# The registered manifest folder must stay in place for as long as the package is registered.
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

# Re-registering the same identity from another location fails, so drop an earlier registration first.
Get-AppxPackage -Name $packageName | Remove-AppxPackage
Add-AppxPackage -Register (Join-Path $registrationDirectory 'AppxManifest.xml') -ExternalLocation $ProductDirectory
Write-Host "Registered $packageName for the current user; external location: $ProductDirectory"
Write-Host 'Restart Explorer or sign in again; the .exe first-level menu then shows Run with Locale Emulator.'
