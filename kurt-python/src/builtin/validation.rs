use kurt::{PrimitiveType, TensorMetadata};
use pyo3::PyErr;
use pyo3::exceptions::{PyNotImplementedError, PyTypeError, PyValueError};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Family {
    Unary,
    Binary,
    Conversion(PrimitiveType),
    Sum,
    Cumsum,
    Statistics,
    MinMax,
    At,
    Mask,
    Seq,
    Normal,
}

pub(crate) fn descriptor(name: &str, arity: usize) -> Result<Family, PyErr> {
    let (family, valid) = match name {
        "abs" | "acos" | "acosh" | "asin" | "asinh" | "atan" | "atanh" | "cbrt" | "cos"
        | "cosh" | "exp" | "log" | "neg" | "reciprocal" | "sin" | "sinh" | "sqrt" | "tan"
        | "tanh" => (Family::Unary, arity == 1),
        "add" | "sub" | "mul" | "div" | "ratio" | "mod" | "xor" | "bitAnd" | "bitOr" | "bitXor"
        | "eq" | "ne" | "ge" | "gt" | "le" | "lt" | "pow" => (Family::Binary, arity == 2),
        "bool" => (Family::Conversion(PrimitiveType::Boolean), arity == 1),
        "char" => (Family::Conversion(PrimitiveType::Byte), arity == 1),
        "short" => (Family::Conversion(PrimitiveType::I16), arity == 1),
        "int" => (Family::Conversion(PrimitiveType::I32), arity == 1),
        "long" => (Family::Conversion(PrimitiveType::I64), arity == 1),
        "float" => (Family::Conversion(PrimitiveType::F32), arity == 1),
        "double" => (Family::Conversion(PrimitiveType::F64), arity == 1),
        "sum" => (Family::Sum, arity == 1),
        "cumsum" => (Family::Cumsum, arity == 1),
        "avg" | "std" | "var" | "skew" | "kurtosis" => (Family::Statistics, arity == 1),
        "min" | "max" => (Family::MinMax, arity == 1 || arity == 2),
        "at" => (Family::At, (2..=9).contains(&arity)),
        "mask" => (Family::Mask, arity == 2),
        "seq" => (Family::Seq, (2..=4).contains(&arity)),
        "normal" => (Family::Normal, arity == 4),
        "dot" => {
            return Err(PyNotImplementedError::new_err(
                "dot is not registered on the CPU backend",
            ));
        }
        _ => {
            return Err(PyNotImplementedError::new_err(format!(
                "unaudited builtin: {name}"
            )));
        }
    };
    if !valid {
        return Err(PyTypeError::new_err(format!(
            "{name} does not accept {arity} operands"
        )));
    }
    Ok(family)
}

pub(crate) fn width(dtype: PrimitiveType) -> usize {
    match dtype {
        PrimitiveType::Boolean | PrimitiveType::Byte => 1,
        PrimitiveType::I16 => 2,
        PrimitiveType::I32 | PrimitiveType::F32 => 4,
        PrimitiveType::I64 | PrimitiveType::F64 => 8,
        PrimitiveType::None => 0,
    }
}

fn order(dtype: PrimitiveType) -> usize {
    match dtype {
        PrimitiveType::None => 0,
        PrimitiveType::Boolean => 1,
        PrimitiveType::Byte => 2,
        PrimitiveType::I16 => 3,
        PrimitiveType::I32 => 4,
        PrimitiveType::I64 => 5,
        PrimitiveType::F32 => 6,
        PrimitiveType::F64 => 7,
    }
}

pub(crate) fn binary_output(
    name: &str,
    left: PrimitiveType,
    right: PrimitiveType,
) -> PrimitiveType {
    let promoted = if order(left) >= order(right) {
        left
    } else {
        right
    };
    match name {
        "eq" | "ne" | "ge" | "gt" | "le" | "lt" | "xor" => PrimitiveType::Boolean,
        "ratio" => {
            if promoted == PrimitiveType::F32 {
                PrimitiveType::F32
            } else {
                PrimitiveType::F64
            }
        }
        "pow" => PrimitiveType::F64,
        "add" | "sub" | "mul" => match promoted {
            PrimitiveType::Boolean => PrimitiveType::Byte,
            PrimitiveType::Byte => PrimitiveType::I16,
            PrimitiveType::I16 => PrimitiveType::I32,
            _ => promoted,
        },
        _ => promoted,
    }
}

pub(crate) fn unary_output(name: &str, input: PrimitiveType) -> PrimitiveType {
    if matches!(name, "abs" | "neg") {
        input
    } else if input == PrimitiveType::F32 {
        PrimitiveType::F32
    } else {
        PrimitiveType::F64
    }
}

pub(crate) fn checked_output(shape: &[usize], width: usize) -> Result<usize, PyErr> {
    let mut count = 1usize;
    for &extent in shape {
        if extent > i64::MAX as usize {
            return Err(PyValueError::new_err("tensor extent exceeds int64 range"));
        }
        count = count
            .checked_mul(extent)
            .filter(|&n| n <= isize::MAX as usize)
            .ok_or_else(|| PyValueError::new_err("tensor element count exceeds supported range"))?;
    }
    count
        .checked_mul(width)
        .filter(|&n| n <= isize::MAX as usize)
        .ok_or_else(|| PyValueError::new_err("tensor byte count exceeds supported range"))?;
    Ok(count)
}

pub(crate) fn metadata(meta: &TensorMetadata, max_rank: usize) -> Result<usize, PyErr> {
    if meta.shape.len() > max_rank || meta.shape.len() != meta.strides.len() {
        return Err(PyValueError::new_err("unsupported tensor rank or strides"));
    }
    let count = checked_output(&meta.shape, width(meta.primitive_type))?;
    if count != 0 {
        let mut stride = 1;
        for (&extent, &actual) in meta.shape.iter().zip(&meta.strides) {
            if actual != stride {
                return Err(PyValueError::new_err(
                    "tensor must be column-major contiguous",
                ));
            }
            stride = stride
                .checked_mul(extent)
                .ok_or_else(|| PyValueError::new_err("tensor stride overflows"))?;
        }
    }
    Ok(count)
}

pub(crate) fn numeric(dtype: PrimitiveType, name: &str) -> Result<(), PyErr> {
    if dtype == PrimitiveType::None
        || ((name == "abs" || name == "neg" || name == "ratio" || name == "pow")
            && dtype == PrimitiveType::Boolean)
    {
        return Err(PyTypeError::new_err(format!(
            "{name} does not accept {dtype:?}"
        )));
    }
    Ok(())
}

pub(crate) fn binary_types(
    name: &str,
    left: PrimitiveType,
    right: PrimitiveType,
) -> Result<(), PyErr> {
    numeric(left, name)?;
    numeric(right, name)?;
    let integer = |p| {
        matches!(
            p,
            PrimitiveType::Boolean
                | PrimitiveType::Byte
                | PrimitiveType::I16
                | PrimitiveType::I32
                | PrimitiveType::I64
        )
    };
    if name == "mod" && (!integer(left) || !integer(right)) {
        return Err(PyTypeError::new_err("mod requires integer operands"));
    }
    if matches!(name, "bitAnd" | "bitOr" | "bitXor")
        && !((left == PrimitiveType::Boolean && right == PrimitiveType::Boolean)
            || (left != PrimitiveType::Boolean
                && right != PrimitiveType::Boolean
                && integer(left)
                && integer(right)))
    {
        return Err(PyTypeError::new_err(
            "bit operations require two booleans or two signed integers",
        ));
    }
    Ok(())
}

pub(crate) fn broadcast(left: &[usize], right: &[usize]) -> Result<Vec<usize>, PyErr> {
    let left_size = checked_output(left, 1)?;
    let right_size = checked_output(right, 1)?;
    let output = if left == right {
        left.to_vec()
    } else if left.len() == 2 && right.len() == 2 {
        if left[1] == 1 {
            right.to_vec()
        } else if right[1] == 1 {
            left.to_vec()
        } else {
            return Err(PyValueError::new_err("incompatible broadcast shapes"));
        }
    } else if left.len() != right.len() {
        let (high, low_size, low_rank) = if left.len() > right.len() {
            (left, right_size, right.len())
        } else {
            (right, left_size, left.len())
        };
        let mut compatible = false;
        for index in low_rank..high.len() {
            let leading = checked_output(&high[..index], 1)?;
            compatible |= leading == low_size;
        }
        if !compatible {
            return Err(PyValueError::new_err("incompatible broadcast shapes"));
        }
        high.to_vec()
    } else {
        return Err(PyValueError::new_err("incompatible broadcast shapes"));
    };
    let output_size = checked_output(&output, 1)?;
    if output_size != 0 && (left_size == 0 || right_size == 0) {
        return Err(PyValueError::new_err(
            "nonempty broadcast cannot recycle an empty operand",
        ));
    }
    Ok(output)
}
