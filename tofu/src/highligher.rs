use logos::Logos;
use syntax::lexer::Token;
use rustyline::highlight::{CmdKind, Highlighter};
use rustyline_derive::{Completer, Helper, Hinter, Validator};
use std::borrow::Cow::{self, Owned};

#[derive(Helper, Completer, Hinter, Validator)]
pub struct TofuHelper;

/// Color Code:
/// \x1b[1;34m    粗体亮蓝 (适合 func, dim)
/// \x1b[32m    绿色 (适合类型或字符串)
/// \x1b[33m    黄色 (适合数字字面量)
/// \x1b[35m    品红 (适合 @kernel)
/// \x1b[36m    青色 (适合内置函数)
/// \x1b[90m    灰色 (适合注释)
/// \x1b[0m    重置 (必须加在末尾)

impl Highlighter for TofuHelper {
    fn highlight<'l>(&self, line: &'l str, _pos: usize) -> Cow<'l, str> {
        let mut highlighted = String::new();
        let mut le = Token::lexer(line);
        let mut last_end = 0;

        while let Some(token_res) = le.next() {
            let span = le.span();
            // 填充 Token 之间的空白（空格、注释等如果 Lexer 没跳过的话）
            highlighted.push_str(&line[last_end..span.start]);

            let slice = le.slice();

            // 根据 Token 类型着色
            let color = match token_res {
                Ok(Token::KwDim) | Ok(Token::KwFunc) | Ok(Token::KwLet) | Ok(Token::KwStruct)
                | Ok(Token::KwReturn) => "\x1b[1;34m",
                Ok(Token::TypeF32) | Ok(Token::TypeI32) | Ok(Token::TypeBool) => "\x1b[32m",
                Ok(Token::Attribute) => "\x1b[35m",
                Ok(Token::IntegerLiteral) | Ok(Token::FloatLiteral) => "\x1b[33m",
                Ok(Token::StringLiteral) => "\x1b[32m",
                Ok(Token::Identifier) => "\x1b[37m",
                _ => "\x1b[0m",
            };

            highlighted.push_str(color);
            highlighted.push_str(slice);
            highlighted.push_str("\x1b[0m"); // 重置颜色
            last_end = span.end;
        }

        // 填充剩余部分
        highlighted.push_str(&line[last_end..]);
        Owned(highlighted)
    }

    fn highlight_char(&self, _line: &str, _pos: usize, _kind: CmdKind) -> bool {
        true
    }
}
