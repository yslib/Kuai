use crate::attr::traits::Artifact;
use kuai_core::Interner;
use kuai_core::spur::Id;
use sema::symbol::*;

use std::sync::{Arc, RwLock};

pub struct Context {
    pub interner: kuai_core::Interner,
    pub global_scope: Arc<RwLock<Scope>>,
    pub artifacts: Vec<Artifact>,
}

impl Context {
    pub fn new() -> Self {
        Context {
            interner: Interner::new(),
            global_scope: Arc::new(RwLock::new(Scope::new(None, ScopeKind::Global))),
            artifacts: Vec::new(),
        }
    }

    pub fn lookup(&self, id: Id) -> &str {
        self.interner.resolve(&id)
    }

    pub fn inject_builtins(&mut self) {
        let builtins = vec!["print", "exit", "help"];
        for name in builtins {
            let id = self.interner.get_or_intern(name);
            let mut scope = self.global_scope.write().unwrap();
            let _ = scope.define(Symbol {
                id,
                kind: SymbolKind::BuiltinFunction,
                ty: None,
                span: 0..0,
            });
        }
    }
}

impl Default for Context {
    fn default() -> Self {
        Self::new()
    }
}
