use kurt::{DeviceType, InstanceOptions, KuInstance, Vendor};
use pyo3::exceptions::{PyRuntimeError, PyValueError};
use pyo3::prelude::*;
use pyo3::pyclass::{PyTraverseError, PyVisit};
use pyo3::types::{PyDict, PyWeakrefMethods, PyWeakrefReference};

use crate::errors::native_error;

fn closed() -> PyErr {
    PyRuntimeError::new_err("instance or device has been cleared")
}

#[pyclass(module = "kupy._native", name = "Instance", weakref)]
pub(crate) struct PyInstance {
    native: Option<KuInstance>,
    vendor: &'static str,
    default_device_id: i32,
    cache: Option<Py<PyDict>>,
}

impl PyInstance {
    pub(crate) fn clone_native(&self) -> PyResult<KuInstance> {
        self.native.as_ref().cloned().ok_or_else(closed)
    }
}

#[pymethods]
impl PyInstance {
    #[new]
    #[pyo3(signature = (vendor, *, device_ids = vec![0], default_device_id = 0))]
    fn new(
        py: Python<'_>,
        vendor: &str,
        device_ids: Vec<i32>,
        default_device_id: i32,
    ) -> PyResult<Self> {
        let (name, vendor) = match vendor {
            "cpu" => ("cpu", Vendor::Cpu),
            "cuda" => ("cuda", Vendor::Cuda),
            _ => {
                return Err(PyValueError::new_err(format!(
                    "unsupported vendor: {vendor}"
                )));
            }
        };
        let native = KuInstance::with_options(InstanceOptions {
            vendor,
            device_ids,
            default_device_id,
            worker_threads: 0,
        })
        .map_err(|error| native_error(error, "initialize instance"))?;
        Ok(Self {
            native: Some(native),
            vendor: name,
            default_device_id,
            cache: Some(PyDict::new(py).unbind()),
        })
    }

    #[getter]
    fn vendor(&self) -> &str {
        self.vendor
    }

    #[getter]
    fn default_device(slf: &Bound<'_, Self>) -> PyResult<Py<PyDevice>> {
        let id = slf.borrow().default_device_id;
        Self::device(slf, id)
    }

    fn device(slf: &Bound<'_, Self>, id: i32) -> PyResult<Py<PyDevice>> {
        let py = slf.py();
        let cache = {
            let instance = slf.borrow();
            let native = instance.native.as_ref().ok_or_else(closed)?;
            native
                .device(id)
                .map_err(|error| native_error(error, "lookup device"))?;
            instance.cache.as_ref().ok_or_else(closed)?.clone_ref(py)
        };
        let cache = cache.bind(py);
        if let Some(reference) = cache.get_item(id)? {
            let reference = reference.cast::<PyWeakrefReference>()?;
            if let Some(device) = reference.upgrade_as::<PyDevice>()? {
                return Ok(device.unbind());
            }
        }
        let device = Py::new(
            py,
            PyDevice {
                parent: Some(slf.clone().unbind()),
                id,
            },
        )?;
        let reference = PyWeakrefReference::new(device.bind(py).as_any())?;
        cache.set_item(id, reference)?;
        Ok(device)
    }

    fn flush(&self) -> PyResult<()> {
        self.native
            .as_ref()
            .ok_or_else(closed)?
            .flush()
            .map_err(|error| native_error(error, "flush instance"))
    }

    fn __traverse__(&self, visit: PyVisit<'_>) -> Result<(), PyTraverseError> {
        visit.call(&self.cache)
    }

    fn __clear__(slf: &Bound<'_, Self>) {
        let (cache, native) = {
            let mut instance = slf.borrow_mut();
            (instance.cache.take(), instance.native.take())
        };
        drop(cache);
        drop(native);
    }
}

#[pyclass(module = "kupy._native", name = "Device", weakref)]
pub(crate) struct PyDevice {
    parent: Option<Py<PyInstance>>,
    id: i32,
}

#[pymethods]
impl PyDevice {
    #[getter]
    pub(crate) fn id(&self) -> i32 {
        self.id
    }

    #[getter]
    fn r#type(&self, py: Python<'_>) -> PyResult<i32> {
        let parent = self.parent.as_ref().ok_or_else(closed)?.bind(py).borrow();
        let native = parent.native.as_ref().ok_or_else(closed)?;
        let device = native
            .device(self.id)
            .map_err(|error| native_error(error, "lookup device"))?;
        let (kind, _) = device
            .info()
            .map_err(|error| native_error(error, "query device"))?;
        Ok(match kind {
            DeviceType::Cpu => 1,
            DeviceType::Cuda => 2,
            DeviceType::CudaHost => 3,
            DeviceType::CudaManaged => 4,
            DeviceType::External => 12,
            DeviceType::Unknown(raw) => raw,
        })
    }

    #[getter]
    pub(crate) fn instance(&self, py: Python<'_>) -> PyResult<Py<PyInstance>> {
        self.parent
            .as_ref()
            .map(|parent| parent.clone_ref(py))
            .ok_or_else(closed)
    }

    fn flush(&self, py: Python<'_>) -> PyResult<()> {
        let parent = self.parent.as_ref().ok_or_else(closed)?.bind(py).borrow();
        let native = parent.native.as_ref().ok_or_else(closed)?;
        native
            .device(self.id)
            .map_err(|error| native_error(error, "lookup device"))?
            .flush()
            .map_err(|error| native_error(error, "flush device"))
    }

    fn __traverse__(&self, visit: PyVisit<'_>) -> Result<(), PyTraverseError> {
        visit.call(&self.parent)
    }

    fn __clear__(slf: &Bound<'_, Self>) {
        let parent = slf.borrow_mut().parent.take();
        drop(parent);
    }
}
