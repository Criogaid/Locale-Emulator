# 源码来源与合仓记录

本项目由 Locale Emulator 前端与原生 Core 合并而成。导入采用保留原始提交的双亲合并，未压缩或重写上游历史。

| 组件 | 上游 | 导入提交 | 仓库内位置 |
| --- | --- | --- | --- |
| 前端、配置、安装器及传统菜单 | https://github.com/xupefei/Locale-Emulator | `2f3f30dc5169b3cf051af389ec1dcc52f167a93d` | 根目录各 C# 工程 |
| 原生注入与语言模拟 | https://github.com/xupefei/Locale-Emulator-Core | `ae7160dc5deb97947396abcd784f9b98b6ee38b3` | `Native/` |

合仓提交的第一父提交是前端版本，第二父提交是原始 Core 版本。导入时 `Native/` 的 Git tree 与 Core 原始 tree 完全一致；后续维护在独立提交中进行。Core 的 31 个原始提交均保留为祖先，可用 `git log --all` 查看。上游附带的工具链归档也保留在原始历史中。

`Native/ExplorerCommand/`、`Scripts/ModernMenu.ps1` 以及安装器的无窗口模式与一级菜单集成是本仓库新增的代码，不来自任何上游导入或外部 Fork。该部分尚未单独指定许可证；它随仓库分发，沿用前端的归属记录，是否另行声明由维护者决定。

前端保留根目录 `COPYING` 和 `COPYING.LESSER`；Shell 示例代码另有 Microsoft Public License，图标来源见上游前端历史。Core 保留 `Native/LICENSE-GPL.txt` 与 `Native/LICENSE-LGPL.txt`。原始 Core README 明确将 `Loader`、`LoaderDll`、`LocaleEmulator` 三目录源码以 LGPL-3.0 授权；Microsoft 工具链、SDK/WDK 等依赖的授权另行适用。

前期调查固定于 2026-11-05，比较基线是前端 `2f3f30dc5169b3cf051af389ec1dcc52f167a93d` 与 Core `ae7160dc5deb97947396abcd784f9b98b6ee38b3`。本轮逐项复现后适配有效改动，不整体合并 Fork；原始比较仓库保存在同级 `Locale-Emulator-comparison`。采用记录如下：

| 来源与固定提交 | 适配内容与依据 | 许可 |
| --- | --- | --- |
| [ParkSnoopy/LocaleEmulator](https://github.com/ParkSnoopy/LocaleEmulator/commit/ce5d6dcca23e728b4c100029d95ce3cbd8e65b8f) | Framework 4.8 迁移方向；本项目以统一 props、官方 reference assemblies 和派生配置实现，冷缓存完整产品与 ZIP 回归通过 | 前端保留 LGPL-3.0，Shell 原有 MS-PL 边界不变 |
| [Cliff-Yang/Locale-Emulator-Core](https://github.com/Cliff-Yang/Locale-Emulator-Core/commit/889fbda1d78e680f50285b8cccff7b44b08630e5) | owner-draw 崩溃线索；原补丁缺少控件类型、HASSTRINGS 与完整消息区分。本项目按 SDK 消息契约重新实现，含 superclass、ComboLBox 与自定义窗口边界，218 项真实控件组合验证 | Core LGPL-3.0 |
| [SashiRin/Locale-Emulator](https://github.com/SashiRin/Locale-Emulator/commit/9fbf4847688a720b9f2277f68dbb8ca8664b8cdb)，复核快照 `f553456bd902098cfa13a992905ed7532a6cf733` | 只适配 DaylightName 使用实际夏令时名称的修正；保留原有标准时及不自动应用 DST 的产品契约，名称按 SDK WCHAR[32] 边界序列化 | 前端 LGPL-3.0 |
| [julixian/LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core/commit/536f3604ca72e0c9d3537d008873b7e811e810ee) | x64 Core、Loader 和 support 目录的移植参考；本项目按官方 MSVC/SDK 头文件、导入库和 `/GS`、ASLR、DEP 约束重新构建，并修复字体 name 表的堆分配边界。Fork 中关闭 GS、关闭 ASLR、合并可写可执行节等不安全链接选项均拒绝采用；缺少 WDK 的 ntnls/mountmgr 声明保留为本地兼容头 | LGPL-3.0 |


上述改动保留原作者、源码与许可归属。dual1208 的必需文件校验和自动汇总经验已经由本项目完整产品打包与实际 ZIP 复测覆盖；未带入其 MSIX 身份及机器专用路径。KidiXDev 的 Core 最终 tree 与原版相同，不制造无净变化的移植。SashiRin Core `72a03dcc368a87dab2e77a1671493247083ea29a` 的旧版 Windows 字体 hook 和前端 MUI 名称分支没有当前目标上的故障证据，暂不带入；Locale Remulator 的 x64 路线与 ARM64 实验不混入当前 x86 产品。
