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

    fn get_attrs(&self, _node: &Stmt) -> Vec<Attribute> {
        Vec::<Attribute>::new()
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
        let attrs = Vec::<Attribute>::new(); // TODO:: get current stmt attrs

        let mut current_env = AttrEnvironment {
            parent: Some(env),
            local_attrs: &attrs,
            recursive_attrs: Vec::new(), // visible to sub nodes
            delta_meta: AttrMetadata::new(),
        };

        for attr in &attrs {
            let handlers = self.registry.get_handlers(&attr.name, stage);
            for h in &handlers {
                h.evaluate(ctx, attr, &mut current_env.delta_meta);
                if h.is_recursive() {
                    current_env.recursive_attrs.push(attr);
                }
            }
        }
        let mut current_node = node;
        let mut should_recurse = true;

        for attr in &attrs {
            let handlers = self.registry.get_handlers(&attr.name, stage);
            for handler in handlers {
                // 执行转换
                let action =
                    handler.transform(ctx, self, attr, current_node, &current_env.delta_meta)?;

                match action {
                    AttrAction::Continue(new_node) => {
                        current_node = new_node;
                    }
                    AttrAction::SkipChildren(new_node) => {
                        current_node = new_node;
                        should_recurse = false;
                    }
                    _ => {
                        return Ok(action);
                    }
                }
            }
        }

        if should_recurse {}

        todo!()
    }
}
