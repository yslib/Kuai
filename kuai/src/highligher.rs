use logos::Logos;
use rustyline::highlight::{CmdKind, Highlighter};
use rustyline_derive::{Completer, Helper, Hinter, Validator};
use std::borrow::Cow::{self, Owned};
use syntax::lexer::Token;

#[derive(Helper, Completer, Hinter, Validator)]
pub struct KuaiHelper;

/// Color Code:
/// \x1b[1;34m    Bold bright blue (for func and dim)
/// \x1b[32m    Green (for types or strings)
/// \x1b[33m    Yellow (for numeric literals)
/// \x1b[35m    Magenta (for @kernel)
/// \x1b[36m    Cyan (for built-in functions)
/// \x1b[90m    Gray (for comments)
/// \x1b[0m    Reset (must be appended at the end)

impl Highlighter for KuaiHelper {
    fn highlight<'l>(&self, line: &'l str, _pos: usize) -> Cow<'l, str> {
        let mut highlighted = String::new();
        let mut le = Token::lexer(line);
        let mut last_end = 0;

        while let Some(token_res) = le.next() {
            let span = le.span();
            // Preserve text between tokens, such as whitespace and comments.
            highlighted.push_str(&line[last_end..span.start]);

            let slice = le.slice();

            // Choose a color based on the token type.
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
            highlighted.push_str("\x1b[0m"); // Reset the color.
            last_end = span.end;
        }

        // Append the remaining text.
        highlighted.push_str(&line[last_end..]);
        Owned(highlighted)
    }

    fn highlight_char(&self, _line: &str, _pos: usize, _kind: CmdKind) -> bool {
        true
    }
}
