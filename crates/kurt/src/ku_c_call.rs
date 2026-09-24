use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ptr::{self, NonNull};
use std::rc::Rc;

use crate::{
    Error, KuArc, KuDevice, KuInstance, KuObject, NativeObject, Result, error::check,
    ku_arc::OwnedRaw, ku_instance::InstanceInner, string_view, sys,
};

#[derive(Debug)]
pub struct KuFrameContext<'r> {
    raw: NonNull<sys::_ku_frame_ctx>,
    device: KuDevice<'r>,
    _thread: PhantomData<Rc<()>>,
}

impl<'i> KuDevice<'i> {
    pub fn frame_context(&self) -> Result<KuFrameContext<'i>> {
        let mut raw = ptr::null_mut();
        // SAFETY: the context inherits the device's runtime borrow for 'i.
        check(unsafe { sys::ku_frame_ctx_create(self.as_raw(), &mut raw) })?;
        Ok(KuFrameContext {
            raw: NonNull::new(raw).expect("successful frame context output is non-null"),
            device: *self,
            _thread: PhantomData,
        })
    }
}

impl<'r> KuFrameContext<'r> {
    pub fn device(&self) -> &KuDevice<'r> {
        &self.device
    }
}

impl Drop for KuFrameContext<'_> {
    fn drop(&mut self) {
        // SAFETY: the context is uniquely owned and its device is still alive.
        unsafe {
            sys::ku_frame_ctx_destroy(self.raw.as_ptr());
        }
    }
}

/// A resolved callable normalized through the C ABI, regardless of implementation language.
/// Borrows the instance that owns its capture. Resolving a name is safe;
/// execution requires the callable's native contract.
#[derive(Clone)]
pub struct KuCCall<'instance> {
    target: sys::ku_call_t,
    instance: &'instance InstanceInner,
    _thread: PhantomData<Rc<()>>,
}

impl std::fmt::Debug for KuCCall<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("KuCCall")
            .field("kind", &self.target.kind)
            .finish_non_exhaustive()
    }
}

impl KuInstance {
    pub fn builtin_names(&self) -> Result<Vec<String>> {
        let mut count = 0;
        // SAFETY: the enumeration API explicitly allows null storage at zero capacity.
        check(unsafe {
            sys::ku_instance_get_builtin_info(self.inner.raw(), ptr::null_mut(), 0, &mut count)
        })?;
        if count == 0 {
            return Ok(Vec::new());
        }
        let mut items = vec![
            sys::ku_builtin_info_t {
                name: string_view(b"")
            };
            count
        ];
        check(unsafe {
            sys::ku_instance_get_builtin_info(
                self.inner.raw(),
                items.as_mut_ptr(),
                items.len(),
                &mut count,
            )
        })?;
        items
            .into_iter()
            .take(count)
            .map(|item| {
                // SAFETY: names borrow from the live immutable instance registry.
                let bytes = if item.name.size == 0 {
                    &[]
                } else {
                    unsafe { std::slice::from_raw_parts(item.name.data.cast(), item.name.size) }
                };
                std::str::from_utf8(bytes)
                    .map(str::to_owned)
                    .map_err(Error::InvalidUtf8)
            })
            .collect()
    }

    pub fn builtin(&self, name: &str) -> Result<KuCCall<'_>> {
        let mut target = MaybeUninit::uninit();
        // SAFETY: name is live through the call; the target borrows this instance.
        unsafe {
            check(sys::ku_instance_get_proc_address(
                self.inner.raw(),
                string_view(name.as_bytes()),
                target.as_mut_ptr(),
            ))?;
            Ok(KuCCall {
                target: target.assume_init(),
                instance: &self.inner,
                _thread: PhantomData,
            })
        }
    }
}

struct Outputs<'r> {
    raw: Vec<sys::ku_object_t>,
    _runtime: PhantomData<&'r InstanceInner>,
}

impl Drop for Outputs<'_> {
    fn drop(&mut self) {
        for raw in &self.raw {
            if !raw.is_null() {
                // SAFETY: each non-null result owns one reference not yet transferred.
                unsafe {
                    let _ = sys::ku_object_release(*raw);
                }
            }
        }
    }
}

impl KuCCall<'_> {
    /// Calls a native builtin with borrowed arguments and owned result management.
    /// Validates callable/context instance identity and waits for the context's
    /// default stream before returning results or releasing inputs. Argument
    /// device compatibility is interpreted by the runtime, not this wrapper.
    /// `None` is a nullable C argument/result, distinct from the
    /// `ScalarValue::None` primitive.
    /// Results borrow the context's runtime lifetime. They can outlive the
    /// context, callable, and argument wrappers, but not that runtime borrow.
    ///
    /// # Safety
    /// The caller must establish the selected builtin's semantic preconditions
    /// (including index bounds, supported shapes, and arithmetic domains).
    /// It must not mutate immutable arguments, retain borrowed arguments beyond
    /// the call's synchronized default stream work, or return uninitialized data.
    /// Results must be valid materializable objects whose device resources are
    /// owned by the supplied context's instance. Unknown vendor code cannot be
    /// made safe merely by checking the C call frame's layout.
    pub unsafe fn call_unchecked<'r>(
        &self,
        context: &mut KuFrameContext<'r>,
        positional: &[Option<&KuObject<'_>>],
        keywords: &[(&str, Option<&KuObject<'_>>)],
    ) -> Result<Vec<Option<KuArc<KuObject<'r>>>>> {
        if self.instance.raw() != context.device.instance().as_raw() {
            return Err(Error::DeviceMismatch);
        }
        let arguments = positional
            .iter()
            .copied()
            .chain(keywords.iter().map(|(_, value)| *value));
        let mut argv = Vec::with_capacity(
            positional
                .len()
                .checked_add(keywords.len())
                .ok_or(Error::InvalidArgument("too many builtin arguments"))?,
        );
        for argument in arguments {
            argv.push(argument.map_or(ptr::null_mut(), KuObject::as_raw));
        }
        let names: Vec<_> = keywords
            .iter()
            .map(|(name, _)| string_view(name.as_bytes()))
            .collect();
        let mut outputs = Outputs::<'r> {
            raw: vec![ptr::null_mut()],
            _runtime: PhantomData,
        };
        loop {
            let mut frame = sys::ku_frame_t {
                ctx: context.raw.as_ptr(),
                argv: argv.as_ptr(),
                pargn: positional.len(),
                knames: names.as_ptr(),
                kargn: names.len(),
                results: outputs.raw.as_mut_ptr(),
                result_capacity: outputs.raw.len(),
                result_count: 0,
            };
            // SAFETY: the caller supplies native semantic preconditions. All
            // frame storage and handles remain live with valid capacities.
            let status = unsafe {
                match self.target.kind {
                    sys::KU_CALL_FFI => (self.target.value.ffi)(&mut frame),
                    sys::KU_CALL_CALLABLE => {
                        let closure = self.target.value.closure;
                        (closure.ffi)(closure.capture, &mut frame)
                    }
                    _ => return Err(Error::InvalidArgument("invalid native callable kind")),
                }
            };
            // Synchronize even on failure before outputs and borrowed inputs can drop.
            let synchronized = context.device.flush();
            if status == sys::KU_STATUS_BUFFER_TOO_SMALL && frame.result_count > outputs.raw.len() {
                synchronized?;
                // The native binding checks capacity before executing the handler.
                outputs.raw.resize(frame.result_count, ptr::null_mut());
                continue;
            }
            check(status)?;
            synchronized?;
            if frame.result_count > outputs.raw.len() {
                return Err(Error::InvalidArgument("invalid native result count"));
            }
            let mut decoded = Vec::with_capacity(frame.result_count);
            for slot in &mut outputs.raw[..frame.result_count] {
                // Transfer before decoding: on error, OwnedRaw releases this
                // output, decoded drops prior results, and Outputs drops the rest.
                let raw = std::mem::replace(slot, ptr::null_mut());
                let value = if raw.is_null() {
                    None
                } else {
                    // SAFETY: every non-null slot owns one reference. The caller
                    // guarantees all descendant resources belong to this instance.
                    let native = unsafe { OwnedRaw::<'r>::new(raw) };
                    // SAFETY: the caller guarantees valid materializable
                    // results whose transitive dependencies are owned by the
                    // context's KuInstance, kept alive throughout 'r and cleanup.
                    Some(unsafe { KuObject::from_native(native)? })
                };
                decoded.push(value);
            }
            return Ok(decoded);
        }
    }
}
