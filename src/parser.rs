// src/parser.rs
use crate::ast::*;
use crate::lexer::Token;
use logos::{Lexer, Logos};

pub struct Parser<'source> {
    lexer: Lexer<'source, Token>,
    current_token: Option<Token>,
    current_slice: &'source str,
}

impl<'source> Parser<'source> {
    pub fn new(source: &'source str) -> Self {
        let mut lexer = Token::lexer(source);
        let current_token = lexer.next().and_then(|r| r.ok());
        let current_slice = lexer.slice();

        Self {
            lexer,
            current_token,
            current_slice,
        }
    }

    // --- 基础工具 ---

    fn advance(&mut self) {
        self.current_token = self.lexer.next().and_then(|r| r.ok());
        self.current_slice = self.lexer.slice();
    }

    fn check(&self, token: Token) -> bool {
        self.current_token == Some(token)
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
            // Attribute Token 的 slice 包含了 @，比如 "@kernel"
            // 去掉 @ 符号
            let name = self.current_slice[1..].to_string();
            self.advance();

            // 解析参数 @attr(args...)
            let mut args = Vec::<Expr>::new();

            if self.check(Token::LParen) {
                self.advance(); // 消费 '('

                while !self.check(Token::RParen) {
                    // 解析单个参数
                    // 支持 key=value 或直接 value
                    if let Some(Token::Identifier) = self.current_token {
                        let key_or_value = self.parse_identifier();
                        self.advance();

                        if self.check(Token::Assign) {
                            // key=value 形式
                            self.advance(); // 消费 '='
                            // parse as expr
                            let value = self.parse_expr(0);
                            args.push(value);
                        } else {
                            // 直接 value 形式，作为变量处理
                            args.push(Expr::Variable(key_or_value));
                        }
                    } else {
                        panic!(
                            "Unexpected token in attribute argument: {:?} at {}",
                            self.current_token, self.current_slice
                        );
                    }

                    // 如果有逗号，消费它
                    if self.check(Token::Comma) {
                        self.advance();
                    }
                }

                self.consume(Token::RParen, "Expected ')' after attribute arguments");
            }

            attributes.push(Attribute { name, args });
        }

        // 属性解析完后，调用传入的解析函数
        parse_fn(self, attributes)
    }

    // --- 声明解析 ---

    pub fn parse(&mut self) -> Vec<Stmt> {
        let mut stmts = Vec::new();
        while self.current_token.is_some() {
            stmts.push(self.parse_stmt());
        }
        stmts
    }

    fn parse_stmt(&mut self) -> Stmt {
        // 先检查是否是带有属性的结构
        if self.check(Token::Attribute) || self.check(Token::KwFunc) || self.check(Token::KwStruct)
        {
            return self.parse_decl();
        }

        match self.current_token {
            Some(Token::KwImport) => self.parse_import(),
            Some(Token::KwReturn) => {
                self.advance();
                let expr = self.parse_expr(0); // 0 是最低优先级
                Stmt::Return(expr)
            }
            _ => {
                let expr = self.parse_expr(0);
                Stmt::Expr(expr)
            }
        }
    }

    fn parse_identifier(&mut self) -> Ident {
        if let Some(Token::Identifier) = self.current_token.clone() {
            let name = self.current_slice.to_string();
            let span = self.lexer.span();
            self.advance();
            Ident { name, span }
        } else {
            panic!(
                "Syntax Error: expected identifier, got {:?}",
                self.current_token
            );
        }
    }

    fn parse_import(&mut self) -> Stmt {
        self.consume(Token::KwImport, "Expected import");
        // 简化：假设 import 后面直接跟标识符
        let path = self.current_slice.to_string();
        self.consume(Token::Identifier, "Expected identifier");
        Stmt::Import(path)
    }

    fn parse_decl(&mut self) -> Stmt {
        // 利用 parse_with_attributes 统一处理 @kernel, @swizzle_set
        self.parse_with_attributes(|p, attrs| {
            if p.check(Token::KwFunc) {
                p.parse_func(attrs)
            } else if p.check(Token::KwStruct) {
                p.parse_struct(attrs)
            } else {
                panic!("Attributes must precede func or struct");
            }
        })
    }

    fn parse_generic_param(&mut self) -> Vec<GenericParam> {
        let mut generics = Vec::new();
        if self.check(Token::Lt) {
            self.advance(); // consume '<'
            while !self.check(Token::Gt) {
                let name = self.parse_identifier();

                // 如果有约束 (简化起见，这里不处理复杂的约束)
                generics.push(GenericParam { name });

                if self.check(Token::Comma) {
                    self.advance(); // consume ','
                }
            }
            self.consume(Token::Gt, "Expected '>' after generic parameters");
        }
        generics
    }

    fn parse_shape_expr(&mut self) -> Vec<Expr> {
        let mut shape = Vec::new();
        if self.check(Token::LBracket) {
            self.advance(); // consume '['
            while !self.check(Token::RBracket) {
                let dim_expr = self.parse_expr(0);
                shape.push(dim_expr);

                if self.check(Token::Comma) {
                    self.advance(); // consume ','
                }
            }
            self.consume(Token::RBracket, "Expected ']' after shape expression");
        }
        shape
    }

    // 解析比如 <f32, Tensor[N]>
    fn parse_generic_args(&mut self) -> Vec<TypeExpr> {
        let mut args = Vec::new();
        if self.check(Token::Lt) {
            self.advance(); // consume '<'
            while !self.check(Token::Gt) {
                let ty = self.parse_type_expr();
                args.push(ty);

                if self.check(Token::Comma) {
                    self.advance(); // consume ','
                }
            }
            self.consume(Token::Gt, "Expected '>' after generic arguments");
        }
        args
    }

    fn parse_type_expr(&mut self) -> TypeExpr {
        // base type Ident | Primitive
        let mut base_type = match self.current_token {
            Some(Token::TypeF32) => {
                self.advance();
                TypeExpr::Primitive(PrimitiveType::F32)
            }
            Some(Token::TypeI32) => {
                self.advance();
                TypeExpr::Primitive(PrimitiveType::I32)
            }
            Some(Token::TypeBool) => {
                self.advance();
                TypeExpr::Primitive(PrimitiveType::Bool)
            }
            Some(Token::Identifier) => {
                // 在类型表达式中，如果有泛型参数，不是用 turbo-fish 语法，只用尖括号包裹类型参数
                let name = self.parse_identifier(); // 泛型是递归的类型表达式
                let generics = self.parse_generic_args();
                TypeExpr::Named { name, generics }
            }
            _ => panic!("Unexpected token in type expression"),
        };

        // tensor type  Ident[ExprType, ...]
        if self.check(Token::LBracket) {
            self.advance(); // consume '['
            let mut shape = Vec::new();
            while !self.check(Token::RBracket) {
                let dim_expr = self.parse_expr(0);
                shape.push(dim_expr);

                if self.check(Token::Comma) {
                    self.advance(); // consume ','
                }
            }
            self.consume(Token::RBracket, "Expected ']' after tensor dimensions");

            base_type = TypeExpr::Tensor {
                base: Box::new(base_type),
                shape,
            };
        }
        base_type
    }

    //
    fn parse_func_proto(&mut self) -> FuncProto {
        let name = self.parse_identifier();
        let generics = self.parse_generic_param();

        // 参数列表 (略)
        self.consume(Token::LParen, "(");
        let mut params = Vec::new();
        while !self.check(Token::RParen) {
            let param_name = self.parse_identifier();
            self.consume(Token::Colon, ":");
            let param_type = self.parse_type_expr();
            params.push(Param {
                name: param_name,
                ty: param_type,
            });

            if self.check(Token::Comma) {
                self.advance();
            }
        }
        self.consume(Token::RParen, ")");

        // 返回类型 -> (略)
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
        self.consume(Token::KwFunc, "Expected func");
        let proto = self.parse_func_proto();

        // 参数列表 (略)
        self.consume(Token::LParen, "(");
        // ... parse args ...
        // 跳过直到 )
        while !self.check(Token::RParen) {
            self.advance();
        }
        self.consume(Token::RParen, ")");

        // 返回类型 -> (略)
        if self.check(Token::Arrow) {
            self.advance();
            self.advance(); // 跳过类型
        }

        self.consume(Token::LBrace, "{");
        let mut body = Vec::new();
        while !self.check(Token::RBrace) {
            body.push(self.parse_stmt());
        }
        self.consume(Token::RBrace, "}");

        Stmt::Function(FuncDecl {
            attributes,
            proto: proto,
            body: Block { stmts: body },
        })
    }

    fn parse_struct(&mut self, attributes: Vec<Attribute>) -> Stmt {
        self.consume(Token::KwStruct, "Expected struct");
        let name = self.parse_identifier();
        // Body 略
        self.consume(Token::LBrace, "{");
        while !self.check(Token::RBrace) {
            self.advance();
        }
        self.consume(Token::RBrace, "}");
        Stmt::Struct(StructDecl { attributes, name })
    }

    // --- 表达式解析 (Pratt Parser 核心) ---
    // binding_power: 当前操作符的紧密度
    fn parse_expr(&mut self, min_bp: u8) -> Expr {
        // 1. Prefix (前缀) 处理：字面量, 变量, (, [
        let mut left = match self.current_token.clone() {
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
            Some(Token::Identifier) => Expr::Variable(self.parse_identifier()),
            Some(Token::StringLiteral) => {
                let val = self.current_slice.to_string();
                self.advance();
                Expr::Literal(Literal::String(val))
            }
            Some(Token::LBracket) => self.parse_array_or_tensor(),
            _ => panic!("Unexpected token in expression: {:?}", self.current_token),
        };

        // 2. Infix / Postfix (中缀/后缀) 处理：+, *, ., [
        loop {
            let op = match self.current_token {
                Some(Token::Plus) => BinaryOp::Plus,
                Some(Token::Star) => BinaryOp::Star,
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
                Some(Token::LBracket) => {
                    // 索引访问/Einsum: variable[i, j]
                    // 注意：这和上面的 parse_array 不同，这里 [ 是跟在表达式后面的
                    left = self.parse_index_access(left);
                    continue;
                }
                Some(Token::Assign) => BinaryOp::Assign, // 赋值作为表达式
                _ => break,                              // 遇到不能处理的 Token，停止
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

    // 处理 [1 2 3] 或 [1, 2, 3]
    fn parse_array_or_tensor(&mut self) -> Expr {
        self.consume(Token::LBracket, "[");
        let mut elements = Vec::new();

        while !self.check(Token::RBracket) {
            elements.push(self.parse_expr(0));

            // 核心逻辑：Julia 风格空格支持
            // 如果下一个是逗号，吃掉它
            if self.check(Token::Comma) {
                self.advance();
            }
            // 如果不是逗号也不是 ]，说明是空格分隔（Lexer 已经吃掉了空格）
            // 这里不需要做任何事，直接 loop continue 解析下一个 expr 即可！
            // 这就是手写 Parser 的魅力：隐式逻辑显式化。
        }

        self.consume(Token::RBracket, "]");
        Expr::VectorLiteral(elements)
    }

    fn parse_index_access(&mut self, target: Expr) -> Expr {
        self.consume(Token::LBracket, "[");
        let mut indices = Vec::new();
        while !self.check(Token::RBracket) {
            indices.push(self.parse_expr(0));
            if self.check(Token::Comma) {
                self.advance();
            }
        }
        self.consume(Token::RBracket, "]");
        Expr::Index {
            target: Box::new(target),
            indices,
        }
    }

    fn infix_binding_power(&self, op: &BinaryOp) -> (u8, u8) {
        match op {
            BinaryOp::Assign => (2, 1), // 右结合
            BinaryOp::Plus | BinaryOp::Minus => (3, 4),
            BinaryOp::Star | BinaryOp::Slash => (5, 6),
            _ => (0, 0),
        }
    }
}
