   Goal mode is active. Complete this goal fully.

   <goal_objective>
   在允许破坏性更新、无需兼容旧版 Loader/Core ABI 的前提下，重构 Locale-Emulator 的 LEProc 与 x86/x64 Loader/Core 环境
 传递方式：

   1. 用一套架构无关、版本化、可验证的运行时环境 ABI 替换旧版 x86 LEB 和独立的 x64 LEPB。
   2. 让 x86 与 x64 的托管启动层共享统一的启动模型、参数校验、进程等待和句柄清理流程。
   3. 保留 x86/x64 原生 Core 的必要分叉；不要为了代码表面统一而合并不同架构的 PEB、Hook、系统调用和注入实现。
   4. 删除旧的 x86 专用注册表重定向序列化、旧 ABI 封送路径和没有调用者的兼容代码。
   5. 在业务行为不变的前提下，使 x86/x64 两套实现都遵守 Windows SDK、MSVC、ABI 边界、错误处理和资源回收规范。
   6. 按里程碑提交本地变更，不推送。
   7. 完成后按顺序循环执行所有 review skills；任何一项发现并修复问题，都必须从第一项重新开始，直到一整轮没有新问题。
   </goal_objective>

   <repository>
   E:/Github/Locale-Emulator
   </repository>

   <branch>
   maintenance/monorepo-win11
   </branch>

   <starting_state>
   - 当前 HEAD：94a6ca2
   - 工作树应保持干净；开始前必须重新检查 git status、分支和 HEAD。
   - 最近相关里程碑：
     - 5a2c369：x64 双包目标 PE 选择和跨架构边界修复
     - ae41871：x64 路径常量清理
     - ba75c42：修复双包兼容文件重复复制
     - 569d8d7：删除不兼容 x64→x86 Loader broker 调用，明确跨架构子进程继承边界
     - 65e93b5：删除无调用者的 x64 broker 协议
     - 94a6ca2：清理 Visual Studio filters 残留
   - 当前 x64 包根目录携带 x64 LEProc/Loader/Core，x86/ 子目录携带 x86 LEProc/Loader/Core。
   - Launcher 根据目标 PE 架构选择包根目录或 x86/ 下的 LEProc。
   - x64 目标自行创建 x86 子进程时，当前实现会快速返回 STATUS_NOT_SUPPORTED，让系统继续创建；当前产品不承诺跨架构子进程
 继承 LE 区域模拟。
   </starting_state>

   <user_decisions>
   - 允许破坏性更新，不需要兼容旧版 LEB、旧版 LoaderDll 调用者或旧版 Core ABI。
   - Core 由本项目维护，可以同时修改 x86 Core、x64 Core、x86 Loader、x64 Loader、LEProc 和测试。
   - 不使用 subagent。
   - 保持当前业务目的：启动非合作的旧 Windows 程序，并模拟其 ACP/OEMCP、LocaleID、字体 charset、UI 语言、时区、注册表行
 为和命令行/工作目录语义。
   - 不把“多语言 UI”误认为目标程序区域模拟；产品自身 UI 语言和目标程序区域模拟仍是两个独立概念。
   - 不要为了兼容未知外部调用者保留旧 ABI；如果删除导出、结构或文件，必须记录为破坏性更新。
   </user_decisions>

   <required_design>

   一、统一运行时环境 ABI

   设计一套架构无关的版本化环境块，例如：

   - 固定宽度整数类型。
   - Size。
   - Version。
   - AnsiCodePage。
   - OemCodePage。
   - LocaleID。
   - FontCharset。
   - RegistryMode。
   - HookUILanguageMode。
   - TimezoneId 的相对偏移和长度。
   - StandardNameOverride、DaylightNameOverride 的相对偏移和长度。
   - 可选注册表重定向项数组的相对偏移、数量和总长度。

   要求：

   - ABI 中不放裸指针。
   - ABI 中不放依赖进程位数的 `UNICODE_STRING`、`ULONG_PTR` 或 C# 对象布局。
   - 所有偏移、长度、数量先做整数溢出和边界校验。
   - 所有字符串必须明确编码、长度和终止规则。
   - Loader 在信任边界只验证一次，Core 使用已验证的表示。
   - x86/x64 Loader 和 Core 使用同一份权威 ABI 定义；不要维护两个结构副本。
   - 版本不匹配、大小不足、偏移越界、非终止字符串和无效枚举值必须返回明确失败状态。
   - 旧 LEB 和旧 LEPB 不再作为正式输入；如果临时保留迁移代码，必须有明确删除提交，不能留永久兼容层。

   二、统一托管启动模型

   把当前 LoaderWrapper.cs 与 LoaderWrapperX64.cs 中真正相同的职责收敛到共享模型：

   - ApplicationName。
   - CommandLine。
   - CurrentDirectory。
   - DebugMode。
   - ACP/OEMCP。
   - LocaleID。
   - FontCharset。
   - TimezoneId。
   - UI language mode。
   - Registry redirection policy。
   - 启动、等待、失败返回和句柄回收。

   保留两个很薄的架构适配层：

   - x86 NativeLoaderAdapter。
   - x64 NativeLoaderAdapter。

   共享层负责启动流程和生命周期；架构适配层只负责：

   - ABI 序列化。
   - 对应 LoaderDll 导出。
   - 架构专属 P/Invoke。
   - 架构专属类型和错误转换。

   不要把大量 `#if`、指针转换和架构条件塞回一个“万能 Wrapper”。

   三、统一时区责任

   把时区 ID 作为统一 ABI 输入：

   - 托管层只传递 Windows 时区 ID。
   - x86/x64 Loader 或 Core 在原生侧统一读取时区注册表和转换 TZI。
   - 删除 x86 托管层专用的时区字节编码、重复注册表读取和手工结构封送。
   - 保持当前失败语义：无效或不存在的时区必须可观察地失败，不得静默使用错误时区。
   - 保留东京等当前回归覆盖的标准名和夏令时语义。

   四、统一注册表重定向责任

   先确认当前产品真正需要的语义：

   - 如果标准/高级模式只需要模式级控制，则统一使用 RegistryMode，删除逐项注册表数组。
   - 如果高级模式确实需要逐项重定向，则在新 ABI 中使用架构无关的相对偏移数组，并让 x86/x64 Core 使用相同解码规则。

   无论选择哪条路径：

   - 删除旧的 `LERegistryRedirector` 专用旧 ABI 序列化。
   - 不保留“看起来支持逐项添加、实际完全忽略”的无声接口。
   - 如果 x64 不使用逐项项而只使用模式，托管层必须明确表达这一点，不能保留误导性的 no-op `AddRegistryRedirectEntry`。
   - 更新所有调用者、测试、文档和错误路径。

   五、原生实现边界

   - x86/x64 Core 的 Hook、PEB、TEB、系统调用、跳板、重定位和私有系统结构仍可独立实现。
   - 可以共享 ABI 验证、字段语义和测试向量。
   - 不要强行合并架构专属 Hook 代码。
   - 不要重新引入已经删除的 x64→x86 broker，除非同时提供完整、可验证、具备正确位数导出的 x86 broker，并完成真实跨架构回
 归。
   - 在没有完整实现和测试之前，继续明确“不支持 x64 目标创建的 x86 子进程继承 LE 环境”。
   - 不能用 `throw`、`__fastfail` 或测试开关掩盖 SAL、分配失败或 ABI 验证问题。
   - 保留失败可回退行为、句柄清理、补丁回滚、影子映像释放和子进程回收。

   </required_design>

   <behavior_to_preserve>

   必须继续通过并保持这些行为：

   - x86 目标由 x86 Loader/Core 启动。
   - x64 目标由 x64 Loader/Core 启动。
   - x64 包中的 Launcher、Shell、GUI 保存后运行、桌面快捷方式都根据目标 PE 架构选择正确 helper。
   - x86 单包仍能启动 x86 目标。
   - 命令行参数、空格、引号、工作目录和相对文件访问保持现有语义。
   - ACP/OEMCP、LocaleID、FontCharset、时区、UI language mode 和注册表模式行为保持现有产品契约。
   - DebugMode/CREATE_SUSPENDED 行为保持现有语义。
   - 启动失败、提权失败、无文件关联、无效配置和无效时区都必须返回可观察的失败。
   - 进程句柄、线程句柄、临时内存、映射、影子映像和测试子进程必须成对清理。
   - 不修改系统字体、注册表、机器 DLL 或系统区域设置。
   - 不新增产品导出、生产测试开关或测试专用生产路径。

   </behavior_to_preserve>

   <files_to_inspect_first>

   - `LEProc/LoaderWrapper.cs`
   - `LEProc/LoaderWrapperX64.cs`
   - `LEProc/LERegistryRedirector.cs`
   - `LEProc/Program.cs`
   - `LEProc/LEProc.csproj`
   - `Native/LoaderDll/LoaderDll.cpp`
   - `Native/LoaderDll/LoaderDll.h`
   - `Native/LocaleEmulatorX64/LoaderDll/LoaderDll.cpp`
   - `Native/LocaleEmulatorX64/LoaderDll/LoaderDll.h`
   - `Native/LocaleEmulatorX64/LoaderDll/Environment.cpp`
   - `Native/LocaleEmulatorX64/LoaderDll/Environment.h`
   - `Native/LocaleEmulator/LocaleEmulator.h`
   - `Native/LocaleEmulatorX64/LocaleEmulatorPlus/LocaleEmulatorPlus.h`
   - `Native/LocaleEmulator/LocaleEmulator.cpp`
   - `Native/LocaleEmulatorX64/LocaleEmulatorPlus/LocaleEmulatorPlus.cpp`
   - `LECommonLibrary/GlobalHelper.cs`
   - `build.ps1`
   - `build-dual.ps1`
   - `Scripts/Test.ps1`
   - `tests/Managed/LaunchRegression.cs`
   - `tests/Managed/LaunchInteropRegression.cs`
   - `tests/Native/NativeContracts.cpp`
   - `tests/Native/HardeningCheck.cpp`
   - `tests/Native/HardeningMessages.cpp`
   - `docs/QUALITY.md`
   - `docs/MAINTENANCE.md`
   - `README.md`

   </files_to_inspect_first>

   <migration_requirements>

   1. 先建立行为和 ABI 对照表，记录旧 x86 LEB、旧 x64 LEPB、新 ABI 的字段、大小、所有权和失败语义。
   2. 先实现新 ABI 的公共定义和验证，再迁移 x86 Loader/Core，再迁移 x64 Loader/Core。
   3. 再重写 LEProc 的共享启动模型和两个薄适配器。
   4. 删除旧 LEB、旧 LERegistryRedirector、旧序列化路径和无调用者导出。
   5. 更新所有项目文件、filters、导出定义、测试、构建脚本和文档。
   6. 每个里程碑单独提交；提交标题使用 Conventional Commit，中文摘要和正文，并包含当前 `$env:PI_MODEL` 的
 Co-Authored-By。
   7. 每次提交前只 stage 当前里程碑的明确文件，检查 `git diff --cached` 和 `git diff --check`。
   8. 不推送。

   </migration_requirements>

   <verification>

   至少必须运行：

   - x86 完整构建和回归：
     `pwsh -NoProfile -File ./build.ps1`
   - x86/x64 双包构建和回归：
     `pwsh -NoProfile -File ./build-dual.ps1`
   - x64 原生 Clean 构建：
     `pwsh -NoProfile -File ./Native/LocaleEmulatorX64/build-x64.ps1 -Clean`
   - 适用的托管项目构建和测试。
   - 原生 ABI 合同测试：
     - 合法环境块。
     - Size/Version 错误。
     - 偏移溢出。
     - 字符串未终止。
     - 数量和长度边界。
     - 分配失败。
     - 失败后的句柄、映射、临时内存和进程清理。
   - x86/x64 目标 PE 选择测试。
   - x86 和 x64 Loader/Core 的实际启动烟测。
   - ZIP 解包后再次执行 SHA256、PE machine、ASLR/NX 和启动检查。
   - 独立检查测试进程、broker、rundll32 和临时目录没有残留。
   - `git status --short` 必须干净。

   测试行为，不要只测试提示文本、标签、字符串格式或实现细节。字体 AllocStack/C6255 压力探针按既有用户决定跳过，不要重新
 尝试触发被拦截的安全分类器路径；但应继续验证失败可回退的堆分配和正常字体枚举。

   </verification>

   <review_loop>

   实现完成后严格按以下顺序执行，不能只顺序跑一遍：

   1. `code-review`
   2. `code-health-pass`
   3. `architecture-review`
   4. `overengineering-review`
   5. `reuse-review`
   6. `review-check`
   7. `ponytail-review`
   8. `ponytail-audit`

   规则：

   - 每项都要读取对应 `SKILL.md`。
   - 一次只执行当前项，完成并记录结果后再进入下一项。
   - 任意一项发现真实问题并提交修复，立即回到第 1 项。
   - 只有完整一轮八项都没有新问题，才算审查收敛。
   - 不使用 subagent。
   - review 结论必须区分已确认缺陷、已证伪候选、未验证风险和明确支持边界。
   - 不为了“继续找到问题”而制造风格意见或假设性缺陷。

   建议加载的 skills：

   - `interface-design`
   - `code-health-pass`
   - `architecture-review`
   - `overengineering-review`
   - `reuse-review`
   - `review-check`
   - `code-review`
   - `ponytail-review`
   - `ponytail-audit`
   - `diagnose`（只在出现真实运行时失败时使用）

   </review_loop>

   <completion_criteria>

   只有同时满足以下条件才能结束：

   - 新 ABI 已被 x86/x64 Loader/Core 使用。
   - LEProc 不再依赖旧版 x86 LEB 和旧的逐项序列化器。
   - x86/x64 托管启动流程共享公共编排，架构差异集中在薄适配层。
   - 旧 ABI、旧序列化代码、无调用者导出、项目文件残留引用和过时文档全部删除。
   - x86、x64、双包和解包后的实际产品验证全部通过。
   - 所有行为回归通过。
   - 完整八项 review loop 连续一轮没有新问题。
   - 每个里程碑都有本地提交，未推送。
   - 工作树干净。
   - 最终报告列出：
     - 所有提交。
     - 新旧 ABI 迁移摘要。
     - 保留的 x86/x64 原生分叉及原因。
     - 实际运行的命令和结果。
     - 破坏性变化。
     - 明确未支持的跨架构子进程继承边界。
     - 跳过的字体 C6255 压力探针及其风险。
   </completion_criteria>

   开始前不要只给计划。先检查当前状态、读取项目指导文件和相关 skills，然后直接开始第一项可执行工作。只有在所有
 completion criteria 都满足后才结束。