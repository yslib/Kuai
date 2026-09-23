//! Interned source names, not declaration or IR entity identities.

use std::fmt;

/// An interned name belonging to the Interner that created it.
///
/// Use the matching pool when resolving a name. This type separates name keys
/// from other IDs; it does not distinguish two Interner instances.
///
/// Raw integers and raw interning keys are not name IDs:
/// ```compile_fail
/// use syntax::name::NameId;
/// let name: NameId = 0_u32;
/// ```
/// ```compile_fail
/// use syntax::name::NameId;
/// let mut raw_pool = lasso::Rodeo::default();
/// let name: NameId = raw_pool.get_or_intern("value");
/// ```
#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct NameId(lasso::Spur);

impl fmt::Debug for NameId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

/// Storage for the names used by syntax and consumers of its AST.
#[derive(Default)]
pub struct Interner {
    names: lasso::Rodeo,
}

impl Interner {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn get_or_intern(&mut self, name: &str) -> NameId {
        NameId(self.names.get_or_intern(name))
    }

    /// Resolves a name allocated by this pool.
    ///
    /// As with the underlying interner, a key from another pool is not valid
    /// here: it may resolve to unrelated text or panic if it is out of bounds.
    pub fn resolve(&self, name: &NameId) -> &str {
        self.names.resolve(&name.0)
    }
}
