use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ptr::{self, NonNull};
use std::sync::Arc;

use crate::{Error, Result, error::check, string_view, sys};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Vendor {
    #[default]
    Cpu,
    Cuda,
}

impl Vendor {
    fn name(self) -> &'static [u8] {
        match self {
            Self::Cpu => b"cpu",
            Self::Cuda => b"cuda",
        }
    }
}

/// Zero worker threads asks the runtime to select its default concurrency.
#[derive(Clone, Debug)]
pub struct InstanceOptions {
    pub vendor: Vendor,
    pub device_ids: Vec<i32>,
    pub default_device_id: i32,
    pub worker_threads: u32,
}

impl Default for InstanceOptions {
    fn default() -> Self {
        Self {
            vendor: Vendor::Cpu,
            device_ids: vec![0],
            default_device_id: 0,
            worker_threads: 0,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DeviceType {
    Cpu,
    Cuda,
    CudaHost,
    CudaManaged,
    External,
    Unknown(i32),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DeviceCapabilities {
    pub device_id: i32,
    pub max_compute_streams: u32,
    pub max_copy_streams: u32,
    pub max_memory_bytes: u64,
}

impl From<sys::ku_device_capabilities_t> for DeviceCapabilities {
    fn from(value: sys::ku_device_capabilities_t) -> Self {
        Self {
            device_id: value.device_id,
            max_compute_streams: value.streams.max_compute_streams,
            max_copy_streams: value.streams.max_copy_streams,
            max_memory_bytes: value.max_memory_bytes,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct InstanceCapabilities {
    pub worker_threads: u32,
    pub max_host_memory_bytes: u64,
    pub devices: Vec<DeviceCapabilities>,
}

#[derive(Debug)]
pub(crate) struct InstanceInner {
    raw: Option<NonNull<sys::_ku_instance>>,
}

// SAFETY: the registry/capabilities are immutable after initialization. Native
// submissions and waits synchronize internally; each device owns an explicit
// stream and selects/restores its backend device for allocation and destruction.
// KuInstance owners and Rust borrows exclude destruction while an operation can use
// the instance. The final owner may destroy it on any application thread;
// synchronous transfers finish using borrowed buffers before returning.
unsafe impl Send for InstanceInner {}
unsafe impl Sync for InstanceInner {}

impl InstanceInner {
    pub(crate) fn raw(&self) -> sys::ku_instance_t {
        self.raw.expect("live instance").as_ptr()
    }
}

impl Drop for InstanceInner {
    fn drop(&mut self) {
        if let Some(raw) = self.raw.take() {
            // SAFETY: the final Rust owner destroys the uniquely owned instance.
            // Rust lifetimes prevent dependent views from outliving this owner.
            unsafe {
                let _ = sys::ku_instance_destroy(raw.as_ptr());
            }
        }
    }
}

/// Shared ownership of an instance, usable across threads.
#[derive(Clone, Debug)]
pub struct KuInstance {
    pub(crate) inner: Arc<InstanceInner>,
    default_device: NonNull<sys::_ku_device>,
}

// SAFETY: default_device is an immutable identity owned by inner. Its safe
// operations obey InstanceInner's synchronization and device-selection contract.
unsafe impl Send for KuInstance {}
unsafe impl Sync for KuInstance {}

impl KuInstance {
    pub fn new(vendor: Vendor) -> Result<Self> {
        Self::with_options(InstanceOptions {
            vendor,
            ..InstanceOptions::default()
        })
    }

    pub fn with_options(options: InstanceOptions) -> Result<Self> {
        if options.device_ids.is_empty() || !options.device_ids.contains(&options.default_device_id)
        {
            return Err(Error::InvalidArgument(
                "device list must contain the default device",
            ));
        }
        let mut seen = std::collections::HashSet::new();
        if options
            .device_ids
            .iter()
            .any(|&id| id < 0 || !seen.insert(id))
        {
            return Err(Error::InvalidArgument(
                "device IDs must be nonnegative and unique",
            ));
        }
        let devices: Vec<_> = options
            .device_ids
            .iter()
            .map(|&device_id| sys::ku_device_capabilities_t {
                device_id,
                streams: sys::ku_device_stream_capabilities_t {
                    max_compute_streams: 0,
                    max_copy_streams: 0,
                },
                max_memory_bytes: 0,
            })
            .collect();
        let info = sys::ku_instance_init_info_t {
            vendor: string_view(options.vendor.name()),
            default_device_id: options.default_device_id,
            capabilities: sys::ku_instance_capabilities_t {
                scheduler: sys::ku_scheduler_t {
                    ctx: ptr::null_mut(),
                    submit: ptr::null(),
                },
                max_worker_concurrency: options.worker_threads,
                max_host_memory_bytes: 0,
                devices: devices.as_ptr(),
                device_count: devices.len(),
            },
        };
        let mut raw = ptr::null_mut();
        // SAFETY: all descriptor storage is live through init and copied by C++.
        check(unsafe { sys::ku_instance_init(&info, &mut raw) })?;
        // Guard the native instance before any further fallible initialization.
        let inner = InstanceInner {
            raw: Some(NonNull::new(raw).expect("successful instance output is non-null")),
        };
        let mut default_device = ptr::null_mut();
        // SAFETY: initialization created all configured devices; inner owns the instance.
        check(unsafe { sys::ku_instance_get_default_device(inner.raw(), &mut default_device) })?;
        let default_device =
            NonNull::new(default_device).expect("successful device output is non-null");
        Ok(Self {
            inner: Arc::new(inner),
            default_device,
        })
    }

    /// Borrows the existing default device from this instance.
    pub fn default_device(&self) -> KuDevice<'_> {
        KuDevice {
            raw: self.default_device,
            _runtime: PhantomData,
        }
    }

    pub fn device(&self, id: i32) -> Result<KuDevice<'_>> {
        let mut raw = ptr::null_mut();
        check(unsafe { sys::ku_instance_get_device(self.inner.raw(), id, &mut raw) })?;
        Ok(KuDevice {
            raw: NonNull::new(raw).expect("successful device output is non-null"),
            _runtime: PhantomData,
        })
    }

    pub fn capabilities(&self) -> Result<InstanceCapabilities> {
        let mut out = MaybeUninit::uninit();
        // SAFETY: the instance owns the returned list; copy it before returning.
        unsafe {
            check(sys::ku_instance_get_capabilities(
                self.inner.raw(),
                out.as_mut_ptr(),
            ))?;
            let out = out.assume_init();
            let devices = if out.device_count == 0 {
                Vec::new()
            } else {
                std::slice::from_raw_parts(out.devices, out.device_count)
                    .iter()
                    .copied()
                    .map(Into::into)
                    .collect()
            };
            Ok(InstanceCapabilities {
                worker_threads: out.max_worker_concurrency,
                max_host_memory_bytes: out.max_host_memory_bytes,
                devices,
            })
        }
    }

    pub fn flush(&self) -> Result<()> {
        check(unsafe { sys::ku_instance_flush(self.inner.raw()) })
    }

    /// Explicitly closes the last instance owner and reports shutdown errors.
    /// Other KuInstance owners cause `ResourcesInUse`. Dependent views borrow
    /// this wrapper, so Rust prevents closing it while those borrows are used.
    pub fn close(self) -> Result<()> {
        let mut inner = Arc::try_unwrap(self.inner).map_err(|_| Error::ResourcesInUse)?;
        let raw = inner.raw.take().expect("live instance");
        // Destruction invalidates the handle even if it returns an error.
        check(unsafe { sys::ku_instance_destroy(raw.as_ptr()) })
    }

    /// Borrows the native instance handle without extending its lifetime.
    pub fn as_raw(&self) -> sys::ku_instance_t {
        self.inner.raw()
    }
}

/// A borrowed native instance identity; copying it does not retain the runtime.
#[derive(Clone, Copy, Debug)]
pub struct KuInstanceRef<'i> {
    raw: NonNull<sys::_ku_instance>,
    _runtime: PhantomData<&'i InstanceInner>,
}

// SAFETY: the immutable identity borrows a synchronized live runtime for 'i.
unsafe impl Send for KuInstanceRef<'_> {}
unsafe impl Sync for KuInstanceRef<'_> {}

impl KuInstanceRef<'_> {
    pub fn as_raw(&self) -> sys::ku_instance_t {
        self.raw.as_ptr()
    }
}

/// A borrowed device identity, valid for its instance's lifetime.
#[derive(Clone, Copy, Debug)]
pub struct KuDevice<'i> {
    raw: NonNull<sys::_ku_device>,
    _runtime: PhantomData<&'i InstanceInner>,
}

// SAFETY: the runtime borrow protects the immutable device identity. Native
// transfers synchronize submission internally and use the device-owned stream;
// allocation/free select the correct backend device on the calling thread.
unsafe impl Send for KuDevice<'_> {}
unsafe impl Sync for KuDevice<'_> {}

impl<'i> KuDevice<'i> {
    /// # Safety
    /// raw must be a valid device owned by a runtime kept alive throughout 'i.
    pub(crate) unsafe fn from_raw(raw: sys::ku_device_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid device is non-null"),
            _runtime: PhantomData,
        }
    }

    /// Borrowed raw handle, valid for the instance lifetime.
    pub fn as_raw(&self) -> sys::ku_device_t {
        self.raw.as_ptr()
    }

    /// Queries the native owning instance without retaining or synchronizing.
    pub fn instance(&self) -> KuInstanceRef<'i> {
        let mut raw = ptr::null_mut();
        // SAFETY: the device and its owning instance remain live throughout 'i.
        let status = unsafe { sys::ku_device_get_instance(self.as_raw(), &mut raw) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        KuInstanceRef {
            raw: NonNull::new(raw).expect("valid device has an instance"),
            _runtime: PhantomData,
        }
    }

    pub fn info(&self) -> Result<(DeviceType, i32)> {
        let mut out = MaybeUninit::uninit();
        unsafe {
            check(sys::ku_device_get_info(self.as_raw(), out.as_mut_ptr()))?;
            let out = out.assume_init();
            let kind = match out.device_type {
                sys::KU_DEVICE_CPU => DeviceType::Cpu,
                sys::KU_DEVICE_CUDA => DeviceType::Cuda,
                sys::KU_DEVICE_CUDA_HOST => DeviceType::CudaHost,
                sys::KU_DEVICE_CUDA_MANAGED => DeviceType::CudaManaged,
                sys::KU_DEVICE_EXT => DeviceType::External,
                other => DeviceType::Unknown(other),
            };
            Ok((kind, out.device_id))
        }
    }

    pub fn capabilities(&self) -> Result<DeviceCapabilities> {
        let mut out = MaybeUninit::uninit();
        unsafe {
            check(sys::ku_device_get_capabilities(
                self.as_raw(),
                out.as_mut_ptr(),
            ))?;
            Ok(out.assume_init().into())
        }
    }

    /// Waits for host transfers and the device's default stream.
    pub fn flush(&self) -> Result<()> {
        check(unsafe { sys::ku_device_flush(self.as_raw()) })
    }

    pub(crate) fn stream(&self) -> Result<sys::ku_stream_t> {
        let mut stream = ptr::null_mut();
        check(unsafe { sys::ku_device_get_default_stream(self.as_raw(), &mut stream) })?;
        Ok(stream)
    }
}
