# 托管回归程序

需要 Windows、VS2019 MSBuild 和 .NET Framework 4.8 运行时。项目保留旧 csproj 和 x86 测试宿主；微软 reference assemblies 作为构建期 NuGet 包统一定义在根 `Directory.Build.props`，不依赖管理员安装 targeting pack 或现代 .NET SDK。

从仓库根目录完成产品构建后，独立复测实际产品：

```powershell
pwsh -NoProfile -File ./build.ps1
pwsh -NoProfile -File ./Scripts/Test.ps1 `
  -BuildDirectory ./Build/Release `
  -ResultsDirectory ./artifacts/tests/manual
```

`Scripts/Test.ps1` 将测试程序放到指定产品的隔离副本旁，加载该产品的实际程序集和语言文件。位置检查拒绝从 GAC、其他输出目录或源码副本加载产品；内部启动计划通过实际 LEProc 程序集调用。任一测试失败时宿主返回非零，不写整轮成功标记。不要用缺少产品资源的测试输出目录代替完整产品验收。

参数回归使用 Windows `CommandLineToArgvW` 往返，并由真实子进程读取参数、工作目录及相对文件。显式配置中的 Parameter 保持原始 Windows 命令行片段优先，不再次编码。

配置回归覆盖字段、顺序、旧字段默认值、无效 XML 读取契约、默认配置创建和已有文件保护。写入检查包括独占锁、非法 XML 控制字符及孤立代理项，确认原文件字节保持、临时文件清理和修正后重试。

WPF 回归在 STA 线程实例化真实配置窗口，触发保存和快捷方式事件，确认失败不继续启动或关闭，持久化失败不改变内存列表；不可用区域或时区必须重新选择后保存。独立语言子进程读取只读翻译并在加载后移动文件，检查实际语言选择及句柄释放。它们验证行为，不锁定提示文案。

实际数量和失败详情以每轮 `managed.log` 为准。Dispatcher 的交互式对话框、UAC 以及完整 Shell 安装需要各自的验收，不能由参数或配置测试推定。
