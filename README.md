# Locale Emulator

在一个仓库中维护 Locale Emulator 的 C# 前端和原生 Core，统一构建、验证及打包。现有配置界面和传统右键菜单继续沿用。

当前维护范围是 **Windows 11 x64 主机运行 x86 或 x64 目标程序**。默认 `build.ps1` 生成 x86 包；`build-dual.ps1` 在同一官方工具链上额外生成独立的 x64 包，ARM64 主机仍需要独立的兼容性工作。

## 构建

需要安装：

- Git 和 PowerShell 7。
- Visual Studio 2019 Build Tools：MSBuild、MSVC v142 x86/x64 工具、Windows SDK `10.0.19041.0`。原生版本统一定义在 [Toolchain.props](Native/Toolchain.props)。
- .NET Framework 4.8 运行时（Windows 11 已提供）。托管目标统一定义在 [Directory.Build.props](Directory.Build.props)；首次构建从 NuGet 恢复微软官方 reference assemblies，无需管理员安装 targeting pack，也不需要现代 .NET SDK。引用包仅用于构建，不进入发行包。

在仓库目录运行：

```powershell
pwsh -NoProfile -File ./build.ps1
```

该入口会编译五个托管项目和两个原生 DLL，构建回归程序，运行测试，收集完整发行文件，再从 ZIP 解包复测。遇到构建、测试或文件校验失败会停止，不产生成功标记。

需要同时生成两个架构包时运行：

```powershell
pwsh -NoProfile -File ./build-dual.ps1
```

该入口先完成 x86 的完整构建和回归，再用 Hostx86\x64 官方编译器构建 x64 原生 Core、Loader 和 x64 LEProc，分别装配 ZIP；两包会解包复核 SHA256，并对 x64 Loader 执行目标进程启动烟测。


原生构建使用官方编译器、Windows SDK 及系统 DLL 导出生成的导入库，不依赖 `_Compilers`、旧 WDK 或预编译 MyLib 库。相关旧文件保留在上游提交历史中。

主要输出：

| 位置 | 内容 |
| --- | --- |
| `Build/Release/` | x86 前端、原生 DLL 和测试程序 |
| `Build/Dual/x64/` | x64 原生与托管构建中间产物 |
| `artifacts/Locale.Emulator.<版本>.zip` | 默认 x86 完整便携发行包 |
| `artifacts/package-dual/Locale.Emulator.<版本>-x86.zip` | x86 双架构发行入口生成的 x86 便携包 |
| `artifacts/package-dual/Locale.Emulator.<版本>-x64.zip` | x86 双架构发行入口生成的 x64 便携包 |
| `artifacts/package/Locale.Emulator.<版本>/` | 默认包的打包目录、构建提交信息与 SHA256SUMS |
| `artifacts/logs/` | 各工程构建日志 |
| `artifacts/tests/source/` | 源码构建产物的运行报告 |
| `artifacts/tests/package/` | 实际 ZIP 解包后的运行报告 |

Visual Studio 可打开 `LocaleEmulator.sln` 构建默认 x86 产品；x64 原生与 LEProc 的发布配置由 `build-dual.ps1` 调用官方 Hostx86\x64 工具链完成。发布和回归以构建脚本的 Release 配置为准。

## 使用

将发行包解压到可写目录。直接指定 EXE 时，配置优先级为应用自己的 `.le.config`、第一个全局配置、默认日文配置：

```powershell
./LEProc.exe 'D:/Games/My Game/game.exe' 'argument with spaces'
```

使用 `LEGUI.exe` 编辑全局配置；将目标程序拖到 LEGUI 可创建应用配置，再通过 `LEProc.exe -run <目标程序> [参数]` 使用它。配置中的 Parameter 仍是完整 Windows 命令行片段，并优先于额外传入的参数。

需要传统右键菜单时，可手动运行 `LEInstaller.exe` 安装或卸载。Windows 11 下该菜单位于“显示更多选项”；新式一级菜单尚未加入。构建回归只打开和关闭安装器，不点击安装按钮，也不修改 Shell 注册。

界面语言跟随 Windows 的 UI 语言。GUI 与 Shell 菜单保留仓库现有语言资源；安装器和 LEProc/公共错误提示提供英文、简体中文，并在缺少对应翻译时回退到英文。发行包中的 `Lang`、`InstallerLang` 和 `CommonLang` 目录可随包更新，不会改变目标程序的区域模拟语言。

已移除访问归档上游 HTTP 清单的自动更新器。更新通过本仓库的源码或后续正式发行包完成。

## 回归验证

独立复测已构建的产品：

```powershell
pwsh -NoProfile -File ./Scripts/Test.ps1 `
  -BuildDirectory ./Build/Release `
  -ResultsDirectory ./artifacts/tests/manual
```

托管回归使用指定产品的实际程序集，覆盖加载位置、Windows 命令行参数往返、真实子进程的工作目录和相对文件访问、配置完整读写、非法 XML 输入与原子保存、锁定文件、不可用区域或时区、只读翻译及实际 WPF 保存流程。原生探针通过真实 LEProc 注入，分别检查 CP932/936 的默认转换、显式异码页转换、字体枚举字符集、Unicode/空格目录和子进程继承。另外验证只读安装目录的独立配置启动，以及盘符根目录的父子进程工作目录。GUI 与安装器主窗口也接受启动和正常关闭检查；实际测试数量见每轮的 `managed.log`。

每轮在 `environment.json` 记录 Windows DisplayVersion、build、UBR、主机架构和两个 Core DLL 的 SHA256。`result.json` 仅在整轮通过后写入。字体探针验证 API 枚举行为，不代替实际游戏的像素显示、输入法或字体布局测试。

原版 v2.5.0.1 与源码构建的原生对照、已复现的问题及验证范围见 [维护记录](docs/MAINTENANCE.md)。

## 仓库结构与来源

C# 工程保留原目录；`Native/` 包含注入器与语言模拟实现；`tests/Managed` 和 `tests/Native` 保存无需第三方测试依赖的回归程序。两个仓库的原始提交历史已经合并，来源 SHA 和导入方式见 [源码来源](docs/SOURCES.md)。

## 许可

前端保留 LGPL-3.0 的 `COPYING`、`COPYING.LESSER`。传统 Shell 扩展中的 Microsoft 示例代码另按 Ms-PL 授权，图标保留上游来源。

Core 保留 `Native/LICENSE-GPL.txt`、`Native/LICENSE-LGPL.txt`，其上游 README 明确授予 Loader、LoaderDll、LocaleEmulator 三个源码目录 LGPL-3.0。具体来源、第三方边界及 Fork 调查取舍见 [源码来源](docs/SOURCES.md)。发行包携带许可文本及来源记录。

上游：[Locale-Emulator](https://github.com/xupefei/Locale-Emulator)、[Locale-Emulator-Core](https://github.com/xupefei/Locale-Emulator-Core)。
