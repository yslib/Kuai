#![allow(dead_code)]

//  tofu语言的抽象语法树定义, tofu作为一个中间语言,
//  语法相对简单。主要是描述一个语法前端，
//  作为微架构编译器，在语法解析层面不做过多假设，语法尽可能宽松。后端主要靠属性系统来实现。
//  即用户实现自己的属性拿到ast之后进行后端代码生成。

pub type Span = std::ops::Range<usize>;
#[derive(Debug, Clone, PartialEq)]
pub struct Ident {
    pub name: String,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Attribute {
    pub name: String,
    // 简化起见，属性参数暂时只支持字符串或数字
    // 实际项目中这里应该是 Vec<Expr>
    pub args: Vec<Expr>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    Import(String),
    Function(FuncDecl),
    Struct(StructDecl),
    Return(Expr),
    Expr(Expr),
}

#[derive(Debug, Clone, PartialEq)]
pub enum PrimitiveType {
    F32,
    I32,
    Bool,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TypeExpr {
    Primitive(PrimitiveType),
    //  Struct or Generic  type like Vector<f32>
    Named {
        name: Ident,

        //  类型上下文中不用turbo-fish语法，只用尖括号包裹类型参数
        generics: Vec<TypeExpr>,
    },

    // base类型本身可以是一个表达式类型
    // 例如 Vector<f32>[128, 128]
    Tensor {
        base: Box<TypeExpr>,
        shape: Vec<Expr>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub struct FuncProto {
    pub name: Ident,
    pub generics: Vec<GenericParam>,
    pub params: Vec<Param>,
    pub return_type: Option<TypeExpr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Block {
    pub stmts: Vec<Stmt>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct FuncDecl {
    pub attributes: Vec<Attribute>,
    pub proto: FuncProto,
    pub body: Block,
}

#[derive(Debug, Clone, PartialEq)]
pub struct StructDecl {
    pub attributes: Vec<Attribute>,
    pub name: Ident,
    // 字段暂略
}

#[derive(Debug, Clone, PartialEq)]
pub struct Param {
    pub name: Ident,
    pub ty: TypeExpr,
}

#[derive(Debug, Clone, PartialEq)]
pub struct GenericParam {
    pub name: Ident,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Literal(Literal),
    Variable(Ident),

    // 二元操作: a + b
    Binary {
        left: Box<Expr>,
        op: BinaryOp,
        right: Box<Expr>,
    },

    // 成员访问: a.b (可能是字段，可能是 Swizzle)
    MemberAccess {
        target: Box<Expr>,
        member: Ident,
    },

    // 索引/Einsum: a[i, j]
    Index {
        target: Box<Expr>,
        indices: Vec<Expr>,
    },

    // 向量字面量: [1 2 3] 或 [1, 2, 3]
    VectorLiteral(Vec<Expr>),

    Call {
        func: Box<Expr>,
        generics: Vec<TypeExpr>, // turbo-fish style generics in expressions
        args: Vec<Expr>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum Literal {
    Float(f64),
    Int(i64),
    String(String),
}

#[derive(Debug, Clone, PartialEq)]
pub enum BinaryOp {
    Plus,
    Minus,
    Star,
    Slash,
    Assign,
}
