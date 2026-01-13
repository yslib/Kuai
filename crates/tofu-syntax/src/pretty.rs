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
            PrimitiveType::Bool => alloc.text("<bool>"),
            PrimitiveType::Char => alloc.text("<char>"),
            PrimitiveType::I8 => alloc.text("<i8>"),
            PrimitiveType::I16 => alloc.text("<i16>"),
            PrimitiveType::I32 => alloc.text("<i32>"),
            PrimitiveType::I64 => alloc.text("<i64>"),
            PrimitiveType::F32 => alloc.text("<f32>"),
            PrimitiveType::F64 => alloc.text("<f64>"),
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
            Literal::Float(f) => alloc.text(format!("(<f64> {})", f)),
            Literal::Int(i) => alloc.text(format!("(<i64> {})", i)),
            Literal::String(s) => alloc.text(format!("(<str> {:?})", s)),
        }
    }
}

impl Pretty for Ident {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc.text(format!("{:?}", self.id))
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
                .append(alloc.text(" = "))
                .append(value.to_doc(alloc)),
            Argument::Attribute(attr) => attr.to_doc(alloc),
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
            doc = doc.append(wrap_list(alloc, "(", &self.args, ")"));
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
            BinaryOp::Plus => alloc.text("<+>"),
            BinaryOp::Minus => alloc.text("<->"),
            BinaryOp::Star => alloc.text("<*>"),
            BinaryOp::Slash => alloc.text("</>"),
        }
    }
}

impl Pretty for Expr {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match &self {
            Expr::Literal(lit) => lit.to_doc(alloc),
            Expr::Variable(ident) => ident.to_doc(alloc),
            Expr::Binary { left, op, right } => alloc
                .text("(")
                .append(op.to_doc(alloc))
                .append(alloc.space())
                .append(
                    left.to_doc(alloc)
                        .append(alloc.text(","))
                        .append(alloc.softline())
                        .append(right.to_doc(alloc)),
                )
                .append(alloc.text(")"))
                .group(),
            Expr::MemberAccess { target, member } => alloc
                .text("(<dot> ")
                .append(target.to_doc(alloc))
                .append(alloc.text(", "))
                .append(member.to_doc(alloc))
                .append(alloc.text(")"))
                .group(),
            Expr::NamespaceAccess { namespace, member } => alloc
                .text("(<::> ")
                .append(namespace.to_doc(alloc))
                .append(alloc.text(", "))
                .append(member.to_doc(alloc))
                .append(alloc.text(")"))
                .group(),
            Expr::Index { target, indices } => alloc
                .text("(<index> ")
                .append(target.to_doc(alloc))
                .append(alloc.space())
                .append(wrap_list(alloc, "[", indices, "]"))
                .append(alloc.text(")"))
                .group(),
            Expr::VectorLiteral(exprs) => wrap_list(alloc, "[", exprs, "]"),
            Expr::Call {
                func,
                generics,
                args,
            } => {
                let mut doc = alloc.text("(<call> ").append(func.to_doc(alloc));
                if !generics.is_empty() {
                    doc = doc.append(wrap_list(alloc, " <", generics, ">"));
                }
                doc.append(alloc.space())
                    .append(wrap_list(alloc, "(", args, ")"))
                    .append(alloc.text(")"))
                    .group()
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
                let mut doc = alloc.text("(<type> ").append(name.to_doc(alloc));
                if !generics.is_empty() {
                    doc = doc.append(wrap_list(alloc, " <", generics, ">"));
                }
                doc.append(alloc.text(")")).group()
            }
            TypeExpr::Tensor { base, shape } => alloc
                .text("(<tensor> ")
                .append(base.to_doc(alloc))
                .append(alloc.space())
                .append(wrap_list(alloc, "[", shape, "]"))
                .append(alloc.text(")"))
                .group(),
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
        let mut doc = alloc
            .text("(<param> ")
            .append(self.name.to_doc(alloc))
            .append(alloc.text(": "))
            .append(self.ty.to_doc(alloc));

        if let Some(default) = &self.default_value {
            doc = doc.append(alloc.text(" = ")).append(default.to_doc(alloc));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for GenericParam {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<generic> ").append(self.name.to_doc(alloc));
        if let Some(ref bound) = self.bound {
            doc = doc.append(alloc.text(": ")).append(bound.to_doc(alloc));
        }
        if let Some(ref default) = self.default_type {
            doc = doc.append(alloc.text(" = ")).append(default.to_doc(alloc));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for Block {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let body = alloc.intersperse(self.stmts.iter().map(|s| s.to_doc(alloc)), alloc.hardline());
        alloc
            .text("(<block>")
            .append(alloc.hardline().append(body).nest(2))
            .append(alloc.hardline())
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for FuncProto {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<proto> ").append(self.name.to_doc(alloc));
        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.space())
                .append(wrap_list(alloc, "<", &self.generics, ">"));
        }
        doc = doc
            .append(alloc.space())
            .append(wrap_list(alloc, "(", &self.params, ")"));

        if let Some(ref ret) = self.return_type {
            doc = doc.append(alloc.text(" -> ")).append(ret.to_doc(alloc));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for FuncDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<func> ");
        if !self.attributes.is_empty() {
            doc = doc
                .append(wrap_list(alloc, "[", &self.attributes, "]"))
                .append(alloc.space());
        }
        doc = doc.append(self.proto.to_doc(alloc));

        if let Some(ref body) = self.body {
            doc = doc.append(alloc.space()).append(body.to_doc(alloc));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for FieldDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc
            .text("(<field> ")
            .append(self.name.to_doc(alloc))
            .append(alloc.text(": "))
            .append(self.ty.to_doc(alloc))
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for StructDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<struct> ");
        if !self.attributes.is_empty() {
            doc = doc
                .append(wrap_list(alloc, "[", &self.attributes, "]"))
                .append(alloc.space());
        }
        doc = doc.append(self.name.to_doc(alloc));

        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.space())
                .append(wrap_list(alloc, "<", &self.generics, ">"));
        }

        let fields_doc = alloc.intersperse(
            self.fields.iter().map(|f| f.to_doc(alloc)),
            alloc.hardline(),
        );

        doc.append(alloc.hardline().append(fields_doc).nest(2))
            .append(alloc.hardline())
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for Range {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc
            .text("(<range> ")
            .append(self.start.to_doc(alloc))
            .append(alloc.text(".."))
            .append(self.end.to_doc(alloc));

        if let Some(step) = &self.step {
            doc = doc.append(alloc.text(" step ")).append(step.to_doc(alloc));
        }
        doc.append(alloc.text(")"))
    }
}

impl Pretty for DimDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<dim> ").append(self.name.to_doc(alloc));
        if let Some(bound) = &self.bound {
            doc = doc.append(alloc.text(": ")).append(bound.to_doc(alloc));
        }
        if let Some(val) = &self.value {
            doc = doc.append(alloc.text(" = ")).append(val.to_doc(alloc));
        }
        doc.append(alloc.text(")"))
    }
}

impl Pretty for ElseBranch {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            ElseBranch::Block(b) => b.to_doc(alloc),
            ElseBranch::If(s) => s.to_doc(alloc),
        }
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
                .text("(<import> ")
                .append(path.to_doc(alloc))
                .append(alloc.text(")")),
            Stmt::Function(f) => f.to_doc(alloc),
            Stmt::Struct(s) => s.to_doc(alloc),
            Stmt::Expr(expr) => expr.to_doc(alloc),
            Stmt::Return(expr) => alloc
                .text("(<return> ")
                .append(expr.to_doc(alloc))
                .append(alloc.text(")"))
                .group(),
            Stmt::DimDecl(decls) => wrap_list(alloc, "(<dim_group> ", decls, ")"),
            Stmt::VarDecl(VarDecl { name, ty, init, .. }) => {
                let mut doc = alloc.text("(<let> ").append(name.to_doc(alloc));
                if let Some(ty) = ty {
                    doc = doc.append(alloc.text(": ")).append(ty.to_doc(alloc));
                }
                if let Some(init) = init {
                    doc = doc.append(alloc.text(" = ")).append(init.to_doc(alloc));
                }
                doc.append(alloc.text(")")).group()
            }
            Stmt::Assignment { target, value, .. } => alloc
                .text("(<set> ")
                .append(target.to_doc(alloc))
                .append(alloc.text(" = "))
                .append(value.to_doc(alloc))
                .append(alloc.text(")"))
                .group(),
            Stmt::If {
                attributes,
                condtion,
                then_branch,
                else_branch,
            } => {
                let mut doc = alloc.text("(<if> ");
                if !attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, "[", attributes, "]"))
                        .append(alloc.space());
                }
                doc = doc
                    .append(condtion.to_doc(alloc))
                    .append(alloc.space())
                    .append(then_branch.to_doc(alloc));
                if let Some(eb) = else_branch {
                    doc = doc.append(alloc.space()).append(eb.to_doc(alloc));
                }
                doc.append(alloc.text(")")).group()
            }
            Stmt::For {
                attributes,
                var,
                range,
                body,
                ..
            } => {
                let mut doc = alloc.text("(<for> ");
                if !attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, "[", attributes, "]"))
                        .append(alloc.space());
                }
                doc.append(var.to_doc(alloc))
                    .append(alloc.space())
                    .append(range.to_doc(alloc))
                    .append(alloc.space())
                    .append(body.to_doc(alloc))
                    .append(alloc.text(")"))
                    .group()
            }
            Stmt::Break(_) => alloc.text("<break>"),
            Stmt::Continue(_) => alloc.text("<continue>"),
            Stmt::Error { .. } => alloc.text("<stmt_error>"),
        }
    }
}

impl Pretty for Module {
    fn to_doc<'a, D>(&self, alloc: &'a D) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = if self.attributes.is_empty() {
            alloc.nil()
        } else {
            wrap_list(alloc, "(<module_attrs> ", &self.attributes, ")").append(alloc.hardline())
        };

        doc = doc.append(alloc.intersperse(
            self.stmts.iter().map(|stmt| stmt.to_doc(alloc)),
            alloc.hardline(),
        ));
        doc
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
