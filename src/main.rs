// src/main.rs
mod ast;
mod lexer;
mod parser;
use lexer::Token;
use logos::Logos;

fn main() {
    // 模拟一段符合你描述的 DSL 代码
    let source_code = r#"
        import math

        // 1. 属性扩展：定义 Swizzle 字符集
        @swizzle_set(chars="nchw", indices=[0, 1, 2, 3])
        struct Tensor {
            data: f32
        }

        // 2. 内核定义
        @kernel(threads=256)
        func forward(input: f32[B, C, H, W]) -> f32[B, C, H, W] {
            // 3. Julia 风格向量 (Parser 将利用上下文处理 [0.5 0.5])
            let factor = [0.5 0.5 0.5]

            // 4. Swizzle 访问 (词法上是 Ident(input) Dot Ident(nchw))
            // 5. Einsum 风格 (词法上是 LBracket Ident(b) Comma Ident(c) ...)
            let result = input.nchw * factor
            return result
        }
    "#;

    println!("Scanning source code:\n---");

    // 创建 Lexer
    // let mut lexer = Token::lexer(source_code);
    //
    // while let Some(result) = lexer.next() {
    //     match result {
    //         Ok(token) => {
    //             let text = lexer.slice();
    //             // 打印 Token 类型和原始文本
    //             println!("{:?} -> '{}'", token, text);
    //         }
    //         Err(_) => {
    //             let range = lexer.span();
    //             eprintln!("Error: Unexpected token at {:?}: '{}'", range, lexer.slice());
    //         }
    //     }
    // }

    let mut parser = parser::Parser::new(source_code);

    let _stmts = parser.parse();
}
