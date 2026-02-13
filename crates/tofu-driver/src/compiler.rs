use crate::attr::registry::AttributeRegistry;
use crate::context::Context;
use sema::resolver::Resolver;
use std::sync::{Arc, Mutex};
use syntax::parser::Parser;
use tofu_core::diagnostic::Report;
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
        let mut ctx = self.ctx.lock().unwrap();
        let ctx = &mut *ctx; // reborrow ctx to mutable reference
        let mut parser = Parser::new(source, &mut ctx.interner);

        match parser.parse() {
            Ok(module) => {
                let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
                resolver.resolve(&module);
                if resolver.diagnostics().is_empty() {
                    println!("✨ Parse successful.");
                } else {
                    for diag in resolver.diagnostics() {
                        let adapter =
                            diag.render_as_miette("stdin".to_string(), source.to_string());
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
