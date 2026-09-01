use crate::symbol::ScopeKind;
use crate::symbol::{Scope, Symbol, SymbolKind};
use kuai_core::Interner;
use kuai_core::diagnostic::*;
use std::sync::{Arc, RwLock};
use syntax::ast::*;

pub struct Resolver<'ctx> {
    interner: &'ctx Interner,
    scope_stack: Arc<RwLock<Scope>>,
    diagnostics: Vec<Diagnostic>,
}

impl<'ctx> Resolver<'ctx> {
    pub fn new(ctx: &'ctx Interner, parent_scope: Arc<RwLock<Scope>>) -> Self {
        Resolver {
            interner: ctx,
            scope_stack: parent_scope,
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
        ctx: &'ctx Interner,
        diagnostic: &mut Vec<Diagnostic>,
        kind: &SymbolKind,
        old: &Symbol,
    ) {
        let msg = format!(
            "{:?} symbol '{}' is already defined at {:?}",
            kind,
            ctx.resolve(&old.id),
            old.span
        );
        let diag = Diagnostic::error(Severity::Error, old.span.clone(), msg);
        diagnostic.push(diag);
    }

    fn report_undefine(&mut self, name: &str, span: Span) {
        let msg = format!("Variable '{}' is not defined", name);
        let diag = Diagnostic::error(Severity::Error, span, msg);
        self.diagnostics.push(diag);
    }

    fn push_scope(&mut self, kind: ScopeKind) {
        let parent = Arc::clone(&self.scope_stack);
        self.scope_stack = Arc::new(RwLock::new(Scope::new(Some(parent), kind)));
    }

    fn pop_scope(&mut self) {
        let parent = self
            .scope_stack
            .read()
            .unwrap()
            .parent
            .as_ref()
            .unwrap()
            .clone();
        self.scope_stack = parent;
    }

    fn resolve_stmt(&mut self, stmt: &Stmt) {
        match &stmt.stmt {
            StmtImpl::DimDecl(dim_decls) => {
                for dim_decl in dim_decls {
                    // Resolve bound and value expressions first
                    if let Some(bound) = &dim_decl.bound {
                        self.resolve_expr(bound);
                    }
                    if let Some(value) = &dim_decl.value {
                        self.resolve_expr(value);
                    }
                    // Then define the dimension
                    self.define_dim(dim_decl);
                }
            }
            StmtImpl::VarDecl(var_decl) => {
                // Resolve type expression first
                if let Some(ty) = &var_decl.ty {
                    self.resolve_type_expr(ty);
                }
                // Resolve initializer (before defining the variable)
                if let Some(init) = &var_decl.init {
                    self.resolve_expr(init);
                }
                // Define the variable
                self.define_variable(var_decl);
            }
            StmtImpl::Function(func_decl) => self.resolve_func(func_decl),
            StmtImpl::Struct(struct_decl) => self.resolve_struct(struct_decl),
            StmtImpl::Assignment { target, value, .. } => {
                self.resolve_expr(target);
                self.resolve_expr(value);
            }
            StmtImpl::For {
                var, range, body, ..
            } => {
                self.push_scope(ScopeKind::Block);
                // Define loop variable
                let symbol = Symbol {
                    id: var.id,
                    kind: SymbolKind::Variable,
                    ty: None,
                    span: var.span.clone(),
                };
                let _ = self.scope_stack.write().unwrap().define(symbol);

                // Resolve range
                self.resolve_expr(&range.start);
                self.resolve_expr(&range.end);
                if let Some(step) = &range.step {
                    self.resolve_expr(step);
                }

                // Resolve body
                self.resolve_block(body);
                self.pop_scope();
            }
            StmtImpl::If {
                condtion,
                then_branch,
                else_branch,
            } => {
                self.resolve_expr(condtion);
                self.push_scope(ScopeKind::Block);
                self.resolve_block(then_branch);
                self.pop_scope();

                if let Some(else_br) = else_branch {
                    match else_br {
                        ElseBranch::Block(block) => {
                            self.push_scope(ScopeKind::Block);
                            self.resolve_block(block);
                            self.pop_scope();
                        }
                        ElseBranch::If(if_stmt) => {
                            self.resolve_stmt(if_stmt);
                        }
                    }
                }
            }
            StmtImpl::Return(expr) => {
                self.resolve_expr(expr);
            }
            StmtImpl::Expr(expr) => {
                self.resolve_expr(expr);
            }
            StmtImpl::Break(_) | StmtImpl::Continue(_) => {
                // Nothing to resolve
            }
            StmtImpl::Import(_path) => {
                // TODO: implement import resolution
            }
            StmtImpl::Error { .. } => {
                // Skip error nodes
            }
        }
    }

    fn resolve_block(&mut self, block: &Block) {
        for stmt in &block.stmts {
            self.resolve_stmt(stmt);
        }
    }

    fn resolve_func(&mut self, func: &FuncDecl) {
        // Define function in current scope
        let symbol = Symbol {
            id: func.proto.name.id,
            kind: SymbolKind::Function,
            ty: None,
            span: func.proto.name.span.clone(),
        };
        if let Err(old) = self.scope_stack.write().unwrap().define(symbol) {
            Self::report_redefine(
                self.interner,
                &mut self.diagnostics,
                &SymbolKind::Function,
                &old,
            );
        }

        // Only resolve body if it exists
        if let Some(body) = &func.body {
            // Create function scope
            self.push_scope(ScopeKind::Function);

            // Define parameters
            for p in &func.proto.params {
                self.define_param(p);
                // Resolve parameter type
                self.resolve_type_expr(&p.ty);
            }

            // Resolve return type
            if let Some(ret_ty) = &func.proto.return_type {
                self.resolve_type_expr(ret_ty);
            }

            // Resolve function body
            self.resolve_block(body);

            self.pop_scope();
        }
    }

    fn resolve_struct(&mut self, struct_decl: &StructDecl) {
        // Define struct in current scope
        let symbol = Symbol {
            id: struct_decl.name.id,
            kind: SymbolKind::Struct,
            ty: None,
            span: struct_decl.name.span.clone(),
        };
        if let Err(old) = self.scope_stack.write().unwrap().define(symbol) {
            Self::report_redefine(
                self.interner,
                &mut self.diagnostics,
                &SymbolKind::Struct,
                &old,
            );
        }

        // Resolve field types
        for field in &struct_decl.fields {
            self.resolve_type_expr(&field.ty);
        }
    }

    fn resolve_type_expr(&mut self, ty: &TypeExpr) {
        match ty {
            TypeExpr::Named { name, generics } => {
                // Check if the type is defined
                if self.scope_stack.read().unwrap().resolve(name.id).is_none() {
                    let type_name = self.interner.resolve(&name.id);
                    // Only report error if it's not a primitive type
                    if !self.is_primitive_type(type_name) {
                        self.report_undefine(type_name, name.span.clone());
                    }
                }
                // Resolve generic type arguments
                for generic in generics {
                    self.resolve_type_expr(generic);
                }
            }
            TypeExpr::Tensor { base, shape } => {
                self.resolve_type_expr(base);
                for dim in shape {
                    self.resolve_expr(dim);
                }
            }
            TypeExpr::Primitive(_) => {
                // Primitive types are always valid
            }
            TypeExpr::Error { .. } => {
                // Skip error nodes
            }
        }
    }

    fn is_primitive_type(&self, name: &str) -> bool {
        matches!(
            name,
            "i8" | "i16" | "i32" | "i64" | "f32" | "f64" | "bool" | "char"
        )
    }

    fn resolve_expr(&mut self, e: &Expr) {
        match e {
            Expr::Literal(_lit) => {}
            Expr::Variable(ident) => {
                if self.scope_stack.read().unwrap().resolve(ident.id).is_none() {
                    let name = self.interner.resolve(&ident.id);
                    self.report_undefine(name, ident.span.clone());
                }
            }
            Expr::Binary { left, right, .. } => {
                self.resolve_expr(left);
                self.resolve_expr(right);
            }
            Expr::MemberAccess { target, .. } => {
                self.resolve_expr(target);
                // Member name doesn't need to be resolved here
            }
            Expr::NamespaceAccess { namespace, .. } => {
                self.resolve_expr(namespace);
            }
            Expr::Call {
                func,
                generics,
                args,
            } => {
                self.resolve_expr(func);
                // Resolve generic type arguments
                for generic in generics {
                    self.resolve_type_expr(generic);
                }
                // Resolve arguments
                for arg in args {
                    match arg {
                        Argument::Positional(expr) => self.resolve_expr(expr),
                        Argument::Named { value, .. } => self.resolve_expr(value),
                        Argument::Attribute(_) => {}
                    }
                }
            }
            Expr::Index { target, indices } => {
                self.resolve_expr(target);
                for index in indices {
                    self.resolve_expr(index);
                }
            }
            Expr::VectorLiteral(elements) => {
                for elem in elements {
                    self.resolve_expr(elem);
                }
            }
            Expr::Error(_) => {
                // Skip error nodes
            }
        }
    }

    fn define_dim(&mut self, decl: &DimDecl) {
        let symbol = Symbol {
            id: decl.name.id,
            kind: SymbolKind::Dimension,
            ty: None,
            span: decl.name.span.clone(),
        };
        if let Err(old) = self.scope_stack.write().unwrap().define(symbol) {
            Self::report_redefine(
                self.interner,
                &mut self.diagnostics,
                &SymbolKind::Dimension,
                &old,
            );
        }
    }

    fn define_variable(&mut self, decl: &VarDecl) {
        let symbol = Symbol {
            id: decl.name.id,
            kind: SymbolKind::Variable,
            ty: decl.ty.clone(),
            span: decl.name.span.clone(),
        };
        if let Err(old) = self.scope_stack.write().unwrap().define(symbol) {
            Self::report_redefine(
                self.interner,
                &mut self.diagnostics,
                &SymbolKind::Variable,
                &old,
            );
        }
    }

    fn define_param(&mut self, p: &Param) {
        let sym = Symbol {
            id: p.name.id,
            kind: SymbolKind::Param,
            ty: Some(p.ty.clone()),
            span: p.span.clone(),
        };
        if let Err(old) = self.scope_stack.write().unwrap().define(sym) {
            Self::report_redefine(
                self.interner,
                &mut self.diagnostics,
                &SymbolKind::Param,
                &old,
            );
        }
    }
}
