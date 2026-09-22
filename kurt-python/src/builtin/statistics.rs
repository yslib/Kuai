use kurt::{PrimitiveType as P, ScalarValue as S};
use pyo3::PyErr;
use pyo3::exceptions::PyValueError;

use super::arithmetic;

fn count_error() -> PyErr {
    PyValueError::new_err("statistics count arithmetic exceeds native range")
}
fn cast_error() -> PyErr {
    PyValueError::new_err("statistics float32 intermediate is outside finite cast range")
}

fn add_count(a: i64, b: i64, bits: u32) -> Result<i64, PyErr> {
    let value = a.checked_add(b).ok_or_else(count_error)?;
    if bits == 32 && value > i32::MAX as i64 {
        return Err(count_error());
    }
    Ok(value)
}

fn mul_count(a: i64, b: i64, bits: u32) -> Result<i64, PyErr> {
    let value = a.checked_mul(b).ok_or_else(count_error)?;
    if bits == 32 && !(i32::MIN as i64..=i32::MAX as i64).contains(&value) {
        return Err(count_error());
    }
    Ok(value)
}

pub(crate) fn skew_final_count(count: i64, m2: f64) -> Result<(), PyErr> {
    if count < 3 || m2 == 0.0 {
        return Ok(());
    }
    mul_count(count, count.checked_sub(1).ok_or_else(count_error)?, 32)?;
    Ok(())
}

pub(crate) fn kurtosis_merge_count(left: i64, right: i64) -> Result<(), PyErr> {
    let count = add_count(left, right, 64)?;
    let square = mul_count(count, count, 64)?;
    mul_count(square, count, 64)?;
    let l2 = mul_count(left, left, 64)?;
    let lr = mul_count(left, right, 64)?;
    let r2 = mul_count(right, right, 64)?;
    l2.checked_sub(lr)
        .and_then(|n| n.checked_add(r2))
        .ok_or_else(count_error)?;
    Ok(())
}

fn null(dtype: P) -> f64 {
    if dtype == P::F32 {
        f32::MIN as f64
    } else {
        f64::MIN
    }
}
fn is_null(dtype: P, value: f64) -> bool {
    value == null(dtype)
}
fn accum(dtype: P, value: f64) -> f64 {
    if dtype == P::F32 {
        (value as f32) as f64
    } else {
        value
    }
}
fn count_float(dtype: P, count: i64) -> f64 {
    if dtype == P::F32 {
        (count as f32) as f64
    } else {
        count as f64
    }
}
fn raw_add(dtype: P, a: f64, b: f64) -> f64 {
    if dtype == P::F32 {
        ((a as f32) + (b as f32)) as f64
    } else {
        a + b
    }
}
fn raw_sub(dtype: P, a: f64, b: f64) -> f64 {
    if dtype == P::F32 {
        ((a as f32) - (b as f32)) as f64
    } else {
        a - b
    }
}
fn raw_mul(dtype: P, a: f64, b: f64) -> f64 {
    if dtype == P::F32 {
        ((a as f32) * (b as f32)) as f64
    } else {
        a * b
    }
}
fn raw_div(dtype: P, a: f64, b: f64) -> f64 {
    if dtype == P::F32 {
        ((a as f32) / (b as f32)) as f64
    } else {
        a / b
    }
}
fn narrow_mixed(dtype: P, value: f64) -> Result<f64, PyErr> {
    if dtype == P::F32 && value.is_finite() && value.abs() > f32::MAX as f64 {
        return Err(cast_error());
    }
    Ok(accum(dtype, value))
}
fn number(dtype: P, value: S) -> f64 {
    if dtype == P::F32 {
        return match value {
            S::Boolean(v) => {
                if v.value().unwrap_or(false) {
                    1.0
                } else {
                    0.0
                }
            }
            S::Byte(v) => (v as f32) as f64,
            S::I16(v) => (v as f32) as f64,
            S::I32(v) => (v as f32) as f64,
            S::I64(v) => (v as f32) as f64,
            S::F32(v) => v as f64,
            S::F64(v) => (v as f32) as f64,
            S::None => unreachable!(),
        };
    }
    match value {
        S::Boolean(v) => {
            if v.value().unwrap_or(false) {
                1.0
            } else {
                0.0
            }
        }
        S::Byte(v) => v as f64,
        S::I16(v) => v as f64,
        S::I32(v) => v as f64,
        S::I64(v) => v as f64,
        S::F32(v) => v as f64,
        S::F64(v) => v,
        S::None => unreachable!(),
    }
}
fn add(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || is_null(dtype, b) {
        null(dtype)
    } else {
        raw_add(dtype, a, b)
    }
}
fn sub(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || is_null(dtype, b) {
        null(dtype)
    } else {
        raw_sub(dtype, a, b)
    }
}
fn mul(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || is_null(dtype, b) {
        null(dtype)
    } else {
        raw_mul(dtype, a, b)
    }
}
fn div(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || is_null(dtype, b) || b == 0.0 {
        null(dtype)
    } else {
        raw_div(dtype, a, b)
    }
}
fn mixed_mul(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || b == f64::MIN {
        f64::MIN
    } else {
        a * b
    }
}
fn mixed_add(dtype: P, a: f64, b: f64) -> f64 {
    if is_null(dtype, a) || b == f64::MIN {
        f64::MIN
    } else {
        a + b
    }
}

#[derive(Clone, Copy)]
struct State {
    mean: f64,
    m2: f64,
    m3: f64,
    m4: f64,
    count: i64,
}

impl State {
    fn initial(dtype: P, name: &str) -> Self {
        Self {
            mean: if name == "kurtosis" { 0.0 } else { null(dtype) },
            m2: null(dtype),
            m3: 0.0,
            m4: 0.0,
            count: 0,
        }
    }
    fn singleton(dtype: P, value: S, name: &str) -> Self {
        if arithmetic::null(value) {
            return Self::initial(dtype, name);
        }
        Self {
            mean: number(dtype, value),
            m2: 0.0,
            m3: 0.0,
            m4: 0.0,
            count: 1,
        }
    }
    fn is_null(self, dtype: P, name: &str) -> bool {
        if name == "kurtosis" {
            is_null(dtype, self.m2)
        } else if name == "var" {
            is_null(dtype, self.mean) || is_null(dtype, self.m2)
        } else {
            is_null(dtype, self.mean)
        }
    }
}

pub(crate) fn check(name: &str, dtype: P, shape: &[usize], values: &[S]) -> Result<(), PyErr> {
    let axis = shape.first().copied().unwrap_or(1);
    if axis == 0 {
        return Ok(());
    }
    for segment in values.chunks(axis) {
        let mut state = State::initial(dtype, name);
        for &value in segment {
            state = step(name, dtype, state, value)?;
        }
        match name {
            "skew" if !state.is_null(dtype, name) => skew_final_count(state.count, state.m2)?,
            "kurtosis" if state.count > 3 && !state.is_null(dtype, name) && state.m2 != 0.0 => {
                add_count(state.count, 1, 64)?;
                mul_count(3, state.count - 1, 64)?;
            }
            _ => {}
        }
    }
    Ok(())
}

fn step(name: &str, dtype: P, state: State, value: S) -> Result<State, PyErr> {
    let mapped = State::singleton(dtype, value, name);
    if state.is_null(dtype, name) {
        return Ok(mapped);
    }
    if mapped.is_null(dtype, name) {
        return Ok(state);
    }
    merge(name, dtype, state, mapped)
}

fn merge(name: &str, dtype: P, x: State, y: State) -> Result<State, PyErr> {
    let count_bits = if matches!(name, "std" | "var" | "skew") {
        32
    } else {
        64
    };
    let count = add_count(x.count, y.count, count_bits)?;
    if name == "avg" {
        let delta = sub(dtype, y.mean, x.mean);
        let mean = add(
            dtype,
            x.mean,
            div(
                dtype,
                mul(dtype, delta, count_float(dtype, y.count)),
                count_float(dtype, count),
            ),
        );
        return Ok(State { mean, count, ..x });
    }
    if matches!(name, "std" | "var") {
        mul_count(x.count, y.count, 32)?;
        let delta = sub(dtype, x.mean, y.mean);
        let delta2 = mul(dtype, delta, delta);
        let weighted = mixed_mul(dtype, delta, x.count as f64 / count as f64);
        let mean = narrow_mixed(dtype, mixed_add(dtype, y.mean, weighted))?;
        let ssr = add(dtype, x.m2, y.m2);
        let tmp = narrow_mixed(
            dtype,
            if is_null(dtype, delta2) {
                f64::MIN
            } else {
                (x.count * y.count) as f64 / count as f64 * delta2
            },
        )?;
        let m2 = add(dtype, ssr, tmp);
        return Ok(State {
            mean,
            m2,
            count,
            ..x
        });
    }
    if name == "skew" {
        mul_count(count, count, 64)?;
        x.count
            .checked_sub(y.count)
            .filter(|n| *n >= i32::MIN as i64)
            .ok_or_else(count_error)?;
    } else {
        kurtosis_merge_count(x.count, y.count)?;
    }
    let delta = raw_sub(dtype, y.mean, x.mean);
    let delta2 = raw_mul(dtype, delta, delta);
    let delta3 = raw_mul(dtype, delta2, delta);
    let delta4 = raw_mul(dtype, delta3, delta);
    let cf = count_float(dtype, count);
    let xf = count_float(dtype, x.count);
    let yf = count_float(dtype, y.count);
    let count2 = count_float(dtype, mul_count(count, count, 64)?);
    let count3 = if name == "kurtosis" {
        count_float(dtype, mul_count(count, mul_count(count, count, 64)?, 64)?)
    } else {
        0.0
    };
    let mean = raw_add(dtype, x.mean, raw_div(dtype, raw_mul(dtype, delta, yf), cf));
    let m2base = raw_add(dtype, x.m2, y.m2);
    let m2term = raw_div(dtype, raw_mul(dtype, raw_mul(dtype, delta2, xf), yf), cf);
    let m2 = raw_add(dtype, m2base, m2term);
    let m3base = raw_add(dtype, x.m3, y.m3);
    let count_diff = count_float(dtype, x.count - y.count);
    let m3term1 = raw_div(
        dtype,
        raw_mul(
            dtype,
            raw_mul(dtype, raw_mul(dtype, delta3, xf), yf),
            count_diff,
        ),
        count2,
    );
    let cross = raw_sub(dtype, raw_mul(dtype, xf, y.m2), raw_mul(dtype, yf, x.m2));
    let m3term2 = raw_div(dtype, raw_mul(dtype, raw_mul(dtype, 3.0, delta), cross), cf);
    let m3 = raw_add(dtype, raw_add(dtype, m3base, m3term1), m3term2);
    let m4 = if name == "kurtosis" {
        let l2 = mul_count(x.count, x.count, 64)?;
        let lr = mul_count(x.count, y.count, 64)?;
        let r2 = mul_count(y.count, y.count, 64)?;
        let expression = l2
            .checked_sub(lr)
            .and_then(|n| n.checked_add(r2))
            .ok_or_else(count_error)?;
        let m4base = raw_add(dtype, x.m4, y.m4);
        let m4term1 = raw_div(
            dtype,
            raw_mul(
                dtype,
                raw_mul(dtype, raw_mul(dtype, delta4, xf), yf),
                count_float(dtype, expression),
            ),
            count3,
        );
        let weighted_m2 = raw_add(
            dtype,
            raw_mul(dtype, count_float(dtype, l2), y.m2),
            raw_mul(dtype, count_float(dtype, r2), x.m2),
        );
        let m4term2 = raw_div(
            dtype,
            raw_mul(dtype, raw_mul(dtype, 6.0, delta2), weighted_m2),
            count2,
        );
        let weighted_m3 = raw_sub(dtype, raw_mul(dtype, xf, y.m3), raw_mul(dtype, yf, x.m3));
        let m4term3 = raw_div(
            dtype,
            raw_mul(dtype, raw_mul(dtype, 4.0, delta), weighted_m3),
            cf,
        );
        raw_add(
            dtype,
            raw_add(dtype, raw_add(dtype, m4base, m4term1), m4term2),
            m4term3,
        )
    } else {
        x.m4
    };
    Ok(State {
        mean,
        m2,
        m3,
        m4,
        count,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn std_var_f32_mixed_null_narrowing_rejects() {
        let values = [S::F32(0.0), S::F32(f32::MAX)];
        assert!(check("std", P::F32, &[2], &values).is_err());
        assert!(check("var", P::F32, &[2], &values).is_err());
    }

    #[test]
    fn var_ssr_sentinel_resets_but_std_only_tests_mean() {
        let state = State {
            mean: 1.0,
            m2: f32::MIN as f64,
            m3: 0.0,
            m4: 0.0,
            count: 100,
        };
        assert_eq!(step("var", P::F32, state, S::F32(2.0)).unwrap().count, 1);
        assert_eq!(step("std", P::F32, state, S::F32(2.0)).unwrap().count, 101);
    }

    #[test]
    fn null_mean_resets_count_for_mean_state() {
        let state = State {
            mean: f32::MIN as f64,
            m2: 0.0,
            m3: 0.0,
            m4: 0.0,
            count: i64::MAX,
        };
        assert_eq!(step("avg", P::F32, state, S::F32(3.0)).unwrap().count, 1);
    }
}
