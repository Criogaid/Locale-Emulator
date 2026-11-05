# 循环审查与官方开发契约

本轮同时使用 overengineering-review、code-health-pass、architecture-review。审查范围覆盖原生注入与 hooks、C# 配置和启动、GUI、安装器、传统菜单、构建、回归与发行。修复按独立里程碑提交；每轮修复后重新核对调用者、失败路径和行为回归，再进行全范围独立复审。

## 第 1 轮问题台账

审查基线为 `3ea9ecde90073a13364d611d82defaef19f80ce3`。本机证据保存在 `artifacts/review-round1/`，系统为 Windows 11 24H2 / x64 / 26100.9168。问题编号用于后续复审追踪，不表示不同代码必须共用一套抽象。

| 编号 | 已确认的触发条件与后果 | 处理状态 |
| --- | --- | --- |
| C01 | XML 不允许的字符使保存截断已有配置 | 已改为同目录临时文件、完整写入和原子替换；保留旧文件，失败不继续启动 |
| C02 | 本机缺少已保存的区域或时区，编辑器以 -1 索引列表 | 保存前要求有效选择；旧配置和内存状态保持，重新选择后可重试 |
| C03 | 只读翻译文件被以读写方式打开，流未释放 | 以只读流加载并及时释放；可选翻译失败保留内置语言 |
| N01 | 合法注册表 LEB 的复制长度重复增加，读取越过末尾保护页 | 修复与保护页回归进行中 |
| N02 | 子进程仿真注入失败仍报告创建成功，部分失败分支遗漏回滚 | 修复与失败注入验证进行中 |
| N03 | Sandbox 枚举声明的缓冲区长度为实际分配的两倍 | 修复与边界检查进行中 |
| N04 | 注册表完整信息的 NameLength 包含额外终止 WCHAR | 修复与输出 ABI 回归进行中 |
| S01 | 安装器替换第二个被占用 DLL 失败后，第一个 DLL 已被删除 | 修复与文件锁回归进行中 |
| S02 | 安装、卸载异常被吞掉，按钮继续报告成功，HKCR 映射未必恢复 | 修复失败传播和资源恢复进行中 |
| S03 | 菜单实例创建四个 HBITMAP，但只释放两个 | 修复与 GDI 资源计数回归进行中 |
| S04 | 菜单忽略宿主命令 ID 上限、插入位置，并多报占用数 | 修复与独立菜单宿主验证进行中 |
| L01 | 提权重启改变工作目录，原始相对路径指向另一个位置 | 调用链已确认，维护中 |
| L02 | 语言重定向写入 CodePage 键且使用十进制 LCID | 调用链及本机实际 Language 键已确认，维护中 |
| L03 | 非 EXE 关联命令丢弃 CLI 尾参数 | 调用链已确认，维护中 |
| L04 | 夏令时名称被标准名称覆盖 | Fork 与当前消费者已比对，维护中 |
| F01 | owner-draw 无 HASSTRINGS 控件的 item data 被当字符串转换 | 依据消息契约适配与真实控件回归进行中 |
| B01 | 托管目标仍为已停止支持的 Framework 4.0 Client | 官方 reference assemblies 与 4.8 构建迁移进行中 |
| B02 | 编译器诊断、重复 SDK 常量、无效链接选项和关闭的 PE 保护 | 按实际编译命令与二进制逐项整改进行中 |
| B03 | 无调用的旧签名入口固定 SDK16299，失败不可靠传播 | 依据实际调用决定删除或收敛，不新增证书策略 |

C01—C03 已用实际产品程序集验证：非法 XML 和锁定文件失败后原数据保持；无效选择不会变更配置；只读翻译可加载且加载后文件可移动。现有参数、配置、WPF 以及 CP932/936、父子进程、目录和主窗口检查继续通过。

## 规范与支持边界

官方工具链、C++ 声明、SDK 的公开函数参数、输出结构和资源生命周期是本轮直接整改对象。规范依据包括：

- [C++ 安全开发建议](https://learn.microsoft.com/en-us/cpp/security/security-best-practices-for-cpp)：GS、ASLR、DEP、SafeSEH 和静态分析分别验证。
- [自定义入口的 security cookie 初始化](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/security-init-cookie)：不能启用 GS 却跳过初始化。
- [官方 Framework reference assemblies](https://learn.microsoft.com/en-us/dotnet/framework/migration-guide/reference-assemblies)：构建期引用不等于运行时安装，也不应装入发行目录。
- [QueryContextMenu 契约](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-querycontextmenu)：遵守插入位置、命令区间和准确占用数。
- [位图句柄所有权](https://learn.microsoft.com/en-us/dotnet/api/system.drawing.bitmap.gethbitmap)：每个创建的句柄必须配对释放。

Core 的 NLS/PEB 替换、私有调用扫描和 hooks 是当前仿真机制的组成部分。微软的[内部 API 说明](https://learn.microsoft.com/en-us/windows/win32/devnotes/calling-internal-apis)和[未文档化 API 指导](https://learn.microsoft.com/en-us/windows/compatibility/undocumented-apis)明确不保证其跨版本稳定。现有进程内托管菜单还涉及[托管 Shell 扩展指导](https://learn.microsoft.com/en-us/windows/win32/shell/shell-and-managed-code)的限制。这些支持边界不能用“官方 SDK 编译通过”或单个 Windows build 的回归通过消除；本轮必须单独审查和说明。

## 行为变化记录

| 里程碑 | 行为影响 |
| --- | --- |
| 配置安全保存与编辑器边界 | 失败时保留旧文件与窗口；非法 XML、不可用区域或时区产生可处理的错误，修正输入后可重试 |
| 翻译读取 | 只读文件正常使用；读取完成后释放文件句柄；缺失或损坏的可选翻译继续回退内置语言 |

原有 XML 字段、GUID、参数优先级、原生公开导出及 x86 目标边界继续作为兼容性约束。后续轮次以当前代码和可重复探针为准；没有触发证据的重写、通用框架和预留选项不作为整改项。
