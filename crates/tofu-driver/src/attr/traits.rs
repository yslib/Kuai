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
}

pub enum HanlderType {
    Observer,
    Transformer,
    Finalizer,
}

pub enum Artifact {
    Source(String),
    Binary(Vec<u8>),
    Object(Box<dyn Any + Send + Sync>),
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

impl Default for AttrMetadata {
    fn default() -> Self {
        Self::new()
    }
}

pub struct AttrEnvironment<'a> {
    pub parent: Option<&'a AttrEnvironment<'a>>,
    pub local_attrs: &'a [(&'a Attribute, bool)],
    pub delta_meta: AttrMetadata,
}

impl<'a> AttrEnvironment<'a> {
    pub fn new(
        parent: Option<&'a AttrEnvironment<'a>>,
        local_attrs: &'a [(&'a Attribute, bool)],
        delta_meta: AttrMetadata,
    ) -> Self {
        AttrEnvironment {
            parent,
            local_attrs,
            delta_meta,
        }
    }

    pub fn get_metadata<T>(&self, key: &str) -> Option<&T>
    where
        T: Send + Sync + 'static,
    {
        if let Some(value) = self.delta_meta.get::<T>(key) {
            Some(value)
        } else if let Some(parent) = self.parent {
            parent.get_metadata::<T>(key)
        } else {
            None
        }
    }

    pub fn for_active_attrs<F>(
        &self,
        mut node: Stmt,
        mut f: F,
    ) -> Result<AttrAction, Vec<Diagnostic>>
    where
        F: FnMut(&Self, &Attribute, Stmt) -> Result<AttrAction, Vec<Diagnostic>>,
    {
        for (attr, visible) in self.local_attrs {
            if *visible {
                node = match f(self, attr, node)? {
                    AttrAction::Continue(n) => n,
                    AttrAction::SkipChildren(n) => return Ok(AttrAction::Continue(n)),
                    AttrAction::Terminal(n) => return Ok(AttrAction::Terminal(n)),
                    AttrAction::Lowered(a) => return Ok(AttrAction::Lowered(a)),
                }
            }
        }

        if let Some(parent) = self.parent {
            return parent.for_active_attrs(node, f);
        }
        Ok(AttrAction::Continue(node))
    }
}

pub enum AttrAction {
    Continue(Stmt),
    SkipChildren(Stmt),
    Terminal(Stmt),
    Lowered(Artifact),
}

pub trait AttrEngine {
    fn apply<'a>(
        &self,
        ctx: &Context,
        node: Stmt,
        stage: CompileStage,
        env: &'a AttrEnvironment<'a>,
    ) -> Result<AttrAction, Vec<Diagnostic>>;
}

pub trait AttrBase: Send + Sync {
    fn name(&self) -> &str;
    fn handler_type(&self) -> HanlderType;

    fn stage(&self) -> CompileStage;
    fn is_recursive(&self) -> bool {
        false
    }
    fn evaluate(&self, ctx: &Context, attr: &Attribute, meta: &mut AttrMetadata);

    fn transform(
        &self,
        ctx: &Context,
        engine: &dyn AttrEngine,
        attr: &Attribute,
        node: Stmt,
        meta: &AttrMetadata,
    ) -> Result<AttrAction, Vec<Diagnostic>>;
}
