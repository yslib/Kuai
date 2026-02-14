
为了同时覆盖 REPL（持久状态）、LSP（多文件/增量分析） 和 纯编译器（单次构建） 这三种场景，我们需要一个“中心化状态 + 模块化驱动”的架构。


/tofu (Workspace Root)
├── tofu-cli (二进制: REPL, Daemon, CLI)
│
├── crates/
│   ├── tofu-base      (基础: Span, Diagnostic, Id/Spur)
│   ├── tofu-syntax    (语法: AST, Lexer, Parser, 嵌套 Attribute 定义)
│   ├── tofu-sema      (语义: SymbolTable, Scope, Resolver, Inference)
│   ├── tofu-driver    (核心驱动: AttrRegistry, Pipeline, Session, Context)
│   └── tofu-plugins   (属性插件: @kernel, @export, @einsum 等)
│
└── dev_tools/ (Editor support)


1. TofuSession (全局静态配置)
在进程启动时创建，持有所有只读、全局的信息。
配置：目标架构（CPU/GPU）、优化等级。
注册表：AttrRegistry（加载了哪些属性插件）。
管理：在 LSP 模式下管理多个 Context。
2. TofuContext (内存/记忆体)
这是你所谓的“黑板”所在地。它持有所有可变的编译状态。
String Pool：ThreadedRodeo。
Global Scope：Arc<RwLock<Scope>>（存储 dim、func 声明）。
Artifacts：属性生成的后端代码。
3. TofuCompiler (无状态驱动)

它是一个“执行器”，负责把 Context 和 Source 扔进 Pipeline。

二、 三大场景的适配方案
场景 1：REPL / Daemon (持久记忆模式)
生命周期：Session 和 Context 随进程常驻。
逻辑：
启动 Session，初始化 Context。
REPL 输入或 Socket 收到代码块。
调用 Compiler::run_pipeline(&session, &mut context, source)。
关键：context 的 GlobalScope 在调用后不销毁，因此下次输入依然记得 dim M。
场景 2：纯编译器 (单次构建模式)
生命周期：Session 和 Context 随任务创建和销毁。
逻辑：
CLI 读取 main.tof。
创建临时 Context。
Compiler::run_pipeline 跑完所有 Stage (Raw -> Resolved -> Analyzed -> Backend)。
提取 context.artifacts 写入磁盘。
场景 3：LSP (增量/多文件模式)
生命周期：Session 常驻，为每个文件维护一个 Context。
逻辑：
当用户打开 a.tof 和 b.tof 时，LSP 服务器持有 Map<Path, Context>。
错误恢复：即便 a.tof 有语法错，LSP 会通过 Parser 的诊断信息并在 Context 中保留“部分正确的 AST”，以支持语法高亮和成员补全。
跳转定义：通过 Context 里的 GlobalScope 快速查找 Ident 的定义位置。

REPL: 一个session 一个context
编译器模式： 一个session 一个context
LSP：一个session, 多个context

compiler 是无状态的
