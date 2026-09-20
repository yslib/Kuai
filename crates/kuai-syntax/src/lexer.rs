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
// Skip whitespace and newlines; vector parsing uses bracket context, not line breaks.
#[logos(skip r"[ \t\n\f]+")]
pub enum Token {
    // ============================================
    // 1. Keywords
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

    // Primitive type keywords (reserved even with gradual typing).
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
    // 2. Attributes - the main extension point
    // ============================================
    // Match @ followed by an identifier, such as @kernel or @swizzle_set.
    // slice() includes the @ prefix, which can be removed during parsing.
    #[regex(r"@[a-zA-Z_][a-zA-Z0-9_]*")]
    Attribute,

    // ============================================
    // 3. Literals
    // ============================================
    // Floating-point numbers: supports 1.0, 0.1, 1e10, 1.2e-5.
    // Note: must precede Integer or use a greedier match.
    #[regex(r"-?(?:0|[1-9]\d*)\.\d+(?:[eE][+-]?\d+)?")]
    #[regex(r"-?\.\d+(?:[eE][+-]?\d+)?")]
    FloatLiteral,

    // Integers
    #[regex(r"-?(?:0|[1-9]\d*)")]
    IntegerLiteral,

    // String literals (for import "path" or attribute arguments).
    #[regex(r#""([^"\\]|\\[\s\S])*""#)]
    StringLiteral,

    // ============================================
    // 4. Identifiers
    // ============================================
    // Variable names, function names, and swizzle components.
    // For example, .xyz is tokenized as Dot followed by Identifier.
    #[regex(r"[a-zA-Z_][a-zA-Z0-9_]*")]
    Identifier,

    // ============================================
    // 5. Symbols & Operators
    // ============================================
    #[token(".")]
    Dot, // Start of member access or a swizzle.

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
    Arrow, // Function return type arrow.

    // Brackets
    #[token("(")]
    LParen,
    #[token(")")]
    RParen,
    #[token("[")]
    LBracket, // Tensor shape definition or indexing.
    #[token("]")]
    RBracket,
    #[token("{")]
    LBrace,
    #[token("}")]
    RBrace,

    // Arithmetic operators
    #[token("+")]
    Plus,
    #[token("-")]
    Minus,
    #[token("*")]
    Star,
    #[token("/")]
    Slash,

    // Comparison operators
    #[token("==")]
    Eq,
    #[token("!=")]
    Ne,
    #[token("<")]
    Lt,
    #[token(">")]
    Gt,

    // Comment handling (Logos ignores the matched text when the callback returns Skip).
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
