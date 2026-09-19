// C integer enums must accept values that are not known to this binding.
macro_rules! c_enum {
    ($name:ident: $repr:ty { $($constant:ident = $value:expr),* $(,)? }) => {
        pub type $name = $repr;
        $(pub const $constant: $name = $value;)*
    };
}

macro_rules! opaque_handles {
    ($($raw:ident => $handle:ident),* $(,)?) => {
        $(
            #[repr(C)]
            pub struct $raw {
                _private: [u8; 0],
                // Do not infer Send, Sync, or Unpin for foreign objects.
                _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
            }
            pub type $handle = *mut $raw;
        )*
    };
}

// Mirror KU_PRIMITIVE_TYPE_DEFS: tags and payload fields share one table.
macro_rules! primitive_types {
    ($($constant:ident = $tag:expr => $field:ident: $payload:ty),* $(,)?) => {
        c_enum!(ku_primitive_type_t: i32 { $($constant = $tag),* });

        /// Payload of the anonymous C union in `ku_union_t`.
        #[repr(C)]
        #[derive(Clone, Copy)]
        pub union ku_union_value_t {
            $(pub $field: $payload,)*
        }
    };
}
