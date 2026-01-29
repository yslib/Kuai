use crate::attr::registry::AttributeRegistry;
use crate::context::Context;
use core::diagnostic::Report;
use sema::resolver::Resolver;
use std::sync::{Arc, Mutex};
use syntax::parser::Parser;
pub struct Compiler {
    pub ctx: Arc<Mutex<Context>>,
    pub registry: Arc<AttributeRegistry>,
}

impl Compiler {
    pub fn new() -> Compiler {
        Compiler {
            ctx: Arc::new(Mutex::new(Context::new())),
            registry: Arc::new(AttributeRegistry::new()),
        }
    }

    fn compile_and_run(&self, source: &str) {
        let mut parser = Parser::new(source, &self.ctx.interner);

        match parser.parse() {
            Ok(module) => {
                let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
                resolver.resolve(&module);
                if resolver.diagnostics().is_empty() {
                    println!("✨ Parse successful.");
                } else {
                    for diag in resolver.diagnostics() {
                        let adapter = diag.render_as_miette("stdin".to_string(), source.to_string());
                        println!("{:?}", Report::new(adapter));
                    }
                }
            }
            Err(diagnostics) => {
                for diag in diagnostics {
                    let adapter = diag.render_as_miette("stdin".to_string(), source.to_string());
                    println!("{:?}", Report::new(adapter));
                }
            }
        }
    }
}
