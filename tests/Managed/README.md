# 托管回归程序

需要 Windows、VS2019 MSBuild 和 .NET Framework 4 targeting pack；没有 NuGet 或现代 .NET SDK 依赖。项目保持旧格式、.NET 4 Client Profile、x86。

在仓库根目录用 PowerShell 构建并运行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe' .\tests\Managed\ManagedRegression.csproj /m:1 /p:Configuration=Release /p:PostBuildEvent= /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Managed regression build failed.' }
& .\tests\Managed\bin\Release\ManagedRegression.exe
if ($LASTEXITCODE -ne 0) { throw 'Managed regression tests failed.' }
```

`PostBuildEvent=` 禁用安装器资源和外部语言文件的构建后复制动作；本项目只构建自身及 LECommonLibrary、LEProc、LEGUI 引用，不构建根解决方案或原生项目。WPF 使用程序集内置的默认语言资源。任一测试失败时 exe 返回 1。

完整产品回归使用根目录 `Scripts/Test.ps1`：它把测试 exe 放到指定产品的隔离副本旁，加载该产品的 Common、LEProc 和 GUI。运行时位置检查会拒绝从 GAC、源码输出目录或其他位置加载程序集；直接运行上述测试输出目录时，验证范围为该目录中随项目引用复制的产品。

覆盖 Windows `CommandLineToArgvW` 的参数往返（空参数、空白、嵌入引号、尾部反斜杠和 Unicode）、显式配置参数原样优先、XML 配置所有字段及顺序、缺失可选字段默认值、无效 XML 返回空数组的现有契约、默认配置创建与已有文件保护、独占锁下保存失败抛出异常。

工作目录测试把回归 exe 和公共库复制到临时目标目录，反射调用实际产品 `LEProc.exe` 内部启动计划给出的路径、命令行和工作目录，验证子进程参数及相对文件读取。没有编译一份启动计划源码副本，也没有添加生产测试接口。临时目录在测试结束后删除。

WPF 回归在 STA 线程中实例化真实配置窗口，锁定文件后触发保存和快捷方式按钮，验证应用保存异常会中止后续操作、窗口仍可编辑并重试保存；全局保存和另存为失败时内存列表与文件保持一致，解锁后能重试。测试通过反射调用现有事件或保存方法，不比较界面文案，不添加生产测试接口。

旧 GUI 二进制下，全局保存和另存为回归分别检出“保存失败后仍更改内存配置”和“保存失败后仍添加配置”；编译修复后的 GUI 后通过。Dispatcher 错误对话框、删除确认、另存为模糊遮罩清理及启动提权流程仍需交互式 UI smoke。

共享命令行接口位于 `LECommonLibrary.WindowsCommandLine`：

- `QuoteArgument(string argument)` 编码一个已解析参数。
- `JoinArguments(IEnumerable<string> arguments)` 编码参数序列。

两者拒绝 null，单个参数也拒绝 Windows 命令行无法表示的 NUL 字符。空参数合法。不要把已经是原始 Windows 命令行的 `profile.Parameter` 传入这些接口。

实现前已用 Windows 解析器复现旧 LEProc 空格拼接和旧提权套引号算法的参数损坏；还调用旧 Release 公共库确认锁定目标文件时保存 API 会吞掉写入异常。相应回归现在验证实际参数边界及调用者可观察到的保存失败，不比较界面文案。
