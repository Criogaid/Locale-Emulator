# Monorepo 维护记录

## 当前目标与实现

本轮完成前端与 Core 的历史合并、统一构建与发行、托管及原生回归，以及官方原生工具链迁移。维护范围为 Windows 11 x64 主机上的 x86 与 x64 目标程序；默认包保持 x86，双包入口生成根目录 x64 LEProc/Loader/Core，并在 `x86/` 携带对应的 x86 LEProc/Loader/Core 与公共库，界面与 Shell 托管程序集保持 AnyCPU。

合仓提交为 `8d41af63e4a1b163da2a2f654b6a83b33d58e060`，父提交分别是前端 `2f3f30dc5169b3cf051af389ec1dcc52f167a93d` 和 Core `ae7160dc5deb97947396abcd784f9b98b6ee38b3`。Core 的 31 个原始提交均保留；导入时 Native tree 与原始 Core tree 相同。来源和许可见 [SOURCES.md](SOURCES.md) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

`build.ps1` 是 x86 完整产品构建入口，`build-dual.ps1` 在其通过后追加 x64 原生与托管构建并装配两个架构包，`Scripts/Test.ps1` 是构建产物的回归入口。正式版本取自 `LEProc/LEVersion.xml`；原生工具集和 SDK 取自 `Native/Toolchain.props`。完整入口先完成全部项目的 Clean，再 Build，随后构建测试宿主；安装器按项目引用内嵌实际 DLL，不向源码目录复制二进制，也不重复认领 GUI/启动器的输出。脚本自动收集前端、Core DLL、语言资源、许可、来源、构建提交与文件哈希，并验证实际 ZIP 解包产物。包内保留工具链元数据，解包后检查随包文档的相对链接。

多语言资源按产品界面职责分组：`LEGUI/Lang` 保留现有 20 种 WPF 界面语言，`LEContextMenuHandler/Lang` 保留现有 20 种 Shell 菜单语言；安装器使用 `InstallerLang`，LEProc 与公共诊断使用 `CommonLang`。托管界面按 `CurrentUICulture.Name`、两字母语言名、内置英文默认资源依次选择；GUI 对已存在但缺少新键的语言也回退到英文默认键。当前安装器、启动器和公共诊断提供英文与简体中文资源，其他语言继续显示英文回退文本，不影响目标进程的区域模拟语言。x86 与 x64 包均携带三类资源目录。

## 原版行为基线与工具链迁移

行为基线采用上游正式发行版 v2.5.0.1 的两个原生 DLL。下载包为 `Locale.Emulator.2.5.0.1.zip`，SHA256：

```text
808ff584426d52cc775ad6406da00622f454be95bd4c8fbca42eef4b7235ad5c
```

先用同一原生探针和测试启动器对照原版及源码 DLL，再用当前 LEProc 分别装配两套 DLL 复测。源码构建与原版 DLL 均通过 CP932、CP936 的父子进程检查。对照只替换两个原生 DLL，避免旧前端的参数拼接缺陷干扰 Core 比较。

官方工具链迁移使用 MSVC v142、Windows SDK 19041，不执行仓库附带的修改编译器。删除当前树中不再使用的工具链归档、旧头文件及独立 Loader 工程；产品启动器为 LEProc，原始文件和历史保留在合仓祖先中。

原生源码的必要改动：

- 移除旧 MyLib/undoc 库链接依赖，用官方 `lib.exe` 为已导出的系统符号生成导入库。
- 使用官方 SDK 的头文件和内建指令声明，保留必须的原生私有声明与 Win32 NLS 布局断言。
- 修复注册表封装中临时 UNICODE_STRING 地址在下一条系统调用前失效的问题：原版写法在官方优化器下使初始化返回 `STATUS_OBJECT_PATH_SYNTAX_BAD`。四个调用点改用具名对象，原生注入回归覆盖修复后的实际路径。
- 对旧式宽字符格式化调用提供有界适配，避免现代声明与旧系统导出调用约定错配；运行时解析 CreateProcessInternalW。
- 补全 `LeSetupAnsiOemCodeHashNodes` 成功路径返回值，并将缺失返回的 C4715 作为构建错误。

迁移后 Core 由于官方 SEH/GS 支持增加直接 KERNEL32 导入，没有 UCRT 或 VC runtime DLL 依赖。当前两产品启用 W4/WX、GS、ASLR、DEP 和 SafeSEH，正常及私有映像入口均使用官方 cookie 初始化，代码与数据分节、跳板发布为 RX；Loader 另启用 CFG。核心完整 CFG 的实际失败与私有 NLS 依赖见 [QUALITY.md](QUALITY.md)。本机早期注入及行为回归通过，其他系统版本需相同验证。

## 业务目的与替代方案研究

项目的业务目的仍是启动一个不配合改造的旧 Windows 目标进程，让它在自己的进程边界内看到指定的 ACP/OEMCP、LocaleID、UI 语言、时区、注册表和字体行为；它不负责翻译游戏内容。LEProc 负责配置和命令行，Loader 负责创建并注入目标，Core 负责目标进程内的兼容行为。

公开 API 只能覆盖其中一部分：[`SetThreadLocale`](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-setthreadlocale) 只改变调用线程的 Locale；[`SetProcessPreferredUILanguages`](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-setprocesspreferreduilanguages) 只设置进程首选 UI 语言；[`activeCodePage`](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page) 只提供 UTF-8 清单声明，官方文档明确说明 GDI 不支持按进程设置它。公开代码页 API也不能把任意旧程序的 ACP/OEMCP 变成目标值，公开时区和注册表 API也不能提供按目标进程的覆盖层。

因此，改成纯公开 API、UTF-8 清单或 Windows 兼容性数据库都不能保持当前业务目的；它们只能作为目标程序自身愿意配合时的补充方案。x86 与 x64 Loader/Core 的分叉属于 ABI、PEB 布局和系统调用实现的运行时边界，强行合并会把架构条件扩散到 hook 代码，维护收益小于兼容风险。当前有证据的重构集中在 LEProc：启动失败现在返回非零退出码，提权失败和无文件关联也不再伪装成成功；区域解析在一次 `CultureInfo` 读取中完成，避免同一启动路径重复解析。

## 已复现并修复的托管问题

- 旧 LEProc 用空格连接已解析参数，提权路径简单包引号，导致空参数、空格、嵌入引号及尾部反斜杠丢失。现由 `WindowsCommandLine` 统一编码，启动器、提权和 GUI 生成的命令使用同一实现。
- 配置中的 Parameter 继续作为用户明确输入的原始命令行片段，保持其优先级。目标工作目录由实际目标位置确定。
- 配置保存失败曾被吞掉；现在传出文件异常。WPF 文件错误处理保留窗口，失败保存不继续启动程序，也不提前变更全局配置列表。
- 应用配置路径只移除末尾 `.le.config`，不会替换目录中的同名片段。缺参数、缺目标和无效配置产生可观察的错误反馈。
- 删除向归档上游 HTTP 清单请求更新的组件及调度，清除其日期 API 和解决方案引用。

## 实际验证

测试主机：Windows 11 IoT 企业版 LTSC / 24H2 / x64 / **26100.9168**；VS2019 Build Tools 16.11、MSVC 14.29.30133。每轮回归重新记录 CIM Caption、系统 Edition、DisplayVersion、build、UBR 和实际产品哈希，避免注册表 ProductName 的历史字符串把 Windows 11 误标为 Windows 10。

| 检查 | 实际验证内容 |
| --- | --- |
| 完整源码构建 | 五个托管工程与 LoaderDll、LocaleEmulator 两个原生 DLL |
| 托管回归 | Windows 参数往返、提权/关联参数和真实子进程工作目录、XML 换行与字段读写、原子保存、WPF 状态与只读翻译、时区名称/标准时、语言规则、P/Invoke 可写缓冲区与异常资源回收 |
| CP932 与 CP936 原生回归 | 真实 LEProc 注入；默认/显式转换、字体字符集、Unicode 路径、目录、子进程继承；218 项 owner-draw 控件组合与自定义类边界，明确记录原生 thunk 跳过数 |
| 原生 ABI 与防护 | LEB 保护页/溢出、注册表短缓冲区/名称/canary、目录扩容、真实失败注入和回滚；RX 跳板、显式长度保护页、字体指标、EDIT 消息尾部、私有映像权限和 GS cookie、CFG 宿主 |
| 安装与 Shell | x86/x64 受控宿主验证 DLL 锁定/回滚、内嵌实际产品一致性、注册失败和映射恢复、命令 ID 预算/插入位置/占用数、GDI 与注册表句柄回收 |
| 只读目录与根目录 | 对隔离产品副本拒绝文件创建，验证独立配置启动无需写安装目录；原生辅助器验证盘符根目录中的父子进程工作目录 |
| 主窗口启动 | GUI 与安装器实际创建对应框架主窗口并正常关闭；没有点击安装按钮或注册扩展 |
| 发行包复测 | 从实际 ZIP 解包，逐文件 SHA256 验证，再通过相同产品运行回归 |
| 干净检出集成 | 在没有 Build、bin 或 artifacts 的独立工作树运行完整构建及发行包复测 |

双包入口的 x64 验收检查根目录与 `x86/` 子目录中各自的 `LEProc.exe`、`LoaderDll.dll` 和 `LocaleEmulator.dll` 的 PE machine、ASLR/NX 属性，并在解包后的 x64 包中分别用私有临时副本启动 x64 与 x86 `cmd.exe`。Shell 扩展根据目标 PE 架构选择根目录或 `x86/` helper；GUI、安装器和 Shell 托管程序集是 AnyCPU 且关闭 Prefer32Bit。

本机运行报告位于 `artifacts/tests/`：`environment.json` 记录环境和产品哈希；`managed.log`、`shell64.log` 记录双架构检查；`native-contracts.log`、`native-directory.log`、`hardening-check.log` 和 `hardening-*.txt` 记录边界及防护；`native-932.txt`/`native-936.txt` 与 `.child` 记录父子结果；`ui-smoke.json` 记录窗口。`result.json` 仅在整轮成功后生成。包内 `build-info.json` 区分提交构建与有未提交修改的开发构建。

测试覆盖具体 API 行为及基本启动，不代表实际游戏的完整渲染、IME、保护程序、交互式 UAC 或所有 Windows 累积更新兼容性。新式 Win11 一级菜单作为后续独立模块，当前使用传统菜单。

## 独立审查后的回归修正

- 实际复现把发行目录的 GUI 换成旧版本后，旧测试入口仍错误通过。现将测试 exe 与指定产品程序集装配在同一隔离目录，并验证运行时程序集位置；内部启动计划也来自产品程序集。相同旧 GUI 现在触发两项 WPF 行为失败，并且不写入成功标记。
- CLI 不再无条件创建全局配置，由 GUI 负责创建和编辑。独立配置启动在拒绝文件创建的安装目录中通过，避免全局配置写入阻断无关命令。
- 解析器复现原生测试程序把根目录尾部反斜杠当作引号转义。两个原生测试程序共用参数编码，并以真实 Windows 解析器自检；真实注入的根目录父子回归通过，保留超时和子进程退出检查。

## 前期 Fork 调查的采用决定

| 来源 | 当前决定 |
| --- | --- |
| ParkSnoopy 的框架/工具链升级 | 适配到统一 Framework 4.8 目标、官方构建期 reference assemblies 和按目标生成的运行时配置；保留旧项目结构和 x86 边界 |
| dual1208 的打包/LocalState/静态菜单 | 采用自动汇总完整发行文件的方向；保留当前便携配置位置，未引入新的安装模式和发行身份 |
| KidiXDev 的 CLI 发布与 Core 子模块 | 不带入只覆盖 CLI 的裁剪；本项目统一构建完整产品，Core 原本没有净修复可移植 |
| Cliff-Yang 的 owner-draw 消息修补 | 真实控件复现后按消息、实际系统类和 HASSTRINGS 语义重写，并覆盖 superclass；不复制原补丁过宽的样式判断 |
| SashiRin 的字体 hook 与 MUI 时区名 | 采用独立 DaylightName 的明确修正；保留标准时契约并验证 SDK 名称边界。字体 hook 和 MUI 分支仍等待当前目标的具体故障证据 |
| julixian 的 LocaleEmulatorPlus-Core x64 迁移 | 采用其 x64 源码布局作为移植参考，改用官方 SDK/MSVC 导入库、兼容声明和安全链接选项；修复字体 name 表的堆分配边界。Fork 的 `/GS-`、关闭 ASLR 和 ERW 节合并均拒绝采用，完整决定与 LGPL-3.0 来源见 [SOURCES.md](SOURCES.md) |
| luojunyuan 的 URI/ARM64 路线 | 不纳入当前 x64 主机上的 x86 基线 |
| Locale Remulator 与 ARM64 实验 | 保留为后续 ARM64 路线参考；本轮先完成 x86/x64 双包，未把 ARM64 代码或额外后端混入产品 |

历史调查原件在同级工作区 `Locale-Emulator-comparison`；采用来源的完整 SHA、许可和逐项决定见 [SOURCES.md](SOURCES.md)。仅在新增复现或来源变化时重新评估，避免整体带入 Fork 的机器路径、无净变化提交和未验证功能。

## 循环审查与构建依赖整理

第 1 轮之后的缺陷、复审轮次、行为变化和官方支持边界统一记录在 [QUALITY.md](QUALITY.md)。托管项目通过根 `Directory.Build.props` 使用官方 Framework 4.8 reference assemblies；`Directory.Build.targets` 从目标 moniker 生成实际 EXE 运行时配置，发行包携带三份 `.exe.config`。

VS2019 的旧 WPF 临时工程不自动导入 NuGet targets，因此共享参考程序集根目录；恢复与构建分开启动 MSBuild，并禁用节点重用，避免首次下载之前的路径缓存影响后续构建。配置值不从开发机已安装的旧 targeting pack 回退。

已删除无调用且固定 SDK16299 的旧签名脚本。当前本地构建输出未签名；发行者选择证书后的签名应发生在最终哈希和打包之前，Strong Name 不能代替发布者签名。

## 历史提交元数据说明

本轮审查发现历史提交 `3057efd`、`6f3bf66`、`e4dadeb`、`f996108` 和 `beeff20` 的提交正文含有未展开的 `Co-Authored-By: :PI_MODEL <noreply@pi.dev>`。这些提交已经成为当前本地分支的历史，源码、构建和发行包不读取该字段；本轮不改写历史，避免改变已记录的里程碑哈希、审查证据和后续父子关系。自 `d6cefaa` 起的新提交均在提交前读取实际 `PI_MODEL`，使用完整的协作者值；若发行者要求历史元数据重写，应作为独立的仓库维护操作重新生成全部受影响哈希并重建证据目录。
