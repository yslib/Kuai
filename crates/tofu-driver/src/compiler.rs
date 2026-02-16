use crate::attr::traits::*;
use crate::context::Context;
use crate::pipeline::Pipeline;
use crate::session::Session;
use sema::resolver::Resolver;
use std::sync::Arc;
use syntax::ast::*;
use syntax::parser::Parser;
use tofu_core::diagnostic::{CompileResult, Diagnostic};

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
    ) -> CompileResult<Module> {
        let pipeline = Pipeline::new(&sess.attr_registry);
        let mut diagnostics = Vec::new();

        // Stage 1: Parse
        let mut parser = Parser::new(source, &mut ctx.interner);
        let module = match parser.parse() {
            Ok(m) => m,
            Err(parse_errors) => {
                // Parser failed, return partial result with errors
                return CompileResult::err(parse_errors);
            }
        };
        let mut stmts = module.stmts;

        // Stage 2: Raw Stage (Attribute Transform)
        stmts = match self.apply_stage(ctx, stmts, CompileStage::Raw, &pipeline) {
            Ok(s) => s,
            Err(errs) => {
                diagnostics.extend(errs);
                // Return early with what we have
                return CompileResult::with_diagnostics(
                    Module {
                        stmts: Vec::new(),
                        top_level_attributes: module.top_level_attributes,
                        span: module.span,
                    },
                    diagnostics,
                );
            }
        };

        // Stage 3: Semantic Analysis (Resolving Symbols)
        let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
        let temp_module = Module {
            stmts: stmts.clone(),
            top_level_attributes: module.top_level_attributes.clone(),
            span: module.span.clone(),
        };
        resolver.resolve(&temp_module);
        diagnostics.extend(resolver.diagnostics().clone());

        // Stage 4: Resolved Stage
        stmts = match self.apply_stage(ctx, stmts, CompileStage::Resolved, &pipeline) {
            Ok(s) => s,
            Err(errs) => {
                diagnostics.extend(errs);
                // Return early with partial result
                return CompileResult::with_diagnostics(
                    Module {
                        stmts: Vec::new(),
                        top_level_attributes: module.top_level_attributes,
                        span: module.span,
                    },
                    diagnostics,
                );
            }
        };

        // Stage 5: Analyzed Stage (Code Generation / Lowering to IR)
        stmts = match self.apply_stage(ctx, stmts, CompileStage::Analyzed, &pipeline) {
            Ok(s) => s,
            Err(errs) => {
                diagnostics.extend(errs);
                // Return early with partial result
                return CompileResult::with_diagnostics(
                    Module {
                        stmts: Vec::new(),
                        top_level_attributes: module.top_level_attributes,
                        span: module.span,
                    },
                    diagnostics,
                );
            }
        };

        let final_module = Module {
            stmts,
            top_level_attributes: module.top_level_attributes,
            span: module.span,
        };

        CompileResult::with_diagnostics(final_module, diagnostics)
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
                    // Continue processing other statements
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
