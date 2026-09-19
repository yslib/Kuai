from __future__ import annotations

from typing import Any, Optional

import numpy as np

from . import _native
from ._native import (
    Device,
    Instance,
    Tensor,
    _from_dlpack,
    _primitive_types,
    _to_device,
    _to_host,
)

_NUMPY_TO_PRIMITIVE = {
    np.dtype(np.bool_): _primitive_types["boolean"],
    np.dtype(np.int8): _primitive_types["byte"],
    np.dtype(np.int16): _primitive_types["i16"],
    np.dtype(np.int32): _primitive_types["i32"],
    np.dtype(np.int64): _primitive_types["i64"],
    np.dtype(np.float32): _primitive_types["f32"],
    np.dtype(np.float64): _primitive_types["f64"],
}


def _invoke_builtin(device: Device, name: str, args: tuple[object, ...]) -> object:
    return _native._invoke_builtin(device, name, args)


def to_device(value: np.ndarray[Any, Any], *, device: Device) -> Tensor:
    if not isinstance(value, np.ndarray):
        raise TypeError("to_device expects a numpy.ndarray")
    if not isinstance(device, Device):
        raise TypeError("device must be a kupy.Device")
    try:
        primitive_type = _NUMPY_TO_PRIMITIVE[value.dtype]
    except KeyError as error:
        raise ValueError(f"unsupported NumPy dtype: {value.dtype}") from error
    if value.ndim > 8:
        raise ValueError("kuai tensor rank exceeds the supported maximum of 8")
    if value.ndim == 0:
        normalized = value
    elif value.ndim == 1:
        normalized = np.ascontiguousarray(value)
    else:
        normalized = np.asfortranarray(value)
    return _to_device(device, memoryview(normalized), primitive_type, list(normalized.shape))


def from_dlpack(
    source: object,
    *,
    device: Device,
    copy: Optional[bool] = None,
) -> Tensor:
    if not isinstance(device, Device):
        raise TypeError("device must be a kupy.Device")
    if copy is not None and type(copy) is not bool:
        raise TypeError("copy must be bool or None")
    return _from_dlpack(device, source, copy)


def to_host(value: Tensor) -> np.ndarray[Any, Any]:
    if not isinstance(value, Tensor):
        raise TypeError("to_host expects a kupy.Tensor")
    return _to_host(value)


__all__ = ["Device", "Instance", "Tensor", "from_dlpack", "to_device", "to_host"]
