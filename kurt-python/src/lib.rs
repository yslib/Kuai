use kurt::{DeviceType, KuInstance, Vendor};
use pyo3::exceptions::PyRuntimeError;
use pyo3::prelude::*;
use pyo3::types::PyDict;

mod builtin;
mod errors;
mod instance;
mod owned_tensor;
mod tensor;
mod transfers;

fn runtime_error(error: impl std::fmt::Display) -> PyErr {
    PyRuntimeError::new_err(error.to_string())
}

#[pyfunction]
fn runtime_info(py: Python<'_>) -> PyResult<Py<PyDict>> {
    let instance = KuInstance::new(Vendor::Cpu).map_err(runtime_error)?;
    let info = instance.default_device().info();
    let close = instance.close();
    let (device_type, device_id) = info.map_err(runtime_error)?;
    close.map_err(runtime_error)?;

    if device_type != DeviceType::Cpu {
        return Err(PyRuntimeError::new_err(format!(
            "expected CPU default device, got {device_type:?}"
        )));
    }

    let info = PyDict::new(py);
    info.set_item("device_type", "cpu")?;
    info.set_item("device_id", device_id)?;
    Ok(info.unbind())
}

#[pymodule]
fn _native(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add("__version__", env!("CARGO_PKG_VERSION"))?;
    module.add_function(wrap_pyfunction!(runtime_info, module)?)?;
    module.add_class::<instance::PyInstance>()?;
    module.add_class::<instance::PyDevice>()?;
    module.add_class::<tensor::PyTensor>()?;
    tensor::primitive_types(module)?;
    module.add_function(wrap_pyfunction!(transfers::_to_device, module)?)?;
    module.add_function(wrap_pyfunction!(transfers::_to_host, module)?)?;
    module.add_function(wrap_pyfunction!(builtin::_invoke_builtin, module)?)?;
    module.gil_used(true)?;
    Ok(())
}
