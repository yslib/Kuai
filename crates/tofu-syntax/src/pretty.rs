use crate::ast::*;
use tofu_core::Interner;
use pretty::{Arena, DocAllocator, DocBuilder};

pub struct PrintContext<'a> {
    pub interner: &'a Interner,
}

pub trait Pretty {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone;
}

fn wrap_list<'a, 'b, D, T>(
    alloc: &'a D,
    ctx: &PrintContext<'a>,
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
        items.iter().map(|item| item.to_doc(alloc, ctx)),
        alloc.text(", "),
    );
    alloc
        .text(open)
        .append(inner)
        .append(alloc.text(close))
        .group()
}

impl Pretty for PrimitiveType {
    fn to_doc<'a, D>(&self, alloc: &'a D, _ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
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
    fn to_doc<'a, D>(&self, alloc: &'a D, _ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
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
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc.text(ctx.interner.resolve(&self.id))
    }
}

// display as (name: value)
impl Pretty for Argument {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            Argument::Positional(expr) => expr.to_doc(alloc, ctx),
            Argument::Named { name, value } => name
                .to_doc(alloc, ctx)
                .append(alloc.text(" = "))
                .append(value.to_doc(alloc, ctx)),
            Argument::Attribute(attr) => attr.to_doc(alloc, ctx),
        }
    }
}

impl Pretty for Path {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc.intersperse(
            self.segments.iter().map(|seg| seg.to_doc(alloc, ctx)),
            alloc.text("::"),
        )
    }
}

impl Pretty for Attribute {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("@").append(alloc.text(self.name.clone()));
        if !self.args.is_empty() {
            doc = doc.append(wrap_list(alloc, ctx, "(", &self.args, ")"));
        }
        doc
    }
}

impl Pretty for BinaryOp {
    fn to_doc<'a, D>(&self, alloc: &'a D, _ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
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
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match &self {
            Expr::Literal(lit) => lit.to_doc(alloc, ctx),
            Expr::Variable(ident) => ident.to_doc(alloc, ctx),
            Expr::Binary { left, op, right } => alloc
                .text("(")
                .append(op.to_doc(alloc, ctx))
                .append(alloc.space())
                .append(
                    left.to_doc(alloc, ctx)
                        .append(alloc.text(","))
                        .append(alloc.softline())
                        .append(right.to_doc(alloc, ctx)),
                )
                .append(alloc.text(")"))
                .group(),
            Expr::MemberAccess { target, member } => alloc
                .text("(<dot> ")
                .append(target.to_doc(alloc, ctx))
                .append(alloc.text(", "))
                .append(member.to_doc(alloc, ctx))
                .append(alloc.text(")"))
                .group(),
            Expr::NamespaceAccess { namespace, member } => alloc
                .text("(<::> ")
                .append(namespace.to_doc(alloc, ctx))
                .append(alloc.text(", "))
                .append(member.to_doc(alloc, ctx))
                .append(alloc.text(")"))
                .group(),
            Expr::Index { target, indices } => alloc
                .text("(<index> ")
                .append(target.to_doc(alloc, ctx))
                .append(alloc.space())
                .append(wrap_list(alloc, ctx, "[", indices, "]"))
                .append(alloc.text(")"))
                .group(),
            Expr::VectorLiteral(exprs) => wrap_list(alloc, ctx, "[", exprs, "]"),
            Expr::Call {
                func,
                generics,
                args,
            } => {
                let mut doc = alloc.text("(<call> ").append(func.to_doc(alloc, ctx));
                if !generics.is_empty() {
                    doc = doc.append(wrap_list(alloc, ctx, " <", generics, ">"));
                }
                doc.append(alloc.space())
                    .append(wrap_list(alloc, ctx, "(", args, ")"))
                    .append(alloc.text(")"))
                    .group()
            }
            Expr::Error(_) => alloc.text("<error>"),
        }
    }
}

impl Pretty for TypeExpr {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            TypeExpr::Primitive(p) => p.to_doc(alloc, ctx),
            TypeExpr::Named { name, generics } => {
                let mut doc = alloc.text("(<type> ").append(name.to_doc(alloc, ctx));
                if !generics.is_empty() {
                    doc = doc.append(wrap_list(alloc, ctx, " <", generics, ">"));
                }
                doc.append(alloc.text(")")).group()
            }
            TypeExpr::Tensor { base, shape } => alloc
                .text("(<tensor> ")
                .append(base.to_doc(alloc, ctx))
                .append(alloc.space())
                .append(wrap_list(alloc, ctx, "[", shape, "]"))
                .append(alloc.text(")"))
                .group(),
            TypeExpr::Error { .. } => alloc.text("<type_error>"),
        }
    }
}

impl Pretty for Param {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc
            .text("(<param> ")
            .append(self.name.to_doc(alloc, ctx))
            .append(alloc.text(": "))
            .append(self.ty.to_doc(alloc, ctx));

        if let Some(default) = &self.default_value {
            doc = doc
                .append(alloc.text(" = "))
                .append(default.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for GenericParam {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc
            .text("(<generic> ")
            .append(self.name.to_doc(alloc, ctx));
        if let Some(ref bound) = self.bound {
            doc = doc
                .append(alloc.text(": "))
                .append(bound.to_doc(alloc, ctx));
        }
        if let Some(ref default) = self.default_type {
            doc = doc
                .append(alloc.text(" = "))
                .append(default.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for Block {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let body = alloc.intersperse(
            self.stmts.iter().map(|s| s.to_doc(alloc, ctx)),
            alloc.hardline(),
        );
        alloc
            .text("(<block>")
            .append(alloc.hardline().append(body).nest(2))
            .append(alloc.hardline())
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for FuncProto {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<proto> ").append(self.name.to_doc(alloc, ctx));
        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.space())
                .append(wrap_list(alloc, ctx, "<", &self.generics, ">"));
        }
        doc = doc
            .append(alloc.space())
            .append(wrap_list(alloc, ctx, "(", &self.params, ")"));

        if let Some(ref ret) = self.return_type {
            doc = doc
                .append(alloc.text(" -> "))
                .append(ret.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for FuncDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<func> ");
        doc = doc.append(self.proto.to_doc(alloc, ctx));

        if let Some(ref body) = self.body {
            doc = doc.append(alloc.space()).append(body.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")")).group()
    }
}

impl Pretty for FieldDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        alloc
            .text("(<field> ")
            .append(self.name.to_doc(alloc, ctx))
            .append(alloc.text(": "))
            .append(self.ty.to_doc(alloc, ctx))
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for StructDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let fields_doc = alloc.intersperse(
            self.fields.iter().map(|f| f.to_doc(alloc, ctx)),
            alloc.hardline(),
        );

        let mut doc = alloc.text("").append(self.name.to_doc(alloc, ctx));
        if !self.generics.is_empty() {
            doc = doc
                .append(alloc.space())
                .append(wrap_list(alloc, ctx, "<", &self.generics, ">"));
        }
        doc.append(alloc.hardline().append(fields_doc).nest(2))
            .append(alloc.hardline())
            .append(alloc.text(")"))
            .group()
    }
}

impl Pretty for Range {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc
            .text("(<range> ")
            .append(self.start.to_doc(alloc, ctx))
            .append(alloc.text(".."))
            .append(self.end.to_doc(alloc, ctx));

        if let Some(step) = &self.step {
            doc = doc
                .append(alloc.text(" step "))
                .append(step.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")"))
    }
}

impl Pretty for DimDecl {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = alloc.text("(<dim> ").append(self.name.to_doc(alloc, ctx));
        if let Some(bound) = &self.bound {
            doc = doc
                .append(alloc.text(": "))
                .append(bound.to_doc(alloc, ctx));
        }
        if let Some(val) = &self.value {
            doc = doc.append(alloc.text(" = ")).append(val.to_doc(alloc, ctx));
        }
        doc.append(alloc.text(")"))
    }
}

impl Pretty for ElseBranch {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match self {
            ElseBranch::Block(b) => b.to_doc(alloc, ctx),
            ElseBranch::If(s) => s.to_doc(alloc, ctx),
        }
    }
}

impl Pretty for Stmt {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        match &self.stmt {
            StmtImpl::Import(path) => alloc
                .text("(<import> ")
                .append(path.to_doc(alloc, ctx))
                .append(alloc.text(")")),
            StmtImpl::Function(f) => {
                let mut doc = alloc.text("(<func> ");
                if !self.attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, ctx, "[", &self.attributes, "]"))
                        .append(alloc.space());
                }
                doc = doc.append(f.proto.to_doc(alloc, ctx)).append(alloc.space());
                if let Some(ref body) = f.body {
                    doc = doc.append(body.to_doc(alloc, ctx));
                }
                doc.append(alloc.text(")")).group()
            }
            StmtImpl::Struct(s) => {
                let mut doc = alloc.text("(<struct> ");
                if !self.attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, ctx, "[", &self.attributes, "]"))
                        .append(alloc.space());
                }
                doc.append(s.to_doc(alloc, ctx))
            }
            StmtImpl::Expr(expr) => expr.to_doc(alloc, ctx),
            StmtImpl::Return(expr) => alloc
                .text("(<return> ")
                .append(expr.to_doc(alloc, ctx))
                .append(alloc.text(")"))
                .group(),
            StmtImpl::DimDecl(decls) => wrap_list(alloc, ctx, "(<dim_group> ", decls, ")"),
            StmtImpl::VarDecl(VarDecl { name, ty, init, .. }) => {
                let mut doc = alloc.text("(<let> ").append(name.to_doc(alloc, ctx));
                if let Some(ty) = ty {
                    doc = doc.append(alloc.text(": ")).append(ty.to_doc(alloc, ctx));
                }
                if let Some(init) = init {
                    doc = doc
                        .append(alloc.text(" = "))
                        .append(init.to_doc(alloc, ctx));
                }
                doc.append(alloc.text(")")).group()
            }
            StmtImpl::Assignment { target, value, .. } => alloc
                .text("(<set> ")
                .append(target.to_doc(alloc, ctx))
                .append(alloc.text(" = "))
                .append(value.to_doc(alloc, ctx))
                .append(alloc.text(")"))
                .group(),
            StmtImpl::If {
                condtion,
                then_branch,
                else_branch,
                ..
            } => {
                let mut doc = alloc.text("(<if> ");
                if !self.attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, ctx, "[", &self.attributes, "]"))
                        .append(alloc.space());
                }
                doc = doc
                    .append(condtion.to_doc(alloc, ctx))
                    .append(alloc.space())
                    .append(then_branch.to_doc(alloc, ctx));
                if let Some(eb) = else_branch {
                    doc = doc.append(alloc.space()).append(eb.to_doc(alloc, ctx));
                }
                doc.append(alloc.text(")")).group()
            }
            StmtImpl::For {
                var, range, body, ..
            } => {
                let mut doc = alloc.text("(<for> ");
                if !self.attributes.is_empty() {
                    doc = doc
                        .append(wrap_list(alloc, ctx, "[", &self.attributes, "]"))
                        .append(alloc.space());
                }
                doc.append(var.to_doc(alloc, ctx))
                    .append(alloc.space())
                    .append(range.to_doc(alloc, ctx))
                    .append(alloc.space())
                    .append(body.to_doc(alloc, ctx))
                    .append(alloc.text(")"))
                    .group()
            }
            StmtImpl::Break(_) => alloc.text("<break>"),
            StmtImpl::Continue(_) => alloc.text("<continue>"),
            StmtImpl::Error { .. } => alloc.text("<stmt_error>"),
        }
    }
}

impl Pretty for Module {
    fn to_doc<'a, D>(&self, alloc: &'a D, ctx: &PrintContext<'a>) -> DocBuilder<'a, D>
    where
        D: DocAllocator<'a>,
        D::Doc: Clone,
    {
        let mut doc = if self.top_level_attributes.is_empty() {
            alloc.nil()
        } else {
            wrap_list(
                alloc,
                ctx,
                "(<module_attrs> ",
                &self.top_level_attributes,
                ")",
            )
            .append(alloc.hardline())
        };

        doc = doc.append(alloc.intersperse(
            self.stmts.iter().map(|stmt| stmt.to_doc(alloc, ctx)),
            alloc.hardline(),
        ));
        doc
    }
}

pub fn print_ast(ast: &Module, interner: &Interner) -> String {
    let arena = Arena::new();
    let ctx = PrintContext { interner };
    let doc = ast.to_doc(&arena, &ctx);
    let mut output = Vec::new();
    let res = doc.1.render(80, &mut output);
    match res {
        Ok(_) => String::from_utf8(output).unwrap_or_else(|_| "<invalid utf8>".to_string()),
        Err(_) => "<rendering error>".to_string(),
    }
}
