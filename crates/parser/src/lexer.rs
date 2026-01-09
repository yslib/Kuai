// src/lexer.rs
use logos::Logos;

fn line_comment(lex: &mut logos::Lexer<Token>) -> logos::Skip {
    if let Some(len) = lex.remainder().find('\n') {
        lex.bump(len);
    } else {
        lex.bump(lex.remainder().len());
    }
    logos::Skip
}

#[derive(Logos, Debug, PartialEq, Clone)]
#[logos(skip r"[ \t\n\f]+")] // 忽略空格、换行符（核心：我们依靠方括号上下文来解析向量，而不是依靠换行）
pub enum Token {
    // ============================================
    // 1. 关键字 (Keywords)
    // ============================================
    #[token("func")]
    KwFunc,

    #[token("struct")]
    KwStruct,

    #[token("import")]
    KwImport,

    #[token("return")]
    KwReturn,

    #[token("let")]
    KwLet,

    #[token("dim")]
    KwDim,

    #[token("if")]
    KwIf,

    #[token("else")]
    KwElse,

    #[token("for")]
    KwFor,

    #[token("in")]
    KwIn,

    #[token("step")]
    KwStep,

    #[token("break")]
    KwBreak,

    #[token("continue")]
    KwContinue,

    // 基础类型关键字 (虽然是渐进类型，但基础类型通常作为关键字保留)
    #[token("bool")]
    TypeBool,
    #[token("char")]
    TypeChar,
    #[token("i8")]
    TypeI8,
    #[token("i16")]
    TypeI16,
    #[token("i32")]
    TypeI32,
    #[token("i64")]
    TypeI64,
    #[token("f32")]
    TypeF32,
    #[token("f64")]
    TypeF64,

    // ============================================
    // 2. 属性系统 (Attributes) - 核心扩展点
    // ============================================
    // 匹配 @ 后面跟标识符，例如 @kernel, @swizzle_set
    // slice() 会包含 @ 符号，Parser 阶段可以去掉
    #[regex(r"@[a-zA-Z_][a-zA-Z0-9_]*")]
    Attribute,

    // ============================================
    // 3. Literals
    // ============================================
    // 浮点数：支持 1.0, 0.1, 1e10, 1.2e-5
    // 注意：必须放在 Integer 之前，或者是更贪婪的匹配
    #[regex(r"-?(?:0|[1-9]\d*)\.\d+(?:[eE][+-]?\d+)?")]
    #[regex(r"-?\.\d+(?:[eE][+-]?\d+)?")]
    FloatLiteral,

    // 整数
    #[regex(r"-?(?:0|[1-9]\d*)")]
    IntegerLiteral,

    // 字符串字面量 (用于 import "path" 或 属性参数)
    #[regex(r#""([^"\\]|\\[\s\S])*""#)]
    StringLiteral,

    // ============================================
    // 4. Identifiers
    // ============================================
    // 包含变量名、函数名、Swizzle 分量名（如 .xyz 中的 xyz 部分会先被识别为 Dot 然后是 Ident）
    #[regex(r"[a-zA-Z_][a-zA-Z0-9_]*")]
    Identifier,

    // ============================================
    // 5. Symbols & Operators
    // ============================================
    #[token(".")]
    Dot, // 成员访问或 Swizzle 的开始

    #[token("..")]
    DoubleDot, // range

    #[token(",")]
    Comma,

    #[token(":")]
    Colon,

    #[token("::")]
    DoubleColon,

    #[token(";")]
    SemiColon,

    #[token("=")]
    Assign,

    #[token("->")]
    Arrow, // 函数返回值箭头

    // 括号
    #[token("(")]
    LParen,
    #[token(")")]
    RParen,
    #[token("[")]
    LBracket, // 张量形状定义或索引
    #[token("]")]
    RBracket,
    #[token("{")]
    LBrace,
    #[token("}")]
    RBrace,

    // 数学运算
    #[token("+")]
    Plus,
    #[token("-")]
    Minus,
    #[token("*")]
    Star,
    #[token("/")]
    Slash,

    // 比较
    #[token("==")]
    Eq,
    #[token("!=")]
    Ne,
    #[token("<")]
    Lt,
    #[token(">")]
    Gt,

    // 注释处理 (Logos 会自动忽略匹配到的内容，如果返回 Skip)
    #[regex(r"//", line_comment)]
    Comment,
}

pub fn check_balanced(source: &str) -> (bool, i32) {
    let lexer = Token::lexer(source);
    let mut depth = 0;
    let mut has_token = false;

    for token in lexer {
        has_token = true;
        match token {
            Ok(Token::LBracket) | Ok(Token::LBrace) => depth += 1,
            Ok(Token::RBracket) | Ok(Token::RBrace) => {
                depth -= 1;
            }
            _ => {}
        }
    }
    (has_token && depth <= 0, depth)
}
