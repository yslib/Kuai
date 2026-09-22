use kurt::{PrimitiveType, TensorMetadata};
use pyo3::exceptions::{PyRuntimeError, PyValueError};
use pyo3::prelude::*;
use pyo3::pyclass::{PyTraverseError, PyVisit};
use pyo3::types::PyTuple;

use crate::instance::PyDevice;
use crate::owned_tensor::OwnedTensor;

pub(crate) fn primitive_from_id(id: i32) -> PyResult<PrimitiveType> {
    match id {
        x if x == PrimitiveType::Boolean as i32 => Ok(PrimitiveType::Boolean),
        x if x == PrimitiveType::Byte as i32 => Ok(PrimitiveType::Byte),
        x if x == PrimitiveType::I16 as i32 => Ok(PrimitiveType::I16),
        x if x == PrimitiveType::I32 as i32 => Ok(PrimitiveType::I32),
        x if x == PrimitiveType::I64 as i32 => Ok(PrimitiveType::I64),
        x if x == PrimitiveType::F32 as i32 => Ok(PrimitiveType::F32),
        x if x == PrimitiveType::F64 as i32 => Ok(PrimitiveType::F64),
        _ => Err(PyValueError::new_err(
            "unsupported kuai tensor primitive type",
        )),
    }
}

pub(crate) fn dtype_name(primitive: PrimitiveType) -> &'static str {
    match primitive {
        PrimitiveType::Boolean => "bool",
        PrimitiveType::Byte => "int8",
        PrimitiveType::I16 => "int16",
        PrimitiveType::I32 => "int32",
        PrimitiveType::I64 => "int64",
        PrimitiveType::F32 => "float32",
        PrimitiveType::F64 => "float64",
        PrimitiveType::None => unreachable!(),
    }
}

pub(crate) fn primitive_types(module: &Bound<'_, PyModule>) -> PyResult<()> {
    let mapping = pyo3::types::PyDict::new(module.py());
    for (name, primitive) in [
        ("boolean", PrimitiveType::Boolean),
        ("byte", PrimitiveType::Byte),
        ("i16", PrimitiveType::I16),
        ("i32", PrimitiveType::I32),
        ("i64", PrimitiveType::I64),
        ("f32", PrimitiveType::F32),
        ("f64", PrimitiveType::F64),
    ] {
        mapping.set_item(name, primitive as i32)?;
    }
    module.add("_primitive_types", mapping)
}

fn cleared() -> PyErr {
    PyRuntimeError::new_err("tensor has been cleared")
}

#[pyclass(module = "kupy._native", name = "Tensor", weakref)]
pub(crate) struct PyTensor {
    native: Option<OwnedTensor>,
    device: Option<Py<PyDevice>>,
}

impl PyTensor {
    pub(crate) fn materialize(native: OwnedTensor, device: Py<PyDevice>) -> Self {
        Self {
            native: Some(native),
            device: Some(device),
        }
    }

    pub(crate) fn tensor(&self) -> PyResult<&kurt::KuTensor<'_>> {
        Ok(self.native.as_ref().ok_or_else(cleared)?.tensor())
    }

    fn metadata(&self) -> PyResult<TensorMetadata> {
        Ok(self.tensor()?.metadata())
    }
}

#[pymethods]
impl PyTensor {
    #[getter]
    fn device(&self, py: Python<'_>) -> PyResult<Py<PyDevice>> {
        self.device
            .as_ref()
            .map(|d| d.clone_ref(py))
            .ok_or_else(cleared)
    }

    #[getter]
    fn dtype(&self, py: Python<'_>) -> PyResult<Py<PyAny>> {
        let np = py.import("numpy")?;
        Ok(np
            .call_method1("dtype", (dtype_name(self.metadata()?.primitive_type),))?
            .unbind())
    }

    #[getter]
    fn shape(&self, py: Python<'_>) -> PyResult<Py<PyTuple>> {
        Ok(PyTuple::new(py, self.metadata()?.shape)?.unbind())
    }

    #[getter]
    fn strides(&self, py: Python<'_>) -> PyResult<Py<PyTuple>> {
        Ok(PyTuple::new(py, self.metadata()?.strides)?.unbind())
    }

    #[getter]
    fn ndim(&self) -> PyResult<usize> {
        Ok(self.metadata()?.shape.len())
    }

    #[getter]
    fn size(&self) -> PyResult<usize> {
        Ok(self.tensor()?.len())
    }

    fn __traverse__(&self, visit: PyVisit<'_>) -> Result<(), PyTraverseError> {
        visit.call(&self.device)
    }

    fn __clear__(slf: &Bound<'_, Self>) {
        let (native, device) = {
            let mut tensor = slf.borrow_mut();
            (tensor.native.take(), tensor.device.take())
        };
        drop(native);
        drop(device);
    }
}
