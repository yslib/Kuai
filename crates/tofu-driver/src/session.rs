use crate::attr::registry::AttributeRegistry;

#[derive(Clone, Debug)]
pub enum TargetArch {
    X86_64,
    AArch64,
}

// global state of the compiler, including plugin manager, target architecture, etc. READ ONLY

pub struct Session<'a> {
    pub target_arch: TargetArch,
    pub attr_registry: &'a AttributeRegistry,
    // plugin manager
}

impl<'a> Clone for Session<'a> {
    fn clone(&self) -> Self {
        Session {
            target_arch: self.target_arch.clone(),
            attr_registry: self.attr_registry,
        }
    }
}
