use crate::ast::*;
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

#[derive(Debug, Clone, PartialEq)]
pub enum SymbolKind {
    Dimension,
    Variable,
    Function,
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
    pub name: String,
    pub kind: SymbolKind,
    pub ty: Option<TypeExpr>,
    pub span: Span,
}

pub struct Scope {
    pub symbols: HashMap<Id, Symbol>,
    pub parent: Option<Rc<RefCell<Scope>>>,
    // 记录这是什么类型的作用域：Global, Function, Block
    pub kind: ScopeKind,
}
impl Scope {
    pub fn new(parent: Option<Rc<RefCell<Scope>>>, kind: ScopeKind) -> Self {
        Self {
            symbols: HashMap::new(),
            parent,
            kind,
        }
    }

    // 递归查找符号（向上查找）
    pub fn resolve(&self, name: &str) -> Option<Symbol> {
        if let Some(sym) = self.symbols.get(name) {
            return Some(sym.clone());
        }
        self.parent.as_ref().and_then(|p| p.borrow().resolve(name))
    }

    // 在当前作用域定义符号（检查冲突）
    pub fn define(&mut self, sym: Symbol) -> Result<(), Symbol> {
        if self.symbols.contains_key(&sym.name) {
            return Err(self.symbols.get(&sym.name).unwrap().clone());
        }
        self.symbols.insert(sym.name.clone(), sym);
        Ok(())
    }
}
