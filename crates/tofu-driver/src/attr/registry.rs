use crate::attr::traits::*;
use std::collections::HashMap;
use syntax::ast::*;
pub struct AttributeRegistry {
    handlers: HashMap<String, Vec<Box<dyn AttrBase>>>,
}

impl AttributeRegistry {
    pub fn new() -> Self {
        AttributeRegistry {
            handlers: HashMap::new(),
        }
    }

    pub fn get_handlers(&self, attr_name: &str, stage: CompileStage) -> Vec<&dyn AttrBase> {
        if let Some(handlers) = self.handlers.get(attr_name) {
            handlers
                .iter()
                .filter(|h| h.stage() == stage)
                .map(|h| h.as_ref())
                .collect()
        } else {
            Vec::new()
        }
    }
}

impl Default for AttributeRegistry {
    fn default() -> Self {
        Self::new()
    }
}
