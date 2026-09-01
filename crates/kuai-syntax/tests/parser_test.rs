use kuai_core::Interner;
use syntax::parser::Parser;
use syntax::pretty::print_ast;

#[test]
fn test_parser() {
    insta::glob!("**/*.ku", |path| {
        eprintln!("Testing: {:?}", path);
        let input = std::fs::read_to_string(path).unwrap();
        let mut interner = Interner::new();
        let mut parser = Parser::new(&input, &mut interner);

        // Check if this is an invalid test case
        let is_invalid = path.to_str().unwrap().contains("/invalid/");

        let result = parser.parse();

        if is_invalid {
            // Invalid test case - snapshot the errors
            // let mut output = String::new();
            // for diag in &result.diagnostics {
            //     output.push_str(&format!(
            //         "[{:?}] {}:{} - {}\n",
            //         diag.severity, diag.span.start, diag.span.end, diag.message
            //     ));
            // }
            // insta::assert_snapshot!(output);
        } else {
            // Valid test case - snapshot the AST
            if let Some(module) = result.output {
                let pretty_output = print_ast(&module, &interner);
                insta::assert_snapshot!(pretty_output);
            }
        }
    });
}

#[test]
fn test_parser_unclosed_block_does_not_loop_forever() {
    let input = "func broken(a: i32) { let x = 1;";
    let mut interner = Interner::new();
    let mut parser = Parser::new(input, &mut interner);

    let result = parser.parse();

    assert!(
        result.diagnostics.len() < 100,
        "too many diagnostics ({}), possible infinite loop",
        result.diagnostics.len()
    );
    assert!(
        result
            .diagnostics
            .iter()
            .any(|d| d.message.contains("Expected '}' at end of block")),
        "missing expected unclosed block diagnostic"
    );
}
