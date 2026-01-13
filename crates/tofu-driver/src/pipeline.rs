use crate::attr::{registry::*, traits::*};
use crate::context::Context;
use core::diagnostic::*;
use syntax::ast::*;
pub struct Pipeline {
    pub registry: AttributeRegistry,
}

impl Pipeline {
    pub fn new(registry: AttributeRegistry) -> Self {
        Pipeline { registry }
    }
}

impl AttrEngine for Pipeline {
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
        let mut should_recurse = true;

        let action = current_env.for_active_attrs(node, |env, attr, mut input| {
            let handlers = self.registry.get_handlers(&attr.name, stage);
            for handler in handlers {
                input = match handler.transform(ctx, self, attr, input, &env.delta_meta)? {
                    AttrAction::Continue(n) => n,
                    AttrAction::SkipChildren(n) => return Ok(AttrAction::SkipChildren(n)),
                    AttrAction::Terminal(n) => return Ok(AttrAction::Terminal(n)),
                    AttrAction::Lowered(n) => return Ok(AttrAction::Lowered(n)),
                };
            }
            Ok(AttrAction::Continue(input))
        });

        // for attr in current_env.local_attrs {
        //     let handlers = self.registry.get_handlers(&attr.name, stage);
        //     for handler in handlers {
        //         let action =
        //             handler.transform(ctx, self, attr, current_node, &current_env.delta_meta)?;
        //
        //         match action {
        //             AttrAction::Continue(new_node) => {
        //                 current_node = new_node;
        //             }
        //             AttrAction::SkipChildren(new_node) => {
        //                 current_node = new_node;
        //                 should_recurse = false;
        //             }
        //             _ => {
        //                 return Ok(action);
        //             }
        //         }
        //     }
        // }

        // if should_recurse {
        //     current_node.for_each_child(|child| {
        //         let _ = self.apply(ctx, child.clone(), stage, &current_env);
        //     });
        // }

        todo!()
    }
}
