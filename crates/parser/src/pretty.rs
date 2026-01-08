use crate::ast::*;
use pretty::{Arena, DocAllocator, DocBuilder};

pub trait Pretty {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone;
}

fn wrap_list<'a, 'b, D, T>(
    alloc: &'a D,
    open: &'a str,
    items: &'b [T],
    close: &'a str,
) -> DocBuilder<'a, D>
where
    D: DocAllocator<'a>,
    D::Doc: Clone,
    T: Pretty,
{
    if items.is_empty() {
        return alloc.text(format!("{}{}", open, close));
    }
    let inner = alloc.intersperse(
        items.iter().map(|item| item.to_doc(alloc)),
        alloc.text(", "),
    );
    alloc
        .text(open)
        .append(inner)
        .append(alloc.softline())
        .append(alloc.text(close))
        .group()
}

impl Pretty for PrimitiveType {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            PrimitiveType::F32 => alloc.text("f32"),
            PrimitiveType::I32 => alloc.text("i32"),
            PrimitiveType::Bool => alloc.text("bool"),
        }
    }
}

// display as (type value)
impl Pretty for Literal {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            Literal::Float(f) => alloc.text(format!("(f32 {})", f)),
            Literal::Int(i) => alloc.text(format!("(i32 {:.1})", i)),
            Literal::String(s) => alloc.text(format!("(str {:?})", s)),
        }
    }
}

impl Pretty for Ident {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc.text(self.name.clone())
    }
}

// display as (name: value)
impl Pretty for Argument {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            Argument::Positional(expr) => expr.to_doc(alloc),
            Argument::Named { name, value } => name
                .to_doc(alloc)
                .append(alloc.text(": "))
                .append(value.to_doc(alloc)),
        }
    }
}

impl Pretty for Path {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc.intersperse(
            self.segments.iter().map(|seg| seg.to_doc(alloc)),
            alloc.text("::"),
        )
    }
}

impl Pretty for Attribute {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("@").append(alloc.text(self.name.clone()));
        if !self.args.is_empty() {
            doc = doc
                .append(alloc.text("("))
                .append(alloc.intersperse(
                    self.args.iter().map(|arg| arg.to_doc(alloc)),
                    alloc.text(", "),
                ))
                .append(alloc.text(")"));
        }
        doc
    }
}

impl Pretty for BinaryOp {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            BinaryOp::Plus => alloc.text("+"),
            BinaryOp::Minus => alloc.text("-"),
            BinaryOp::Star => alloc.text("*"),
            BinaryOp::Slash => alloc.text("/"),
        }
    }
}

impl Pretty for Expr {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            Expr::Literal(lit) => lit.to_doc(alloc),
            Expr::Variable(ident) => ident.to_doc(alloc),
            Expr::Binary { left, op, right } => alloc
                .text("(")
                .append(op.to_doc(alloc))
                .append(alloc.space())
                .append(alloc.concat([
                    left.to_doc(alloc),
                    alloc.text(","),
                    alloc.softline(),
                    right.to_doc(alloc),
                ]))
                .append(alloc.text(")"))
                .group(),
            Expr::MemberAccess { target, member } => alloc
                .text("(<dot> ")
                .append(member.to_doc(alloc))
                .append(alloc.text(", "))
                .append(target.to_doc(alloc))
                .append(alloc.text(")"))
                .group(),
            Expr::Index { target, indices } => target
                .to_doc(alloc)
                .append(wrap_list(alloc, "[", indices, "]")),
            Expr::VectorLiteral(exprs) => wrap_list(alloc, "[", exprs, "]"),
            Expr::Call {
                func,
                generics,
                args,
            } => {
                func.to_doc(alloc);
                if !generics.is_empty() {
                    wrap_list(alloc, "<", generics, ">");
                }
                wrap_list(alloc, "(", args, ")")
            }
            Expr::Error(_) => alloc.text("<error>"),
        }
    }
}

impl Pretty for TypeExpr {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            TypeExpr::Primitive(p) => p.to_doc(alloc),
            TypeExpr::Named { name, generics } => {
                let mut doc = name.to_doc(alloc);
                if !generics.is_empty() {
                    doc = doc
                        .append(alloc.text("<"))
                        .append(alloc.intersperse(
                            generics.iter().map(|g| g.to_doc(alloc)),
                            alloc.text(", "),
                        ))
                        .append(alloc.text(">"));
                }
                doc
            }
            TypeExpr::Tensor { base, shape } => base
                .to_doc(alloc)
                .append(alloc.text("["))
                .append(alloc.intersperse(shape.iter().map(|s| s.to_doc(alloc)), alloc.text(", ")))
                .append(alloc.text("]")),
            TypeExpr::Error { .. } => alloc.text("<type_error>"),
        }
    }
}

impl Pretty for Param {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = self.name.to_doc(alloc).append(alloc.text(": "));
        doc = doc.append(self.ty.to_doc(alloc));
        if let Some(ref default) = self.default_value {
            doc = doc.append(alloc.text(" = ")).append(default.to_doc(alloc));
        }
        doc
    }
}

impl Pretty for GenericParam {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = self.name.to_doc(alloc);
        if let Some(ref bound) = self.bound {
            doc = doc.append(alloc.text(": ")).append(bound.to_doc(alloc));
        }
        if let Some(ref default) = self.default_type {
            doc = doc.append(alloc.text(" = ")).append(default.to_doc(alloc));
        }
        doc
    }
}

impl Pretty for Block {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        if self.stmts.is_empty() {
            alloc.text("{}")
        } else {
            let body = alloc.hardline().append(
                alloc.intersperse(self.stmts.iter().map(|s| s.to_doc(alloc)), alloc.hardline()),
            );
            alloc
                .text("{")
                .append(body.nest(2))
                .append(alloc.hardline())
                .append(alloc.text("}"))
        }
    }
}

impl Pretty for FuncProto {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("func ").append(self.name.to_doc(alloc));

        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.text("<"))
                .append(alloc.intersperse(
                    self.generics.iter().map(|g| g.to_doc(alloc)),
                    alloc.text(", "),
                ))
                .append(alloc.text(">"));
        }

        doc = doc
            .append(alloc.text("("))
            .append(alloc.intersperse(
                self.params.iter().map(|p| p.to_doc(alloc)),
                alloc.text(", "),
            ))
            .append(alloc.text(")"));

        if let Some(ref ret) = self.return_type {
            doc = doc.append(alloc.text(" -> ")).append(ret.to_doc(alloc));
        }

        doc
    }
}

impl Pretty for FuncDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = if self.attributes.is_empty() {
            alloc.nil()
        } else {
            alloc
                .intersperse(
                    self.attributes.iter().map(|a| a.to_doc(alloc)),
                    alloc.hardline(),
                )
                .append(alloc.hardline())
        };

        doc = doc.append(self.proto.to_doc(alloc));

        if let Some(ref body) = self.body {
            doc = doc.append(alloc.space()).append(body.to_doc(alloc));
        } else {
            doc = doc.append(alloc.text(";"));
        }

        doc
    }
}

impl Pretty for FieldDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        self.name
            .to_doc(alloc)
            .append(alloc.text(": "))
            .append(self.ty.to_doc(alloc))
    }
}

impl Pretty for StructDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = if self.attributes.is_empty() {
            alloc.nil()
        } else {
            alloc
                .intersperse(
                    self.attributes.iter().map(|a| a.to_doc(alloc)),
                    alloc.hardline(),
                )
                .append(alloc.hardline())
        };

        doc = doc
            .append(alloc.text("struct "))
            .append(self.name.to_doc(alloc));

        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.text("<"))
                .append(alloc.intersperse(
                    self.generics.iter().map(|g| g.to_doc(alloc)),
                    alloc.text(", "),
                ))
                .append(alloc.text(">"));
        }

        let fields_doc = alloc.hardline().append(
            alloc.intersperse(
                self.fields
                    .iter()
                    .map(|f| f.to_doc(alloc).append(alloc.text(","))),
                alloc.hardline(),
            ),
        );

        doc.append(alloc.space())
            .append(alloc.text("{"))
            .append(fields_doc.nest(2))
            .append(alloc.hardline())
            .append(alloc.text("}"))
    }
}

impl Pretty for Stmt {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            Stmt::Import(path) => alloc
                .text("import")
                .append(alloc.space())
                .append(path.to_doc(alloc))
                .append(alloc.text(";")),
            Stmt::Function(f) => f.to_doc(alloc),
            Stmt::Struct(s) => s.to_doc(alloc),
            Stmt::Expr(expr) => expr.to_doc(alloc).append(alloc.text(";")),
            Stmt::Return(expr) => alloc
                .text("return")
                .append(alloc.space())
                .append(expr.to_doc(alloc))
                .append(alloc.text(";"))
                .group(),
            Stmt::VarDecl { name, ty, init, .. } => {
                let mut doc = alloc.text("let ").append(name.to_doc(alloc));
                if let Some(ty) = ty {
                    doc = doc.append(alloc.text(": ")).append(ty.to_doc(alloc));
                }
                if let Some(init) = init {
                    doc = doc.append(alloc.text(" = ")).append(init.to_doc(alloc));
                }
                doc.append(alloc.text(";"))
            }
            Stmt::Assignment { target, value, .. } => target
                .to_doc(alloc)
                .append(alloc.text(" = "))
                .append(value.to_doc(alloc))
                .append(alloc.text(";")),
            Stmt::If {
                attributes: _attrs,
                condtion: _c,
                then_branch: _a,
                else_branch: _b,
            } => alloc.text("<if_stmt>;"),
            Stmt::For {
                attributes: _attrs,
                var: _var,
                range: _range,
                body: _body,
                ..
            } => alloc.text("<for_stmt>;"),
            Stmt::Break { .. } => alloc.text("break;"),
            Stmt::Continue { .. } => alloc.text("continue;"),
            Stmt::Error { .. } => alloc.text("<stmt_error>;"),
        }
    }
}

impl Pretty for Module {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let doc = if self.attributes.is_empty() {
            alloc.nil()
        } else {
            alloc
                .intersperse(
                    self.attributes.iter().map(|a| a.to_doc(alloc)),
                    alloc.hardline(),
                )
                .append(alloc.hardline())
        };

        doc.append(alloc.intersperse(
            self.stmts.iter().map(|stmt| stmt.to_doc(alloc)),
            alloc.hardline(),
        ))
    }
}

pub fn print_ast(ast: &Module) -> String {
    let arena = Arena::new();
    let doc = ast.to_doc(&arena);
    let mut output = Vec::new();
    let res = doc.1.render(80, &mut output);
    match res {
        Ok(_) => String::from_utf8(output).unwrap_or_else(|_| "<invalid utf8>".to_string()),
        Err(_) => "<rendering error>".to_string(),
    }
}
