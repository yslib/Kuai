use tofu_core::Interner;
use syntax::parser::Parser;
use syntax::pretty::print_ast;

#[test]
fn test_parser() {
    insta::glob!("**/*.tof", |path| {
        let input = std::fs::read_to_string(path).unwrap();
        let mut interner = Interner::new();
        let mut parser = Parser::new(&input, &mut interner);

        // Check if this is an invalid test case
        let is_invalid = path.to_str().unwrap().contains("/invalid/");

        let result = parser.parse();

        if is_invalid {
            // Invalid test case - snapshot the errors
            let mut output = String::new();
            for diag in &result.diagnostics {
                output.push_str(&format!(
                    "[{:?}] {}:{} - {}\n",
                    diag.severity,
                    diag.span.start,
                    diag.span.end,
                    diag.message
                ));
            }
            insta::assert_snapshot!(output);
        } else {
            // Valid test case - snapshot the AST
            if let Some(module) = result.output {
                let pretty_output = print_ast(&module, &interner);
                insta::assert_snapshot!(pretty_output);
            } else {
                // Valid test failed - output errors
                let mut output = String::new();
                for diag in &result.diagnostics {
                    output.push_str(&format!(
                        "[{:?}] {}:{} - {}\n",
                        diag.severity,
                        diag.span.start,
                        diag.span.end,
                        diag.message
                    ));
                }
                insta::assert_snapshot!(output);
            }
        }
    });
}
