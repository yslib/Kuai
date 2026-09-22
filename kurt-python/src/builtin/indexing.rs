use kurt::{PrimitiveType, SliceSpec};
use pyo3::PyErr;
use pyo3::exceptions::{PyTypeError, PyValueError};

pub(crate) use super::validation::checked_output;

pub(crate) fn slice_extent(spec: SliceSpec, extent: usize) -> Result<usize, PyErr> {
    let len = i64::try_from(extent)
        .map_err(|_| PyValueError::new_err("slice axis exceeds int64 range"))?;
    let step = spec.step.map(|n| n.get()).unwrap_or(1);
    if step == 0 {
        return Err(PyValueError::new_err("slice step must not be zero"));
    }
    let start = spec
        .start
        .map(|n| {
            if n < 0 {
                (n as i128 + len as i128).clamp(
                    if step < 0 { -1 } else { 0 },
                    if step < 0 {
                        len as i128 - 1
                    } else {
                        len as i128
                    },
                )
            } else {
                (n as i128).clamp(
                    if step < 0 { -1 } else { 0 },
                    if step < 0 {
                        len as i128 - 1
                    } else {
                        len as i128
                    },
                )
            }
        })
        .unwrap_or(if step < 0 { len as i128 - 1 } else { 0 });
    let stop = spec
        .stop
        .map(|n| {
            if n < 0 {
                (n as i128 + len as i128).clamp(
                    if step < 0 { -1 } else { 0 },
                    if step < 0 {
                        len as i128 - 1
                    } else {
                        len as i128
                    },
                )
            } else {
                (n as i128).clamp(
                    if step < 0 { -1 } else { 0 },
                    if step < 0 {
                        len as i128 - 1
                    } else {
                        len as i128
                    },
                )
            }
        })
        .unwrap_or(if step < 0 { -1 } else { len as i128 });
    let n = if step > 0 {
        if start >= stop {
            0
        } else {
            (stop - start - 1) / step as i128 + 1
        }
    } else if start <= stop {
        0
    } else {
        (start - stop - 1) / -(step as i128) + 1
    };
    usize::try_from(n).map_err(|_| PyValueError::new_err("slice result exceeds supported range"))
}

pub(crate) fn index_tensor(dtype: PrimitiveType, shape: &[usize]) -> Result<usize, PyErr> {
    if !matches!(dtype, PrimitiveType::I32 | PrimitiveType::I64) || shape.len() != 1 {
        return Err(PyTypeError::new_err(
            "at index must be a rank-one I32 or I64 tensor",
        ));
    }
    checked_output(shape, super::validation::width(dtype))
}

pub(crate) fn sequence_length(begin: i64, end: i64, step: i64) -> Result<usize, PyErr> {
    if step == 0 {
        return Err(PyValueError::new_err("seq step must not be zero"));
    }
    if (step > 0 && begin >= end) || (step < 0 && begin <= end) {
        return Ok(0);
    }
    let distance = if step > 0 {
        (end as i128 - begin as i128) as u128
    } else {
        (begin as i128 - end as i128) as u128
    };
    let magnitude = (step as i128).unsigned_abs();
    let count = (distance - 1) / magnitude + 1;
    usize::try_from(count)
        .map_err(|_| PyValueError::new_err("seq result length exceeds supported range"))
}
