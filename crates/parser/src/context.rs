use crate::ast::Id;
use lasso::Rodeo;

pub struct Context {
    pub interner: Rodeo,
}

impl Context {
    pub fn new() -> Self {
        Context {
            interner: Rodeo::new(),
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
