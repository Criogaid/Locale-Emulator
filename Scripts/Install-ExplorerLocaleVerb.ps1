[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$Uninstall,
    [string]$PackageRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$launcher = Join-Path $PackageRoot 'LEProc.exe'
$verbKey = 'HKCU:\Software\Classes\exefile\shell\LocaleEmulator.Run'
$commandKey = Join-Path $verbKey 'command'

function Notify-ShellAssociationChanged {
    if (-not ('LocaleEmulator.ShellChangeNotifier' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace LocaleEmulator
{
    public static class ShellChangeNotifier
    {
        [DllImport("shell32.dll")]
        private static extern void SHChangeNotify(
            uint eventId,
            uint flags,
            IntPtr item1,
            IntPtr item2);

        public static void AssociationsChanged()
        {
            const uint SHCNE_ASSOCCHANGED = 0x08000000;
            const uint SHCNF_IDLIST = 0x0000;
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, IntPtr.Zero, IntPtr.Zero);
        }
    }
}
'@
    }

    [LocaleEmulator.ShellChangeNotifier]::AssociationsChanged()
}

if ($Uninstall) {
    if (Test-Path -LiteralPath $verbKey) {
        if ($PSCmdlet.ShouldProcess($verbKey, '卸载 Locale Emulator 用户级右键菜单')) {
            Remove-Item -LiteralPath $verbKey -Recurse -Force
            Notify-ShellAssociationChanged
            Write-Output '已卸载 Locale Emulator 用户级右键菜单。'
        }
    }
    else {
        Write-Output 'Locale Emulator 用户级右键菜单尚未安装。'
    }

    return
}

if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "发行目录缺少 LEProc.exe：$launcher"
}

$command = ('"{0}" "%1"' -f $launcher)
if ($PSCmdlet.ShouldProcess($verbKey, '安装 Locale Emulator 用户级右键菜单')) {
    New-Item -Path $commandKey -Force | Out-Null
    Set-Item -LiteralPath $verbKey -Value '使用 Locale Emulator 运行'
    New-ItemProperty -LiteralPath $verbKey -Name 'MUIVerb' -PropertyType String -Value '使用 Locale Emulator 运行' -Force | Out-Null
    New-ItemProperty -LiteralPath $verbKey -Name 'Icon' -PropertyType String -Value $launcher -Force | Out-Null
    New-ItemProperty -LiteralPath $verbKey -Name 'NeverDefault' -PropertyType String -Value '' -Force | Out-Null
    New-ItemProperty -LiteralPath $verbKey -Name 'MultiSelectModel' -PropertyType String -Value 'Single' -Force | Out-Null
    Set-Item -LiteralPath $commandKey -Value $command

    Notify-ShellAssociationChanged

    Write-Output '已安装 Locale Emulator 用户级右键菜单。'
    Write-Output "注册表项：$verbKey"
    Write-Output "命令：$command"
}
