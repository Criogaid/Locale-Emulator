# Monorepo 维护记录

## 当前目标与实现

本轮完成前端与 Core 的历史合并、统一构建与发行、托管及原生回归，以及官方原生工具链迁移。维护范围为 Windows 11 x64 主机上的 x86 目标程序；保留原有界面和传统右键菜单。

合仓提交为 `8d41af63e4a1b163da2a2f654b6a83b33d58e060`，父提交分别是前端 `2f3f30dc5169b3cf051af389ec1dcc52f167a93d` 和 Core `ae7160dc5deb97947396abcd784f9b98b6ee38b3`。Core 的 31 个原始提交均保留；导入时 Native tree 与原始 Core tree 相同。来源和许可见 [SOURCES.md](SOURCES.md) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

`build.ps1` 是完整产品构建入口，`Scripts/Test.ps1` 是构建产物的回归入口。正式版本取自 `LEProc/LEVersion.xml`；原生工具集和 SDK 取自 `Native/Toolchain.props`。完整入口先完成全部项目的 Clean，再 Build，随后构建测试宿主；安装器按项目引用内嵌实际 DLL，不向源码目录复制二进制，也不重复认领 GUI/启动器的输出。脚本自动收集前端、Core DLL、语言资源、许可、来源、构建提交与文件哈希，并验证实际 ZIP 解包产物。包内保留工具链元数据，解包后检查随包文档的相对链接。

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
| luojunyuan 的 URI/ARM64 路线 | 不纳入当前 x64 主机上的 x86 基线 |
| Locale Remulator 与 LE Plus | 保留为后续 x64 路线参考，未复制其实现或混入额外后端 |

历史调查原件在同级工作区 `Locale-Emulator-comparison`；采用来源的完整 SHA、许可和逐项决定见 [SOURCES.md](SOURCES.md)。仅在新增复现或来源变化时重新评估，避免整体带入 Fork 的机器路径、无净变化提交和未验证功能。

## 循环审查与构建依赖整理

第 1 轮之后的缺陷、复审轮次、行为变化和官方支持边界统一记录在 [QUALITY.md](QUALITY.md)。托管项目通过根 `Directory.Build.props` 使用官方 Framework 4.8 reference assemblies；`Directory.Build.targets` 从目标 moniker 生成实际 EXE 运行时配置，发行包携带三份 `.exe.config`。

VS2019 的旧 WPF 临时工程不自动导入 NuGet targets，因此共享参考程序集根目录；恢复与构建分开启动 MSBuild，并禁用节点重用，避免首次下载之前的路径缓存影响后续构建。配置值不从开发机已安装的旧 targeting pack 回退。

已删除无调用且固定 SDK16299 的旧签名脚本。当前本地构建输出未签名；发行者选择证书后的签名应发生在最终哈希和打包之前，Strong Name 不能代替发布者签名。
