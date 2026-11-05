# 源码来源与合仓记录

本项目由 Locale Emulator 前端与原生 Core 合并而成。导入采用保留原始提交的双亲合并，未压缩或重写上游历史。

| 组件 | 上游 | 导入提交 | 仓库内位置 |
| --- | --- | --- | --- |
| 前端、配置、安装器及传统菜单 | https://github.com/xupefei/Locale-Emulator | `2f3f30dc5169b3cf051af389ec1dcc52f167a93d` | 根目录各 C# 工程 |
| 原生注入与语言模拟 | https://github.com/xupefei/Locale-Emulator-Core | `ae7160dc5deb97947396abcd784f9b98b6ee38b3` | `Native/` |

合仓提交的第一父提交是前端版本，第二父提交是原始 Core 版本。导入时 `Native/` 的 Git tree 与 Core 原始 tree 完全一致；后续维护在独立提交中进行。Core 的 31 个原始提交均保留为祖先，可用 `git log --all` 查看。上游附带的工具链归档也保留在原始历史中。

前端保留根目录 `COPYING` 和 `COPYING.LESSER`；Shell 示例代码另有 Microsoft Public License，图标来源见上游前端历史。Core 保留 `Native/LICENSE-GPL.txt` 与 `Native/LICENSE-LGPL.txt`。原始 Core README 明确将 `Loader`、`LoaderDll`、`LocaleEmulator` 三目录源码以 LGPL-3.0 授权；Microsoft 工具链、SDK/WDK 等依赖的授权另行适用。

前期调查固定于 2026-11-05：不整体采用其他 Fork。ParkSnoopy 的托管构建升级、dual1208 的打包和用户配置思路作为参考；KidiXDev 的 Core 虽增加 5 个提交，最终 tree 与原版一致。Cliff-Yang 的 owner-draw 修补和 SashiRin 的字体 hook 只在相应问题被复现并有回归验证后考虑适配。Locale Remulator 的 x64 路线和 ARM64 实验不混入本次 Win11 x64 主机上的 x86 维护范围。
