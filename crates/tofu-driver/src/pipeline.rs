use crate::attr::{registry::*, traits::*};
use crate::context::Context;
use syntax::ast::*;
use tofu_core::diagnostic::*;
pub struct Pipeline<'a> {
    pub registry: &'a AttributeRegistry,
}

impl<'a> Pipeline<'a> {
    pub fn new(registry: &'a AttributeRegistry) -> Self {
        Pipeline { registry }
    }
}

impl<'b> AttrEngine for Pipeline<'b> {
    fn apply<'a>(
        &self,
        ctx: &Context,
        node: Stmt,
        stage: CompileStage,
        env: &'a AttrEnvironment<'a>,
    ) -> Result<AttrAction, Vec<Diagnostic>> {
        // apply attributes in reverse order

        // for example
        //
        // @pass1
        // @pass2
        // @pass3
        // fn foo() {}
        //
        // will be applied in order of pass3 -> pass2 -> pass1 and
        // then apply the attribute from parent scope if any
        // so that inner attributes can override outer ones, inner ones has higher priority
        let attrs = node
            .get_attributes()
            .map_or_else(Vec::new, |a| a.clone().into_iter().rev().collect());

        let mut delta_meta = AttrMetadata::new();
        let mut local_attrs = Vec::<(&Attribute, bool)>::new();

        for attr in &attrs {
            let handlers = self.registry.get_handlers(&attr.name, stage);
            for h in &handlers {
                h.evaluate(ctx, attr, &mut delta_meta);
                if h.is_recursive() {
                    local_attrs.push((attr, true)); // visible to child nodes
                } else {
                    local_attrs.push((attr, false));
                }
            }
        }

        let current_env = AttrEnvironment {
            parent: Some(env),
            local_attrs: &local_attrs,
            delta_meta,
        };
        //let mut current_node = node;

        current_env.for_active_attrs(node, |env, attr, mut input| {
            let handlers = self.registry.get_handlers(&attr.name, stage);
            let mut skip_child = false;
            for handler in handlers {
                input = match handler.transform(ctx, self, attr, input, &env.delta_meta)? {
                    AttrAction::Continue(n) => n,
                    AttrAction::SkipChildren(n) => {
                        skip_child = true;
                        n
                    }
                    AttrAction::Terminal(n) => return Ok(AttrAction::Terminal(n)),
                    AttrAction::Lowered(n) => return Ok(AttrAction::Lowered(n)),
                };
            }
            if skip_child {
                Ok(AttrAction::SkipChildren(input))
            } else {
                Ok(AttrAction::Continue(input))
            }
        })
        // action
        // TODO::
        // only apply attributes for top level nodes now
        // such as functions, structs, enums, etc.
    }
}
