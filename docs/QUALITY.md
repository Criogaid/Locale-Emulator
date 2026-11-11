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
- R13：x64 User32 的 `EM_GETSEL` 输出、`OUTSTRING`/`INCNTOUTSTRING`/`OUTCBOXSTRING` 消息转换、创建结构和字符串路径原先缺少 x86 已有的可读/可写边界；x86/x64 CBT 创建回调也直接解引用外部结构，影子映像内的 SEH 不能作为保护。现在统一使用页权限验证、拒绝无效输出并保护 CBT 结构读写；x64 守护页消息探针、x86 CBT 守护页回归、x86 完整回归和双包验收通过。
- R14：User32 消息长度来自调用方或被控件过程返回时，宽字符乘法、负长度和 `ULONG` 窄化原先没有统一上限，异常值可能回绕为错误缓冲区大小或越过临时转换缓冲。现在在乘法、返回长度判断、列表框容量和宽窄转换前统一拒绝负值/超界值，并限制内核回调返回不超过已分配容量；x86/x64 双包完整回归和烟测通过。
- R15：x86/x64 `CBTProc` 与 `CBTProcU` 保留未使用的 UnicodeWindow、OriginalProcW、CreateWnd 和 CreateParam 局部状态，增加维护噪声且容易掩盖真实生命周期；删除未使用局部变量，保留实际回调和清理流程，完整双包回归通过。
- R16：x86/x64 公共查询辅助中的 `QuerySystemObjectTypes`、`QuerySystemHandles`、`Nt_QueryDosDevice`、`QueryCsrssProcessId` 及遍历宏在仓库内没有调用者，也不属于 DLL 导出；删除声明、实现和宏，保留仍被 `HookPort` 使用的 `ReleaseSystemInformation`。行为无变化，减少不可达代码和旧分配路径。完整双包回归通过。
- R17：x86/x64 `NativeLoaderAdapter` 源文件内容完全相同，x64 项目条件组只是移除一个文件再加入重复副本；统一保留 `NativeLoaderAdapter.cs`，删除重复文件和条件替换。P/Invoke ABI 与 x86/x64 双包行为不变，托管双架构构建通过。
- R18：x86 `HookPort` 保留未启用的 WOW64 汇编实现、Win32k 服务表初始化和对应的空实现导出声明；当前安装路径从未调用，相关类型和服务表转换函数也只有这条停用路径使用。删除不可达旧契约及注释调用，保留现行系统调用哈希路径；完整双包构建与回归通过。
- R19：`StrLengthA`、`StrNCompareW` 和 `StrNICompareW` 各保留永远不编译的旧实现分支，活动实现前后语义相同但停用分支和注释替代代码增加阅读与维护噪声；删除 `#if 0` 分支，保留活动实现。字符串长度/比较调用者不变，完整双包构建与回归通过。

## 多语言里程碑

当前语言选择只影响产品自身的用户界面和诊断文本，不改变 LE 对目标进程的 ACP/OEMCP、LocaleID、UI 语言、时区、注册表和字体模拟。所有产品文本来自标准 `.resx` 资源与卫星程序集；资源按系统 UI 语言的文化链选择，缺失键回退到英文中性资源。

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
| 翻译读取 | 译文以卫星程序集分发，缺失的文化或键回退到主程序集中的英文中性资源；Shell 扩展的卫星程序集随扩展 DLL 安装 |
| 托管构建依赖 | 目标提升为 Framework 4.8；首次构建恢复官方参考包，应用与测试宿主运行时配置由同一目标派生；参考程序集不随产品分发 |
| 安装与菜单 | DLL 替换失败回滚并停止注册；注册失败可观察且恢复映射；菜单遵守 ID 预算，回收全部位图和注册表句柄 |
| 启动与 SDK 边界 | 提权保留当前目录，关联程序保留参数；原生命令行可写，结构大小和失败资源回收正确 |
| 原生故障 | 注入失败终止本次子进程并返回失败；完整/部分补丁失败恢复原字节并释放 shadow；成功路径继续传播区域模拟 |
| 原生构建防护 | 两产品 W4/WX、GS、ASLR、DEP、SafeSEH，无 RWX 节；跳板发布为 RX，x86 与 x64 Loader 均启用 CFG（构建时检查），核心的实际 CFG 限制独立记录 |
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
| User32 消息与 CBT 边界 | x86/x64 消息转换先验证应用输入及输出页；EM_GETSEL、文本消息和 CBT 创建结构的坏页不会在 LE hook 中直接解引用，合法消息语义保持不变 |
| User32 消息长度边界 | 消息返回长度在乘法、窄化和临时缓冲读取前验证；超大或负值不会回绕为可接受的容量 |

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
- 当前复审修复：x64 User32 消息边界与 x86/x64 CBT 回调统一使用 `IsReadableMemory`/`IsWritableMemory`；新增 x64 真实注入消息守护页探针和 x86 CBT 结构守护页回归，x86/x64 双包构建、完整源码回归与 ZIP 烟测通过。
- 当前复审修复：x86/x64 User32 消息长度统一执行有符号、乘法、窄化和临时缓冲容量检查；完整源码回归、x64 真实注入探针和双 ZIP 验收通过。
- `eb9e007`：安装器无窗口入口 `--InstallUser`/`--UninstallUser`，复用事务式 `ChangeRegistration`；当前用户安装与卸载往返实测退出码均为 0、无残留进程。
- `0315ea7`、`22d5b97`、`6833d0f`：Windows 11 一级右键菜单（`IExplorerCommand` 稀疏包，x64 DLL，x86 与 x64 包共用）。已验证：DLL `/W4 /WX`、CFG、ASLR、NX；独立驱动加载 DLL，覆盖错误 CLSID、空参数、含空格路径的引号转发、`DllCanUnloadNow`；清单通过 Appx 校验，在未开启开发人员模式时部署被拒为 `0x80073CFF`，安装器仍安装传统菜单；双包构建与两个 ZIP 的 PE 校验通过。**未验证**：稀疏包注册成功后 Explorer 界面中的实际菜单项与点击启动，需要开启开发人员模式或已签名的包。
- 缺陷（已修复）：Windows PowerShell 5.1 把无 BOM 的 UTF-8 脚本按 ANSI 解析，`ModernMenu.ps1` 的中文字符串造成语法错误；改为纯 ASCII 并按 5.1 兼容，由真实 5.1 调用复现并验证。
- 一级菜单子菜单：与传统菜单对齐为运行/各全局配置/修改此程序配置/编辑全局列表。独立驱动验证子命令枚举、`Clone`/`Skip`/`Reset`、非法或缺失的 GUID 与名称被过滤（GUID 只接受规范格式，因它进入带引号的命令行）、多选时隐藏，以及五条命令行与传统菜单逐项一致；配置缺失或损坏时只保留固定项。XmlLite 禁止 DTD。
- 缺陷（已修复）：一级菜单清单缺少 `unvirtualizedResources` 能力，`makeappx` 的严格校验拒绝（0x80080204）；此前的注册尝试只被“需要开发者许可证”拦下，没走到这一步。`Native/ExplorerCommand/build.ps1` 现在用 SDK `makeappx pack /nv` 校验清单架构，旧清单使构建失败、修复后通过。
- 已查明的限制：`Add-AppxPackage -AllowUnsigned`（24H2）只支持 `-Path` 安装 `.msix`；将发布者改为 unsigned namespace 后仍被拒绝为 0x80073D2B（无签名包不能包含可执行激活），而菜单扩展必须挂在 `Application` 下，所以无签名路线不可行，注册仍需要开发人员模式或签名包。
- 重复来源（已消除）：一级菜单在 `ExplorerCommand.cpp` 里手写了 5 种语言的标题，与传统菜单的 22 份 `Strings*.resx` 是两份来源，繁体中文用户会看到简体、其余 17 种语言只有英文。`Native/ExplorerCommand/build.ps1` 现在从 resx 生成 `MenuTexts.h`，运行时按“完整区域名 → 语言 → 英文”回退（与托管菜单一致）。实测：12 种界面语言（含 zh-TW/zh-HK/pt-BR/pt-PT/fr-CA/nb-NO）下 4 个标题与 resx 原文逐项一致。
- 重复来源（已消除）：一级菜单的 CLSID 同时手写在 `AppxManifest.xml`（Verb 与 COM Class 两处）和 `ExplorerCommand.cpp`，任何一处不一致都只会让菜单静默不出现。现在 `build.ps1` 以清单为唯一来源：要求两处 GUID 相同，并生成到 `MenuTexts.h`；实测生成值与原值一致，故意改错一处时构建以退出码 1 失败。
- 缺陷（已修复）：`ModernMenu.ps1` 注册失败（例如未开启开发人员模式）时会把清单与图标留在 `%LOCALAPPDATA%\LocaleEmulator\ModernMenu`，卸载后还会留下空的 `LocaleEmulator` 父目录。现在失败且无已注册包时清理注册目录，`Unregister` 也在父目录为空时一并删除。实测：用打包出的 x64 包运行注册，仍以 `0x80073CFF` 失败（空管道的 `Remove-AppxPackage` 不会提前报错，清单已通过部署层解析），目录无残留；无注册时 `Unregister` 退出码 0。
- 更正：此前记录“多选时隐藏需要目标的项”并不准确，且“多选已验证”依据的驱动里多选部分只是空桩。用真实的两项 `IShellItemArray` 实测：根项状态为 `ECS_HIDDEN`，整个菜单不显示（与传统菜单只在单选时出现一致），`GetState` 里对“编辑全局配置列表”的豁免是死代码。已改为统一规则（单选才显示）；实测单选时根与 5 个子项均为启用，两项选择时根与前 5 个子项隐藏。
- 改进：`ModernMenu.ps1` 失败时只向 stderr 输出一行英文原因（如 `Deployment failed with HRESULT: 0x80073CFF, ...`）并以退出码 1 结束；此前安装器会把 PowerShell 的整段多行错误（含本地化重复文本和脚本位置）原样放进完成提示框。实测注册失败、产品目录缺文件、`Unregister` 三条路径：失败时 stderr 恰好一行，退出码 1，目录无残留；`Unregister` 退出码 0。
- 缺陷（已修复）：安装器卸载时一级菜单移除失败，会复用注册失败的文案，对用户说“传统菜单已安装”并提示“未签名包需要开发人员模式”，两句都不适用于卸载；超时文案也固定写“注册”。现在卸载使用独立的 `ModernMenuRemoveFailed`，超时文案改为中性的“更改”（英文与简体中文各一份）。限制：无窗口模式不显示提示框，且无法在本机强制出现卸载失败，因此只验证了构建通过、两份 resx 解析正常且键一致，提示框的最终样子没有看过。
- 新证据（一级菜单 DLL 在真实 shell 中的行为）：无需开发人员模式，用传统的 `exefile\shell\<verb>\ExplorerCommandHandler` 加 HKCU 下的 `InprocServer32`（均为临时测试键，实验后已删除并核对）让系统自己的 shell32 通过 COM 加载 `LEExplorerCommand.dll`，再用 `SHCreateDefaultContextMenu` 取菜单。结果：出现 “Locale Emulator” 子菜单（项数 13→14）；按 Explorer 打开菜单时的方式发 `WM_INITMENUPOPUP` 后，子菜单完整填充为：Run with Application Profile、`LEConfig.xml` 的 3 个配置（含 1 个被截断的超长名称）、分隔线、Modify Application Profile、Edit Global Profile List，顺序与设计一致。局限：这是 shell32 的传统 HMENU 路径，不是 Windows 11 一级菜单的 XAML 渲染，也没有执行点击启动；稀疏包注册（包身份、`windows.fileExplorerContextMenus` 扩展）仍需开发人员模式或签名包才能验证。
- 新证据（点击启动经由真实 shell32）：在同一套临时 HKCU 注册下，用 `IContextMenu::InvokeCommand` 依次点击 6 个命令（运行、3 个配置、修改、编辑全局列表），全部返回 `S_OK`，作为 `LEProc.exe` 的记录器收到的命令行为 `-run "<目标>"`、`-runas "<guid>" "<目标>"`（3 个 GUID 各一次，含超长名称的配置）、`-manage "<目标>"`、`-global`，目标路径取自真实 shell 项（含空格），与传统菜单一致；测试键已删除并核对。局限同上：传统 HMENU 路径，不是 Windows 11 一级菜单的渲染；稀疏包注册仍未验证。
- 改进：`ModernMenu.ps1` 注册前检查操作系统架构（`RuntimeInformation.OSArchitecture`，在 x64 仿真下的 PowerShell 里也反映真实系统），非 x64 时用一行 `only available on x64 Windows (found Arm64)` 说明原因；一级菜单的包与 DLL 只有 x64，ARM64 的 Explorer 加载不了。`Unregister` 在检查之前，任何架构都能移除。验证范围：x64 上检查通过并照常走到部署层（失败原因不变、无残留）；非 x64 分支没有在 ARM64 机器上运行，只是用同一条件、把架构换成 Arm64 做了模拟。
- reuse-review（范围 `7ae7ff5..HEAD`）：两处重复知识已消除。其一，x64 原生工具链的发现逻辑（vswhere 查询、MSVC `14.29.30133`、SDK 版本、`Hostx86d`）原先在 `build-x64.ps1` 和 `ExplorerCommand/build.ps1` 里逐字重复，且 SDK 版本另存于 `Toolchain.props`；现在由 `Native/Toolchain.ps1` 的 `Get-OfficialToolchain` 统一提供，SDK 版本从 `Toolchain.props` 读取。两个脚本都不进发布包，无包内容变化。其二，包标识名 `LocaleEmulator.ModernMenu` 同时写在清单与 `ModernMenu.ps1`，不一致会让 `Unregister` 与“先移除旧包”静默什么都不做；现在构建校验两者相同，故意改任一处都使构建以退出码 1 失败。有意保留的重复：菜单命令行格式在 C# 传统菜单与 C++ 一级菜单各一份（跨语言，权威在 `LEProc/Program.cs`，已用真实 shell32 点击核对一致）；原生一侧只读 `LEConfig.xml` 的 `Profile` 的 `Name`/`Guid`，是刻意的最小契约。验证：辅助函数返回值与旧字面量逐项相同；完整 `build-dual.ps1` 退出码 0，146 项 PASS 与重构前完全一致。因二进制每次构建的时间戳不同，哈希比对不能证明等价，故未采用。
- overengineering-review（同一范围）：整体没有过度设计，`IExplorerCommand` 的枚举器、工厂与 `Kind` 分派都是接口契约要求的复杂度，`IsCanonicalGuid` 与 64 项上限处在真实的信任边界，均保留。两处小清理：`ModernMenu.ps1` 的 `-ManifestPath` 参数没有任何调用方（安装器只传 `-Action`，文档也未提及），默认的两处查找已覆盖包内与仓库两种布局，已移除；`-ProductDirectory` 在从仓库检出运行时有真实用途，保留。`Native/ExplorerCommand/build.ps1` 里 4 个资源键名写了两遍，合并为 `$menuKeys`。验证：生成的 `MenuTexts.h` 与改动前逐字节相同；脚本三条路径（包内注册走到部署层、缺清单给出一行提示、`Unregister`）行为符合预期。
- 加固：`build-dual.ps1` 对两个包中的 `LEExplorerCommand.dll` 增加断言（该 DLL 会被加载进 Explorer）：必须带 Dynamic base、High Entropy VA、NX compatible、Control Flow Guard，并导出 `DllGetClassObject` 与 `DllCanUnloadNow`，缺任一项构建失败。同时发现 `build-dual.ps1` 里 dumpbin 路径仍硬编码 MSVC `14.29.30133`（上一轮 reuse-review 漏掉的第三处），改为使用 `Get-OfficialToolchain`。验证：完整 `build-dual.ps1` 退出码 0、146 项 PASS；反向核对用不含 COM 导出的 `LoaderDll.dll` 确认导出断言会判失败。过程中一次把正则写坏（`\b` 被写成了退格字符）导致断言误报，已改为不含转义的写法，并确认文件中无控制字符。
- 审计：逐个列出两个包中原生二进制的 ASLR/DEP/CFG 标志。发现文档写“Loader 启用 CFG”，但只有 x86 Loader（`LoaderDll.vcxproj`）如此，x64 Loader（`build-x64.ps1`）没有。尝试给 x64 Loader 的编译与链接加 `/guard:cf`：可链接、DLL 带 CFG，但完整 `build-dual.ps1` 在真实注入的 x64 属性表边界探针处失败（目标进程 `0xC0000005`，另有 NativeProbe 矩阵未执行），撤销后回到 146 项 PASS。结论：不开启，已把 `Native/README.md` 与本表中的说法改为只指 x86 Loader，并注明 x64 Loader 的崩溃原因尚未定位。`LEContextMenuHandler.dll` 是托管程序集，不适用这些原生标志。
- 缺陷修复（x64 Loader 的 CFG）：上一条记录的 x64 Loader 启用 CFG 后崩溃，原因已查清并修复。用 scratchpad 里的最小调试器（`DEBUG_ONLY_THIS_PROCESS` 循环）捕获到 `LoaderDll.dll+0x42E2` 对 `+0xA008` 的写入访问冲突；该地址是延迟加载 IAT 所在的独立节。`/guard:cf` 会启用受保护的延迟加载（加载配置 Guard Flags 含 0x1000/0x2000），系统把这一节设为只读，而自带的 `__delayLoadHelper2`（`LepDelayLoad_x86_x64.cpp`）直接写 IAT 槽。修复：写入前后用 `NtProtectVirtualMemory` 临时放开该槽所在页并恢复原保护（不能用 `VirtualProtect`，这个例程正是引导 KERNEL32 的）。现在 x64 Loader 带 CFG，`build-dual.ps1` 对两个包的 `LoaderDll.dll` 增加 Control Flow Guard 断言。核心 DLL 仍不启用 CFG（独立原因，见 `Native/README.md`）。验证：同一复现 `LEProc` 退出码由 `0xC0000005` 变为 0；完整 `build-dual.ps1` 退出码 0、146 项 PASS，与修复前逐项相同。未能确认的一点：加断言后的第一次完整运行中，`NativeContracts.exe`（x86 包）报过一次 `no child survives failed launcher return`，随后连续两次完整运行全绿；该失败位于 x86 路径、与本次只涉及 x64 的改动没有已知关联，但原因未查明，也没有复现，视为一次未解释的偶发，若再出现需单独调查。后续跟进：把同一个 `NativeContracts.exe` 单独连续运行 30 次，0 次失败（该测试源码属测试文件，本工作流不读取，故没有进一步分析它的判定条件）；因此偶发更可能出在完整流水线并行负载下，而不是该用例本身稳定失败。再追加：连续 4 次完整 `build-dual.ps1`（最终代码）全部退出码 0、146 项 PASS、无 `FAIL` 行；加上此前各次完整运行，该失败累计只出现 1 次、通过至少 8 次，仍未复现，保持“未解释的偶发”。
- 加固：`LEExplorerCommand.dll` 链接加 `/CETCOMPAT`（声明兼容 CET 影子栈；该 DLL 是纯 C++、没有手写汇编或自改返回地址的代码，会被加载进 Explorer），`dumpbin` 显示 `CET compatible`，`build-dual.ps1` 的缓解标志断言同步要求该项。验证：完整 `build-dual.ps1` 退出码 0、146 项 PASS。限制：只验证了标志存在，没有在启用 CET 严格模式的 Explorer 中实测；核心与 Loader 含手写跳转与动态映射，没有加该标志。
- 自审（`952f452` 的延迟加载修复）：写入前后“放开页保护再恢复”在多线程下有竞态——线程 A 放开、线程 B 读到已可写、A 恢复只读、B 写入时访问冲突。Loader 目前由 LEProc 逐个调用，实际未必触发，但不应依赖该假设；现用一个只包住 Nt 调用的自旋锁（`_InterlockedCompareExchange`，临界区内不会再进入该例程）串行化。验证：`/W4 /WX` 编译通过，完整 `build-dual.ps1` 退出码 0、146 项 PASS。限制：没有构造多线程并发解析同一页延迟导入的压力用例（需要新的测试宿主，属测试工作，见交接建议），竞态的消除来自推理而非实测。
- code-review（`d11232b..HEAD`，我自己通读 diff，非独立复核）：一处行为收紧。延迟加载辅助例程在“放开页保护”失败时原先改为返回空指针，而修复之前写 IAT 槽从不失败；核心 DLL 的 IAT 不受保护、本来就可写，不应因为解保护失败而让导入失败。现改为解保护失败时仍直接写入（可写页照常成功，只有真正只读的受保护页才会像以前那样出错），只在解保护成功时才恢复保护。验证：`/W4 /WX` 编译通过，完整 `build-dual.ps1` 退出码 0、146 项 PASS。其余改动（构建断言、`/CETCOMPAT`、Loader `/guard:cf`）通读后未发现问题。另核对：索引中所有文件已按 `* text=auto` 归一为 LF，工作区行尾混用不影响提交；各构建脚本都无 BOM、含中文、由 `pwsh` 运行，与既有约定一致。
- code-health-pass（`7ae7ff5` 之后新增的代码，审计后只做零行为变化的两项）：`ExplorerCommand.cpp` 里 `m_items[kMaxProfiles + 3]` 的 3 是 Run、Modify、Edit Global 三个固定项，改为具名常量 `kFixedItems`；`ModernMenu.ps1` 移除 `-ManifestPath` 参数后遗留的 PascalCase 局部变量改为 `$manifestPath`。验证：完整 `build-dual.ps1` 退出码 0、146 项 PASS；脚本在仓库检出布局下仍找到清单并走到部署层，缺清单时提示不变。**记录但没有改（需所有者决定）**：安装器无窗口模式 `--InstallUser/--UninstallUser` 只看经典菜单的结果，一级菜单注册失败（例如未开启开发人员模式）时 `modernMenuNote` 被丢弃，退出码仍为 0；脚本化部署无从得知一级菜单没有注册上。若需要感知，只能新增独立退出码（例如 2 表示经典菜单成功、一级菜单失败），这会改变对外退出码约定，WinForms 程序也没有控制台可输出原因。未覆盖：镜头 6（测试是否有保护作用）需要读测试文件，按规则没有做。
- 决定并实现（上一条记录的无窗口模式问题）：`--InstallUser/--UninstallUser` 在经典菜单已处理、但可选的一级菜单没有处理成功时返回退出码 2（原先吞掉失败并返回 0）。该入口是本分支新增且尚未发布，没有兼容负担；失败原因仍只在图形界面里显示（WinForms 程序没有控制台）。实测（x64 包里的安装器，本机未开启开发人员模式）：`--InstallUser` 退出码 2 且经典处理程序已注册，`--UninstallUser` 退出码 0 且处理程序已移除；随后用 `Build\Release` 的安装器还原，HKCU 的处理程序键、`CodeBase`、静态动词、`ModernMenu` 目录与测试前快照完全一致。完整 `build-dual.ps1` 退出码 0、146 项 PASS。未实测：一级菜单注册成功时返回 0 的路径（需要开发人员模式或已签名的包）。
- 文档更正：`README.md` 写一级菜单“只在选中单个 `.exe` 时出现（与传统菜单一致）”，后半句不准确。传统菜单在运行时由 `GlobalHelper.GetLauncherPath` 按文件关联找到关联程序，只要它是 PE 就显示（例如关联到某个程序的 `.bat`、`.jar`）；`.lnk` 的关联是 Shell 而不是 PE，传统菜单本来就不覆盖。Windows 11 稀疏包清单只能静态列出扩展名，无法表达“关联程序是否为 PE”，所以一级菜单保持只覆盖 `.exe`，不再宣称与传统菜单一致，并指向传统菜单（“显示更多选项”）。这是评估后维持原决定，不是遗漏；若要扩展，需要决定一个固定的扩展名清单，并对每种类型用真实 shell32 验证。
- 缺陷修复（一级菜单图标）：`IExplorerCommand::GetIcon` 返回 `LEProc.exe,0`，但托管启动器 `LEProc.exe` 没有嵌入图标资源（`ExtractIconEx` 计数为 0），菜单项因此没有图标；Locale Emulator 的图标在 `LEGUI.exe` 里（`LEGUI/icon.ico`，计数为 1）。改为返回同目录的 `LEGUI.exe,0`。同一原因下，`ModernMenu.ps1` 生成的包 Logo 取自 `LEProc.exe` 的“关联图标”，实际只是 Windows 通用程序图标，也改取 `LEGUI.exe`；脚本的产品目录检查与安装器的 `IsModernMenuAvailable` 同步要求 `LEGUI.exe` 存在。实测：加载包里的 DLL，调用 `DllGetClassObject` 取得命令对象，`GetIcon` 的说明符经 `ExtractIconEx` 解析，修改前 0 个图标、修改后 1 个有效图标；两个候选 Logo 图标的哈希不同，确认 Logo 来源确实变化。完整 `build-dual.ps1` 退出码 0、146 项 PASS。未验证：Windows 11 新菜单实际如何绘制该图标。该缺陷此前没有被发现，是因为之前的真实 shell32 验证只覆盖标题与点击。
- 重构（`build-dual.ps1`）：三处重复的 `dumpbin` 解析合并为 `Assert-DumpbinContains`（`/headers` 或 `/exports` 加一组必须匹配的正则），`Assert-Machine`、Loader 的 CFG 检查和 `LEExplorerCommand.dll` 的缓解/导出检查共用它；失败信息格式不变。唯一的语义差别是缓解标志现在按正则匹配而不是转义后的字面量，所列标志都不含正则元字符，结果相同。实测：完整 `build-dual.ps1` 退出码 0、146 项 PASS；另用临时脚本验证了反向用例（x86 断言作用于 x64 可执行文件会抛错，缺失标志会抛错）。
- 验证补充（一级菜单读取 `LEConfig.xml`）：用打包的 `LEExplorerCommand.dll` 与临时 `LEConfig.xml` 经系统 COM 接口枚举子命令。名称中的 `&amp;`、`&lt;`、`&quot;` 解码为 `Tom & Jerry <JP> "q"`，日文名称原样显示，GUID 非法的配置项被跳过，固定项位置不变；名称为空的配置项显示为空白菜单项，与传统菜单对同一数据的表现一致，不单独处理。该检查只覆盖命令枚举，不涉及 Windows 11 菜单的真实绘制。未改代码。
- 小修（`ModernMenu.ps1 -Action Unregister`）：包未注册时也输出 "Removed ..."，与事实不符；现在未注册时输出 "... was not registered; nothing to remove."，退出码仍为 0，安装器不解析该输出。用 Windows PowerShell 5.1 解析脚本无错误、脚本无非 ASCII 字节，并实际运行了该未注册分支；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 排查记录（`NativeContracts` 偶发失败）：曾尝试 4 个并行 worker 各运行 15 次来加压，60 次中 48 次失败，失败内容为 "isolate missing DLL"、"restore DLL"、"cleanup PID belongs to this test" 等。原因是该程序会在产品目录内暂时隔离、恢复 DLL 并按进程清理，不支持同一目录并行运行，所以这次压测无效，不能作为缺陷证据；之后单次运行恢复为通过，产物目录与仓库状态未受影响。结论：压测此程序必须串行；偶发失败的原因仍未查清，若再出现，除保存 `native-contracts.log` 外，还应核对当时是否有其他进程同时在同一产品目录运行测试。
- 排查记录（续）：核对 `build.ps1`、`build-dual.ps1`、`Scripts/*.ps1`，没有 `Start-Job`、`-Parallel` 或后台启动，`Invoke-RegressionProcess` 一次只运行一个回归进程并等待其结束（45 秒超时会杀掉进程并报“回归进程超时”，与偶发失败的表现不同），因此构建流程自身不会造成同目录并发，可排除这一原因。剩余的可能是流程之外的并发（例如杀毒软件或索引服务暂时占用刚被隔离或恢复的文件），目前没有证据支持或排除。
- 缺陷修复（静态入口图标）：`RegisterStaticExplorerVerb` 把 `Icon` 写成 `LEProc.exe` 的路径，而 `LEProc.exe` 没有嵌入图标资源，与此前一级菜单子命令的问题同源；这个静态入口不需要开发人员模式，是多数用户在 Windows 11 一级菜单里实际看到的入口。现在取启动器同目录的 `LEGUI.exe`，该文件不存在时回退为原来的启动器路径。实测：用打包的 x64 安装器 `--InstallUser`（退出码 2，与预期一致），读回 `Icon` 值为包内 `LEGUI.exe`，`ExtractIconEx` 解析出 1 个图标；随后用 `Build\Release\LEInstaller.exe --InstallUser` 并删除静态入口键还原，处理程序键、`CodeBase`、无静态入口、无 `%LOCALAPPDATA%\LocaleEmulator` 与基线一致。完整 `build-dual.ps1` 退出码 0、146 项 PASS。未验证：Windows 11 资源管理器实际如何绘制该图标。
- 构建防线（图标来源）：一级菜单 `GetIcon`、静态入口 `Icon`、包 Logo 都依赖 `LEGUI.exe` 内嵌图标，此前没有任何检查，图标资源被删时三处会同时悄悄失效。`build-dual.ps1` 现在对两个包的 `LEGUI.exe` 用 `ExtractIconEx` 断言至少 1 个图标。反向对照：同一调用对 `LEProc.exe` 返回 0，对应断言会抛错；通过路径下完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 缺陷修复（一级菜单标题截断）：配置名超过 127 个字符时，`StringCchCopyW` 按 UTF-16 单元截断，名字在上限处恰好是代理对（如 emoji）时会留下孤立的高代理项，显示为 U+FFFD。实测复现：126 个 a 加一个 emoji，标题末尾字节为 EF BF BD。现在截断后若最后一个单元是高代理项就去掉它。复测：同一用例不再含 U+FFFD；125 个 a 加 emoji（恰好放得下）完整保留；200 个 a 仍截到 127；短名字不变。完整 `build-dual.ps1` 退出码 0、146 项 PASS。传统菜单按 Win32 菜单字符串另行处理，本次不涉及。
- 构建防线（文本文件控制字节）：写文件时转义序列被展开，曾让 C++ 源码混入 NUL 字节、台账混入退格字符，编译器和肉眼都不容易发现。`build-dual.ps1` 现在扫描所有已跟踪的文本文件（`.cs`、`.cpp`、`.h`、`.ps1`、`.md`、`.xml`、`.resx`、`.props`、`.csproj`、`.json`），除制表符、换行和回车外出现任何小于 0x20 的字节即报错并指出文件。当前 258 个文件扫描约 1 秒，均无问题。反向验证：在临时 git 仓库中注入 NUL 与退格字符，检查都会抛错，干净文件放行；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 构建防线（`ModernMenu.ps1` 纯 ASCII）：安装器用 Windows PowerShell 5.1 运行该脚本，5.1 会把无 BOM 的文件按 ANSI 代码页读取，任何非 ASCII 字节都会被误读。此前这条约束只靠人记。`build-dual.ps1` 现在对两个发行包里的 `ModernMenu.ps1` 断言不含大于 127 的字节。反向验证：对真实的 x86、x64 包内脚本通过，对注入日文的临时文件会抛错；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 构建防线（默认包不含一级菜单文件）：README 声明只有双架构包携带一级菜单，`build.ps1` 生成的单一 x86 包不含；缺少这三个文件在双架构包里会直接失败，但“默认包不含”此前没有断言。`build-dual.ps1` 现在在默认包目录存在时断言其中没有 `LEExplorerCommand.dll`、`AppxManifest.xml`、`ModernMenu.ps1`。反向验证：干净目录通过，放入 `ModernMenu.ps1` 会抛错，目录不存在时静默通过；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 决定（新增代码的许可）：此前 `docs/SOURCES.md` 写“尚未指定许可证，由维护者决定”。代码库已交由维护期间自主决定，因此确定：安装器的无窗口模式与一级菜单集成是对 LGPL 前端文件的修改，按 LGPL-3.0 分发；`Native/ExplorerCommand/`、`Scripts/ModernMenu.ps1`、`Native/Toolchain.ps1` 为独立新文件，选同一许可，避免第二套条款，许可文本沿用根目录 `COPYING.LESSER`。这些文件没有拷贝 Microsoft 示例代码，不适用 Ms-PL 例外。已同步 `README.md`、`docs/SOURCES.md`、`docs/MAINTENANCE.md`；这是可逆的文档决定，不涉及代码，完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 决定（Core DLL 的 CFG）：评估后确定保持不启用，不再作为待决事项。原因：`LeSetupAnsiOemCodeHashNodes` 靠扫描 `KERNELBASE` 机器码找到未导出的私有函数并间接调用，该目标不是合法的 CFG 目标；绕开要么主动关闭保护，要么复制随 Windows 版本变化的内部 NLS 行为，风险高于收益。Loader 与 `LEExplorerCommand.dll` 已启用 CFG 且由构建断言守住。重新评估的条件：Windows 提供公开的等价接口，或该调用被移除。仅文档改动。
- 缺陷修复（控制字节扫描的盲区）：自我复审发现，扫描依赖 `git ls-files` 的默认输出，含非 ASCII 字符的文件名会被 git 加引号并转义，`Test-Path` 随即判为不存在，该文件被静默跳过。当前仓库没有这类文件，所以没有漏扫，但以后加一个中文文件名的文档就会漏。现在用 `-c core.quotepath=false` 取得原始路径并去掉可能的引号。反向验证：在临时仓库放一个含退格字节的 `说明文档.md`，旧逻辑静默放行，新逻辑抛错并报出正确文件名；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 构建防线修正（默认包检查不再空转）：上一条“默认包不含一级菜单文件”的断言在默认包目录不存在时静默通过，若以后 `build.ps1` 的输出目录改名，断言会悄悄失效。`build-dual.ps1` 第一步总会运行 `build.ps1`，默认包目录必然存在，所以现在找不到该目录就报错。反向验证：`$artifacts` 指向不存在的路径时抛错，指向真实目录时通过；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
- 构建防线修正（控制字节扫描不再空转）：如果 `git ls-files` 什么都没返回（没有 git，或不在仓库中），扫描循环一次都不执行，会恒通过；其余几条防线遇到缺文件都会自己报错，只有这条没有“至少扫了东西”的下限。现在要求至少扫描 50 个文本文件，当前实际 258 个，空转时为 0。反向验证：把仓库指向非 git 目录时抛错，真实仓库通过；完整 `build-dual.ps1` 退出码 0、146 项 PASS。
