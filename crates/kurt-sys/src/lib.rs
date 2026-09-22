//! Raw bindings to the public `kuai-runtime` C API.
//!
//! Types and constants retain their C names. Handles are raw pointers; copying
//! one does not retain the resource. These bindings do not provide RAII or a
//! safe Rust interface. See the crate README and the runtime's public headers
//! for ownership and synchronization contracts.
//!
//! # Safety
//!
//! Unless explicitly allowed, every pointer and handle passed to the runtime
//! must be non-null, aligned, and valid for the entire operation. Output slots
//! must be writable. Only read output values after the documented status has
//! been returned. Borrowed data must not outlive its owner. Release owned object
//! and completion references exactly once per acquisition, and destroy frame
//! contexts and device-dependent resources before their instance.
//!
//! Callbacks must obey the runtime's threading requirements and must never
//! unwind across the C ABI. Required callbacks use `unsafe extern "C" fn`.
//! Nullable callback slots use raw code pointers; check for null before
//! converting to the documented function signature and calling them.
#![doc = include_str!("../README.md")]
#![allow(non_camel_case_types)]
#![deny(improper_ctypes, improper_ctypes_definitions)]

#[macro_use]
mod macros;
mod builtin;
mod object;
mod runtime;
mod tensor;
mod types;

pub use builtin::*;
pub use object::*;
pub use runtime::*;
pub use tensor::*;
pub use types::*;
