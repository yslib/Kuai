use kurt::{Boolean, PrimitiveType};
use pyo3::buffer::{ElementType, PyUntypedBuffer};
use pyo3::exceptions::PyValueError;
use pyo3::prelude::*;
use pyo3::types::{PyBytes, PyDict, PyMemoryView, PyTuple};
use std::ffi::{CStr, CString};

use crate::errors::native_error;
use crate::instance::PyDevice;
use crate::owned_tensor::{self, BuiltValue, OwnedValue};
use crate::tensor::{PyTensor, dtype_name, primitive_from_id};

fn invalid(message: &'static str) -> PyErr {
    PyValueError::new_err(message)
}

fn primitive_width(primitive: PrimitiveType) -> usize {
    match primitive {
        PrimitiveType::Boolean | PrimitiveType::Byte => 1,
        PrimitiveType::I16 => 2,
        PrimitiveType::I32 | PrimitiveType::F32 => 4,
        PrimitiveType::I64 | PrimitiveType::F64 => 8,
        PrimitiveType::None => unreachable!(),
    }
}

fn check_format(primitive: PrimitiveType, buffer_format: &CStr, item_size: usize) -> PyResult<()> {
    let format = buffer_format.to_bytes();
    let (prefix, code) = match format {
        [code] => (b'@', *code),
        [prefix, code] => (*prefix, *code),
        _ => return Err(invalid("host buffer format does not match requested dtype")),
    };
    let native_endian = match prefix {
        b'@' | b'=' => true,
        b'<' => cfg!(target_endian = "little"),
        b'>' | b'!' => cfg!(target_endian = "big"),
        _ => false,
    };
    if !native_endian && primitive_width(primitive) > 1 {
        return Err(invalid("host buffer must use native byte order"));
    }
    let element = ElementType::from_format(buffer_format);
    let compatible = match primitive {
        PrimitiveType::Boolean => element == ElementType::Bool && code == b'?',
        PrimitiveType::Byte => element == ElementType::SignedInteger { bytes: 1 },
        PrimitiveType::I16 => element == ElementType::SignedInteger { bytes: 2 },
        PrimitiveType::I32 => element == ElementType::SignedInteger { bytes: 4 },
        PrimitiveType::I64 => element == ElementType::SignedInteger { bytes: 8 },
        PrimitiveType::F32 => element == ElementType::Float { bytes: 4 },
        PrimitiveType::F64 => element == ElementType::Float { bytes: 8 },
        PrimitiveType::None => false,
    };
    if !compatible || item_size != primitive_width(primitive) {
        return Err(invalid("host buffer format does not match requested dtype"));
    }
    Ok(())
}

fn checked_shape(shape: &[i64], width: usize) -> PyResult<(Vec<usize>, Vec<usize>, usize)> {
    if shape.len() > 8 {
        return Err(invalid(
            "kuai tensor rank exceeds the supported maximum of 8",
        ));
    }
    let mut extents = Vec::with_capacity(shape.len());
    let mut count = 1_usize;
    let mut expected_strides = Vec::with_capacity(shape.len());
    for &extent in shape {
        let Ok(actual_extent) = usize::try_from(extent) else {
            return Err(invalid(
                "host buffer shape does not match requested tensor shape",
            ));
        };
        let expected = count
            .checked_mul(width)
            .filter(|&n| n <= isize::MAX as usize)
            .ok_or_else(|| invalid("host buffer byte strides exceed the supported range"))?;
        expected_strides.push(expected);
        extents.push(actual_extent);
        count = count
            .checked_mul(actual_extent)
            .filter(|&n| n <= isize::MAX as usize)
            .ok_or_else(|| invalid("host buffer element count exceeds the supported range"))?;
    }
    let bytes = count
        .checked_mul(width)
        .filter(|&n| n <= isize::MAX as usize)
        .ok_or_else(|| invalid("host buffer byte count exceeds the supported range"))?;
    Ok((extents, expected_strides, bytes))
}

fn checked_layout(shape: &[i64], buffer: &PyUntypedBuffer, width: usize) -> PyResult<Vec<usize>> {
    let (extents, expected_strides, bytes) = checked_shape(shape, width)?;
    if buffer.dimensions() != shape.len() || buffer.shape() != extents {
        return Err(invalid(
            "host buffer shape does not match requested tensor shape",
        ));
    }
    if buffer
        .suboffsets()
        .is_some_and(|offsets| offsets.iter().any(|&n| n >= 0))
    {
        return Err(invalid("indirect host buffers are unsupported"));
    }
    if bytes != buffer.len_bytes() {
        return Err(invalid(
            "host buffer byte count does not match requested tensor shape",
        ));
    }
    if bytes != 0 {
        for ((&extent, &stride), &expected) in
            extents.iter().zip(buffer.strides()).zip(&expected_strides)
        {
            if extent > 1 && stride != expected as isize {
                return Err(invalid(
                    "host buffer must use column-major contiguous storage",
                ));
            }
        }
    }
    Ok(extents)
}

fn decode<const N: usize, T>(bytes: &[u8], convert: impl Fn([u8; N]) -> T) -> Vec<T> {
    bytes
        .as_chunks::<N>()
        .0
        .iter()
        .map(|chunk| convert(*chunk))
        .collect()
}

#[pyfunction]
pub(crate) fn _to_device(
    py: Python<'_>,
    device: Py<PyDevice>,
    host: &Bound<'_, PyAny>,
    primitive_type: i32,
    shape: Vec<i64>,
) -> PyResult<Py<PyTensor>> {
    let primitive = primitive_from_id(primitive_type)?;
    // Keep this real memoryview alive until its format and layout have been
    // validated and its entire contents have been copied into owned bytes.
    let view = PyMemoryView::from(host)?;
    let direct_buffer = PyUntypedBuffer::get(view.as_any());
    let extents = match direct_buffer {
        Ok(buffer) => {
            check_format(primitive, buffer.format(), buffer.item_size())?;
            checked_layout(&shape, &buffer, primitive_width(primitive))?
        }
        Err(_) if view.getattr("ndim")?.extract::<usize>()? == 0 => {
            // PyO3 rejects CPython's valid rank-zero Py_buffer because its
            // shape pointer is null. The genuine memoryview supplies its
            // scalar metadata; a byte cast gives PyO3 a rank-one buffer to
            // validate and checks the actual readable byte extent.
            let format = CString::new(view.getattr("format")?.extract::<String>()?)
                .map_err(|_| invalid("host buffer format does not match requested dtype"))?;
            let item_size = view.getattr("itemsize")?.extract::<usize>()?;
            check_format(primitive, &format, item_size)?;
            let (_, _, expected_bytes) = checked_shape(&shape, primitive_width(primitive))?;
            if !shape.is_empty() || !view.getattr("shape")?.extract::<Vec<usize>>()?.is_empty() {
                return Err(invalid(
                    "host buffer shape does not match requested tensor shape",
                ));
            }
            let byte_view = view.call_method1("cast", ("B",))?;
            let byte_buffer = PyUntypedBuffer::get(&byte_view)?;
            if byte_buffer.len_bytes() != expected_bytes
                || view.getattr("nbytes")?.extract::<usize>()? != expected_bytes
            {
                return Err(invalid(
                    "host buffer byte count does not match requested tensor shape",
                ));
            }
            vec![]
        }
        Err(error) => return Err(error),
    };
    let kwargs = PyDict::new(py);
    kwargs.set_item("order", "F")?;
    let snapshot = view.call_method("tobytes", (), Some(&kwargs))?;
    let bytes = snapshot.cast::<PyBytes>()?.as_bytes();
    let instance = device.bind(py).borrow().instance(py)?;
    let native = instance.bind(py).borrow().clone_native()?;
    let id = device.bind(py).borrow().id();
    let value = owned_tensor::build(native, |owner| {
        let target = owner
            .device(id)
            .map_err(|error| native_error(error, "lookup device"))?;
        let tensor = match primitive {
            PrimitiveType::Boolean => target.tensor_from_slice(
                &extents,
                &bytes
                    .iter()
                    .map(|&b| Boolean::from(b != 0))
                    .collect::<Vec<_>>(),
            ),
            PrimitiveType::Byte => target.tensor_from_slice(
                &extents,
                &bytes.iter().map(|&b| b as i8).collect::<Vec<_>>(),
            ),
            PrimitiveType::I16 => {
                target.tensor_from_slice(&extents, &decode(bytes, i16::from_ne_bytes))
            }
            PrimitiveType::I32 => {
                target.tensor_from_slice(&extents, &decode(bytes, i32::from_ne_bytes))
            }
            PrimitiveType::I64 => {
                target.tensor_from_slice(&extents, &decode(bytes, i64::from_ne_bytes))
            }
            PrimitiveType::F32 => {
                target.tensor_from_slice(&extents, &decode(bytes, f32::from_ne_bytes))
            }
            PrimitiveType::F64 => {
                target.tensor_from_slice(&extents, &decode(bytes, f64::from_ne_bytes))
            }
            PrimitiveType::None => unreachable!(),
        }
        .map_err(|error| native_error(error, "upload tensor"))?;
        Ok::<_, PyErr>(BuiltValue::Tensor(tensor))
    })?;
    let OwnedValue::Tensor(tensor) = value else {
        unreachable!()
    };
    Py::new(py, PyTensor::materialize(tensor, device))
}

fn append_native<const N: usize, T: Copy>(
    values: &[T],
    bytes: &mut Vec<u8>,
    convert: impl Fn(T) -> [u8; N],
) {
    for &value in values {
        bytes.extend_from_slice(&convert(value));
    }
}

#[pyfunction]
pub(crate) fn _to_host(py: Python<'_>, tensor: Py<PyTensor>) -> PyResult<Py<PyAny>> {
    let binding = tensor.bind(py);
    let source = binding.borrow();
    let native = source.tensor()?;
    let metadata = native.metadata();
    let device = native.device();
    let mut bytes = Vec::new();
    match metadata.primitive_type {
        PrimitiveType::Boolean => append_native(
            &device
                .to_vec::<Boolean>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| [u8::from(v.value().unwrap_or(true))],
        ),
        PrimitiveType::Byte => append_native(
            &device
                .to_vec::<i8>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| [v as u8],
        ),
        PrimitiveType::I16 => append_native(
            &device
                .to_vec::<i16>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| v.to_ne_bytes(),
        ),
        PrimitiveType::I32 => append_native(
            &device
                .to_vec::<i32>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| v.to_ne_bytes(),
        ),
        PrimitiveType::I64 => append_native(
            &device
                .to_vec::<i64>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| v.to_ne_bytes(),
        ),
        PrimitiveType::F32 => append_native(
            &device
                .to_vec::<f32>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| v.to_ne_bytes(),
        ),
        PrimitiveType::F64 => append_native(
            &device
                .to_vec::<f64>(native)
                .map_err(|e| native_error(e, "download tensor"))?,
            &mut bytes,
            |v| v.to_ne_bytes(),
        ),
        PrimitiveType::None => unreachable!(),
    }
    let np = py.import("numpy")?;
    let dtype = np.call_method1("dtype", (dtype_name(metadata.primitive_type),))?;
    let buffer = PyBytes::new(py, &bytes);
    let array = np
        .call_method1("frombuffer", (buffer, dtype))?
        .call_method0("copy")?;
    let kwargs = PyDict::new(py);
    kwargs.set_item("order", "F")?;
    let shape = PyTuple::new(py, metadata.shape)?;
    Ok(array
        .call_method("reshape", (shape,), Some(&kwargs))?
        .unbind())
}

#[cfg(test)]
mod tests {
    use super::checked_shape;

    #[test]
    fn shape_products_reject_overflow_without_allocating() {
        assert!(checked_shape(&[i64::MAX, 2], 1).is_err());
        assert!(checked_shape(&[i64::MAX], 8).is_err());
        assert!(checked_shape(&[1; 9], 1).is_err());
        assert!(checked_shape(&[-1], 1).is_err());
        assert_eq!(checked_shape(&[], 8).unwrap().2, 8);
        assert_eq!(checked_shape(&[0, 2], 8).unwrap().2, 0);
    }
}
