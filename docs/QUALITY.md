# 循环审查与官方开发契约

本轮同时使用 overengineering-review、code-health-pass、architecture-review。审查范围覆盖原生注入与 hooks、C# 配置和启动、GUI、安装器、传统菜单、构建、回归与发行。修复按独立里程碑提交；每轮修复后重新核对调用者、失败路径和行为回归，再进行全范围独立复审。

## 第 1 轮问题台账

审查基线为 `3ea9ecde90073a13364d611d82defaef19f80ce3`。本机证据保存在 `artifacts/review-round1/`，系统为 Windows 11 24H2 / x64 / 26100.9168。问题编号用于后续复审追踪，不表示不同代码必须共用一套抽象。

| 编号 | 已确认的触发条件与后果 | 处理状态 |
| --- | --- | --- |
| C01 | XML 不允许的字符使保存截断已有配置 | 已改为同目录临时文件、完整写入和原子替换；保留旧文件，失败不继续启动 |
| C02 | 本机缺少已保存的区域或时区，编辑器以 -1 索引列表 | 保存前要求有效选择；旧配置和内存状态保持，重新选择后可重试 |
| C03 | 只读翻译文件被以读写方式打开，流未释放 | 以只读流加载并及时释放；可选翻译失败保留内置语言 |
| N01 | 合法运行时环境块的复制长度重复增加，读取越过末尾保护页 | 按各相对区间的最大末尾计算；校验计数、偏移及空间溢出；0/1/2 条规则保护页回归通过 |
| N02 | 子进程仿真注入失败仍报告创建成功，部分失败分支遗漏回滚 | 注入期间保持挂起；失败终止本次创建的子进程并清零句柄。真实 IAT 故障注入覆盖 section、保护、写入、部分写入和恢复线程失败 |
| N03 | Sandbox 枚举声明的缓冲区长度为实际分配的两倍 | 分配和声明长度一致，受控扩容并释放；64 项目录触发扩容后查找通过 |
| N04 | 注册表完整信息的 NameLength 包含额外终止 WCHAR | 按 SDK 输出 ABI 返回实际名称长度；标准/Align64 的 0—48 字节、完整名称、数据和 canary 检查通过 |
| S01 | 安装器替换第二个被占用 DLL 失败后，第一个 DLL 已被删除 | 同目录临时文件和备份，失败回滚；首项/次项锁定、相同已锁文件、异工作目录均通过 |
| S02 | 安装、卸载异常被吞掉，按钮继续报告成功，HKCR 映射未必恢复 | 失败传播；注册表映射在 finally 中恢复并核对 SDK 返回值，失败不进入成功提示 |
| S03 | 菜单实例创建四个 HBITMAP，但只释放两个 | 四个句柄各释放一次；x86/x64 连续创建和回收后 GDI 计数增量为 0 |
| S04 | 菜单忽略宿主命令 ID 上限、插入位置，并多报占用数 | 遵守宿主区间与位置；返回真实占用数，包括 UINT 上边界和无容量场景 |
| L01 | 提权重启改变工作目录，原始相对路径指向另一个位置 | 实际调用复用保留当前目录的提权请求；参数和相对文件回归通过 |
| L02 | 语言重定向写入 CodePage 键且使用十进制 LCID | 写入 Nls\\Language 的 InstallLanguage/Default，使用十六进制 LANGID 并去除排序位 |
| L03 | 非 EXE 关联命令丢弃 CLI 尾参数 | 关联与 EXE 启动共用参数优先级：显式配置原始片段优先，否则编码完整 CLI 参数 |
| L04 | 夏令时名称被标准名称覆盖 | 适配 SashiRin 的独立 DaylightName；保留标准时/DST 抑制契约并验证序列化结果 |
| F01 | owner-draw 无 HASSTRINGS 控件的 item data 被当字符串转换 | 按实际控件类型、样式和消息语义共享判断；218 项真实控件组合通过，继承标准类的补漏另记 F02 |
| B01 | 托管目标仍为已停止支持的 Framework 4.0 Client | 已统一 4.8、官方构建期 reference assemblies 与派生运行时配置；空包缓存的完整产品与 ZIP 回归通过 |
| B02 | 编译器诊断、重复 SDK 常量、无效链接选项和关闭的 PE 保护 | 两产品零 W4 诊断、编译/链接 WX、官方 GS 初始化和 PE 防护；`/analyze` 契约类诊断（重复 SDK 声明、CRT 覆盖、nullable new、`%p` 类型）已整改归零，其余既有诊断逐项分类留档 |
| B03 | 无调用的旧签名入口固定 SDK16299，失败不可靠传播 | 已删除失效入口；保留本地未签名构建，发布签名须由发行者在最终哈希之前完成 |
| B04 | 安装器依赖 Release HintPath 和向源码目录复制 DLL 的 PostBuildEvent，单工程/Debug 构建可缺失或嵌入陈旧文件 | 按 ProjectReference 取实际 TargetPath 内嵌；删除中间资源副本；Debug/Release 独立、增量、并行和无变化构建均通过 |
| L05 | STARTUPINFO.cb 未初始化、可写命令行使用只读字符串声明、异常路径泄漏运行时环境块/句柄 | 遵守 SDK 大小和可写缓冲区契约，finally 回收；成功/原生失败/等待失败各 200 次句柄稳定，缺 DLL 100 次无按运行时环境块大小累积的分配 |
| L06 | 合法长时区 ID 被误当显示名拒绝，WCHAR[32] 名称未可靠终止 | ID 由 TimeZoneInfo 验证；名称有界写入、保留终止符并不截半个代理对 |
| S05 | IsInstalled 每次打开注册表键后未关闭 | using 管理；2000 次调用后句柄增量为 0 |
| N05 | 显式长度编码转换额外读取下一字符，在保护页边界越界 | 只转换声明长度并单独写终止符，检查乘加和 SDK 长度上限；保护页回归通过 |
| N06 | ANSI 字体指标的 tmFirstChar 错取 tmStruckOut | 从实际字符字段有界转换；真实转换输出回归通过 |
| N07 | EM_GETLINE 的 WORD 字节容量乘二回绕，并错误补写 NUL | 有界转换并返回实际复制量；保留调用者尾部；私有 thunk 的 32767 单元限制明确记录 |

C01—C03 已用实际产品程序集验证：非法 XML 和锁定文件失败后原数据保持；无效选择不会变更配置；只读翻译可加载且加载后文件可移动。现有参数、配置、WPF 以及 CP932/936、父子进程、目录和主窗口检查继续通过。

## 第 2 轮复审

- C04：参数中的原始 CR 被 XML 默认换行归一化，读取后改变命令行。最小探针在修复前失败；写入改用 `NewLineHandling.Entitize`，保留 CR、CRLF、LF、TAB 和 Unicode，实际产品往返回归通过。
- F02：第一版 owner-draw 修复只识别窗口注册类名，继承标准控件并换名的窗口仍会崩溃。改用 `RealGetWindowClassW`，矩阵扩展到 218 项。6 项 Unicode superclass 文本 thunk 在未注入的 Windows 上同样不转换，明确报告跳过数；这些路径的 item data 回归仍执行。
- T01：安装器资源一致性检查在覆盖后与同一文件自比较。改为与覆盖前保存的实际产品字节比较；分别篡改两个 DLL 后，x86/x64 四个负向用例均只在对应提取断言失败，正常产品继续通过。
- B05：解决方案依赖将 GUI/LEProc 的内容收集为安装器输出；随后独立构建测试宿主时，IncrementalClean 删除产品的 XAML 和版本文件。移除重复的解决方案依赖，嵌入引用标记 Private=false；完整入口先清理全部项目再构建，避免旧共享输出清单与并行重建交错。
- N08：独立原生复审用不含 PROCESS_TERMINATE 的合法调用权限复现注入失败后留下挂起子进程。已改为内部保留不可继承的操作/回收句柄，对外按原请求复制权限与继承属性；受限句柄缺失 DLL、五组权限/属性基线、两处复制失败、七种终止/恢复失败回归通过，独立复审关闭。
- 合并后的完整入口已通过源码及实际 ZIP 验收：34 项当前产品托管回归、8 项 x64 Shell 回归、原生契约/目录、防护、CP932/936 父子进程和窗口启动。该结果早于 N08 修复，不作为其通过证据。
- N09：`/analyze` 确认全局 `vswprintf` 覆盖替换了 CRT 实现，且自定义 nullable `new[]` 与 SDK SAL 冲突。实际分配点迁移到标准 `std::nothrow` 重载，保留可恢复的失败行为；格式化改用传入真实容量的 `_snwprintf`，删除全局覆盖。C28251/C6066 归零，其余既有诊断逐项分类保留在 `artifacts/review-round3/`。
- N10：`AdjustFontDataInternal` 把 `GetFontData` 返回的 name 表长度直接栈分配（C6255 的实际后果）。构造合法 1MiB name 表字体实测：Windows 接受该字体且未注入进程枚举正常，注入进程在 64KiB 与默认 1MiB 栈均于 `gdi32full!EnumFontFamiliesExW` 回调链触发 `0xC00000FD` 栈溢出。改为运行时堆分配并在全部退出路径释放，分配失败退回保留原始字体数据；修复后 12/12 组合通过，CP932/936 字符集调整与完整回归不变。证据在 `artifacts/review-round3/font-table/`。

## 第 3 轮复审

- N11：消息编组的影子映像 SEH 不可作为异常边界；WM_CREATE/WM_MDICREATE/WM_SETTEXT 的应用结构先按 `NtQueryVirtualMemory` 验证可读，野 `WM_GETTEXT` 因原生同样崩溃保留对等行为。
- N12/N13：注册表重定向分别验证 `UNICODE_STRING.Buffer` 和 `UNICODE_STRING` 结构本身；失效输入不在影子映像中继续解引用。
- N14：共享内存探针使用基址减法检查范围，拒绝 `PAGE_EXECUTE`、`PAGE_NOACCESS` 和 `PAGE_GUARD`，避免地址终点回绕及把不可读页当作可读页。
- N16：`NtQuerySystemInformation(SystemCurrentTimeZoneInformation)` 的输出及可选 `ReturnLength` 在 Hook 直接写回前验证可写；坏输出页在原生、932、936 注入下均返回 `STATUS_ACCESS_VIOLATION` 且进程存活。
- N17：注册表重定向分支在填充 `KeyValueInformation` 和 `ResultLength` 前验证调用方写范围；单条重定向、坏输出页的 native/932/936 对照通过。
- N18：x86/x64 `NtCreateUserProcess` 的对象属性和属性列表先验证可读，并将由调用方 `TotalLength` 控制的附加 `PS_ATTRIBUTE` 副本从线程栈移到可失败堆分配；溢出、分配失败和全部清理路径保留。
- N19：被 Hook 直接阻断的三个区域语言输出入口验证可写后再写回；合法输出仍分别写回区域 LCID/LANGID，坏输出与原生访问冲突对齐。

## 第 4 轮复审
- R01：x64 双包初版只让 Shell 扩展选择 x86/x64 helper，GUI 保存后运行和桌面快捷方式仍硬编码包根目录 `LEProc.exe`；在 x64 包中处理 x86 目标时会错误启动 x64 Loader。将目标 PE 识别、关联程序解析和 helper 校验集中到 `LECommonLibrary.GlobalHelper.GetLauncherPath`，Shell、GUI 运行和快捷方式共用；x64 包公共选择器、x86 单包回退、完整源码回归及双包烟测通过。
- R02：x64 Core 的 `NtCreateUserProcess` 原先仍用调用方 `TotalLength` 做 `AllocStack`，与 x86 已完成的 N18 修复不一致；现在补齐共享可读性验证、溢出检查和堆副本清理。x86 原生契约与 x64 专用 1MiB 属性表探针均验证为拒绝非法创建且进程存活，双包完整构建与烟测通过。
- R03：公共目录识别在运行目录名为 `x86` 且父目录也有 `LEGUI.exe` 时，原先无法区分独立 x86 包根目录与 x64 包的兼容子目录；现在要求兼容子目录自身缺少 `LEGUI.exe`，并用临时隔离子进程回归。x86 完整回归通过。
- R04：x64 构建脚本原先硬编码 VS2019 Build Tools 默认目录，而 x86/双包入口使用 `vswhere`；现在统一由 `vswhere` 定位 VS2019 官方组件，v142 与 Windows SDK 版本仍由构建契约固定。
- R05：x64 双包的目标 PE 选择器现在把 x86 目标交给 `x86\LEProc.exe`，并校验 helper 的 PE 架构；x64 Core 对自身创建的 x86 子进程不再调用不兼容的 x86 Loader broker，而是快速返回 `STATUS_NOT_SUPPORTED` 让系统继续创建，避免挂死。双包最终烟测分别验证 x64 根链和 `x86\` 兼容链的真实启动与区域模拟；跨架构子进程继承不作为支持承诺。
- R06：PE 架构选择器只检查 DOS 头偏移后的 machine，伪造 DOS 文件或溢出 `e_lfanew` 可被错误归类为 x86；现在验证 `PE\0\0` 签名并以 64 位边界检查读取，右键/双包 Launcher 选择回归通过。
- R07：x86/x64 两份 HandleTable 用数组 new 分配却用标量 delete 销毁，运行时销毁路径具有未定义行为；统一改用 `delete[]`，分配合同覆盖删除后复用和最终销毁。
- R08：合法的文件关联 `Open\Command` 只有可执行文件而没有参数时，解析器返回空关联，非 EXE 目标无法启动或显示 LE 菜单；保留可执行文件并返回空参数，新增托管回归。
- R09：文件关联可以没有 `DefaultIcon`，快捷方式创建却无条件调用图标字符串的 `.Replace`，导致创建失败；缺少图标时保留 Shell Link 默认图标并继续创建。
- R10：注册表 `DefaultIcon` 的逗号索引被当作路径传给 `IShellLink`，常见 `.txt`/`.pdf`/`.docx` 快捷方式图标索引错误；解析为纯路径和索引后再调用 Shell Link。
- R11：HandleTable 最后一个句柄移除时，`RemoveHandle` 仍用标量 delete 释放数组分配的低层表；x86/x64 均改用 `delete[]`，remove/reuse 合同覆盖。
- R12：x64 Core 的区域/时区以及虚拟 MUI 查询输出原先缺少 x86 已有的写权限边界，虚拟 MUI 句柄关闭也没有关闭真实内核句柄；现在统一验证输出页并透传非法指针，关闭路径调用原始 `NtClose`。x64 属性边界探针、x86 完整回归和双包验收通过。

## 多语言里程碑

当前语言选择只影响产品自身的用户界面和诊断文本，不改变 LE 对目标进程的 ACP/OEMCP、LocaleID、UI 语言、时区、注册表和字体模拟。GUI 与 Shell 的现有 20 种资源继续按各自格式加载；安装器使用 `InstallerLang`，LEProc 和公共库使用 `CommonLang`。资源按系统 UI 语言的完整名称、两字母语言名、英文默认资源顺序选择；GUI 对选定文件中缺少的新键再回退到默认英文键。

本轮新增资源键集合检查、`zh-CN` 隔离子进程探针和发行目录检查。源码回归为 35 项托管、8 项 x64 Shell，均通过；探针确认公共错误提示和安装器窗口在 `zh-CN` 下实际切换。安装器和公共资源当前只提供英文与简体中文，其他现有 GUI/Shell 语言缺少对应新键时显示英文回退文本。
## 规范与支持边界

官方工具链、C++ 声明、SDK 的公开函数参数、输出结构和资源生命周期是本轮直接整改对象。规范依据包括：

- [C++ 安全开发建议](https://learn.microsoft.com/en-us/cpp/security/security-best-practices-for-cpp)：GS、ASLR、DEP、SafeSEH 和静态分析分别验证。
- [自定义入口的 security cookie 初始化](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/security-init-cookie)：不能启用 GS 却跳过初始化。
- [官方 Framework reference assemblies](https://learn.microsoft.com/en-us/dotnet/framework/migration-guide/reference-assemblies)：构建期引用不等于运行时安装，也不应装入发行目录。
- [QueryContextMenu 契约](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-querycontextmenu)：遵守插入位置、命令区间和准确占用数。
- [位图句柄所有权](https://learn.microsoft.com/en-us/dotnet/api/system.drawing.bitmap.gethbitmap)：每个创建的句柄必须配对释放。

Core 的 NLS/PEB 替换、私有调用扫描和 hooks 是当前仿真机制的组成部分。微软的[内部 API 说明](https://learn.microsoft.com/en-us/windows/win32/devnotes/calling-internal-apis)要求优先选择公开等价函数，并建议运行时解析内部函数；[未文档化 API 指导](https://learn.microsoft.com/en-us/windows/compatibility/undocumented-apis)明确建议不要依赖未文档化 API、固定系统导出或注册表键。当前 Core 仍有静态 NT 导入和私有系统依赖，不能由官方 SDK 构建成功推定为完全受支持的运行机制。

现有进程内托管菜单也属于微软[明确不视为受支持场景](https://learn.microsoft.com/en-us/windows/win32/shell/shell-and-managed-code)的设计。CLR 4 并存减少部分版本冲突，不改变该支持政策；x64 受控菜单宿主通过也不能替代实际 Explorer、UAC 和系统版本验收。

### 公开 manifest 替代方案实测

在本机使用官方 SDK API 和独立 x86 探针比较五组 manifest，记录于 `artifacts/review-round2/official-manifest/`。CIM 确认系统为 Microsoft Windows 11 IoT 企业版 LTSC / 24H2 / 26100.9168；注册表 ProductName 的历史字符串仍为 Windows 10，不能仅凭该字段判断系统代际。未修改 PreferExternalManifest 或其他系统设置。

| 父 EXE | ACP/OEMCP | 线程/用户 LCID | 默认 DC charset | 时区 Bias | 不同的无 manifest 子 EXE |
| --- | --- | --- | --- | --- | --- |
| 无 manifest | 936/936 | 0409/0409 | 134 | 240 | 保持基线 |
| 仅外部 ja-JP | 932/932 | 0409/0409 | 134 | 240 | 保持基线 |
| 内嵌中性 + 外部 ja-JP | 936/936 | 0409/0409 | 134 | 240 | 保持基线 |
| 内嵌 ja-JP | 932/932 | 0409/0409 | 134 | 240 | 保持基线 |
| 内嵌 zh-CN | 936/936 | 0409/0409 | 134 | 240 | 保持基线 |

[activeCodePage](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#activecodepage) 能提供进程代码页选择，实测不能补齐本产品的 LCID、字体、时区和跨 EXE 子进程继承，也不能用一个外部 manifest 覆盖已有内嵌 manifest。保留完整区域模拟与要求运行机制全部受微软支持，是需要明确取舍的架构约束；当前整改不把这两项支持例外写成已经解决。

## 行为变化记录

| 里程碑 | 行为影响 |
| --- | --- |
| 配置安全保存与编辑器边界 | 失败时保留旧文件与窗口；非法 XML、不可用区域或时区产生可处理的错误，修正输入后可重试 |
| 翻译读取 | 只读文件正常使用；读取完成后释放文件句柄；缺失或损坏的可选翻译继续回退内置语言 |
| 托管构建依赖 | 目标提升为 Framework 4.8；首次构建恢复官方参考包，应用与测试宿主运行时配置由同一目标派生；参考程序集不随产品分发 |
| 安装与菜单 | DLL 替换失败回滚并停止注册；注册失败可观察且恢复映射；菜单遵守 ID 预算，回收全部位图和注册表句柄 |
| 启动与 SDK 边界 | 提权保留当前目录，关联程序保留参数；原生命令行可写，结构大小和失败资源回收正确 |
| 原生故障 | 注入失败终止本次子进程并返回失败；完整/部分补丁失败恢复原字节并释放 shadow；成功路径继续传播区域模拟 |
| 原生构建防护 | 两产品 W4/WX、GS、ASLR、DEP、SafeSEH，无 RWX 节；跳板发布为 RX，Loader 启用 CFG，核心的实际 CFG 限制独立记录 |
| x64 双包兼容链 | x64 包根目录与 `x86/` 分别携带匹配的 LEProc/Loader/Core；Shell、GUI 保存运行和桌面快捷方式按目标 PE 架构自动选择，独立 x86 包根目录不会因目录名为 `x86` 而误用父目录；x64 与 x86 `cmd.exe` 双烟测通过 |
| SDK 分配与格式化边界 | 普通可失败分配显式 nothrow，失败仍可恢复且配对释放；格式化携带真实容量并拒绝缺少 NUL 的路径结果；PE 依赖集合与自定义入口不变 |
| 受限权限创建 | 注入期间持有私有操作句柄；对外句柄权限/继承与系统基线一致，失败不遗留挂起子进程 |

| 超大 name 表字体 | 枚举改用堆缓冲，不再因大表耗尽线程栈；分配失败保留原始字体数据 |
| PE 架构识别与原生表销毁 | 伪 PE 不再获得错误架构 Launcher；HandleTable 数组按匹配的 `delete[]` 释放，双包选择和原生分配合同保持通过 |
| HandleTable 最后项移除 | 最后一个句柄移除后的低层数组按 `delete[]` 释放，随后仍可重新创建句柄 |
| 文件关联无图标 | 关联没有 `DefaultIcon` 时继续创建快捷方式并使用 Shell 默认图标 |
| 文件关联图标索引 | `DefaultIcon` 的路径和索引分别传给 Shell Link，保留注册表指定图标 |
| 文件关联无参数 | 合法的无参数 `Open\Command` 仍能识别关联程序并进入统一启动路径 |
| 野 lParam 消息编组 | WM_CREATE/WM_MDICREATE/WM_SETTEXT 等编组先验证 lParam 可读再解引用，不可读时透传，与原生行为对齐 |
| 野 ValueName 注册表过滤 | 过滤入口先验证 `UNICODE_STRING` 结构及 Buffer 可读，不可读时交给原生系统调用返回访问冲突 |
| 可读性探针边界 | 使用减法检查避免地址终点溢出，并拒绝 `PAGE_EXECUTE`、`PAGE_NOACCESS`、`PAGE_GUARD` |
| 系统信息与区域输出边界 | Hook 直接写回前验证可写页面；非法输出透传给原生处理，合法区域值保持不变 |
| 进程属性与注册表输出边界 | 调用方属性表只在可读且长度合法时复制；重定向输出和结果长度只写入可写范围，失败时不泄漏堆副本 |
| x64 输出与句柄生命周期 | x64 区域/时区/虚拟 MUI 输出遵守系统调用写入边界；非法页不会崩溃，虚拟 MUI 句柄按真实 `NtClose` 回收 |

原有 XML 字段、GUID、参数优先级、原生公开导出及 x86 目标边界继续作为兼容性约束。后续轮次以当前代码和可重复探针为准；没有触发证据的重写、通用框架和预留选项不作为整改项。

## 已验证里程碑

- `90b7183`：配置安全保存与编辑器边界；17 项托管回归及完整原生、窗口、实际 ZIP 复测通过，并在独立检出中重复验证。
- `5414319`：Framework 4.8 构建迁移；独立工作树从空参考包缓存恢复，18 项托管回归及完整产品和 ZIP 检查通过；新增回归核对三个产品 EXE 和测试宿主的配置与编译目标一致。
- `5411661`：原生 SDK/ABI 边界与防护基线；运行时环境块/注册表/目录契约、N08 权限修复、218 控件组合、故障注入与防护回归通过。
- `e76e951`：标准分配与有界格式化边界；AllocationContracts/HardeningCheck 回归、CP932/936 注入、218 控件与防护检查通过，`/analyze` 契约类诊断归零。
- `2779e55`：托管启动、安装与 Shell 资源边界；34 项托管回归、8 项 x64 Shell 回归、完整源码与实际 ZIP 验收通过，含资源不一致与构建图负向用例。
- `f8f1bc6`：统一托管与原生契约回归入口；34 项托管、8 项 x64 Shell、原生契约/目录/防护/消息/分配接入完整入口，干净检出整套复跑通过。
- `81b513e`：字体 name 表枚举堆缓冲；注入进程 1MiB name 表栈溢出实测修复，12/12 组合与完整源码、ZIP 验收通过。
- N10/N18 同型路径复核：字体 name 表和调用方 `PS_ATTRIBUTE_LIST->TotalLength` 不再进入线程栈；剩余 `AllocStack` 仅处理固定结构、已验证配置数据或宿主自构的小型属性表，没有未验证的外部长度控制。
- `61bb26e`：code-health-pass 全库审计（镜头：死契约/重复/放置/类型/可推导/无效测试/风格）：删除产品零调用的陷阱契约 `LEConfig.GetProfile(name)`，FIXME 误导注释改事实陈述；其余候选（LEProfile 占位参数、csproj 逐项目属性组、重定向表尺寸来源）均核实后不动。完整回归与 ZIP 验收通过。
- `1526a96`：架构审查引出 N12：影子映像 SEH 失效是横切不变量而非孤立点——注册表重定向查找同样解引用应用提供的 ValueName 缓冲（空表时长度短路掩盖了路径）；带单条目运行时环境重定向实证崩溃且二阶异常码 STATUS_INVALID_EXCEPTION_HANDLER 直接证实 SafeSEH 拒绝机制。IsReadableMemory 提升至 LocaleEmulator.h 共享，注册表边界同样先验证后解引用。矩阵 30 组除三模式对等的 sendgettext（原生亦崩）外全绿。剩余 SEH 站点盘点：Kernel32Hook/User32Hook 操作码与重定位走查均读受信模块内存，ml.cpp 三处为内核同源 shim，均无应用数据面，不再处置。
- 行为变化：野 ValueName 的 NtQueryValueKey 在注入下由进程崩溃改为内核优雅拒绝，与原生一致。
- `c82d061`：消息编组 SEH 失效实测与修复（N11）：影子映像不在加载器模块表，SafeSEH 拒绝其异常处理函数，上游 FIXME: SEH DOESN'T work 得到实证；WM_CREATE/WM_MDICREATE/WM_SETTEXT 野 lParam 注入崩溃而原生存活，改用 NtQueryVirtualMemory 可读性验证后再解引用（7 处）；WM_GETTEXT 野缓冲原生同样崩溃属对等不修。探针与日志在 `artifacts/review-round4/seh-probe/`，矩阵 21 组。
- 注：临时诊断（ENABLE_LOG 构建、三处 WriteLog 打点）已全部还原，产品源码无残留。
- `b71381c`：N13 入口结构指针边界；验证失效 `ValueName` 结构后再调用 `RtlValidateUnicodeString`，原生/932/936 注入均存活并返回 `STATUS_ACCESS_VIOLATION`。
- `7d3d521`：N14 可读性探针规范化；消除终点指针溢出并按 Windows 页面保护契约拒绝不可读保护，30 组探针与完整源码、ZIP 验收保持通过。
- `0d3ec0d`：N16–N19 应用指针写边界与可变属性表修复；共享读/写页权限探针、系统信息/区域查询、注册表重定向输出和 `NtCreateUserProcess` 属性列表均完成 native/932/936 对照。新增扩展矩阵为 45 组：除 3 组 native 与注入均一致的 `sendgettext` 原生访问冲突外，其余 42 组注入存活且状态一致；合法区域输出对照写回 `0x804`/`0x0804`。
- `b947b23`：x86/x64 双包发布入口；x86 完整回归（34 项托管、8 项 x64 Shell、原生契约/目录/防护、CP932/936）与 x64 原生 W4/WX、PE machine/ASLR/NX、x64 LEProc 和 Loader 启动烟测均通过，生成 `artifacts/package-dual/Locale.Emulator.2.5.1.0-x86.zip` 与 `Locale.Emulator.2.5.1.0-x64.zip`。
- 当前业务目的研究与 LEProc 契约重构：公开 Locale/UI/UTF-8 API 无法替代非合作旧程序的完整区域模拟；保留必要的 x86/x64 Loader/Core 边界。LEProc 启动失败、提权失败和无文件关联现在返回非零退出码，同一启动路径只解析一次 `CultureInfo`；目标进程的区域模拟和成功路径保持不变。
- 当前复审修复：x64 `NtCreateUserProcess` 属性表边界与所有权，新增 `X64AttributeContracts` 仅使用 x64 ABI，避免把 x86 专用创建信息布局带入 x64 测试。
- 当前复审修复：独立 x86 包根目录与 x64 兼容子目录的 `ProductDirectory` 判定，新增临时隔离子进程回归；行为变化仅限目录名为 `x86` 且父目录含 `LEGUI.exe` 的边界布局。
- 当前复审修复：x64 原生构建入口改用与 x86 入口一致的 VS2019 实例发现方式；固定工具集和 SDK 版本不变。
- 当前复审修复：文件关联命令允许无参数形式；非 EXE 目标仍使用统一的关联程序选择和空参数启动语义。
- 当前复审修复：快捷方式创建对可选 `DefaultIcon` 采用 Shell 默认图标回退，并增加无关联图标回归。
- 当前复审修复：快捷方式解析文件关联的 `DefaultIcon` 路径与索引，保留缺失图标时的 Shell 默认图标回退。
- 当前复审修复：HandleTable 的最后项移除路径与最终销毁路径均匹配数组分配方式，x86/x64 保持一致。
