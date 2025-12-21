use parser::parser::Parser;
use parser::pretty::print_ast;

fn parse_and_snapshot(source_code: &str, snapshot_name: &str) {
    let mut parser = Parser::new(source_code);
    let res = parser.parse();
    match res {
        Ok(ast) => {
            let pretty_output = print_ast(&ast);
            insta::assert_snapshot!(snapshot_name, pretty_output);
        }
        Err(errors) => {
            let error_messages: Vec<String> = errors.iter().map(|e| format!("{:?}", e)).collect();
            insta::assert_snapshot!(snapshot_name, error_messages.join("\n"));
        }
    }
}

// ================================
// 基础语法测试
// ================================

#[test]
fn test_import() {
    let code = r#"import std::math;"#;
    parse_and_snapshot(code, "import");
}

#[test]
fn test_simple_expression() {
    // let code = "1 + 2 * 3;";
    // parse_and_snapshot(code, "simple_expression");

    let code2 = "(a + b) * (c - d);";
    parse_and_snapshot(code2, "simple_expression_2");
}

#[test]
fn test_variable_assignment() {
    let code = "x = 42;";
    parse_and_snapshot(code, "variable_assignment");
}

#[test]
fn test_return_statement() {
    let code = "return x + y;";
    parse_and_snapshot(code, "return_statement");
}

// ================================
// 函数声明测试
// ================================

#[test]
fn test_empty_function() {
    let code = r#"
func empty() {}
"#;
    parse_and_snapshot(code, "empty_function");
}

#[test]
fn test_function_with_params() {
    let code = r#"
func add(a: i32, b: i32) -> i32 {
    return a + b;
}
"#;
    parse_and_snapshot(code, "function_with_params");
}

#[test]
fn test_func_decl_with_generic() {
    let code = r#"
    func generic_func<T>(input: T) -> T {
        return input;
    }"#;
    parse_and_snapshot(code, "func_decl_with_generic");
}

#[test]
fn test_generic_function_with_bound() {
    let code = r#"
func process<T: Numeric, U = f32>(x: T, y: U) -> T {
    return x;
}
"#;
    parse_and_snapshot(code, "generic_function_with_bound");
}

// ================================
// 结构体声明测试
// ================================

#[test]
fn test_simple_struct() {
    let code = r#"
struct Point {
    x: f32,
    y: f32,
}
"#;
    parse_and_snapshot(code, "simple_struct");
}

#[test]
fn test_struct_declaration() {
    let code = r#"
@swizzle_set(chars="xyzw", indices=[0, 1, 2, 3])
struct Point<M, N, K> {
    position: f32[4],
    color: f32[4],
}
"#;
    parse_and_snapshot(code, "struct_declaration");
}

#[test]
fn test_generic_struct() {
    let code = r#"
struct Container<T> {
    value: T,
    count: i32,
}
"#;
    parse_and_snapshot(code, "generic_struct");
}

// ================================
// 属性测试
// ================================

#[test]
fn test_attribute_simple() {
    let code = r#"@inline func fast(){}"#;
    parse_and_snapshot(code, "attribute_simple");
}

#[test]
fn test_attribute_with_args() {
    let code = r#"
@kernel(threads=256)
func compute() {}
"#;
    parse_and_snapshot(code, "attribute_with_args");
}

#[test]
fn test_multiple_attributes() {
    let code = r#"
@gpu
@kernel(threads=128)
@unroll(factor=4)
func heavy_compute(data: f32[N]) {}
"#;
    parse_and_snapshot(code, "multiple_attributes");
}

// ================================
// 类型表达式测试
// ================================

#[test]
fn test_primitive_types() {
    let code = r#"
func types(a: f32, b: i32, c: bool) {}
"#;
    parse_and_snapshot(code, "primitive_types");
}

#[test]
fn test_tensor_type() {
    let code = r#"
func tensor_func(input: f32[B, C, H, W]) -> f32[B, C, H, W] {
    return input;
}
"#;
    parse_and_snapshot(code, "tensor_type");
}

#[test]
fn test_generic_type() {
    let code = r#"
func use_generic(v: Vec<f32>, m: Map<String, i32>) {}
"#;
    parse_and_snapshot(code, "generic_type");
}

// ================================
// 表达式测试
// ================================

#[test]
fn test_member_access() {
    let code = "x = obj.field;";
    parse_and_snapshot(code, "member_access");
}

#[test]
fn test_swizzle_and_einsum() {
    let code = "a = t.nchw * b[i, j];";
    parse_and_snapshot(code, "swizzle_and_einsum");
}

#[test]
fn test_index_expression() {
    let code = "result = arr[0, 1, 2];";
    parse_and_snapshot(code, "index_expression");
}

#[test]
fn test_function_call() {
    let code = "result = foo(a, b, c);";
    parse_and_snapshot(code, "function_call");
}

#[test]
fn test_generic_call() {
    let code = "result = cast::<f32>(value);";
    parse_and_snapshot(code, "generic_call");
}

#[test]
fn test_named_arguments() {
    let code = "result = create(width=100, height=200);";
    parse_and_snapshot(code, "named_arguments");
}

#[test]
fn test_vector_literal() {
    let code = "v = [1 2 3 4];";
    parse_and_snapshot(code, "vector_literal");
}

#[test]
fn test_chained_access() {
    let code = "x = a.b.c[0].d;";
    parse_and_snapshot(code, "chained_access");
}

// ================================
// 复杂测试
// ================================

#[test]
fn test_attribute_and_func() {
    let source_code = r#"
import std::util::math;

@swizzle_set(chars="nchw", indices=[0, 1, 2, 3])
struct Tensor {
    data: f32,
}

@kernel(threads=256)
func forward(input: f32[B, C, H, W]) -> f32[B, C, H, W] {
    factor = [0.5, 0.5, 0.5];
    result = input.nchw * factor;
    return result;
}
"#;
    parse_and_snapshot(source_code, "complex_attribute_and_func_struct_decl");
}

#[test]
fn test_complex_module() {
    let code = r#"
import std::math;
import std::math;

@module_attribute(version="1.0")
struct Config<T> {
    value: T,
    enabled: bool,
}

@inline
func helper(x: f32) -> f32 {
    return x * 2.0;
}

@kernel(threads=256)
@optimize(level=3)
func main_kernel<T: Numeric>(input: T[N, M], config: Config<T>) -> T[N, M] {
    temp = helper(input[0, 0]);
    result = input.xy * temp;
    return result;
}
"#;
    parse_and_snapshot(code, "complex_module");
}

// ================================
// 边界情况测试
// ================================

#[test]
fn test_empty_module() {
    let code = "";
    parse_and_snapshot(code, "empty_module");
}

#[test]
fn test_nested_generics() {
    let code = r#"
func nested(data: Map<String, Vec<Option<f32>>>) {}
"#;
    parse_and_snapshot(code, "nested_generics");
}

#[test]
fn test_complex_expression() {
    let e1 = "result = (a + b) * (c - d) / e + A.b  * [1 2 3 4] + (e[a,b,c,e,d]);";
    parse_and_snapshot(e1, "complex_expression");
}

#[test]
fn test_complex_expression_with_call() {
    let e1 = "result = (a + b) * (c - d) / e + A.b  * [1 2 3 4] + (e[a,b,c,e,d]) + foo(a)(b)(c);";
    parse_and_snapshot(e1, "complex_expression_with_func");
}
