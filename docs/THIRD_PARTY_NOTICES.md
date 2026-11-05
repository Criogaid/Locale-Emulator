# 第三方来源与许可说明

## 原始项目

- Locale Emulator：作者 xupefei / Paddy Xu，原始仓库 https://github.com/xupefei/Locale-Emulator 。前端源码保留 LGPL-3.0 许可文本。
- Locale Emulator Core：原始仓库 https://github.com/xupefei/Locale-Emulator-Core 。原始 README 明确将 `Loader`、`LoaderDll`、`LocaleEmulator` 源码目录以 LGPL-3.0 授权，保留原始 GPL 与 LGPL 文本。导入 SHA 及历史关系见 [SOURCES.md](SOURCES.md)。
- 原始源码文件中的作者、版权和第三方声明继续适用。

## Microsoft Shell 示例代码

原版传统右键菜单使用 [Microsoft All-In-One Code Framework](https://blogs.msdn.com/b/onecode/) 的 C# Shell 扩展示例。相关文件保留其 Microsoft 版权和 Microsoft Public License（Ms-PL）声明；完整文本位于仓库 `licenses/MS-PL.txt`，发行包也携带该文件。

完整许可文本取自 SPDX license-list-data 的 `text/MS-PL.txt`，原始文本 Git blob 为 `c61790bc8d8d5dedf3752490fd035e337bae8874`。未对许可条文进行翻译或改写。

## 图标

原版 README 记录的 Flat icon set 来源：[graphicex](https://graphicex.com/icon-and-logo/15983-flat-alphabet-in-9-colors-with-long-shadow-6913875.html)。原始引入提交：[eae9fbc27f1a4c85986577202b61742c6287e10a](https://github.com/xupefei/Locale-Emulator/commit/eae9fbc27f1a4c85986577202b61742c6287e10a)。本轮沿用现有资源和来源信息，未新增图标素材。

## 构建环境

官方 MSVC 和 Windows SDK 由开发者通过 Visual Studio Installer 安装，遵循其各自许可。仓库不再附带其修改版编译器、旧 SDK/WDK 或预编译第三方库；旧上游文件仍可从合并保留的历史访问。生成的补充导入库仅描述 Windows 系统 DLL 已导出的符号。
