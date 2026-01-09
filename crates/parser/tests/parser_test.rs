use parser::parser::Parser;
use parser::pretty::print_ast;

#[test]
fn test_parser_fixture() {
    insta::glob!("fixtures/*.tof", |path| {
        let source_code = std::fs::read_to_string(path).unwrap();
        let mut ctx = parser::context::Context::new();
        let mut parser = Parser::new(&source_code, &mut ctx);
        let res = parser.parse();
        match res {
            Ok(ast) => {
                let pretty_output = print_ast(&ast);
                insta::assert_snapshot!(pretty_output);
            }
            Err(errors) => {
                let error_messages: Vec<String> = errors.iter().map(|e| format!("{}", e)).collect();
                insta::assert_debug_snapshot!(error_messages);
            }
        }
    });
}
