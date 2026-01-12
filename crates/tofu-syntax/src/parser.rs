use crate::ast::*;
use crate::lexer::Token;
use core::Interner;
use core::diagnostic::Diagnostic;
use core::diagnostic::Severity;
use logos::{Lexer, Logos};

pub trait FromToken {
    fn from_token(token: Token) -> Self;
}

impl PrimitiveType {
    pub fn from_token(token: &Token) -> Option<Self> {
        match token {
            Token::TypeBool => Some(PrimitiveType::Bool),
            Token::TypeChar => Some(PrimitiveType::Char),
            Token::TypeI8 => Some(PrimitiveType::I8),
            Token::TypeI16 => Some(PrimitiveType::I16),
            Token::TypeI32 => Some(PrimitiveType::I32),
            Token::TypeI64 => Some(PrimitiveType::I64),
            Token::TypeF32 => Some(PrimitiveType::F32),
            Token::TypeF64 => Some(PrimitiveType::F64),
            _ => None,
        }
    }
}

pub struct Parser<'source> {
    lexer: Lexer<'source, Token>,
    interner: &'source mut Interner,
    current_token: Option<Token>,
    current_slice: &'source str,
    diagnostics: Vec<Diagnostic>,
}

impl<'source> Parser<'source> {
    pub fn new(source: &'source str, interner: &'source mut Interner) -> Self {
        let mut lexer = Token::lexer(source);
        let current_token = lexer.next().and_then(|r| r.ok());
        let current_slice = lexer.slice();

        Self {
            lexer,
            interner,
            current_token,
            current_slice,
            diagnostics: Vec::new(),
        }
    }

    // --- 基础工具 ---

    fn peek(&self) -> Option<Token> {
        self.lexer.clone().next().and_then(|r| r.ok())
    }

    fn advance(&mut self) {
        self.current_token = self.lexer.next().and_then(|r| r.ok());
        self.current_slice = self.lexer.slice();
    }

    /// skip tokens until a reasonable point to continue parsing
    fn synchronize(&mut self) {
        // 简单的同步逻辑：跳过直到下一个分号或块结束
        while let Some(token) = &self.current_token {
            match token {
                Token::SemiColon | Token::RBrace => {
                    // 找到同步点
                    self.advance();
                    break;
                }
                Token::KwFunc | Token::KwStruct | Token::KwImport => return,
                _ => self.advance(),
            }
        }
    }

    fn synchronize_until(&mut self, stop_tokens: &[Token]) {
        while let Some(token) = &self.current_token {
            if stop_tokens.contains(token) {
                break;
            }
            self.advance();
        }
    }

    fn check(&self, token: Token) -> bool {
        self.current_token == Some(token)
    }

    fn expect(&mut self, token: Token, msg: &str) -> bool {
        if self.check(token) {
            self.advance();
            true
        } else {
            self.report_error(msg);
            false
        }
    }

    // 消费期望的 Token，否则 panic (生产环境应该返回 Result)
    fn consume(&mut self, token: Token, msg: &str) {
        //self.advance();
        if self.check(token.clone()) {
            self.advance();
        } else {
            panic!(
                "Syntax Error: expected {:?}, got {:?} ({})",
                token, self.current_token, msg
            );
        }
    }

    fn report_error(&mut self, msg: &str) {
        let span = self.lexer.span();
        self.diagnostics
            .push(Diagnostic::error(Severity::Error, span, msg.to_string()));
        if self.diagnostics.len() > 100 {
            panic!("Too many errors, aborting parsing.");
        }
    }

    pub fn parse(&mut self) -> Result<Module, Vec<Diagnostic>> {
        let start = self.lexer.span().start;
        let attributes = self.parse_top_level_attributes();
        let stmts = self.parse_impl()?;
        let end = self.lexer.span().end;
        Ok(Module {
            attributes,
            stmts,
            span: start..end,
        })
    }

    fn parse_top_level_attributes(&mut self) -> Vec<Attribute> {
        //TODO:: ignore top-level attributes for now, becuase they are not used in current design
        // and it's a bit complex to handle them correctly.
        // for example, @kernel should be attached to func decl, not module.
        // A: only those attributes with semi-colon can be treated as module-level attributes.
        // B: top-level attributes with # prefix are module-level attributes.
        // They're not determined yet.
        Vec::new()
    }

    pub fn parse_impl(&mut self) -> Result<Vec<Stmt>, Vec<Diagnostic>> {
        let mut stmts = Vec::new();
        while self.current_token.is_some() {
            stmts.push(self.parse_stmt());
        }
        if self.diagnostics.is_empty() {
            Ok(stmts)
        } else {
            Err(self.diagnostics.clone())
        }
    }

    // ident: TypeExpr
    fn parse_param(&mut self) -> Result<Param, ()> {
        let start = self.lexer.span().start;
        let name = self.parse_identifier();
        if !self.expect(Token::Colon, "Expected ':' after parameter name") {
            return Err(());
        }
        let ty = self.parse_type_expr();
        if let TypeExpr::Error { .. } = ty {
            return Err(());
        }
        Ok(Param {
            name,
            ty,
            default_value: None,
            span: start..self.lexer.span().end,
        })
    }

    fn parse_param_list(&mut self) -> Vec<Param> {
        let mut params = Vec::new();
        if !self.expect(Token::LParen, "Expected '(' at start of parameter list") {
            return params;
        }
        while !self.check(Token::RParen) {
            match self.parse_param() {
                Ok(p) => {
                    params.push(p);
                }
                Err(_) => {
                    self.synchronize_until(&[Token::Comma, Token::RParen]);
                }
            };
            if self.check(Token::Comma) {
                self.advance();
            } else {
                break;
            }
        }
        self.expect(Token::RParen, "Expected ')' at end of parameter list");
        params
    }

    // AttributeArg = [ Identifier, "=" ], ( Literal | Identifier ) ;
    fn parse_argument(&mut self) -> Result<Argument, ()> {
        if let Some(Token::Identifier) = self.current_token {
            if let Some(Token::Assign) = self.peek() {
                // Named argument
                let name = self.parse_identifier();
                if !self.expect(Token::Assign, "Expected '=' in named argument") {
                    return Err(());
                }
                if self.check(Token::Attribute) {
                    let attr = self.parse_attribute();
                    return Ok(Argument::Attribute(attr));
                }
                let value = self.parse_expr(0);
                return Ok(Argument::Named { name, value });
            }
        }

        if self.check(Token::Attribute) {
            let attr = self.parse_attribute();
            return Ok(Argument::Attribute(attr));
        }
        let expr = self.parse_expr(0);
        if let Expr::Error { .. } = expr {
            return Err(());
        }
        Ok(Argument::Positional(expr))
    }

    //
    fn parse_argument_list(&mut self) -> Vec<Argument> {
        let mut args = Vec::new();
        if !self.expect(Token::LParen, "Expected '(' at begin of argument list") {
            return args;
        }
        while !self.check(Token::RParen) {
            match self.parse_argument() {
                Ok(a) => args.push(a),
                Err(_) => {
                    self.synchronize_until(&[Token::Comma, Token::RParen]);
                }
            };
            if self.check(Token::Comma) {
                self.advance();
            } else {
                break;
            }
        }
        self.expect(Token::RParen, "Expected ')' at end of argument list");
        args
    }

    fn parse_attribute(&mut self) -> Attribute {
        if !self.check(Token::Attribute) {
            self.report_error("Expected attribute");
            return Attribute {
                name: ERROR_IDENT_NAME.to_string(),
                args: Vec::new(),
            };
        }
        // "@attr" or "@attr(args...)"
        // consume '@'
        let name = self.current_slice[1..].to_string();
        self.advance();
        // 解析参数 (args...)
        if self.check(Token::LParen) {
            let args = self.parse_argument_list();
            Attribute { name, args }
        } else {
            Attribute {
                name,
                args: Vec::new(),
            }
        }
    }

    // --- 核心：属性解析器 (扩展性的基石) ---
    //
    //Attribute = AttrTag, [ "(", ArgList, ")" ] ;

    // (* 属性参数列表，比较宽容，支持 key=value 或直接 value *)
    // ArgList   = AttributeArg, { ",", AttributeArg } ;
    // AttributeArg = [ Identifier, "=" ], ( Literal | Identifier ) ;

    // 这是一个高阶解析逻辑：先解析所有属性，再解析具体声明
    fn parse_with_attributes<F, T>(&mut self, parse_fn: F) -> T
    where
        F: Fn(&mut Self, Vec<Attribute>) -> T,
    {
        let mut attributes = Vec::new();

        // 只要看到 @，就一直解析属性
        while self.check(Token::Attribute) {
            let attr = self.parse_attribute();
            attributes.push(attr);
        }
        parse_fn(self, attributes)
    }

    fn parse_dim(&mut self) -> Vec<DimDecl> {
        // 'dim' DimName [':' TypeExpr ] [ '=' Expr ] ;
        let start_span = self.lexer.span().start;
        self.expect(Token::KwDim, "Expected 'dim'");

        let mut decls = Vec::new();

        loop {
            let inner_start = self.lexer.span().start;
            let name = self.parse_identifier();

            let mut bound = None;
            let mut value = None;

            // parse bound
            if self.check(Token::Colon) {
                self.advance();
                bound = Some(self.parse_expr(0));
            }

            // assign value
            if self.check(Token::Assign) {
                self.advance();
                value = Some(self.parse_expr(0));
            }

            decls.push(DimDecl {
                name,
                bound,
                value,
                span: inner_start..self.lexer.span().end,
            });

            if self.check(Token::Comma) {
                self.advance();
            } else {
                break;
            }
        }

        self.expect(Token::SemiColon, "Expected ';' after dim declaration");
        decls
    }

    fn parse_stmt(&mut self) -> Stmt {
        self.parse_with_attributes(|p, attrs| {
            match p.current_token {
                Some(Token::KwFunc) => p.parse_func(attrs),
                Some(Token::KwStruct) => p.parse_struct(attrs),
                Some(Token::KwImport) => p.parse_import(),
                Some(Token::KwDim) => Stmt::DimDecl(p.parse_dim()),
                Some(Token::KwLet) => {
                    // 'let' var_name [: TypeExpr ] [= Expr ];
                    p.advance();
                    let name = p.parse_identifier();
                    let ty = if p.check(Token::Colon) {
                        p.advance();
                        Some(p.parse_type_expr())
                    } else {
                        None
                    };
                    let init = if p.check(Token::Assign) {
                        p.advance();
                        Some(p.parse_expr(0))
                    } else {
                        None
                    };
                    let var = Stmt::VarDecl(VarDecl {
                        name,
                        ty,
                        init,
                        span: p.lexer.span(),
                    });
                    if !p.expect(Token::SemiColon, "Expected ';' at end of let statement") {
                        p.synchronize();
                        Stmt::Error {
                            span: p.lexer.span(),
                        }
                    } else {
                        var
                    }
                }
                Some(Token::KwBreak) => {
                    let span = p.lexer.span();
                    p.advance();
                    let stmt = Stmt::Break(span);
                    if !p.expect(Token::SemiColon, "Expected ';' at end of break statement") {
                        p.synchronize();
                        Stmt::Error {
                            span: p.lexer.span(),
                        }
                    } else {
                        stmt
                    }
                }
                Some(Token::KwContinue) => {
                    let span = p.lexer.span();
                    p.advance();
                    let stmt = Stmt::Continue(span);
                    if !p.expect(
                        Token::SemiColon,
                        "Expected ';' at end of continue statement",
                    ) {
                        p.synchronize();
                        Stmt::Error {
                            span: p.lexer.span(),
                        }
                    } else {
                        stmt
                    }
                }
                Some(Token::KwFor) => p.parse_for(attrs),
                Some(Token::KwIf) => p.parse_if(attrs),
                Some(Token::KwReturn) => {
                    p.advance();
                    let expr = p.parse_expr(0); // 0 是最低优先级
                    let stmt = Stmt::Return(expr);
                    if !p.expect(Token::SemiColon, "Expected ';' at end of return statement") {
                        p.synchronize();
                        Stmt::Error {
                            span: p.lexer.span(),
                        }
                    } else {
                        stmt
                    }
                }
                _ => {
                    let expr = p.parse_expr(0);
                    let stmt = if p.check(Token::Assign) {
                        p.advance();
                        let value = p.parse_expr(0);
                        Stmt::Assignment {
                            target: expr,
                            value,
                            span: p.lexer.span(),
                        }
                    } else {
                        Stmt::Expr(expr)
                    };
                    if !p.expect(
                        Token::SemiColon,
                        "Expected ';' at end of expression statement",
                    ) {
                        p.synchronize();
                        Stmt::Error {
                            span: p.lexer.span(),
                        }
                    } else {
                        stmt
                    }
                }
            }
        })
    }

    fn parse_identifier(&mut self) -> Ident {
        if let Some(Token::Identifier) = self.current_token.clone() {
            let name = self.current_slice.to_string();
            let span = self.lexer.span();
            self.advance();
            let name = self.interner.get_or_intern(name);
            Ident { id: name, span }
        } else {
            self.report_error(&format!(
                "Expected identifier, got {:?}",
                self.current_token
            ));
            Ident {
                id: self.interner.get_or_intern(ERROR_IDENT_NAME), // TODO:: optimize this
                // later
                span: self.lexer.span(),
            }
        }
    }
    fn parse_path(&mut self) -> Path {
        let start = self.lexer.span().start;
        let mut segments = Vec::new();
        loop {
            let ident = self.parse_identifier();
            segments.push(ident);
            if self.check(Token::DoubleColon) {
                self.advance(); // consume '::'
            } else {
                break;
            }
        }
        let end = self.lexer.span().end;
        Path {
            segments,
            span: start..end,
        }
    }

    fn parse_import(&mut self) -> Stmt {
        if !self.expect(Token::KwImport, "Expected import") {
            return Stmt::Error {
                span: self.lexer.span(),
            };
        }
        let path = self.parse_path();
        let stmt = Stmt::Import(path);
        if !self.expect(Token::SemiColon, "Expected ';' at end of import statement") {
            self.synchronize();
            Stmt::Error {
                span: self.lexer.span(),
            }
        } else {
            stmt
        }
    }

    ///  GenericParam = Identifier, [ ":", TypeExpr ], [ "=", TypeExpr ] ;
    fn parse_generic_param(&mut self) -> GenericParam {
        let ident = self.parse_identifier();
        if !self.check(Token::Colon) && !self.check(Token::Assign) {
            return GenericParam {
                name: ident,
                bound: None,
                default_type: None,
                span: self.lexer.span(),
            };
        }
        match self.current_token {
            Some(Token::Colon) => {
                self.advance(); // consume ':'
                let bound = self.parse_type_expr();
                GenericParam {
                    name: ident,
                    bound: Some(bound),
                    default_type: None,
                    span: self.lexer.span(),
                }
            }
            Some(Token::Assign) => {
                self.advance(); // consume '='
                let default_type = self.parse_type_expr();
                GenericParam {
                    name: ident,
                    bound: None,
                    default_type: Some(default_type),
                    span: self.lexer.span(),
                }
            }
            _ => {
                self.report_error("Expected ':' or '=' in generic parameter");
                GenericParam {
                    name: ident,
                    bound: None,
                    default_type: None,
                    span: self.lexer.span(),
                }
            }
        }
    }

    fn parse_generic_param_list(&mut self) -> Vec<GenericParam> {
        let mut generics = Vec::new();
        if !self.expect(Token::Lt, "Expected '<' at start of generic parameters") {
            return generics;
        }
        while !self.check(Token::Gt) {
            let generic = self.parse_generic_param();
            generics.push(generic);
            if self.check(Token::Comma) {
                self.advance(); // consume ','
            } else {
                break;
            }
        }
        self.expect(Token::Gt, "Expected '>' after generic parameters");
        generics
    }

    //  <f32, i32[N], ...>
    fn parse_generic_argument_list(&mut self) -> Vec<TypeExpr> {
        let mut args = Vec::new();
        if !self.expect(Token::Lt, "Expected '<' at start of generic arguments") {
            return args;
        }
        while !self.check(Token::Gt) {
            match self.parse_type_expr() {
                TypeExpr::Error { .. } => {
                    self.synchronize_until(&[Token::Comma, Token::Gt]); // ,|>
                }
                ty => args.push(ty),
            }
            if self.check(Token::Comma) {
                self.advance(); // consume ','
            }
        }
        self.expect(Token::Gt, "Expected '>' after generic arguments");
        args
    }

    // [dim1, dim2, ...]
    fn parse_shape_expr(&mut self) -> Vec<Expr> {
        let mut shape = Vec::new();
        if !self.expect(Token::LBracket, "Expected '[' at start of shape expression") {
            return shape;
        }
        while !self.check(Token::RBracket) {
            match self.parse_expr(0) {
                Expr::Error { .. } => {
                    self.synchronize_until(&[Token::Comma, Token::RBracket]);
                }
                dim_expr => shape.push(dim_expr),
            }
            if self.check(Token::Comma) {
                self.advance(); // consume ','
            }
        }
        self.expect(Token::RBracket, "Expected ']' at end of shape expression");
        shape
    }

    // ShapeExpr := '[', TypeExpr+, ']'
    // GenericArgs := '<', TypeExpr+, '>'
    // UserDefinedType := Ident [GenericArgs]
    // TypeExpr := (UserDefinedType [ GenericArgs] [ ShapeExpr ]) | PrimitiveType [ ShapeExpr ]
    fn parse_type_expr(&mut self) -> TypeExpr {
        // base type  Primitive | Ident [<GenericArgs>]
        let mut ty = if let Some(token) = self.current_token.as_ref() {
            if let Some(prim_type) = PrimitiveType::from_token(token) {
                self.advance();
                TypeExpr::Primitive(prim_type)
            } else {
                match token {
                    Token::Identifier => {
                        // not turbo-fish style in decl context
                        let name = self.parse_identifier();
                        let generics = if self.check(Token::Lt) {
                            self.parse_generic_argument_list()
                        } else {
                            Vec::new()
                        };
                        TypeExpr::Named { name, generics }
                    }
                    _ => {
                        self.report_error("Expected type expression");
                        TypeExpr::Error {
                            span: self.lexer.span(),
                        }
                    }
                }
            }
        } else {
            self.report_error("Invalid token");
            TypeExpr::Error {
                span: self.lexer.span(),
            }
        };

        // parse shape if any like [f32, 128, 128]
        if self.check(Token::LBracket) {
            let shape = self.parse_shape_expr();
            ty = TypeExpr::Tensor {
                base: Box::new(ty),
                shape,
            };
        }
        ty
    }

    // 'for', var, 'in', ident .. ident [step expr] { ... }
    fn parse_for(&mut self, attributes: Vec<Attribute>) -> Stmt {
        if !self.expect(Token::KwFor, "Expected 'for'") {
            self.synchronize();
        }
        let var = self.parse_identifier();
        if !self.expect(Token::KwIn, "Expected 'in' after for variable") {
            self.synchronize();
        }
        let range_start_span = self.lexer.span().start;
        let start_expr = self.parse_expr(0);
        if !self.expect(Token::DoubleDot, "Expected '..' in for range") {
            self.synchronize();
        }
        let end_expr = self.parse_expr(0);
        let range_end_span = self.lexer.span().end;
        let step_expr = if self.check(Token::KwStep) {
            self.advance();
            Some(self.parse_expr(0))
        } else {
            None
        };
        let body = self.parse_block();
        Stmt::For {
            attributes,
            var,
            range: Range {
                start: start_expr,
                end: end_expr,
                step: step_expr,
                span: range_start_span..range_end_span,
            },
            body,
            span: self.lexer.span(),
        }
    }

    // 'if' condition block [ 'else' ( if | block ) ]
    fn parse_if(&mut self, attributes: Vec<Attribute>) -> Stmt {
        if !self.expect(Token::KwIf, "Expected 'if'") {
            self.synchronize();
        }
        let condition = self.parse_expr(0);
        let then_branch = self.parse_block();
        let else_branch = if self.check(Token::KwElse) {
            self.advance();
            if self.check(Token::KwIf) {
                // else if
                Some(ElseBranch::If(Box::new(self.parse_if(Vec::new()))))
            } else {
                // else
                Some(ElseBranch::Block(self.parse_block()))
            }
        } else {
            None
        };
        Stmt::If {
            attributes,
            condtion: condition,
            then_branch,
            else_branch,
        }
    }

    fn parse_block(&mut self) -> Block {
        let start = self.lexer.span().start;
        let mut stmts = Vec::new();
        if !self.expect(Token::LBrace, "Expected '{' at beginning of block") {
            self.synchronize();
            return Block {
                stmts,
                span: start..self.lexer.span().end,
            };
        }
        while !self.check(Token::RBrace) {
            stmts.push(self.parse_stmt());
        }
        self.expect(Token::RBrace, "Expected '}' at end of block");
        Block {
            stmts,
            span: start..self.lexer.span().end,
        }
    }

    // func name[<generic_list>](params) [-> ReturnType]
    fn parse_func_proto(&mut self) -> FuncProto {
        let name = self.parse_identifier();
        let generics = if self.check(Token::Lt) {
            self.parse_generic_param_list()
        } else {
            Vec::new()
        };
        let params = self.parse_param_list();
        let return_type = if self.check(Token::Arrow) {
            self.advance();
            Some(self.parse_type_expr())
        } else {
            None
        };

        FuncProto {
            name,
            generics,
            params,
            return_type,
            span: self.lexer.span(),
        }
    }

    //
    //(* 示例: @kernel
    //         func forward(...) -> ... { ... } *)
    //FuncDecl= { Attribute }, "func", Identifier,["<", GenericParamList, ">"],
    //            "(", [ ParamList ], ")",
    //           [ "->", TypeExpr ],
    //          Block ;
    //ParamList  = Param, { ",", Param } ;
    //Param      = Identifier, ":", TypeExpr ;

    fn parse_func(&mut self, attributes: Vec<Attribute>) -> Stmt {
        if !self.expect(Token::KwFunc, "Expected func") {
            self.synchronize();
        }
        let proto = self.parse_func_proto();
        let body = self.parse_block();
        Stmt::Function(FuncDecl {
            attributes,
            proto,
            body: Some(body),
        })
    }

    // FieldDecl = Identifier, ":", TypeExpr ;
    fn parse_field_decl(&mut self) -> FieldDecl {
        let start = self.lexer.span().start;
        let name = self.parse_identifier();
        if !self.expect(Token::Colon, "Expected ':' after field name") {
            self.synchronize();
        }
        let ty = self.parse_type_expr();
        FieldDecl {
            name,
            ty,
            span: start..self.lexer.span().end,
        }
    }

    fn parse_field_list(&mut self) -> Vec<FieldDecl> {
        let mut fields = Vec::new();
        if !self.expect(Token::LBrace, "Expected '{' at start of struct body") {
            self.synchronize();
        }
        while !self.check(Token::RBrace) {
            let field = self.parse_field_decl();
            fields.push(field);
            if self.check(Token::Comma) {
                self.advance();
            }
        }
        self.expect(Token::RBrace, "Expected '}' at end of struct body");
        fields
    }

    // struct, Ident ['<', generics, '>' ] '{' FieldList '}'
    fn parse_struct(&mut self, attributes: Vec<Attribute>) -> Stmt {
        if !self.expect(Token::KwStruct, "Expected struct") {
            self.synchronize();
        }
        let name = self.parse_identifier();
        let generics = if self.check(Token::Lt) {
            self.parse_generic_param_list()
        } else {
            Vec::new()
        };
        let fields = self.parse_field_list();
        Stmt::Struct(StructDecl {
            attributes,
            name,
            generics,
            fields,
        })
    }

    // --- 表达式解析 (Pratt Parser 核心) ---
    fn infix_binding_power(&self, op: &BinaryOp) -> (u8, u8) {
        match op {
            BinaryOp::Plus | BinaryOp::Minus => (3, 4),
            BinaryOp::Star | BinaryOp::Slash => (5, 6),
        }
    }

    // binding_power: 当前操作符的紧密度
    fn parse_expr(&mut self, min_bp: u8) -> Expr {
        // 1. Prefix (前缀) 处理：字面量, 变量, (, [
        let mut left = match self.current_token {
            Some(Token::IntegerLiteral) => {
                let val = self.current_slice.parse().unwrap();
                self.advance();
                Expr::Literal(Literal::Int(val))
            }
            Some(Token::FloatLiteral) => {
                let val = self.current_slice.parse().unwrap();
                self.advance();
                Expr::Literal(Literal::Float(val))
            }
            Some(Token::Identifier) => {
                let ident = self.parse_identifier();
                Expr::Variable(ident)
            }
            Some(Token::StringLiteral) => {
                let val = self.current_slice.to_string();
                self.advance();
                Expr::Literal(Literal::String(val))
            }
            Some(Token::LBracket) => self.parse_array_literal(),
            Some(Token::LParen) => {
                self.advance(); // consume '('
                let expr = self.parse_expr(0);
                self.expect(Token::RParen, "Expected ')' after expression");
                expr
            }
            _ => {
                self.report_error(&format!(
                    "Unexpected token in expression: {:?}",
                    self.current_token
                ));
                return Expr::Error(self.lexer.span());
            }
        };

        // 2. Infix / Postfix (中缀/后缀) 处理：+, *, ., [, (
        loop {
            let op = match self.current_token {
                Some(Token::Plus) => BinaryOp::Plus,
                Some(Token::Star) => BinaryOp::Star,
                Some(Token::Minus) => BinaryOp::Minus,
                Some(Token::Slash) => BinaryOp::Slash,
                Some(Token::Dot) => {
                    // 成员/Swizzle 访问
                    self.advance();
                    let member = self.parse_identifier();
                    left = Expr::MemberAccess {
                        target: Box::new(left),
                        member,
                    };
                    continue;
                }
                Some(Token::DoubleColon) => {
                    // turbo-fish style generic call: function::<T1, T2>(args...)
                    let (l_bp, _) = (9, 10);
                    if l_bp < min_bp {
                        break;
                    }
                    self.advance(); // consume '::'

                    // foo::
                    if self.check(Token::Lt) {
                        // '<' begin, turbo-fish generic
                        // generic parm list
                        let generics = self.parse_generic_argument_list();
                        if self.check(Token::LParen) {
                            let args = self.parse_argument_list();
                            left = Expr::Call {
                                func: Box::new(left),
                                generics,
                                args,
                            };
                        } else {
                            // TODO:: maybe foo::<T1, T2> is also valid grammar?
                            // for example, it refer to a function pointer or so.
                            // But for simplicity, we don't support it now.
                            self.report_error("Expected '(' after generic arguments");
                            return Expr::Error(self.lexer.span());
                        }
                        continue;
                    } else {
                        // namespace access like foo::current
                        let ident = self.parse_identifier();
                        left = Expr::NamespaceAccess {
                            namespace: Box::new(left),
                            member: ident,
                        };
                        continue;
                    }
                }
                Some(Token::LParen) => {
                    // func call: function(args...)
                    //
                    let (l_bp, _) = (7, 8);

                    if l_bp < min_bp {
                        break;
                    }

                    let args = self.parse_argument_list();
                    left = Expr::Call {
                        func: Box::new(left),
                        generics: Vec::new(),
                        args,
                    };
                    continue;
                }
                Some(Token::LBracket) => {
                    // 索引访问/Einsum: variable[i, j]
                    // 注意：这和上面的 parse_array 不同，这里 [ 是跟在表达式后面的
                    left = self.parse_index_access(left);
                    continue;
                }
                // Some(Token::Assign) => BinaryOp::Assign, // 赋值作为表达式
                _ => break, // 遇到不能处理的 Token，停止
            };

            // 获取操作符的优先级 (Binding Power)
            let (l_bp, r_bp) = self.infix_binding_power(&op);

            // 如果新操作符优先级低于当前上下文，则停止，先处理当前的
            if l_bp < min_bp {
                break;
            }

            // 消费操作符
            if !matches!(self.current_token, Some(Token::Dot) | Some(Token::LBracket)) {
                self.advance();
            }

            // 递归解析右侧
            let right = self.parse_expr(r_bp);
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
            };
        }

        left
    }

    // parse [1 2 3] or [1, 2, 3]
    fn parse_array_literal(&mut self) -> Expr {
        self.expect(Token::LBracket, "Expected '[' at start of array literal");
        let mut elements = Vec::new();
        while !self.check(Token::RBracket) {
            elements.push(self.parse_expr(0));
            if self.check(Token::Comma) {
                self.advance();
            }
        }
        self.expect(Token::RBracket, "Expected ']' at end of array literal");
        Expr::VectorLiteral(elements)
    }

    fn parse_index_access(&mut self, target: Expr) -> Expr {
        self.expect(Token::LBracket, "Expected '[' for index access");
        let mut indices = Vec::new();
        while !self.check(Token::RBracket) {
            indices.push(self.parse_expr(0));
            if self.check(Token::Comma) {
                self.advance();
            }
        }
        self.consume(Token::RBracket, "Expected ']' at end of index access");
        Expr::Index {
            target: Box::new(target),
            indices,
        }
    }
}
