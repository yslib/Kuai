use kurt::{
    HasObjectKind, KuArc, KuObject, KuObjectKind, KuScalar, KuString, KuTensor, ScalarValue,
};
use pyo3::IntoPyObjectExt;
use pyo3::exceptions::PyRuntimeError;
use pyo3::prelude::*;
use pyo3::types::PyTuple;

use super::prepare;
use crate::errors::{builtin_lookup_error, native_error};
use crate::instance::PyDevice;
use crate::owned_tensor::{self, BuiltValue, OwnedValue};
use crate::tensor::PyTensor;

fn decode_result<'i>(
    mut results: Vec<Option<KuArc<KuObject<'i>>>>,
    is_on_selected_device: impl FnOnce(&KuTensor<'i>) -> bool,
) -> PyResult<BuiltValue<'i>> {
    if results.len() != 1 {
        return Err(PyRuntimeError::new_err(format!(
            "builtin returned {} results; expected one",
            results.len()
        )));
    }
    let result = results
        .pop()
        .flatten()
        .ok_or_else(|| PyRuntimeError::new_err("builtin returned a null result"))?;
    match result.kind() {
        KuObjectKind::Tensor => {
            let tensor = KuArc::<KuTensor>::try_from(result)
                .map_err(|error| native_error(error, "decode builtin tensor"))?;
            if !is_on_selected_device(&tensor) {
                return Err(PyRuntimeError::new_err(
                    "builtin tensor result belongs to another device",
                ));
            }
            Ok(BuiltValue::Tensor(tensor))
        }
        KuObjectKind::Scalar => {
            let scalar = KuArc::<KuScalar>::try_from(result)
                .map_err(|error| native_error(error, "decode builtin scalar"))?;
            Ok(BuiltValue::Scalar(scalar.value()))
        }
        KuObjectKind::String => {
            let string = KuArc::<KuString>::try_from(result)
                .map_err(|error| native_error(error, "decode builtin string"))?;
            let text = string
                .to_str()
                .map_err(|error| native_error(error, "decode builtin UTF-8 string"))?
                .to_owned();
            Ok(BuiltValue::String(text))
        }
        KuObjectKind::Array | KuObjectKind::Slice => Err(PyRuntimeError::new_err(
            "builtin returned an unsupported object kind",
        )),
    }
}

fn scalar_to_python(py: Python<'_>, value: ScalarValue) -> PyResult<Py<PyAny>> {
    match value {
        ScalarValue::None => Ok(py.None()),
        ScalarValue::Boolean(value) => match value.value() {
            Some(value) => value.into_py_any(py),
            None => Ok(py.None()),
        },
        ScalarValue::Byte(value) => value.into_py_any(py),
        ScalarValue::I16(value) => value.into_py_any(py),
        ScalarValue::I32(value) => value.into_py_any(py),
        ScalarValue::I64(value) => value.into_py_any(py),
        ScalarValue::F32(value) => value.into_py_any(py),
        ScalarValue::F64(value) => value.into_py_any(py),
    }
}

#[pyfunction]
pub(crate) fn _invoke_builtin<'py>(
    py: Python<'py>,
    device: &Bound<'py, PyDevice>,
    name: &str,
    operands: &Bound<'py, PyTuple>,
) -> PyResult<Py<PyAny>> {
    let prepared = prepare(py, device, name, operands)?;
    let instance = device.borrow().instance(py)?;
    let native = instance.bind(py).borrow().clone_native()?;
    let id = device.borrow().id();
    let value = owned_tensor::build(native, |owner| {
        let target = owner
            .device(id)
            .map_err(|error| native_error(error, "lookup builtin device"))?;
        let builtin = owner
            .builtin(&prepared.name)
            .map_err(|error| builtin_lookup_error(error, "lookup builtin"))?;
        let mut context = target
            .frame_context()
            .map_err(|error| native_error(error, "create builtin context"))?;
        let arguments = prepared.native_arguments()?;
        let positional: Vec<Option<&KuObject<'_>>> =
            arguments.iter().map(|arg| Some(&**arg)).collect();
        // SAFETY: `prepare` admits only audited CPU builtins after checking arity,
        // native device identity, owned initialized tensor metadata/layout, and
        // each family's type, rank, shape, allocation and applicable arithmetic
        // domains. Cumsum relies on the native arithmetic domain and grouping
        // assumptions; these guards do not establish every numeric precondition.
        // Every positional argument is a live owned object, including a boxed
        // NONE scalar for Python None. The audited builtin neither mutates nor
        // retains these immutable inputs and produces no uninitialized storage.
        // `call_unchecked` synchronizes the
        // device on success and failure before arguments or results can drop.
        let results = unsafe { builtin.call_unchecked(&mut context, &positional, &[]) }
            .map_err(|error| native_error(error, "invoke builtin"))?;
        decode_result(results, |tensor| tensor.is_on(&target))
    })?;
    match value {
        OwnedValue::Tensor(tensor) => Ok(Py::new(
            py,
            PyTensor::materialize(tensor, prepared.device.clone_ref(py)),
        )?
        .into_any()),
        OwnedValue::Scalar(value) => scalar_to_python(py, value),
        OwnedValue::String(value) => value.into_py_any(py),
    }
}

#[cfg(test)]
mod tests {
    use kurt::{Boolean, KuArc, KuObject, KuScalar, KuString, ScalarValue};
    use pyo3::{Python, exceptions::PyRuntimeError, types::PyAnyMethods};

    use super::scalar_to_python;
    use crate::owned_tensor::BuiltValue;

    fn assert_runtime_error<T>(result: pyo3::PyResult<T>) {
        let error = result.err().expect("expected a runtime protocol error");
        Python::initialize();
        Python::attach(|py| assert!(error.is_instance_of::<PyRuntimeError>(py)));
    }

    #[test]
    fn scalar_materialization_preserves_null_and_payloads() {
        Python::initialize();
        Python::attach(|py| {
            for value in [ScalarValue::None, ScalarValue::Boolean(Boolean::NULL)] {
                assert!(scalar_to_python(py, value).unwrap().bind(py).is_none());
            }
            assert!(
                scalar_to_python(py, ScalarValue::Boolean(Boolean::TRUE))
                    .unwrap()
                    .bind(py)
                    .extract::<bool>()
                    .unwrap()
            );
            assert_eq!(
                scalar_to_python(py, ScalarValue::I64(i64::MIN))
                    .unwrap()
                    .bind(py)
                    .extract::<i64>()
                    .unwrap(),
                i64::MIN
            );
            assert_eq!(
                scalar_to_python(py, ScalarValue::F32(f32::MIN))
                    .unwrap()
                    .bind(py)
                    .extract::<f64>()
                    .unwrap(),
                f32::MIN as f64
            );
        });
    }

    #[test]
    fn result_protocol_rejects_count_null_and_unsupported_kind() {
        assert_runtime_error(super::decode_result(Vec::new(), |_| unreachable!()));
        assert_runtime_error(super::decode_result(vec![None], |_| unreachable!()));
        let first: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(1_i64).unwrap().into();
        let second: KuArc<KuObject<'_>> = KuArc::<KuString>::new("x").unwrap().into();
        assert_runtime_error(super::decode_result(
            vec![Some(first), Some(second)],
            |_| unreachable!(),
        ));
        let slice: KuArc<KuObject<'_>> = KuArc::<kurt::KuSlice>::new(kurt::SliceSpec {
            start: None,
            stop: None,
            step: None,
        })
        .unwrap()
        .into();
        assert_runtime_error(super::decode_result(vec![Some(slice)], |_| unreachable!()));
    }

    #[test]
    fn host_results_decode_owned_scalar_and_strict_utf8_string() {
        let scalar: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(17_i16).unwrap().into();
        assert!(matches!(
            super::decode_result(vec![Some(scalar)], |_| unreachable!()).unwrap(),
            BuiltValue::Scalar(ScalarValue::I16(17))
        ));

        let string: KuArc<KuObject<'_>> = KuArc::<KuString>::new("Kuai").unwrap().into();
        assert!(matches!(
            super::decode_result(vec![Some(string)], |_| unreachable!()).unwrap(),
            BuiltValue::String(value) if value == "Kuai"
        ));

        let invalid: KuArc<KuObject<'_>> = KuArc::<KuString>::new([0xff]).unwrap().into();
        assert_runtime_error(super::decode_result(
            vec![Some(invalid)],
            |_| unreachable!(),
        ));
    }
}
