use syntax::parser::Parser;
use syntax::pretty::print_ast;
use syntax::ast::Module;
use core::Interner;

#[test]
fn test_parser() {
    insta::glob!("fixtures/*.tof", |path| {
        let input = std::fs::read_to_string(path).unwrap();
        let mut interner = Interner::new();
        let mut parser = Parser::new(&input, &mut interner);
        if let Ok(stmts) = parser.parse_impl() {
            let ast = Module {
                attributes: vec![],
                stmts,
                span: 0..input.len(),
            };
            let pretty_output = print_ast(&ast, &interner);
            insta::assert_snapshot!(pretty_output);
        }
    });
}
