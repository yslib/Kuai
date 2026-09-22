use kurt::{Error, Status};
use pyo3::PyErr;
use pyo3::exceptions::{
    PyMemoryError, PyNotImplementedError, PyRuntimeError, PyTypeError, PyValueError,
};

pub(crate) fn native_error(error: Error, context: &str) -> PyErr {
    convert(error, context, false)
}

pub(crate) fn builtin_lookup_error(error: Error, context: &str) -> PyErr {
    convert(error, context, true)
}

fn convert(error: Error, context: &str, builtin_lookup: bool) -> PyErr {
    let message = format!("{context}: {error}");
    match error {
        Error::Runtime(Status::OUT_OF_HOST_MEMORY | Status::OUT_OF_DEVICE_MEMORY) => {
            PyMemoryError::new_err(message)
        }
        Error::InvalidArgument(_) | Error::DeviceMismatch => PyValueError::new_err(message),
        Error::TypeMismatch => PyTypeError::new_err(message),
        Error::Runtime(status)
            if [
                Status::INVALID_ARGUMENT,
                Status::OUT_OF_RANGE,
                Status::INVALID_STATE,
                Status::ALREADY_INITIALIZED,
                Status::NOT_SUPPORTED,
            ]
            .contains(&status) =>
        {
            PyValueError::new_err(message)
        }
        Error::Runtime(status) if status == Status::TYPE_MISMATCH => PyTypeError::new_err(message),
        Error::Runtime(status) if builtin_lookup && status == Status::NOT_FOUND => {
            PyNotImplementedError::new_err(message)
        }
        _ => PyRuntimeError::new_err(message),
    }
}

#[cfg(test)]
mod tests {
    use kurt::{Error, Status};
    use pyo3::{
        Python,
        exceptions::{
            PyMemoryError, PyNotImplementedError, PyRuntimeError, PyTypeError, PyValueError,
        },
    };

    use super::{builtin_lookup_error, native_error};

    #[test]
    fn error_categories_and_context() {
        Python::initialize();
        Python::attach(|py| {
            for status in [Status::OUT_OF_HOST_MEMORY, Status::OUT_OF_DEVICE_MEMORY] {
                let err = native_error(Error::Runtime(status), "allocate");
                assert!(err.is_instance_of::<PyMemoryError>(py));
                assert!(err.to_string().contains("allocate:"));
                assert!(err.to_string().contains(&status.to_string()));
            }
            for error in [
                Error::InvalidArgument("bad input"),
                Error::DeviceMismatch,
                Error::Runtime(Status::INVALID_ARGUMENT),
                Error::Runtime(Status::OUT_OF_RANGE),
                Error::Runtime(Status::INVALID_STATE),
                Error::Runtime(Status::ALREADY_INITIALIZED),
                Error::Runtime(Status::NOT_SUPPORTED),
            ] {
                assert!(native_error(error, "argument").is_instance_of::<PyValueError>(py));
            }
            for error in [Error::TypeMismatch, Error::Runtime(Status::TYPE_MISMATCH)] {
                assert!(native_error(error, "type").is_instance_of::<PyTypeError>(py));
            }
            assert!(
                native_error(Error::Runtime(Status::NOT_FOUND), "device")
                    .is_instance_of::<PyRuntimeError>(py)
            );
            assert!(
                builtin_lookup_error(Error::Runtime(Status::NOT_FOUND), "builtin")
                    .is_instance_of::<PyNotImplementedError>(py)
            );
            assert!(
                native_error(Error::ResourcesInUse, "close").is_instance_of::<PyRuntimeError>(py)
            );
        });
    }
}
