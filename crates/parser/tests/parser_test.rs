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

#[test]
fn test_parser_fixture() {
    insta::glob!("fixtures/*.tof", |path| {
        let source_code = std::fs::read_to_string(path).unwrap();
        let mut parser = Parser::new(&source_code);
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
