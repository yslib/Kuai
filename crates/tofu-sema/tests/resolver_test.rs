use sema::resolver::Resolver;
use sema::symbol::{Scope, ScopeKind};
use std::sync::{Arc, RwLock};
use syntax::parser::Parser;
use tofu_core::Interner;

#[test]
fn test_resolver() {
    insta::glob!("cases/**/*.tof", |path| {
        let input = std::fs::read_to_string(path).unwrap();
        let mut interner = Interner::new();

        // Parse the input
        let mut parser = Parser::new(&input, &mut interner);
        let parse_result = parser.parse();

        // Check if this is an invalid test case
        let is_invalid = path.to_str().unwrap().contains("/invalid/");

        if let Some(module) = parse_result.output {
            // Create global scope
            let global_scope = Arc::new(RwLock::new(Scope::new(None, ScopeKind::Global)));

            // Run resolver
            let mut resolver = Resolver::new(&interner, global_scope);
            resolver.resolve(&module);

            let diagnostics = resolver.diagnostics();

            if is_invalid {
                // Invalid test case - should have errors
                let mut output = String::new();
                for diag in diagnostics {
                    output.push_str(&format!("[{:?}] {}\n", diag.severity, diag.message));
                }
                insta::assert_snapshot!(output);
            } else {
                // Valid test case - should have no errors
                if !diagnostics.is_empty() {
                    let mut output = String::new();
                    for diag in diagnostics {
                        output.push_str(&format!("[{:?}] {}\n", diag.severity, diag.message));
                    }
                    insta::assert_snapshot!(output);
                } else {
                    insta::assert_snapshot!("No errors");
                }
            }
        } else {
            // Parse failed
            let mut output = String::new();
            for diag in &parse_result.diagnostics {
                output.push_str(&format!(
                    "[{:?}] {}:{} - {}\n",
                    diag.severity, diag.span.start, diag.span.end, diag.message
                ));
            }
            insta::assert_snapshot!(output);
        }
    });
}
