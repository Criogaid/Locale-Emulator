#requires -Version 5.1
<#
.SYNOPSIS
  Registers or removes the Windows 11 first-level "Run with Locale Emulator" menu for .exe files.
.DESCRIPTION
  Registers the sparse package from Native/ExplorerCommand/AppxManifest.xml for the current user.
  The product directory must contain LEProc.exe, LEGUI.exe (icon) and LEExplorerCommand.dll (x64 package).
  Registering a package that is not signed requires Windows Developer Mode.
  Inside a release package this script sits next to AppxManifest.xml and the product files.
#>
[CmdletBinding()]
param(
    [ValidateSet('Register', 'Unregister')][string]$Action = 'Register',
    [string]$ProductDirectory
)

try {
    $ErrorActionPreference = 'Stop'
    $packageName = 'LocaleEmulator.ModernMenu'
    # Holds the registered manifest and logo only; the product files stay in the product directory.
    $registrationDirectory = Join-Path $env:LOCALAPPDATA 'LocaleEmulator\ModernMenu'

    function Remove-RegistrationDirectory {
        Remove-Item -LiteralPath $registrationDirectory -Recurse -Force -ErrorAction SilentlyContinue
        # Drop the product folder as well when nothing else lives in it.
        $parent = Split-Path -Parent $registrationDirectory
        if ((Test-Path -LiteralPath $parent) -and -not (Get-ChildItem -LiteralPath $parent -Force)) {
            Remove-Item -LiteralPath $parent -Force -ErrorAction SilentlyContinue
        }
    }

    if ($Action -eq 'Unregister') {
        $existing = Get-AppxPackage -Name $packageName
        if ($existing) {
            $existing | Remove-AppxPackage
        }
        Remove-RegistrationDirectory
        Write-Host $(if ($existing) { "Removed $packageName." } else { "$packageName was not registered; nothing to remove." })
        return
    }

    # The package and its DLL are x64 only; Explorer on other architectures cannot load them.
    # OSArchitecture reports the real OS even when this script runs under x64 emulation.
    $osArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    if ($osArchitecture -ne [System.Runtime.InteropServices.Architecture]::X64) {
        throw "The Windows 11 first-level menu is only available on x64 Windows (found $osArchitecture)."
    }

    if (-not $ProductDirectory) {
        $ProductDirectory = $PSScriptRoot
    }
    # Inside a package the manifest sits next to this script; in a checkout it is under Native/.
    $manifestPath = @(
        (Join-Path $PSScriptRoot 'AppxManifest.xml'),
        (Join-Path $PSScriptRoot '..\Native\ExplorerCommand\AppxManifest.xml')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $manifestPath) {
        throw 'AppxManifest.xml was not found next to the script or under Native/ExplorerCommand.'
    }
    $ProductDirectory = (Resolve-Path -LiteralPath $ProductDirectory).Path
    foreach ($name in 'LEProc.exe', 'LEGUI.exe', 'LEExplorerCommand.dll') {
        if (-not (Test-Path -LiteralPath (Join-Path $ProductDirectory $name))) {
            throw "The product directory is missing ${name}: $ProductDirectory"
        }
    }

    # The registered manifest folder must stay in place for as long as the package is registered.
    $assetsDirectory = Join-Path $registrationDirectory 'Assets'
    New-Item -ItemType Directory -Force $assetsDirectory | Out-Null
    Copy-Item -LiteralPath $manifestPath -Destination $registrationDirectory -Force

    Add-Type -AssemblyName System.Drawing
    $icon = [System.Drawing.Icon]::ExtractAssociatedIcon((Join-Path $ProductDirectory 'LEGUI.exe'))
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
    try {
        Add-AppxPackage -Register (Join-Path $registrationDirectory 'AppxManifest.xml') -ExternalLocation $ProductDirectory
    }
    catch {
        # A failed registration (for example without Developer Mode) must not leave files behind.
        if (-not (Get-AppxPackage -Name $packageName)) {
            Remove-RegistrationDirectory
        }
        throw
    }
    Write-Host "Registered $packageName for the current user; external location: $ProductDirectory"
    Write-Host 'Restart Explorer or sign in again; the .exe first-level menu then shows Run with Locale Emulator.'
}
catch {
    # One readable line for callers (the installer shows it); the full error record stays out of the way.
    $reason = ($_.Exception.Message -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -First 1)
    [Console]::Error.WriteLine($reason)
    exit 1
}
