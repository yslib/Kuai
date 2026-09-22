use std::num::NonZeroI64;

use kurt::{
    KuArc, KuObject, KuScalar, KuSlice, KuString, KuTensor, PrimitiveType, ScalarValue, SliceSpec,
    TensorMetadata,
};
use pyo3::exceptions::{PyTypeError, PyValueError};
use pyo3::prelude::*;
use pyo3::types::{PyAny, PyBool, PyFloat, PyInt, PySlice, PyString};
use pyo3::{Bound, PyRef, Python};

use crate::errors::native_error;
use crate::tensor::PyTensor;

#[derive(Clone, Debug)]
pub(crate) enum Host {
    Scalar(ScalarValue),
    String(String),
    Slice(SliceSpec),
}

fn slice_field(py: Python<'_>, value: &Bound<'_, PyAny>, field: &str) -> PyResult<Option<i64>> {
    if value.is_none() {
        return Ok(None);
    }
    let index = py
        .import("operator")?
        .call_method1("index", (value,))
        .map_err(|error| {
            if error.is_instance_of::<PyTypeError>(py) {
                PyTypeError::new_err(format!("slice {field} must be None or integer-like"))
            } else {
                error
            }
        })?;
    index
        .extract::<i64>()
        .map(Some)
        .map_err(|_| PyValueError::new_err(format!("slice {field} is outside the int64_t range")))
}

pub(crate) fn box_host(value: &Bound<'_, PyAny>) -> PyResult<Host> {
    let py = value.py();
    let kind = value.get_type();
    if kind.is(py.get_type::<PyBool>()) {
        return Ok(Host::Scalar(ScalarValue::from(value.extract::<bool>()?)));
    }
    if kind.is(py.get_type::<PyInt>()) {
        return Ok(Host::Scalar(ScalarValue::I64(
            value.extract::<i64>().map_err(|_| {
                PyValueError::new_err("Python integer is outside the ku_i64_t range")
            })?,
        )));
    }
    if kind.is(py.get_type::<PyFloat>()) {
        return Ok(Host::Scalar(ScalarValue::F64(value.extract::<f64>()?)));
    }
    if kind.is(py.get_type::<PyString>()) {
        return Ok(Host::String(value.extract::<String>()?));
    }
    if value.is_none() {
        return Ok(Host::Scalar(ScalarValue::None));
    }
    if kind.is(py.get_type::<PySlice>()) {
        let start = slice_field(py, &value.getattr("start")?, "start")?;
        let stop = slice_field(py, &value.getattr("stop")?, "stop")?;
        let step = slice_field(py, &value.getattr("step")?, "step")?;
        if step == Some(0) {
            return Err(PyValueError::new_err("slice step must not be zero"));
        }
        return Ok(Host::Slice(SliceSpec {
            start,
            stop,
            step: step.and_then(NonZeroI64::new),
        }));
    }
    Err(PyTypeError::new_err(
        "builtin operands must be Tensor, bool, int, float, str, None, or slice",
    ))
}

pub(crate) enum Argument<'py> {
    Tensor(PyRef<'py, PyTensor>),
    Host(Host),
}

impl Argument<'_> {
    pub(crate) fn dtype(&self) -> Option<PrimitiveType> {
        match self {
            Self::Tensor(tensor) => Some(tensor.tensor().ok()?.metadata().primitive_type),
            Self::Host(Host::Scalar(value)) => Some(value.primitive_type()),
            _ => None,
        }
    }

    pub(crate) fn shape(&self) -> Option<Vec<usize>> {
        match self {
            Self::Tensor(tensor) => Some(tensor.tensor().ok()?.metadata().shape),
            Self::Host(Host::Scalar(_)) => Some(vec![]),
            _ => None,
        }
    }

    pub(crate) fn metadata(&self) -> PyResult<Option<TensorMetadata>> {
        match self {
            Self::Tensor(tensor) => Ok(Some(tensor.tensor()?.metadata())),
            _ => Ok(None),
        }
    }

    pub(crate) fn scalar(&self) -> Option<ScalarValue> {
        match self {
            Self::Host(Host::Scalar(value)) => Some(*value),
            _ => None,
        }
    }

    pub(crate) fn string(&self) -> Option<&str> {
        match self {
            Self::Host(Host::String(value)) => Some(value),
            _ => None,
        }
    }

    pub(crate) fn snapshot(&self) -> PyResult<Vec<ScalarValue>> {
        match self {
            Self::Host(Host::Scalar(value)) => Ok(vec![*value]),
            Self::Tensor(guard) => {
                let tensor: &KuTensor<'_> = guard.tensor()?;
                let device = tensor.device();
                let values = match tensor.metadata().primitive_type {
                    PrimitiveType::Boolean => device
                        .to_vec::<kurt::Boolean>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::Boolean).collect()),
                    PrimitiveType::Byte => device
                        .to_vec::<i8>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::Byte).collect()),
                    PrimitiveType::I16 => device
                        .to_vec::<i16>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::I16).collect()),
                    PrimitiveType::I32 => device
                        .to_vec::<i32>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::I32).collect()),
                    PrimitiveType::I64 => device
                        .to_vec::<i64>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::I64).collect()),
                    PrimitiveType::F32 => device
                        .to_vec::<f32>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::F32).collect()),
                    PrimitiveType::F64 => device
                        .to_vec::<f64>(tensor)
                        .map(|v| v.into_iter().map(ScalarValue::F64).collect()),
                    PrimitiveType::None => unreachable!(),
                };
                values.map_err(|error| native_error(error, "snapshot builtin operand"))
            }
            _ => Err(PyTypeError::new_err("numeric operand required")),
        }
    }

    pub(crate) fn native<'a>(&'a self) -> PyResult<KuArc<KuObject<'a>>> {
        match self {
            Self::Tensor(tensor) => Ok(KuArc::retain(tensor.tensor()?).into()),
            Self::Host(Host::Scalar(value)) => Ok(KuArc::<KuScalar>::new(*value)
                .map_err(|error| native_error(error, "box scalar operand"))?
                .into()),
            Self::Host(Host::String(value)) => Ok(KuArc::<KuString>::new(value.as_bytes())
                .map_err(|error| native_error(error, "box string operand"))?
                .into()),
            Self::Host(Host::Slice(value)) => Ok(KuArc::<KuSlice>::new(*value)
                .map_err(|error| native_error(error, "box slice operand"))?
                .into()),
        }
    }
}
