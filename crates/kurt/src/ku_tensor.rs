use std::marker::PhantomData;
use std::mem::{MaybeUninit, size_of};
use std::ptr::{self, NonNull};

use crate::{
    Element, Error, HasObjectKind, KuArc, KuDevice, KuObjectKind, NativeObject, NativeType,
    PrimitiveType, Result, error::check, ku_arc::OwnedRaw, ku_instance::InstanceInner, sys,
};

/// A tensor view borrowing its runtime dependencies.
#[derive(Debug)]
pub struct KuTensor<'i> {
    raw: NonNull<sys::_ku_object>,
    _runtime: PhantomData<&'i InstanceInner>,
}

impl crate::native::private::Sealed for KuTensor<'_> {}
impl crate::native::private::View for KuTensor<'_> {}
impl NativeType for KuTensor<'_> {}

// SAFETY: immutable tensor payloads use atomic native references. The runtime
// borrow covers device destruction; native operations synchronize internally
// and restore backend device selection on the calling thread.
unsafe impl Send for KuTensor<'_> {}
unsafe impl Sync for KuTensor<'_> {}

impl NativeObject for KuTensor<'_> {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TensorMetadata {
    pub primitive_type: PrimitiveType,
    pub shape: Vec<usize>,
    /// Strides measured in elements, in column-major order.
    pub strides: Vec<usize>,
}

fn element_count(shape: &[usize]) -> Result<usize> {
    if shape.len() > 8 {
        return Err(Error::InvalidArgument("tensor rank must be at most eight"));
    }
    let mut count = 1_usize;
    for &extent in shape {
        i64::try_from(extent).map_err(|_| Error::InvalidArgument("tensor extent exceeds i64"))?;
        count = count
            .checked_mul(extent)
            .ok_or(Error::InvalidArgument("tensor shape overflows usize"))?;
    }
    Ok(count)
}

fn byte_count<T: Element>(count: usize) -> Result<usize> {
    count
        .checked_mul(size_of::<T>())
        .filter(|&bytes| bytes <= isize::MAX as usize)
        .ok_or(Error::InvalidArgument(
            "tensor byte size exceeds the host address space",
        ))
}

impl<'i> KuDevice<'i> {
    /// Copies initialized host elements into a tensor, preserving column-major order.
    /// The source buffer is no longer in use when this call returns, including errors.
    pub fn tensor_from_slice<T: Element>(
        &self,
        shape: &[usize],
        values: &[T],
    ) -> Result<KuArc<KuTensor<'i>>> {
        let count = element_count(shape)?;
        if count != values.len() {
            return Err(Error::InvalidArgument(
                "shape does not match the number of input elements",
            ));
        }
        let bytes = byte_count::<T>(count)?;
        let shape: Vec<i64> = shape.iter().map(|&n| n as i64).collect();
        let desc = sys::ku_tensor_create_desc_t {
            device: self.as_raw(),
            primitive_type: T::TYPE as i32,
            ndim: shape.len() as i32,
            shape: shape.as_ptr(),
            strides: ptr::null(),
        };
        let mut tensor = ptr::null_mut();
        // Declaration order keeps the completion guard ahead of unpublished
        // tensor cleanup on errors or unwind, within the runtime borrow.
        let native;
        let mut completion = TransferWait::new();
        // SAFETY: Element is sealed to initialized C-compatible representations;
        // shape and byte count are checked. The source borrow survives the wait.
        // Submission errors leave both outputs null and accept no native task.
        unsafe {
            check(sys::ku_tensor_create_from_host_async(
                &desc,
                values.as_ptr().cast(),
                bytes,
                &mut tensor,
                &mut completion.raw,
            ))?;
            native = OwnedRaw::<'i>::new(tensor);
            completion.wait()?;
            // Only publish the typed view after initialization has completed.
            Ok(native.into_view(KuTensor::from_raw(tensor)))
        }
    }

    pub fn zeros<T: Element>(&self, shape: &[usize]) -> Result<KuArc<KuTensor<'i>>> {
        let count = element_count(shape)?;
        byte_count::<T>(count)?;
        let values = vec![T::default(); count];
        self.tensor_from_slice(shape, &values)
    }
}

impl HasObjectKind for KuTensor<'_> {
    fn kind(&self) -> KuObjectKind {
        KuObjectKind::Tensor
    }
}

impl<'i> KuTensor<'i> {
    /// # Safety
    /// raw must identify a guarded live tensor with initialized immutable
    /// metadata/data and all runtime dependencies valid throughout 'i.
    pub(crate) unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid tensor is non-null"),
            _runtime: PhantomData,
        }
    }

    /// Queries the tensor's exact device, borrowing the same runtime lifetime.
    pub fn device(&self) -> KuDevice<'i> {
        // SAFETY: the tensor invariant keeps its actual device valid for 'i.
        unsafe { KuDevice::from_raw(self.raw_device()) }
    }

    fn raw_device(&self) -> sys::ku_device_t {
        let mut raw_device = ptr::null_mut();
        // SAFETY: the view borrows a live tensor with runtime dependencies in 'i.
        // The returned device is borrowed from that runtime.
        let status = unsafe { sys::ku_tensor_get_device(self.as_raw(), &mut raw_device) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        raw_device
    }

    fn info(&self) -> sys::ku_tensor_info_t {
        let mut info = MaybeUninit::uninit();
        // SAFETY: the view borrows a live tensor and its runtime dependencies.
        // The typed-kind invariant guarantees successful initialization.
        let status = unsafe { sys::ku_tensor_get_info(self.as_raw(), info.as_mut_ptr()) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        // SAFETY: the successful query initialized every field.
        unsafe { info.assume_init() }
    }

    pub fn len(&self) -> usize {
        let info = self.info();
        if info.ndim == 0 {
            return 1;
        }
        // SAFETY: info borrows immutable metadata from this live tensor. Native
        // construction checked shape prefix products in their original order.
        unsafe { std::slice::from_raw_parts(info.shape, info.ndim as usize) }
            .iter()
            .product()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn metadata(&self) -> TensorMetadata {
        let info = self.info();
        let primitive_type = match info.primitive_type {
            sys::KU_PRIMITIVE_BOOLEAN => PrimitiveType::Boolean,
            sys::KU_PRIMITIVE_BYTE => PrimitiveType::Byte,
            sys::KU_PRIMITIVE_I16 => PrimitiveType::I16,
            sys::KU_PRIMITIVE_I32 => PrimitiveType::I32,
            sys::KU_PRIMITIVE_I64 => PrimitiveType::I64,
            sys::KU_PRIMITIVE_F32 => PrimitiveType::F32,
            sys::KU_PRIMITIVE_F64 => PrimitiveType::F64,
            _ => unreachable!("a valid tensor has a supported element type"),
        };
        let (shape, strides) = if info.ndim == 0 {
            // Rank-zero tensors have null pointers, never Rust slices.
            (Vec::new(), Vec::new())
        } else {
            // SAFETY: both arrays borrow initialized immutable metadata from
            // this tensor; copying them finishes while this view is borrowed.
            unsafe {
                (
                    std::slice::from_raw_parts(info.shape, info.ndim as usize).to_vec(),
                    std::slice::from_raw_parts(info.strides, info.ndim as usize).to_vec(),
                )
            }
        };
        TensorMetadata {
            primitive_type,
            shape,
            strides,
        }
    }

    /// Checks the tensor's actual native device identity.
    pub fn is_on(&self, device: &KuDevice<'_>) -> bool {
        self.raw_device() == device.as_raw()
    }
}

impl KuDevice<'_> {
    /// Copies a tensor into initialized host storage, waiting before returning.
    pub fn to_vec<T: Element>(&self, tensor: &KuTensor<'_>) -> Result<Vec<T>> {
        if !tensor.is_on(self) {
            return Err(Error::DeviceMismatch);
        }
        let metadata = tensor.metadata();
        if metadata.primitive_type != T::TYPE {
            return Err(Error::TypeMismatch);
        }
        let count = element_count(&metadata.shape)?;
        let bytes = byte_count::<T>(count)?;
        if count != 0 {
            let mut canonical_stride = 1_usize;
            for (&extent, &stride) in metadata.shape.iter().zip(&metadata.strides) {
                if stride != canonical_stride {
                    return Err(Error::InvalidArgument(
                        "download requires a contiguous column-major tensor",
                    ));
                }
                // element_count has already checked the full product.
                canonical_stride *= extent;
            }
        }
        let mut data = vec![T::default(); count];
        if count == 0 {
            return Ok(data);
        }
        let stream = self.stream()?;
        let mut source = ptr::null_mut();
        let mut completion = TransferWait::new();
        // SAFETY: tensor, self, and initialized output storage remain borrowed
        // through the wait, including errors/unwind. The completion guard drops
        // before data. The queried source is a device address, not a host slice.
        unsafe {
            check(sys::ku_tensor_get_data(tensor.as_raw(), &mut source))?;
            check(sys::ku_device_copy_async(
                self.as_raw(),
                data.as_mut_ptr().cast(),
                source,
                bytes,
                sys::KU_MEMCPY_DEVICE_TO_HOST,
                stream,
                &mut completion.raw,
            ))?;
        }
        completion.wait()?;
        Ok(data)
    }
}

// Lexical guard for one owned native completion. It starts empty so the output
// is guarded as soon as submission succeeds, before Rust can return or unwind.
// Callers keep all native dependencies and host buffers alive until it drops.
struct TransferWait {
    raw: sys::ku_completion_t,
    waited: bool,
}

impl TransferWait {
    fn new() -> Self {
        Self {
            raw: ptr::null_mut(),
            waited: false,
        }
    }

    fn wait(mut self) -> Result<()> {
        // SAFETY: successful submission set raw to one owned completion. Native
        // waiting ends all caller-buffer access even when the operation failed.
        let status = unsafe { sys::ku_completion_wait(self.raw) };
        self.waited = true;
        check(status)
    }
}

impl Drop for TransferWait {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            // SAFETY: this guard owns the reference and drops before borrowed
            // buffers and dependencies. Diagnostics must not panic in cleanup.
            unsafe {
                if !self.waited {
                    let _ = sys::ku_completion_wait(self.raw);
                }
                let _ = sys::ku_completion_release(self.raw);
            }
        }
    }
}
