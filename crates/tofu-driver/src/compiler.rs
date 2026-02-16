use crate::attr::traits::*;
use crate::context::Context;
use crate::pipeline::Pipeline;
use crate::session::Session;
use sema::resolver::Resolver;
use std::sync::Arc;
use syntax::ast::*;
use syntax::parser::Parser;
use tofu_core::diagnostic::Diagnostic;

#[derive(Clone, Copy)]
pub struct Compiler;

impl Compiler {
    pub fn new() -> Self {
        Compiler
    }

    pub fn compile(
        &self,
        sess: &Session,
        ctx: &mut Context,
        source: &str,
    ) -> Result<Module, Vec<Diagnostic>> {
        let pipeline = Pipeline::new(&sess.attr_registry);

        // Stage 1: Parse
        let mut parser = Parser::new(source, &mut ctx.interner);
        let module = parser.parse()?;
        let mut stmts = module.stmts;

        // Stage 2: Raw Stage (Attribute Transform)
        stmts = self.apply_stage(ctx, stmts, CompileStage::Raw, &pipeline)?;

        // Stage 3: Semantic Analysis (Resolving Symbols)
        let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
        let module = Module {
            stmts: stmts.clone(),
            ..module
        };
        resolver.resolve(&module);
        if !resolver.diagnostics().is_empty() {
            return Err(resolver.diagnostics().clone());
        }

        // Stage 4: Resolved Stage
        stmts = self.apply_stage(ctx, stmts, CompileStage::Resolved, &pipeline)?;

        // Stage 5: Analyzed Stage (Code Generation / Lowering to IR)
        stmts = self.apply_stage(ctx, stmts, CompileStage::Analyzed, &pipeline)?;

        Ok(Module {
            stmts,
            top_level_attributes: module.top_level_attributes,
            span: module.span,
        })
    }

    fn apply_stage(
        &self,
        ctx: &Context,
        stmts: Vec<Stmt>,
        stage: CompileStage,
        pipeline: &Pipeline,
    ) -> Result<Vec<Stmt>, Vec<Diagnostic>> {
        let mut result = Vec::new();
        let mut diagnostics = Vec::new();

        for stmt in stmts {
            let action = pipeline.apply(
                ctx,
                stmt.clone(),
                stage,
                &AttrEnvironment {
                    parent: None,
                    local_attrs: &[],
                    delta_meta: AttrMetadata::new(),
                },
            );

            match action {
                Ok(AttrAction::Continue(s)) => result.push(s),
                Ok(AttrAction::SkipChildren(s)) => result.push(s),
                Ok(AttrAction::Terminal(s)) => {
                    result.push(s);
                    break;
                }
                Ok(AttrAction::Lowered(_artifact)) => {
                    // Store artifact for later use
                    // For now, we just skip it
                    continue;
                }
                Err(diags) => {
                    diagnostics.extend(diags);
                }
            }
        }

        if !diagnostics.is_empty() {
            return Err(diagnostics);
        }

        Ok(result)
    }
}

impl Default for Compiler {
    fn default() -> Self {
        Self::new()
    }
}
