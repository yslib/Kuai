use crate::context::Context;
use crate::symbol::ScopeKind;
use crate::{
    ast::*,
    diagnostic::*,
    symbol::{Scope, Symbol, SymbolKind},
};
use std::cell::RefCell;
use std::rc::Rc;

pub struct Resolver<'ctx> {
    ctx: &'ctx Context,
    scope_stack: Rc<RefCell<Scope>>,
    diagnostics: Vec<Diagnostic>,
}

impl<'ctx> Resolver<'ctx> {
    pub fn new(ctx: &'ctx Context) -> Self {
        let global_scope = Scope::new(None, crate::symbol::ScopeKind::Global);
        Resolver {
            ctx,
            scope_stack: Rc::new(RefCell::new(global_scope)),
            diagnostics: Vec::new(),
        }
    }

    pub fn diagnostics(&self) -> &Vec<Diagnostic> {
        &self.diagnostics
    }

    pub fn resolve(&mut self, module: &Module) {
        for stmt in &module.stmts {
            self.resolve_stmt(stmt);
        }
    }

    fn report_redefine(
        ctx: &'ctx Context,
        diagnostic: &mut Vec<Diagnostic>,
        kind: &SymbolKind,
        old: &Symbol,
    ) {
        let msg = format!(
            "{:?} symbol '{}' is already defined at {:?}",
            kind,
            ctx.lookup(old.id),
            old.span
        );
        let diag = Diagnostic::error(Severity::Error, old.span.clone(), msg);
        diagnostic.push(diag);
    }

    // fn report_undefine(ctx: &'ctx Context, diagnostic: &mut Vec<Diagnostic>, id: &Id) {
    //     let name = ctx.lookup(*id);
    //     let msg = format!(
    //         "{:?} symbol '{}' is not defined yet {:?}",
    //         kind,
    //         ctx.lookup(old.id),
    //         old.span
    //     );
    //     let diag = Diagnostic::error(Severity::Error, old.span.clone(), msg);
    //     diagnostic.push(diag);
    // }

    fn report_undefine(&mut self, msg: &str) {
        let diag = Diagnostic::error(Severity::Error, 0..0, msg.to_string());
        self.diagnostics.push(diag);
    }

    fn push_scope(&mut self) {
        let parent = Rc::clone(&self.scope_stack);
        self.scope_stack = Rc::new(RefCell::new(Scope::new(Some(parent), ScopeKind::Global)));
    }

    fn pop_scope(&mut self) {
        let parent = self.scope_stack.borrow().parent.clone().unwrap();
        self.scope_stack = parent;
    }

    fn resolve_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::DimDecl(dim_decls) => {
                for dim_decl in dim_decls {
                    self.define_dim(dim_decl);
                }
            }
            Stmt::Function(func_decl) => self.resolve_func(func_decl),
            Stmt::Expr(_expr) => {}
            _ => {}
        }
    }

    fn on_resolve_func(&mut self, func: &FuncDecl) {
        for p in &func.proto.params {
            self.define_param(p);
        }
    }

    fn resolve_func(&mut self, func: &FuncDecl) {
        self.push_scope();
        self.on_resolve_func(func);
        self.pop_scope();
    }

    fn resolve_expr(&mut self, e: &Expr) {
        match e {
            Expr::Literal(lit) => {}
            Expr::Variable(ident) => {
                if let Some(sym) = self.scope_stack.borrow().resolve(ident.id) {
                } else {
                    let name = self.ctx.lookup(ident.id);
                    self.report_undefine(&format!("Variable '{}' is not defined", name));
                }
            }
            Expr::Binary { left, right, .. } => {
                self.resolve_expr(left);
                self.resolve_expr(right);
            }
            _ => {}
        }
    }

    fn define_dim(&mut self, decl: &DimDecl) {
        let symbol = Symbol {
            id: decl.name.id,
            kind: SymbolKind::Dimension,
            ty: None,
            span: decl.name.span.clone(),
        };
        if let Err(old) = self.scope_stack.borrow_mut().define(symbol) {
            Self::report_redefine(
                self.ctx,
                &mut self.diagnostics,
                &SymbolKind::Dimension,
                &old,
            );
        }
    }

    fn define_param(&mut self, p: &Param) {
        let sym = Symbol {
            id: p.name.id,
            kind: SymbolKind::Param,
            ty: None,
            span: p.span.clone(),
        };
        if let Err(old) = self.scope_stack.borrow_mut().define(sym) {
            Self::report_redefine(
                self.ctx,
                &mut self.diagnostics,
                &SymbolKind::Dimension,
                &old,
            );
        }
    }
}
