use tofu_core::spur::Id;
use std::collections::HashMap;
use std::sync::{Arc, RwLock};
use syntax::ast::*;

#[derive(Debug, Clone, PartialEq)]
pub enum SymbolKind {
    Dimension,
    Variable,
    Function,
    BuiltinFunction,
    Struct,
    Param,
}
pub enum ScopeKind {
    Global,
    Function,
    Block,
}

#[derive(Debug, Clone)]
pub struct Symbol {
    pub id: Id,
    pub kind: SymbolKind,
    pub ty: Option<TypeExpr>,
    pub span: Span,
}

pub struct Scope {
    pub symbols: HashMap<Id, Symbol>,
    pub parent: Option<Arc<RwLock<Scope>>>,
    // 记录这是什么类型的作用域：Global, Function, Block
    pub kind: ScopeKind,
}
impl Scope {
    pub fn new(parent: Option<Arc<RwLock<Scope>>>, kind: ScopeKind) -> Self {
        Self {
            symbols: HashMap::new(),
            parent,
            kind,
        }
    }

    pub fn resolve(&self, name: Id) -> Option<Symbol> {
        if let Some(sym) = self.symbols.get(&name) {
            Some(sym.clone())
        } else {
            self.parent
                .as_ref()
                .and_then(|p| p.read().unwrap().resolve(name))
        }
    }

    // 在当前作用域定义符号（检查冲突）
    pub fn define(&mut self, sym: Symbol) -> Result<(), Symbol> {
        if let Some(old) = self.symbols.get(&sym.id) {
            Err(old.clone())
        } else {
            self.symbols.insert(sym.id, sym);
            Ok(())
        }
    }
}
