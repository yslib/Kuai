use crate::attr::registry::AttributeRegistry;
use crate::attr::traits::*;
use crate::context::Context;
use crate::pipeline::Pipeline;
use crate::session::Session;
use sema::resolver::Resolver;
use std::{io::pipe, sync::Arc};
use syntax::ast::*;
use syntax::parser::Parser;
use tofu_core::diagnostic::{Diagnostic, Report};
pub struct Compiler;

impl Compiler {
    pub fn compile(
        &self,
        sess: &Session,
        ctx: &mut Context,
        source: &str,
    ) -> Result<(), Vec<Diagnostic>> {
        let pipeline = Pipeline::new(sess.attr_registry);
        let ctx = &mut *ctx; // reborrow ctx to mutable reference
        //
        //

        // Stage 1: Parse
        let mut parser = Parser::new(source, &mut ctx.interner);
        let module = parser.parse()?;
        let mut stmts = module.stmts;

        // Stage 2: Raw Stage (Attribute Transform)

        stmts = self.apply_stage(ctx, stmts, CompileStage::Raw, &pipeline)?;

        // Stage 3. Semantic Analysis(Resovling Symbols)

        let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
        let module = Module { stmts, ..module };
        resolver.resolve(&module);
        if !resolver.diagnostics().is_empty() {
            return Err(resolver.diagnostics().clone());
        }

        // Stage 4. Resovled Stage

        let mut stmts = module.stmts;
        stmts = self.apply_stage(ctx, stmts, CompileStage::Resolved, &pipeline)?;

        // Stage 5. Code Generation (Lowering to IR or directly to executable code)

        let mut _res = self.apply_stage(ctx, stmts, CompileStage::Analyzed, &pipeline)?;
        return Ok(());
    }

    pub fn apply_stage(
        &self,
        ctx: &Context,
        stmt: Vec<Stmt>,
        stage: CompileStage,
        pipeline: &Pipeline,
    ) -> Result<Vec<Stmt>, Vec<Diagnostic>> {
        let mut result = Vec::new();
        for stmt in stmt {
            let action = pipeline.apply(
                ctx,
                stmt.clone(),
                stage,
                &AttrEnvironment {
                    parent: None,
                    local_attrs: &[],
                    delta_meta: AttrMetadata::new(),
                },
            )?;
            match action {
                AttrAction::Continue(s) => result.push(s),
                _ => {
                    // For simplicity, we only handle Continue action in this example
                    // In a real implementation, you would need to handle other actions as well
                    return Ok(result);
                }
            }
        }
        Ok(result)
    }
}
