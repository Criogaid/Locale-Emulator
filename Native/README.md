# Locale Emulator 原生内核

本目录基于 [xupefei/Locale-Emulator-Core](https://github.com/xupefei/Locale-Emulator-Core) 的 `ae7160dc5deb97947396abcd784f9b98b6ee38b3` 导入。原始完整提交历史及当时的工具链归档保留在 Git 历史中；当前构建使用官方工具链，不需要解压或执行原来的改版编译器、WDK 或预编译 MyLib/undoc 库。

## 构建

安装 Visual Studio 2019 Build Tools 的 C++ 构建工具。工具集 v142 和 Windows SDK 10.0.19041.0 的版本统一定义在 [Toolchain.props](Toolchain.props)；已验证 MSVC 14.29.30133。内核仅构建 `Release|Win32`，不支持原生 x64 目标。产品启动器为托管 LEProc，旧独立 Loader 项目已退役。

从仓库根目录执行（PowerShell）：

```powershell
$msbuild = "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe"
& $msbuild Native/LoaderDll/LoaderDll.vcxproj /p:Configuration=Release /p:Platform=Win32
& $msbuild Native/LocaleEmulator/LocaleEmulator.vcxproj /p:Configuration=Release /p:Platform=Win32
& $msbuild tests/Native/NativeProbe.vcxproj /p:Configuration=Release /p:Platform=Win32
& $msbuild tests/Native/NativeLaunch.vcxproj /p:Configuration=Release /p:Platform=Win32
```

DLL、测试程序与符号输出到 `Build/Release`；中间产物按项目隔离在 `Build/obj`。`OfficialToolchain.targets` 使用已安装的官方 `lib.exe` 从 `NativeNtdll.def` 生成系统 ntdll 的补充导入库。没有外部二进制库下载步骤。NLS 结构使用源码原有定义并检查 Win32 布局，Mount Manager 协议仅保留源码实际使用的结构。

官方 UCRT 头文件的宽字符格式化参数与 ntdll 旧接口不同，源码通过有界 `_vsnwprintf` 适配，避免引入 UCRT DLL。官方静态 `libvcruntime.lib` 提供 SEH 支持；最终内核还导入系统 KERNEL32，USER32/GDI32/dbghelp 延迟加载。相比旧发布版仅直接导入 ntdll，这里存在依赖差异；不据此宣称所有旧 Windows 版本兼容。保留原注入顺序、Win32 调用约定及 hooks，未以关闭 hooks 换取构建成功。

## 运行验证

探针只包含标准 `Windows.h`，不注册组件或修改系统区域设置：

```text
NativeProbe.exe <result-path> <expected-acp> [expected-working-directory]
```

结果为 UTF-8 `key=value` 文本。父进程默认创建同一程序的子进程，子报告为 `<result-path>.child`；父进程等待子进程最多 15 秒，必要时仅终止它自己创建的进程。建议外层测试设置至少 30 秒超时。退出码 0 表示 ACP、默认双向转换、显式 932/1252/UTF-8 转换、字体 charset、工作目录和子进程继承均通过。非零退出码按位表示：1 ACP、2 默认转换、4 显式转换、8 字体、16 工作目录、32 子进程、64 报告写入；参数错误返回 128。字体检查依赖本机已安装字体。

产品集成测试应通过真实 LEProc 启动探针。另有测试专用 `NativeLaunch.exe <dll-directory> <probe-path> <result-path> <932|936> <working-directory>`，用于对照源码构建与未修改的发布 DLL。辅助器自身必须放在待测试 DLL 对所在目录，因为原始 LoaderDll 按宿主 EXE 目录寻找 LocaleEmulator.dll；它等待探针最多 20 秒。

Windows 11 x64 24H2（26100.9168）已验证源码构建与官方 v2.5.0.1 在相同探针下的 932/936 父子进程结果均为 0；真实 LEProc 启动源码构建也通过。编译/导入表与探针报告位于仓库的 `artifacts/logs`，属于本机验证产物。

## 来源与许可证

原作者将 Loader、LoaderDll 和 LocaleEmulator 源码按 LGPL-3.0 发布，具体条款见 [LICENSE-LGPL.txt](LICENSE-LGPL.txt)，其引用的 GPL 条款见 [LICENSE-GPL.txt](LICENSE-GPL.txt)。保留原作者归属及文件内声明。本目录中的官方工具链迁移与缺陷修复不改变原代码的许可证义务；分发修改后的内核时应依许可证提供对应源码及声明。
