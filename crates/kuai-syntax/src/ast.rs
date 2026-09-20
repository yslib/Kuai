use kuai_core::spur::Id;
pub type Span = std::ops::Range<usize>;
pub const ERROR_IDENT_NAME: &str = "<error_ident>";

#[derive(Debug, Clone, PartialEq)]
pub struct Module {
    pub top_level_attributes: Vec<Attribute>,
    pub stmts: Vec<Stmt>,
    pub span: Span,
}

#[derive(Clone, PartialEq)]
pub struct Ident {
    pub id: Id,
    pub span: Span,
}

// temporary impls until we have a better way to display interned strings
impl std::fmt::Debug for Ident {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.id)
    }
}

impl std::fmt::Display for Ident {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.id)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Argument {
    Positional(Expr),
    Named { name: Ident, value: Expr },
    Attribute(Attribute),
}

#[derive(Debug, Clone, PartialEq)]
pub struct Attribute {
    pub name: String,
    pub args: Vec<Argument>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Range {
    pub start: Expr,
    pub end: Expr,
    pub step: Option<Expr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ElseBranch {
    Block(Block),
    If(Box<Stmt>),
}
#[derive(Debug, Clone, PartialEq)]
pub struct Path {
    pub segments: Vec<Ident>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct DimDecl {
    pub name: Ident,
    pub bound: Option<Expr>, // dim M: 16, multiple 16
    pub value: Option<Expr>, // dim M = 128
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct VarDecl {
    pub name: Ident,
    pub ty: Option<TypeExpr>,
    pub init: Option<Expr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Stmt {
    pub attributes: Vec<Attribute>,
    pub stmt: StmtImpl,
}

#[derive(Debug, Clone, PartialEq)]
pub enum StmtImpl {
    Import(Path),
    Function(FuncDecl),
    Struct(StructDecl),
    DimDecl(Vec<DimDecl>), // support multi-dim declaration: dim M = 128, N = 256
    VarDecl(VarDecl),
    Assignment {
        target: Expr,
        value: Expr,
        span: Span,
    },

    For {
        var: Ident,
        range: Range,
        body: Block,
        span: Span,
    },

    If {
        condtion: Expr,
        then_branch: Block,
        else_branch: Option<ElseBranch>,
    },

    Break(Span),
    Continue(Span),
    Return(Expr),
    Expr(Expr),
    Error {
        span: Span,
    },
}

impl Stmt {
    pub fn get_attributes(&self) -> Option<&Vec<Attribute>> {
        Some(&self.attributes)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum PrimitiveType {
    Bool,
    Char,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TypeExpr {
    Primitive(PrimitiveType),
    //  Struct or Generic  type like Vector<f32>
    Named {
        name: Ident,

        // Type contexts use angle brackets for type arguments, without turbofish syntax.
        generics: Vec<TypeExpr>,
    },

    // The base type can itself be a type expression.
    // For example: Vector<f32>[128, 128].
    Tensor {
        base: Box<TypeExpr>,
        shape: Vec<Expr>,
    },

    Error {
        span: Span,
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
    pub span: Span,
    // maybe there could be attributes later
}

#[derive(Debug, Clone, PartialEq)]
pub struct FuncDecl {
    pub proto: FuncProto,
    pub body: Option<Block>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct FieldDecl {
    pub name: Ident,
    pub ty: TypeExpr,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct StructDecl {
    pub name: Ident,
    pub generics: Vec<GenericParam>,
    pub fields: Vec<FieldDecl>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Param {
    pub name: Ident,
    pub ty: TypeExpr,
    pub default_value: Option<Expr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub struct GenericParam {
    pub name: Ident,
    pub bound: Option<TypeExpr>,
    pub default_type: Option<TypeExpr>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Literal(Literal),
    Variable(Ident),

    // Binary operation: a + b
    Binary {
        left: Box<Expr>,
        op: BinaryOp,
        right: Box<Expr>,
    },

    // Member access: a.b (a field or a swizzle)
    MemberAccess {
        target: Box<Expr>,
        member: Ident,
    },

    // Namespace member access: a::b
    NamespaceAccess {
        namespace: Box<Expr>,
        member: Ident,
    },

    // Indexing/Einsum: a[i, j]
    Index {
        target: Box<Expr>,
        indices: Vec<Expr>,
    },

    // Vector literal: [1 2 3] or [1, 2, 3]
    VectorLiteral(Vec<Expr>),

    Call {
        func: Box<Expr>, // it's not identity, could be member access or other expression
        generics: Vec<TypeExpr>, // turbo-fish style generics in expressions
        args: Vec<Argument>,
    },

    Error(Span),
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
}
