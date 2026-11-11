# 托管回归程序

需要 Windows、VS2019 MSBuild 和 .NET Framework 4.8 运行时。项目保留旧 csproj 和 x86 测试宿主；微软 reference assemblies 作为构建期 NuGet 包统一定义在根 `Directory.Build.props`，不依赖管理员安装 targeting pack 或现代 .NET SDK。

从仓库根目录完成产品构建后，独立复测实际产品：

```powershell
pwsh -NoProfile -File ./build.ps1
pwsh -NoProfile -File ./Scripts/Test.ps1 `
  -BuildDirectory ./Build/Release `
  -ResultsDirectory ./artifacts/tests/manual
```

`Scripts/Test.ps1` 将测试程序放到指定产品的隔离副本旁，加载该产品的实际程序集和卫星程序集。位置检查拒绝从 GAC、其他输出目录或源码副本加载产品及实际选用的卫星程序集；内部启动计划通过实际 LEProc 程序集调用。任一测试失败时宿主返回非零，不写整轮成功标记。不要用缺少产品资源的测试输出目录代替完整产品验收。

资源源文件由 .NET Framework 的 `ResXResourceReader` 解析，检查非空且不重复的键、字符串值、规范文化名和译文键集合；GUI 允许缺少键，菜单译文必须包含全部四个键。产物检查按源文件推导所需的 `<culture>/<项目>.resources.dll`，核对程序集名称和文化，并拒绝 `Lang`、`CommonLang`、`InstallerLang` 残留；发现 `x86/` 或包元数据声明 x64 架构时，强制检查兼容目录的公共库及其卫星程序集。

`build.ps1` 对构建输出和实际 ZIP 解包产物执行完整回归。双包构建后，可用同一入口只验证资源源文件与包布局（不执行托管、原生或窗口回归，也不写整轮成功标记）：

```powershell
pwsh -NoProfile -File ./build-dual.ps1
pwsh -NoProfile -File ./Scripts/Test.ps1 `
  -BuildDirectory ./artifacts/package-dual/Locale.Emulator.2.5.1.0-x86 `
  -ResultsDirectory ./artifacts/tests/resources-x86 -LocalizationOnly
pwsh -NoProfile -File ./Scripts/Test.ps1 `
  -BuildDirectory ./artifacts/package-dual/Locale.Emulator.2.5.1.0-x64 `
  -ResultsDirectory ./artifacts/tests/resources-x64 -LocalizationOnly
```

包目录中的版本号以本次构建实际输出为准。

参数回归使用 Windows `CommandLineToArgvW` 往返，并由真实子进程读取参数、工作目录及相对文件。显式配置中的 Parameter 保持原始 Windows 命令行片段优先，不再次编码。

配置回归覆盖字段、顺序、旧字段默认值、无效 XML 读取契约、默认配置创建和已有文件保护，包括 CR、CRLF、LF、TAB 与 Unicode 的原样往返。`LEProfile.CreateDefault` 检查调用者提供的名称和提权标记、默认区域与时区、其余全部字段，以及相同输入重复创建时 GUID 有效且不重复；默认配置文件须按顺序包含普通和提权两个配置，并保持这些默认字段。写入检查包括独占锁、非法 XML 控制字符及孤立代理项，确认原文件字节保持、临时文件清理和修正后重试。

启动回归调用产品实际的提权请求、关联参数选择、语言注册表规则和时区序列化，检查相对路径、排序 LCID、标准时契约以及 SDK WCHAR[32] 边界。`LoaderWrapper` 用例构造实际 `RuntimeEnvironment` 并注入，环境参数设置在该对象上；检查环境引用保持、拒绝空环境，以及未提供时区时拒绝启动。原生边界测试在独立子进程使用 SDK 测试 DLL，验证可写命令行、STARTUPINFO 大小、失败状态和句柄回收；缺 DLL 场景以较大的合法环境块重复调用，确保漏释放时的累计分配超过容忍阈值，再检查实际内存增量。测试 DLL 固定输出到 `Build/tests/LoaderInterop`，复制到隔离测试目录的 `LoaderInterop` 子目录，不覆盖产品 DLL，也不进入发行包。

安装/Shell 检查同时运行 x86 托管宿主与 x64 `ShellRegressionHost`。文件锁场景确认两个内嵌 DLL 和所有卫星程序集与覆盖前的实际产品字节一致、主文件与子目录的中途失败回滚、相同已锁文件不重复替换，以及递归备份清理；使用产品位置而非调用者目录。注册失败使用受控注册表映射，检查恢复和失败传播；菜单宿主验证 ID 预算、插入位置和占用数，计数验证位图及注册表句柄释放。

WPF 回归在 STA 线程实例化真实配置窗口，触发保存和快捷方式事件，确认失败不继续启动或关闭，持久化失败不改变内存列表；不可用区域或时区必须重新选择后保存。独立语言子进程验证 `LocExtension` 对普通字符串属性返回字符串，对 `TextElement.FontFamily` 返回 `FontFamily`，英文字体为 `Segoe UI`，日语及其区域文化为 `Meiryo UI`。资源运行期探针通过 `Assembly.LoadFrom` 加载四个实际产品程序集，切换 `CurrentUICulture`，核对简繁中文、日文文化链、未提供翻译的 `sv-SE` 英文回退、GUI 缺键英文回退、未知键返回键名，以及简体中文安装器窗口。

实际数量和失败详情以每轮 `managed.log`、`shell64.log` 为准，系统 build/UBR 和实际产品哈希在 `environment.json`。Dispatcher 的交互式对话框、UAC 以及实际 Explorer 中的完整 Shell 安装需要各自验收，不能由参数、配置或受控宿主测试推定。
