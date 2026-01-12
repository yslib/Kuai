use crate::ast::Id;
use crate::symbol::Scope;
use lasso::Rodeo;
use std::sync::{Arc, RwLock};

pub struct Context {
    pub interner: Rodeo,
    pub global_scope: Arc<RwLock<Scope>>,
}

impl Context {
    pub fn new() -> Self {
        Context {
            interner: Rodeo::new(),
            global_scope: Arc::new(RwLock::new(Scope::new(
                None,
                crate::symbol::ScopeKind::Global,
            ))),
        }
    }

    pub fn lookup(&self, id: Id) -> &str {
        self.interner.resolve(&id)
    }
}

impl Default for Context {
    fn default() -> Self {
        Self::new()
    }
}
