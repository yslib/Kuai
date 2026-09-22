use kurt::{PrimitiveType as P, ScalarValue as S};
use pyo3::PyErr;
use pyo3::exceptions::PyValueError;

fn overflow() -> PyErr {
    PyValueError::new_err("builtin integer arithmetic exceeds native expression range")
}

pub(crate) fn null(v: S) -> bool {
    match v {
        S::None => true,
        S::Boolean(b) => b.value().is_none(),
        S::Byte(n) => n == i8::MIN,
        S::I16(n) => n == i16::MIN,
        S::I32(n) => n == i32::MIN,
        S::I64(n) => n == i64::MIN,
        S::F32(n) => n == f32::MIN,
        S::F64(n) => n == f64::MIN,
    }
}

fn int(v: S) -> Option<i128> {
    match v {
        S::Boolean(b) => Some(if b.value()? { 1 } else { 0 }),
        S::Byte(n) => Some(n as i128),
        S::I16(n) => Some(n as i128),
        S::I32(n) => Some(n as i128),
        S::I64(n) => Some(n as i128),
        _ => None,
    }
}

pub(crate) fn check_sum_values(values: &[S]) -> Result<(), PyErr> {
    let mut state = i64::MIN;
    for &value in values {
        if null(value) {
            continue;
        }
        let mapped =
            int(value).ok_or_else(|| PyValueError::new_err("sum requires integer payload"))? as i64;
        state = if state == i64::MIN {
            mapped
        } else {
            state.checked_add(mapped).ok_or_else(overflow)?
        };
    }
    Ok(())
}

pub(crate) fn checked_pair(
    name: &str,
    left_type: P,
    left: S,
    right_type: P,
    right: S,
) -> Result<(), PyErr> {
    if null(left) || null(right) || !matches!(name, "add" | "sub" | "mul") {
        return Ok(());
    }
    let (Some(a), Some(b)) = (int(left), int(right)) else {
        return Ok(());
    };
    let result = match name {
        "add" => a.checked_add(b),
        "sub" => a.checked_sub(b),
        _ => a.checked_mul(b),
    }
    .ok_or_else(overflow)?;
    let wide = left_type == P::I64 || right_type == P::I64;
    let (min, max) = if wide {
        (i64::MIN as i128, i64::MAX as i128)
    } else {
        (i32::MIN as i128, i32::MAX as i128)
    };
    if !(min..=max).contains(&result) {
        return Err(overflow());
    }
    Ok(())
}

pub(crate) fn checked_conversion(target: P, source: S) -> Result<(), PyErr> {
    if null(source) {
        return Ok(());
    }
    match (target, source) {
        (P::Byte | P::I16 | P::I32 | P::I64, S::F32(x)) => check_round(x as f64),
        (P::Byte | P::I16 | P::I32 | P::I64, S::F64(x)) => check_round(x),
        (P::F32, S::F64(x)) if x.is_finite() && x.abs() > f32::MAX as f64 => Err(
            PyValueError::new_err("float32 conversion is outside finite range"),
        ),
        _ => Ok(()),
    }
}

pub(crate) fn checked_reciprocal(source: S) -> Result<(), PyErr> {
    if let S::F32(value) = source {
        if value == 0.0 || null(source) {
            return Ok(());
        }
        let result = 1.0_f64 / value as f64;
        if result.is_finite() && result.abs() > f32::MAX as f64 {
            return Err(PyValueError::new_err(
                "reciprocal float32 result is outside finite range",
            ));
        }
    }
    Ok(())
}

fn check_round(x: f64) -> Result<(), PyErr> {
    if !x.is_finite() || x.round() < -9223372036854775808.0 || x.round() >= 9223372036854775808.0 {
        return Err(PyValueError::new_err(
            "floating conversion is outside int64 range",
        ));
    }
    Ok(())
}
