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

    pub fn lookup(&self, id: lasso::Spur) -> &str {
        self.interner.resolve(&id)
    }
}
