# Locale Emulator 原生内核

本目录基于 [xupefei/Locale-Emulator-Core](https://github.com/xupefei/Locale-Emulator-Core) 的 `ae7160dc5deb97947396abcd784f9b98b6ee38b3` 导入。原始完整提交历史及当时的工具链归档保留在 Git 历史中；当前构建使用官方工具链，不需要解压或执行原来的改版编译器、WDK 或预编译 MyLib/undoc 库。

ownerdraw 修复参考 [Cliff-Yang 的崩溃修补](https://github.com/Cliff-Yang/Locale-Emulator-Core/commit/889fbda1d78e680f50285b8cccff7b44b08630e5) 的问题线索，再按 SDK 控件、HASSTRINGS 和消息契约重新实现，包含 superclass 与自定义类边界；未复制原补丁过宽的样式判断。相关代码继续遵守 Core 的 LGPL-3.0 许可。

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

结果为 UTF-8 `key=value` 文本。父进程默认创建同一程序的子进程，子报告为 `<result-path>.child`；父进程等待子进程最多 15 秒，必要时仅终止它自己创建的进程。建议外层测试设置至少 30 秒超时。退出码 0 表示 ACP、默认双向转换、显式 932/1252/UTF-8 转换、字体 charset、工作目录、子进程继承和控件检查均通过。非零退出码按位表示：1 ACP、2 默认转换、4 显式转换、8 字体、16 工作目录、32 子进程或控件、64 报告写入；参数错误返回 128。字体检查依赖本机已安装字体。

控件矩阵在真实子进程检查 ListBox、ComboBox、两者的 superclass、ownerdraw fixed/variable、HASSTRINGS、有/无字符串、四条 A/W 发送/窗口过程路径，以及自定义类边界。不透明 item data 不能被当字符串读取；普通非 ASCII 文本继续转换。当前执行 218 项，另有 6 项 Unicode superclass 的原生文本 thunk 在未注入 Windows 上同样不转换，明确记录跳过数，未计为通过。

产品集成测试应通过真实 LEProc 启动探针。另有测试专用 `NativeLaunch.exe <dll-directory> <probe-path> <result-path> <932|936> <working-directory>`，用于对照源码构建与未修改的发布 DLL。辅助器自身必须放在待测试 DLL 对所在目录，因为原始 LoaderDll 按宿主 EXE 目录寻找 LocaleEmulator.dll；它等待探针最多 20 秒。

Windows 11 x64 24H2（26100.9168）已验证源码构建与官方 v2.5.0.1 在相同探针下的 932/936 父子进程结果均为 0；真实 LEProc 启动源码构建也通过。编译/导入表与探针报告位于仓库的 `artifacts/logs`，属于本机验证产物。

## 安全选项与支持边界

两个产品项目均使用 `/W4 /WX /MT /GS /DYNAMICBASE /NXCOMPAT /SAFESEH`，链接器也启用 `/WX`。分配器仍由原生源码管理，GS 与 SEH 使用官方静态 `libvcruntime.lib`，不依赖动态 VC 运行库。LoaderDll 仅导出 `LeCreateProcess`。代码、只读数据和可写数据保持分节；跳板独立分配为 RW，生成后改为 RX；子进程私有映像也按节恢复权限。

两个 DLL 的自定义入口在进入 GS 保护代码前调用官方 `__security_init_cookie`。子进程私有映像没有系统 DLL 入口调用，且早期尚未映射 KERNEL32，因此先在父进程的临时映像内调用该映像自身的官方初始化函数，再重定位并传输；子进程入口再次调用官方初始化函数确认已初始化状态。不手工生成或写入 cookie 值。

LoaderDll 已启用编译器 `/guard:cf` 和链接器 `/GUARD:CF`。核心 DLL 暂不启用 CFG：在 Windows 11 x64 24H2 的真实 CFG 检查下，`LeSetupAnsiOemCodeHashNodes` 对扫描所得 KERNELBASE 私有 NLS 函数的间接调用触发 `FAST_FAIL_GUARD_ICALL_CHECK_FAILURE`（10）。手工映射实验必须接入系统实际的 CFG 检查函数；仅保留文件中的 no-op 检查指针所得的通过结果不成立。未修改系统 DLL 的 CFG 目标有效性，也未使用批量 `nocf`。后续开启核心 CFG 需要替换私有 NLS 调用方案，或明确评审针对该调用的安全例外。

微软[建议在所有编译代码上启用 CFG](https://learn.microsoft.com/en-us/cpp/build/reference/guard-enable-control-flow-guard)，这不是所有 Win32 应用必须满足的编译要求。动态可执行页默认可作为 CFG 调用目标；本项目跳板使用该默认行为，未宣称已收紧到每个合法入口。汇编跳转不自动获得 C++ 间接调用检查。

`EM_GETLINE` 的 A/W 转换不附加终止符，并保留调用者缓冲区尾部。ANSI 调用经私有 `NtUserMessageCall` 转成宽字符时，单次最多传输 32767 个 UTF-16 单元，以避免内部 WORD 字节容量乘二后回绕；超过该长度的行只返回本次实际复制量。这是当前私有传输的支持限制，不是公开 `EM_GETLINE` 的通用上限。

官方工具链构建成功不等于使用的全部运行接口都有公开 SDK 支持保证。NLS 数据布局、私有 PEB 字段、指令扫描和系统内部函数仍是本引擎的明确支持边界，须按目标 Windows 版本实测；当前探针通过不能外推到所有旧版本或未来版本。

## ABI、失败路径与防护回归

完整入口自动构建并运行以下项目，对源码产物和实际 ZIP 各执行一次：

- `NativeContracts`：通过唯一的 `LeCreateProcess` 导出验证版本化运行时环境块的保护页、相对字符串偏移和溢出边界、注册表标准/Align64 的短缓冲区、名称和 canary，以及缺 DLL、跨位数非注入路径。测试仅修改自己进程的 IAT，实际触发 section、远程保护、写入、部分写入及恢复线程失败，验证适用的补丁回滚、shadow 释放、句柄清零和子进程退出。
- `NativeDirectoryContracts`：创建受控临时对象目录，64 项长名称迫使枚举缓冲区扩容，验证目标项查找；不修改系统目录。
- `HardeningCheck`：直接编译共享实现以验证 RX 跳板发布、原字节恢复和释放、显式长度保护页、SDK 长度边界、TEXTMETRIC 字符与 EM_GETLINE thunk。测试状态只属于 EXE，不新增产品导出或测试开关。
- `HardeningMessages`：在真实注入的 CP932/936 进程检查 EDIT 消息容量和尾部、私有核心映像无 RWX、官方 security cookie 初始化和宿主 CFG 策略。两种防护测试项目均显式启用 CFG，不依赖环境变量开关。

测试项目统一导入 `Toolchain.props`，输出到 `Build/Release`，中间文件按项目隔离；均不进入发行包。Windows build、UBR 和被测 DLL 哈希由 `Scripts/Test.ps1` 每轮重新记录，精确支持边界及复审问题见根目录 `docs/QUALITY.md`。
## 来源与许可证

原作者将 Loader、LoaderDll 和 LocaleEmulator 源码按 LGPL-3.0 发布，具体条款见 [LICENSE-LGPL.txt](LICENSE-LGPL.txt)，其引用的 GPL 条款见 [LICENSE-GPL.txt](LICENSE-GPL.txt)。保留原作者归属及文件内声明。本目录中的官方工具链迁移与缺陷修复不改变原代码的许可证义务；分发修改后的内核时应依许可证提供对应源码及声明。
