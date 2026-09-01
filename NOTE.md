---
### **1. 项目定位与核心哲学**
*   **项目名称**：Kuai (后缀 `.ku`)。
*   **定位**：微架构编译器，专注于 AI 张量计算与 Shader 生成。初期作为 Python 的高性能插件（类似 Triton/Taichi），通过 FFI 和零拷贝（DLPack/Array Interface）与外部生态（NumPy/PyTorch）交互。
*   **设计哲学 (微内核架构)**：编译器内核保持极薄，仅负责基础语法解析和属性分发。所有核心语义（如 `Einsum`、并行化、代码生成、FFI 绑定）均通过插件化的**属性（Attribute）系统**实现。

### **2. 编译器架构 (Industrial-Grade Pipeline)**
项目采用分层的 Workspace 结构，解决了循环依赖并实现了关注点分离：
*   **`kuai-core`**：底层基石，定义 `Span`、`Id` (Spur) 和 `Diagnostic`。
*   **`kuai-syntax`**：基于 **Logos** 的词法分析和 **Pratt Parser** 的语法分析。支持：
    *   `dim` 关键字声明符号维度；
    *   Julia 风格向量字面量（空格/逗号分隔）；
    *   嵌套属性语法：`@parent(arg=@child)`。
*   **`kuai-sema`**：语义分析。实现基于 `Arc<RwLock<Scope>>` 的分层作用域符号表，支持符号持久化记忆。
*   **`kuai-driver`**：核心驱动。
    *   **`KuaiContext`**：持久化状态，持有 `ThreadedRodeo` (String Interning) 和全局作用域。
    *   **`AttrPipeline`**：分阶段（Raw, Resolved, Analyzed, Backend）驱动 AST 流转。
*   **`kuai-cli`**：应用入口。包含：
    *   **智能 REPL**：基于括号平衡算法处理多行输入；
    *   **TCP/Socket Server**：监听 `ip:port`，允许 Neovim 等编辑器以原子块形式发送代码并获取实时反馈。


### **3. 核心机制：递归属性系统**
这是 Kuai 最具特色的设计，旨在通过属性接管编译器流程：
*   **接口拆分**：
    *   `AttrBase`：提供 `evaluate` 接口往“黑板”（Metadata）写数据。
    *   **多态 Trait**：`SyntaxTransformer` (AST 变换), `SemanticChecker` (语义验证), `BackendEmitter` (代码生成)。
    *   **Trait Casting**：通过 `as_syntax()` 等钩子实现从 `dyn AttrBase` 到具体阶段接口的横向转换。
*   **协作逻辑**：
    *   **两阶段执行**：先跑全量 `evaluate` 填充元数据，再跑 `transform` 执行逻辑。
    *   **环境传播**：支持 `recursive` 属性，父节点的属性可作为“环境场”影响子节点。
    *   **控制流信号**：处理器可返回 `Continue`, `SkipChildren`, 或 `Terminal`（接管并终止后续流程）。

    每个属性都有几个固定阶段，分别在不同的编译阶段被调用：


### **4. 当前完成状态 (Milestones)**
*   [x] **前端解析**：完成基础语法、嵌套属性、函数/结构体定义、`dim` 声明的解析。
*   [x] **基础架构**：实现 `KuaiContext` 及其跨线程所有权管理（`Arc<Mutex<Context>>`）。
*   [x] **字符串池化**：全面集成 `lasso` 进行标识符池化。
*   [x] **符号表**：完成基于 `Arc<RwLock>` 的全局和局部作用域管理。
*   [x] **交互环境**：
    *   完善的 `miette` 诊断输出。
    *   带语法高亮的 Rustyline REPL。
    *   Neovim 插件集成（ToggleTerm + TCP Socket 通信）。
    *   支持多模式（REPL, `-e` 字符串执行, 文件执行, Daemon 模式）。

### **5. 待开发与下一步计划**
1.  **实装 `KuaiCompiler` 驱动**：正式整合 `Pipeline` 到 `main.rs` 流程中。
2.  **完善 `Resolver` Pass**：
    *   实现符号重定义检查和未定义引用检查；
    *   实现初步的维度检查（Dimension 符号 vs 普通 Variable 的区分使用）。
3.  **属性处理器原型**：
    *   实现 `@export`：生成 C-ABI 描述。
    *   实现 `@kernel`：尝试在 `Analyzed` 阶段拦截 AST 并打印伪代码。
4.  **形状推导 (Shape Inference)**：这是 AI DSL 的灵魂，需在 `Analyzed` 阶段实现。

---
**新会话启动建议：**
“我已经搭建好了 Kuai 编译器的微内核架构，包括基于 `Arc<Mutex<KuaiContext>>` 的线程安全上下文、支持嵌套属性的 `Parser`、以及分阶段的属性管道。现在请帮我从 `kuai-driver` 的 `KuaiCompiler` 流水线实现开始，并着手编写第一个能够处理 `dim` 声明和验证 Tensor 形状的语义 Pass。”



