#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$ResultsDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = Split-Path -Parent $PSScriptRoot
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
New-Item -ItemType Directory -Path $ResultsDirectory -Force | Out-Null
$ResultsDirectory = (Resolve-Path -LiteralPath $ResultsDirectory).Path
$buildPrefix = $BuildDirectory.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
$resultsPrefix = $ResultsDirectory.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($buildPrefix.StartsWith($resultsPrefix, [StringComparison]::OrdinalIgnoreCase) -or $resultsPrefix.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory 与 ResultsDirectory 必须是互不包含的目录。'
}
$previousResult = Join-Path $ResultsDirectory 'result.json'
if (Test-Path -LiteralPath $previousResult) { Remove-Item -LiteralPath $previousResult }
$product = Join-Path $ResultsDirectory 'product'
if (Test-Path -LiteralPath $product) { Remove-Item -LiteralPath $product -Recurse -Force }
New-Item -ItemType Directory -Path $product -Force | Out-Null
Copy-Item -Path (Join-Path $BuildDirectory '*') -Destination $product -Recurse -Force
$globalConfiguration = Join-Path $product 'LEConfig.xml'
if (Test-Path -LiteralPath $globalConfiguration) { Remove-Item -LiteralPath $globalConfiguration }
foreach ($name in @('ManagedRegression.exe', 'NativeLaunch.exe')) {
    $source = if ($name -eq 'ManagedRegression.exe') { Join-Path $repository "tests/Managed/bin/Release/$name" } else { Join-Path $repository "Build/Release/$name" }
    Copy-Item -LiteralPath $source -Destination $product -Force
}
Copy-Item -LiteralPath (Join-Path $repository 'tests/Managed/bin/Release/ManagedRegression.exe.config') -Destination $product -Force
$os = Get-ItemProperty -LiteralPath 'HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion'
$environment = [ordered]@{
    captured_at = (Get-Date).ToUniversalTime().ToString('o')
    display_version = $os.DisplayVersion
    build = $os.CurrentBuild
    ubr = $os.UBR
    os_version = [Environment]::OSVersion.Version.ToString()
    host_architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    product_directory = $BuildDirectory
    managed_sha256 = [ordered]@{
        common = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'LECommonLibrary.dll')).Hash
        launcher = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'LEProc.exe')).Hash
        gui = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'LEGUI.exe')).Hash
    }
    core_sha256 = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'LocaleEmulator.dll')).Hash
    loader_sha256 = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'LoaderDll.dll')).Hash
}
$environment | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ResultsDirectory 'environment.json') -Encoding utf8NoBOM

function Invoke-RegressionProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$LogName)
    $start = [Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WorkingDirectory = $ResultsDirectory
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(45000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw "回归进程超时：$Executable"
        }
        $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        $output | Set-Content -LiteralPath (Join-Path $ResultsDirectory $LogName) -Encoding utf8NoBOM
        if ($process.ExitCode -ne 0) {
            throw "回归进程失败：$Executable，退出码 $($process.ExitCode)，日志 $(Join-Path $ResultsDirectory $LogName)"
        }
        if (-not [string]::IsNullOrWhiteSpace($output)) { Write-Host $output.TrimEnd() }
    } finally {
        if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
        $process.Dispose()
    }
}

function Assert-NativeReport {
    param([string]$Report, [uint32]$CodePage)
    foreach ($path in @($Report, ($Report + '.child'))) {
        if (-not (Test-Path -LiteralPath $path)) { throw "未生成原生回归报告：$path" }
        $values = @{}
        foreach ($line in Get-Content -LiteralPath $path) {
            $key, $value = $line -split '=', 2
            $values[$key] = $value
        }
        if ($values.failures -ne '0' -or $values.acp -ne "$CodePage") {
            throw "原生回归失败：$path（ACP=$($values.acp)，failures=$($values.failures)）"
        }
    }
}

Invoke-RegressionProcess (Join-Path $product 'ManagedRegression.exe') @() 'managed.log'
$probe = Join-Path $repository 'Build/Release/NativeProbe.exe'
$targetDirectory = Join-Path $ResultsDirectory 'target with spaces 日本語'
New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
$target = Join-Path $targetDirectory 'Native probe.exe'
Copy-Item -LiteralPath $probe -Destination $target -Force
$cases = @(
    @{ CodePage = 932; Locale = 'ja-JP'; Timezone = 'Tokyo Standard Time' },
    @{ CodePage = 936; Locale = 'zh-CN'; Timezone = 'China Standard Time' }
)
# 只限制本轮隔离副本的文件创建权限，验证独立配置启动不需要写安装目录。
$originalAcl = Get-Acl -LiteralPath $product
$readOnlyAcl = Get-Acl -LiteralPath $product
$identity = [Security.Principal.WindowsIdentity]::GetCurrent().User
$rights = [Security.AccessControl.FileSystemRights]::CreateFiles -bor [Security.AccessControl.FileSystemRights]::CreateDirectories
$rule = [Security.AccessControl.FileSystemAccessRule]::new($identity, $rights, [Security.AccessControl.AccessControlType]::Deny)
$readOnlyAcl.AddAccessRule($rule)
try {
    Set-Acl -LiteralPath $product -AclObject $readOnlyAcl
    foreach ($case in $cases) {
        $codePage = $case.CodePage
        $configuration = @"
<LEConfig><Profiles><Profile Name="Regression" Guid="00000000-0000-0000-0000-000000000001" MainMenu="false">
<Parameter></Parameter><Location>$($case.Locale)</Location><Timezone>$($case.Timezone)</Timezone>
<RunAsAdmin>false</RunAsAdmin><RedirectRegistry>true</RedirectRegistry><IsAdvancedRedirection>false</IsAdvancedRedirection><RunWithSuspend>false</RunWithSuspend>
</Profile></Profiles></LEConfig>
"@
        $configuration | Set-Content -LiteralPath ($target + '.le.config') -Encoding utf8NoBOM
        $report = Join-Path $ResultsDirectory "native-$codePage.txt"
        foreach ($path in @($report, ($report + '.child'))) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
        }
        Invoke-RegressionProcess (Join-Path $product 'LEProc.exe') @('-run', $target, $report, "$codePage", $targetDirectory) "native-$codePage-launch.log"
        Assert-NativeReport $report $codePage
        if (Test-Path -LiteralPath $globalConfiguration) { throw '独立配置启动创建了无关的全局配置。' }
        Write-Host "PASS NativeCodePage$codePage (conversion, font enumeration, working directory, child)"
    }
    Write-Host 'PASS ReadOnlyLauncherDirectory'
    $rootDirectory = [IO.Path]::GetPathRoot($targetDirectory)
    $rootReport = Join-Path $ResultsDirectory 'native-root.txt'
    foreach ($path in @($rootReport, ($rootReport + '.child'))) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Invoke-RegressionProcess (Join-Path $product 'NativeLaunch.exe') @($product, $target, $rootReport, '932', $rootDirectory) 'native-root-launch.log'
    Assert-NativeReport $rootReport 932
    Write-Host 'PASS NativeRootWorkingDirectory'
} finally {
    Set-Acl -LiteralPath $product -AclObject $originalAcl
}

# 仅打开与关闭测试目录中的窗口，不点击安装按钮或注册 Shell 扩展。
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class RegressionWindow {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder name, int count);
}
'@
$uiDirectory = Join-Path $ResultsDirectory 'ui-smoke'
if (Test-Path -LiteralPath $uiDirectory) { Remove-Item -LiteralPath $uiDirectory -Recurse -Force }
New-Item -ItemType Directory -Path $uiDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $BuildDirectory '*') -Destination $uiDirectory -Recurse -Force
$uiResults = @()
foreach ($entry in @(@{ Name = 'LEGUI.exe'; Class = 'HwndWrapper' }, @{ Name = 'LEInstaller.exe'; Class = 'WindowsForms10.Window' })) {
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $uiDirectory $entry.Name))
    $start.UseShellExecute = $false
    $start.WorkingDirectory = $uiDirectory
    $process = [Diagnostics.Process]::Start($start)
    try {
        if (-not $process.WaitForInputIdle(15000)) { throw "界面初始化超时：$($entry.Name)" }
        for ($attempt = 0; $attempt -lt 50 -and $process.MainWindowHandle -eq [IntPtr]::Zero; $attempt++) {
            if ($process.WaitForExit(100)) { break }
            $process.Refresh()
        }
        $name = [Text.StringBuilder]::new(256)
        if ($process.HasExited -or [RegressionWindow]::GetClassName($process.MainWindowHandle, $name, $name.Capacity) -eq 0 -or -not $name.ToString().StartsWith($entry.Class)) {
            throw "未创建预期的应用主窗口：$($entry.Name)，窗口类：$name"
        }
        $uiResults += [ordered]@{ executable = $entry.Name; window_class = $name.ToString(); started = $true }
        Write-Host "PASS WindowStartup $($entry.Name)"
        if (-not $process.CloseMainWindow() -or -not $process.WaitForExit(5000)) {
            throw "主窗口不能正常关闭：$($entry.Name)"
        }
    } finally {
        if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
        $process.Dispose()
    }
}
$uiResults | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ResultsDirectory 'ui-smoke.json') -Encoding utf8NoBOM
[ordered]@{ passed = $true; native_code_pages = @(932, 936); product_directory = $BuildDirectory; completed_at = (Get-Date).ToUniversalTime().ToString('o') } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ResultsDirectory 'result.json') -Encoding utf8NoBOM
