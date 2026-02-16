use crate::attr::registry::AttributeRegistry;
use std::sync::Arc;

#[derive(Clone, Debug)]
pub enum TargetArch {
    X86_64,
    AArch64,
}

// global state of the compiler, including plugin manager, target architecture, etc. READ ONLY

pub struct Session {
    pub target_arch: TargetArch,
    pub attr_registry: Arc<AttributeRegistry>,
    // plugin manager
}

impl Clone for Session {
    fn clone(&self) -> Self {
        Session {
            target_arch: self.target_arch.clone(),
            attr_registry: Arc::clone(&self.attr_registry),
        }
    }
}
