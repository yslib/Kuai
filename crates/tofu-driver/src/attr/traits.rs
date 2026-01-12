use crate::context::Context;
use core::diagnostic::Diagnostic;
use std::any::Any;
use std::collections::HashMap;
use syntax::ast::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd)]
pub enum CompileStage {
    Raw,
    Resolved,
    Analyzed,
    Lowered,
}

pub struct AttrMetadata {
    properties: HashMap<String, Box<dyn Any + Send + Sync>>,
}

impl AttrMetadata {
    pub fn new() -> Self {
        AttrMetadata {
            properties: HashMap::new(),
        }
    }

    pub fn set<T: Send + Sync + 'static>(&mut self, key: &str, value: T) {
        self.properties.insert(key.to_string(), Box::new(value));
    }

    pub fn get<T: Send + Sync + 'static>(&self, key: &str) -> Option<&T> {
        self.properties.get(key).and_then(|b| b.downcast_ref::<T>())
    }
}

pub trait AttrHandler: Send + Sync {
    fn name(&self) -> &str;
    fn stage(&self) -> CompileStage;

    fn transform(
        &self,
        ctx: &Context,
        attr: &Attribute,
        node: &Stmt,
    ) -> Result<Stmt, Vec<Diagnostic>>;
}
