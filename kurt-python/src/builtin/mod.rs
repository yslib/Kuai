mod arithmetic;
mod indexing;
mod invocation;
mod statistics;
mod validation;
mod values;

use kurt::{DeviceType, KuArc, KuObject, PrimitiveType as P, ScalarValue as S};
use pyo3::exceptions::{PyTypeError, PyValueError};
use pyo3::prelude::*;
use pyo3::types::PyTuple;

use self::validation::Family;
use self::values::{Argument, Host};
use crate::errors::native_error;
use crate::instance::PyDevice;
use crate::tensor::PyTensor;

pub(crate) use invocation::_invoke_builtin;

/// A validated CPU call. Its tensor guards and boxed host values survive until
/// the synchronous native call and its temporary argument owners have dropped.
pub(crate) struct PreparedCall<'py> {
    pub(crate) name: String,
    pub(crate) device: Py<PyDevice>,
    pub(crate) arguments: Vec<Argument<'py>>,
}

impl PreparedCall<'_> {
    pub(crate) fn native_arguments(&self) -> PyResult<Vec<KuArc<KuObject<'_>>>> {
        self.arguments.iter().map(Argument::native).collect()
    }
}

pub(crate) fn prepare<'py>(
    py: Python<'py>,
    device: &Bound<'py, PyDevice>,
    name: &str,
    operands: &Bound<'py, PyTuple>,
) -> PyResult<PreparedCall<'py>> {
    let family = validation::descriptor(name, operands.len())?;
    let id = device.borrow().id();
    let py_instance = device.borrow().instance(py)?;
    let native = py_instance.bind(py).borrow().clone_native()?;
    let target = native
        .device(id)
        .map_err(|error| native_error(error, "lookup builtin device"))?;
    let (kind, _) = target
        .info()
        .map_err(|error| native_error(error, "query builtin device"))?;
    if kind != DeviceType::Cpu {
        return Err(pyo3::exceptions::PyNotImplementedError::new_err(
            "only the audited CPU builtin backend is supported",
        ));
    }

    let mut arguments = Vec::with_capacity(operands.len());
    for operand in operands.iter() {
        if operand.is_instance_of::<PyTensor>() {
            let actual_device = operand.getattr("device")?;
            if !actual_device.is(device) {
                return Err(PyValueError::new_err(
                    "tensor operand does not belong to selected device",
                ));
            }
            let guard = operand.extract::<PyRef<'py, PyTensor>>()?;
            let tensor = guard.tensor()?;
            if !tensor.is_on(&target) {
                return Err(PyValueError::new_err(
                    "tensor native device does not match selected device",
                ));
            }
            validation::metadata(&tensor.metadata(), 8)?;
            arguments.push(Argument::Tensor(guard));
        } else {
            arguments.push(Argument::Host(values::box_host(&operand)?));
        }
    }
    validate(name, family, &arguments)?;
    Ok(PreparedCall {
        name: name.to_owned(),
        device: device.clone().unbind(),
        arguments,
    })
}

fn numeric(argument: &Argument<'_>, name: &str, max_rank: usize) -> PyResult<(P, Vec<usize>)> {
    let dtype = argument
        .dtype()
        .ok_or_else(|| PyTypeError::new_err(format!("{name} requires a numeric operand")))?;
    validation::numeric(dtype, name)?;
    let shape = argument
        .shape()
        .ok_or_else(|| PyTypeError::new_err("numeric operand is unavailable"))?;
    if shape.len() > max_rank {
        return Err(PyValueError::new_err(
            "builtin tensor rank exceeds supported range",
        ));
    }
    Ok((dtype, shape))
}

fn validate(name: &str, family: Family, args: &[Argument<'_>]) -> PyResult<()> {
    match family {
        Family::Unary
        | Family::Conversion(_)
        | Family::Sum
        | Family::Cumsum
        | Family::Statistics => {
            let (dtype, shape) = numeric(&args[0], name, 2)?;
            let output = if family == Family::Cumsum && shape.is_empty() {
                vec![1]
            } else if matches!(family, Family::Sum | Family::Statistics) && !shape.is_empty() {
                shape[1..].to_vec()
            } else {
                shape.clone()
            };
            let width = match family {
                Family::Conversion(target) => validation::width(target),
                Family::Statistics => 8,
                Family::Sum | Family::Cumsum => 8,
                _ => validation::width(validation::unary_output(name, dtype)),
            };
            validation::checked_output(&output, width)?;
            match family {
                Family::Unary if name == "reciprocal" && dtype == P::F32 => {
                    for value in args[0].snapshot()? {
                        arithmetic::checked_reciprocal(value)?;
                    }
                }
                Family::Conversion(target)
                    if (matches!(dtype, P::F32 | P::F64)
                        && matches!(target, P::Byte | P::I16 | P::I32 | P::I64))
                        || (dtype == P::F64 && target == P::F32) =>
                {
                    for value in args[0].snapshot()? {
                        arithmetic::checked_conversion(target, value)?;
                    }
                }
                Family::Sum if matches!(dtype, P::Boolean | P::Byte | P::I16 | P::I32 | P::I64) => {
                    let values = args[0].snapshot()?;
                    let axis = shape.first().copied().unwrap_or(1);
                    for segment in values.chunks(axis.max(1)) {
                        arithmetic::check_sum_values(segment)?;
                    }
                }
                Family::Statistics => {
                    statistics::check(name, dtype, &shape, &args[0].snapshot()?)?;
                }
                _ => {}
            }
        }
        Family::Binary | Family::MinMax if args.len() == 2 => {
            let (left_type, left_shape) = numeric(&args[0], name, 2)?;
            let (right_type, right_shape) = numeric(&args[1], name, 2)?;
            validation::binary_types(name, left_type, right_type)?;
            let output = validation::broadcast(&left_shape, &right_shape)?;
            let count = validation::checked_output(
                &output,
                validation::width(validation::binary_output(name, left_type, right_type)),
            )?;
            if matches!(name, "add" | "sub" | "mul") && count != 0 {
                let left = args[0].snapshot()?;
                let right = args[1].snapshot()?;
                for index in 0..count {
                    arithmetic::checked_pair(
                        name,
                        left_type,
                        left[index % left.len()],
                        right_type,
                        right[index % right.len()],
                    )?;
                }
            }
        }
        Family::MinMax => {
            let dtype = args[0]
                .dtype()
                .ok_or_else(|| PyTypeError::new_err("min/max requires a scalar or tensor"))?;
            let shape = args[0]
                .shape()
                .ok_or_else(|| PyTypeError::new_err("min/max requires a scalar or tensor"))?;
            if dtype == P::None && !shape.is_empty() {
                return Err(PyTypeError::new_err("NONE tensor is unsupported"));
            }
            if shape.len() > 2 {
                return Err(PyValueError::new_err("min/max rank exceeds two"));
            }
            let output = if shape.is_empty() {
                vec![]
            } else {
                shape[1..].to_vec()
            };
            validation::checked_output(&output, validation::width(dtype))?;
        }
        Family::At => validate_at(args)?,
        Family::Mask => validate_mask(args)?,
        Family::Seq => validate_seq(args)?,
        Family::Normal => validate_normal(args)?,
        Family::Binary => unreachable!(),
    }
    Ok(())
}

fn validate_at(args: &[Argument<'_>]) -> PyResult<()> {
    let Argument::Tensor(_) = args[0] else {
        return Err(PyTypeError::new_err("at source must be a tensor"));
    };
    let source = args[0].metadata()?.unwrap();
    if source.shape.is_empty() || args.len() != source.shape.len() + 1 {
        return Err(PyValueError::new_err(
            "at requires one axis per source dimension",
        ));
    }
    let mut output = Vec::with_capacity(source.shape.len());
    for (&extent, axis) in source.shape.iter().zip(&args[1..]) {
        let selected = match axis {
            Argument::Host(Host::Scalar(S::None)) => extent,
            Argument::Host(Host::Slice(spec)) => indexing::slice_extent(*spec, extent)?,
            Argument::Tensor(_) => {
                let meta = axis.metadata()?.unwrap();
                indexing::index_tensor(meta.primitive_type, &meta.shape)?
            }
            _ => {
                return Err(PyTypeError::new_err(
                    "at axes must be None, slice, or an index tensor",
                ));
            }
        };
        output.push(selected);
    }
    validation::checked_output(&output, validation::width(source.primitive_type))?;
    Ok(())
}

fn validate_mask(args: &[Argument<'_>]) -> PyResult<()> {
    let (Argument::Tensor(_), Argument::Tensor(_)) = (&args[0], &args[1]) else {
        return Err(PyTypeError::new_err(
            "mask requires source and stencil tensors",
        ));
    };
    let source = args[0].metadata()?.unwrap();
    let mask = args[1].metadata()?.unwrap();
    if source.shape.is_empty() || source.shape != mask.shape || mask.primitive_type != P::Boolean {
        return Err(PyValueError::new_err(
            "mask requires an identical-shape Boolean stencil",
        ));
    }
    validation::checked_output(&source.shape, validation::width(source.primitive_type))?;
    Ok(())
}

fn seq_int(arg: &Argument<'_>) -> PyResult<i64> {
    match arg.scalar() {
        Some(S::I32(n)) if n != i32::MIN => Ok(n as i64),
        Some(S::I64(n)) if n != i64::MIN => Ok(n),
        _ => Err(PyTypeError::new_err(
            "seq requires nonnull I32 or I64 scalar bounds",
        )),
    }
}

fn validate_seq(args: &[Argument<'_>]) -> PyResult<()> {
    let dtype = args
        .last()
        .and_then(Argument::string)
        .ok_or_else(|| PyTypeError::new_err("seq dtype must be a string"))?;
    let width = match dtype {
        "i32" => 4,
        "i64" => 8,
        _ => return Err(PyValueError::new_err("seq dtype must be i32 or i64")),
    };
    let (begin, end, step) = match args.len() {
        2 => {
            let end = seq_int(&args[0])?;
            (0, end, if end < 0 { -1 } else { 1 })
        }
        3 => {
            let begin = seq_int(&args[0])?;
            let end = seq_int(&args[1])?;
            (begin, end, if begin > end { -1 } else { 1 })
        }
        4 => (seq_int(&args[0])?, seq_int(&args[1])?, seq_int(&args[2])?),
        _ => unreachable!(),
    };
    let count = indexing::sequence_length(begin, end, step)?;
    validation::checked_output(&[count], width)?;
    Ok(())
}

fn validate_normal(args: &[Argument<'_>]) -> PyResult<()> {
    let (Some(S::F64(mean)), Some(S::F64(std)), Some(S::I64(count))) =
        (args[0].scalar(), args[1].scalar(), args[2].scalar())
    else {
        return Err(PyTypeError::new_err(
            "normal requires F64 mean/std and I64 count",
        ));
    };
    let dtype = args[3]
        .string()
        .ok_or_else(|| PyTypeError::new_err("normal dtype must be a string"))?;
    let width = match dtype {
        "f32" => 4,
        "f64" => 8,
        _ => return Err(PyValueError::new_err("normal dtype must be f32 or f64")),
    };
    if !mean.is_finite() || !std.is_finite() || std < 0.0 || count < 0 {
        return Err(PyValueError::new_err(
            "normal mean/std/count are outside native domain",
        ));
    }
    let count = usize::try_from(count)
        .map_err(|_| PyValueError::new_err("normal count exceeds supported range"))?;
    let capacity = count
        .checked_add(count & 1)
        .ok_or_else(|| PyValueError::new_err("normal rounded capacity overflows"))?;
    validation::checked_output(&[capacity], width)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::{arithmetic, indexing, validation};
    use kurt::{Boolean, PrimitiveType as P, ScalarValue as S};

    #[test]
    fn closed_descriptor_and_arity() {
        assert!(validation::descriptor("add", 2).is_ok());
        assert!(validation::descriptor("at", 9).is_ok());
        assert!(validation::descriptor("at", 10).is_err());
        assert!(validation::descriptor("not_a_builtin", 1).is_err());
        assert!(validation::descriptor("dot", 2).is_err());
    }

    #[test]
    fn native_cyclic_broadcast_and_empty_divisor() {
        assert_eq!(validation::broadcast(&[2, 1], &[3, 2]).unwrap(), vec![3, 2]);
        assert_eq!(validation::broadcast(&[2], &[2, 3]).unwrap(), vec![2, 3]);
        assert!(validation::broadcast(&[0, 1], &[2, 3]).is_err());
        assert!(validation::broadcast(&[1], &[4]).is_err());
        assert!(validation::broadcast(&[6], &[2, 3]).is_err());
    }

    #[test]
    fn output_dtype_tracks_native_promotion() {
        assert_eq!(
            validation::binary_output("add", P::Boolean, P::Boolean),
            P::Byte
        );
        assert_eq!(validation::binary_output("mul", P::I16, P::I16), P::I32);
        assert_eq!(validation::binary_output("eq", P::I64, P::F32), P::Boolean);
        assert_eq!(validation::binary_output("ratio", P::I64, P::I64), P::F64);
    }

    #[test]
    fn arithmetic_expression_width_and_null_short_circuit() {
        assert!(
            arithmetic::checked_pair("mul", P::I32, S::I32(i32::MAX), P::I32, S::I32(2)).is_err()
        );
        assert!(
            arithmetic::checked_pair("add", P::I32, S::I32(i32::MIN), P::I32, S::I32(i32::MAX))
                .is_ok()
        );
        assert!(
            arithmetic::checked_pair("add", P::I64, S::I64(i64::MAX), P::I64, S::I64(1)).is_err()
        );
    }

    #[test]
    fn conversion_rounding_boundaries() {
        assert!(arithmetic::checked_conversion(P::I64, S::F64(9223372036854775808.0)).is_err());
        assert!(arithmetic::checked_conversion(P::I32, S::F64(1.5)).is_ok());
        assert!(arithmetic::checked_conversion(P::F32, S::F64(f64::MAX)).is_err());
        assert!(arithmetic::checked_conversion(P::F32, S::F64(f64::INFINITY)).is_ok());
    }

    #[test]
    fn reciprocal_f32_narrowing_boundary() {
        assert!(arithmetic::checked_reciprocal(S::F32(f32::from_bits(1))).is_err());
        assert!(arithmetic::checked_reciprocal(S::F32(2.0)).is_ok());
        assert!(arithmetic::checked_reciprocal(S::F32(0.0)).is_ok());
    }

    #[test]
    fn sum_replays_intermediate_and_sentinel_reset() {
        assert!(arithmetic::check_sum_values(&[S::I64(i64::MAX), S::I32(1), S::I16(-1)]).is_err());
        assert!(arithmetic::check_sum_values(&[S::I64(-i64::MAX), S::I64(-1), S::Byte(5)]).is_ok());
        assert!(
            arithmetic::check_sum_values(&[
                S::Boolean(Boolean::NULL),
                S::Byte(i8::MIN),
                S::Boolean(Boolean::TRUE),
                S::I16(2)
            ])
            .is_ok()
        );
    }

    #[test]
    fn checked_index_output_product() {
        assert!(indexing::checked_output(&[usize::MAX / 2, 3], 8).is_err());
        assert_eq!(indexing::checked_output(&[2, 3], 8).unwrap(), 6);
    }

    #[test]
    fn exact_python_boxing_and_slice_bounds() {
        use pyo3::types::PySlice;
        use pyo3::{IntoPyObject, Python};
        Python::initialize();
        Python::attach(|py| {
            assert!(matches!(
                super::values::box_host(&true.into_pyobject(py).unwrap()),
                Ok(super::values::Host::Scalar(S::Boolean(_)))
            ));
            assert!(matches!(
                super::values::box_host(&42_i64.into_pyobject(py).unwrap().into_any()),
                Ok(super::values::Host::Scalar(S::I64(42)))
            ));
            let slice = PySlice::new(py, 1, 8, 2);
            assert!(matches!(
                super::values::box_host(slice.as_any()),
                Ok(super::values::Host::Slice(_))
            ));
            let subclass = py
                .eval(pyo3::ffi::c_str!("type('I', (int,), {})(4)"), None, None)
                .unwrap();
            assert!(super::values::box_host(&subclass).is_err());
        });
    }

    #[test]
    fn slice_index_callback_exceptions_keep_their_original_type() {
        use pyo3::Python;
        use pyo3::exceptions::{PyTypeError, PyValueError};

        Python::initialize();
        Python::attach(|py| {
            let callback_error = py
                .eval(
                    pyo3::ffi::c_str!(
                        "slice(type('E', (), {'__index__': lambda self: int('bad')})(), None, None)"
                    ),
                    None,
                    None,
                )
                .unwrap();
            let error = super::values::box_host(&callback_error).unwrap_err();
            assert!(error.is_instance_of::<PyValueError>(py));
            assert!(error.to_string().contains("invalid literal"));

            let invalid_field = py
                .eval(pyo3::ffi::c_str!("slice('bad', None, None)"), None, None)
                .unwrap();
            let error = super::values::box_host(&invalid_field).unwrap_err();
            assert!(error.is_instance_of::<PyTypeError>(py));
            assert!(error.to_string().contains("slice start"));
        });
    }

    #[test]
    fn slice_reverse_extent_and_seq_bounds() {
        use kurt::SliceSpec;
        use std::num::NonZeroI64;
        let reverse = SliceSpec {
            start: None,
            stop: None,
            step: NonZeroI64::new(-1),
        };
        assert_eq!(indexing::slice_extent(reverse, 5).unwrap(), 5);
        assert_eq!(
            indexing::sequence_length(i64::MAX - 1, i64::MAX, i64::MAX).unwrap(),
            1
        );
        let min_step = SliceSpec {
            start: None,
            stop: None,
            step: NonZeroI64::new(i64::MIN),
        };
        assert_eq!(indexing::slice_extent(min_step, 5).unwrap(), 1);
        assert_eq!(indexing::slice_extent(reverse, 0).unwrap(), 0);
        let bounded = SliceSpec {
            start: Some(-100),
            stop: Some(100),
            step: NonZeroI64::new(2),
        };
        assert_eq!(indexing::slice_extent(bounded, 5).unwrap(), 3);
    }

    #[test]
    fn statistics_count_domains_respect_finalizer_short_circuits() {
        assert!(super::statistics::skew_final_count(46_342, 1.0).is_err());
        assert!(super::statistics::skew_final_count(46_342, 0.0).is_ok());
        assert!(super::statistics::kurtosis_merge_count(2_097_151, 1).is_err());
        assert!(super::statistics::kurtosis_merge_count(100, 1).is_ok());
    }

    #[test]
    fn statistics_replay_keeps_constant_skew_valid() {
        let constant = vec![S::I64(1); 46_342];
        assert!(super::statistics::check("skew", P::I64, &[constant.len()], &constant).is_ok());
        let mut varying = constant;
        varying[46_341] = S::I64(2);
        assert!(super::statistics::check("skew", P::I64, &[varying.len()], &varying).is_err());
    }

    #[test]
    fn pure_family_validation_checks_generator_and_none_domains() {
        use super::{
            validate,
            validation::Family,
            values::{Argument, Host},
        };
        let scalar = |value| Argument::Host(Host::Scalar(value));
        assert!(
            validate(
                "seq",
                Family::Seq,
                &[
                    scalar(S::I64(-2)),
                    Argument::Host(Host::String("i64".into()))
                ]
            )
            .is_ok()
        );
        assert!(
            validate(
                "seq",
                Family::Seq,
                &[
                    scalar(S::I64(3)),
                    scalar(S::I64(8)),
                    scalar(S::I64(0)),
                    Argument::Host(Host::String("i32".into()))
                ]
            )
            .is_err()
        );
        assert!(
            validate(
                "normal",
                Family::Normal,
                &[
                    scalar(S::F64(0.0)),
                    scalar(S::F64(1.0)),
                    scalar(S::I64(0)),
                    Argument::Host(Host::String("f32".into()))
                ]
            )
            .is_ok()
        );
        assert!(
            validate(
                "normal",
                Family::Normal,
                &[
                    scalar(S::F64(f64::NAN)),
                    scalar(S::F64(1.0)),
                    scalar(S::I64(0)),
                    Argument::Host(Host::String("f32".into()))
                ]
            )
            .is_err()
        );
        assert!(validate("min", Family::MinMax, &[scalar(S::None)]).is_ok());
        assert!(validate("add", Family::Binary, &[scalar(S::None), scalar(S::I64(1))]).is_err());
    }
}
