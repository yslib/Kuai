use tofu::parser::Parser;

#[test]
fn test_attribute_and_func() {
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

    let mut parser = Parser::new(source_code);

    let _stmts = parser.parse();
}

#[test]
fn test_swizzle_and_einsum() {
    // 测试 t.nchw 和 t[i, j]
    let code = "a = t.nchw * b[i, j]";
    let mut parser = Parser::new(code);
    let stmts = parser.parse();

    // 这是一个复杂的表达式，我们主要验证解析是否成功不 Panic
    // 并且验证最外层结构
    assert_eq!(stmts.len(), 1);
}
