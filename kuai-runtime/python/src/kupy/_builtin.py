from __future__ import annotations

from collections.abc import Callable
from functools import wraps
import inspect
import operator
from typing import Any, Optional, Union

from .runtime import Device, Tensor, _invoke_builtin

_HOST_VALUE_TYPES = (bool, int, float, str, type(None), slice)
_I64_MIN = -(1 << 63)
_I64_MAX = (1 << 63) - 1


def _validate_slice_field(value: Any, field: str) -> Optional[int]:
    if value is None:
        return None
    try:
        normalized = operator.index(value)
    except TypeError as error:
        raise TypeError(f"slice {field} must be None or integer-like") from error
    if not _I64_MIN <= normalized <= _I64_MAX:
        raise ValueError(f"slice {field} is outside the int64_t range")
    return normalized


def _validate_host_value(value: Any) -> None:
    if type(value) is int and not _I64_MIN <= value <= _I64_MAX:
        raise ValueError("Python integer is outside the ku_i64_t range")
    if type(value) is slice:
        _validate_slice_field(value.start, "start")
        _validate_slice_field(value.stop, "stop")
        step = _validate_slice_field(value.step, "step")
        if step == 0:
            raise ValueError("slice step must not be zero")


def _validate_signature(signature: inspect.Signature) -> None:
    for parameter in signature.parameters.values():
        if parameter.name == "device":
            raise TypeError("a kuai builtin operand cannot be named 'device'")
        if parameter.kind not in (
            inspect.Parameter.POSITIONAL_ONLY,
            inspect.Parameter.POSITIONAL_OR_KEYWORD,
        ):
            raise TypeError("kuai builtin operands must be fixed positional parameters")
        if parameter.default is not inspect.Parameter.empty:
            raise TypeError("kuai builtin operands cannot have default values")


def _invoke_declared_builtin(
    runtime_name: str, operands: tuple[Any, ...], explicit_device: Any
) -> Any:
    inferred_device: Optional[Device] = None
    for operand in operands:
        if isinstance(operand, Tensor):
            if inferred_device is None:
                inferred_device = operand.device
            elif operand.device is not inferred_device:
                raise ValueError("all tensor operands must belong to the same device")
        elif type(operand) not in _HOST_VALUE_TYPES:
            raise TypeError(
                "builtin operands must be Tensor, bool, int, float, str, None, "
                "or slice"
            )
        else:
            _validate_host_value(operand)

    if explicit_device is not None and not isinstance(explicit_device, Device):
        raise TypeError("device must be a kupy.Device")
    if (
        inferred_device is not None
        and explicit_device is not None
        and explicit_device is not inferred_device
    ):
        raise ValueError("explicit device does not match tensor operands")

    selected_device = (
        inferred_device if inferred_device is not None else explicit_device
    )
    if selected_device is None:
        raise TypeError("an all-scalar builtin call requires device=")
    return _invoke_builtin(selected_device, runtime_name, operands)


def _decorate(function: Callable[..., Any], runtime_name: str) -> Callable[..., Any]:
    if not runtime_name:
        raise TypeError("kuai builtin name must not be empty")
    signature = inspect.signature(function)
    _validate_signature(signature)
    public_signature = signature.replace(
        parameters=(
            *signature.parameters.values(),
            inspect.Parameter("device", inspect.Parameter.KEYWORD_ONLY, default=None),
        )
    )

    @wraps(function)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        explicit_device = kwargs.pop("device", None)
        bound = signature.bind(*args, **kwargs)
        operands = tuple(bound.arguments[name] for name in signature.parameters)
        return _invoke_declared_builtin(runtime_name, operands, explicit_device)

    wrapper.__signature__ = public_signature
    return wrapper


def _kuai_builtin_overload(
    runtime_name: str, accepted_arities: tuple[int, ...]
) -> Callable[..., Any]:
    if not runtime_name:
        raise TypeError("kuai builtin name must not be empty")
    if not accepted_arities or any(arity < 0 for arity in accepted_arities):
        raise TypeError("kuai builtin overloads require non-negative arities")

    arities = tuple(sorted(set(accepted_arities)))

    def wrapper(*args: Any, device: Any = None) -> Any:
        if len(args) not in arities:
            choices = ", ".join(str(arity) for arity in arities)
            raise TypeError(
                f"{runtime_name} expects one of ({choices}) positional operand counts"
            )
        return _invoke_declared_builtin(runtime_name, args, device)

    wrapper.__name__ = runtime_name
    wrapper.__qualname__ = runtime_name
    wrapper.__signature__ = inspect.Signature(
        parameters=(
            inspect.Parameter("args", inspect.Parameter.VAR_POSITIONAL),
            inspect.Parameter("device", inspect.Parameter.KEYWORD_ONLY, default=None),
        )
    )
    return wrapper


def _kuai_builtin_variadic_axes(
    runtime_name: str, minimum_axes: int, maximum_axes: int
) -> Callable[..., Any]:
    if not runtime_name:
        raise TypeError("kuai builtin name must not be empty")
    if minimum_axes < 0 or maximum_axes < minimum_axes:
        raise TypeError("kuai builtin axis bounds are invalid")

    def wrapper(source: Any, *axes: Any, device: Any = None) -> Any:
        if not minimum_axes <= len(axes) <= maximum_axes:
            raise TypeError(
                f"{runtime_name} expects between {minimum_axes} and "
                f"{maximum_axes} axis operands"
            )
        return _invoke_declared_builtin(runtime_name, (source, *axes), device)

    wrapper.__name__ = runtime_name
    wrapper.__qualname__ = runtime_name
    wrapper.__signature__ = inspect.Signature(
        parameters=(
            inspect.Parameter("source", inspect.Parameter.POSITIONAL_OR_KEYWORD),
            inspect.Parameter("axes", inspect.Parameter.VAR_POSITIONAL),
            inspect.Parameter("device", inspect.Parameter.KEYWORD_ONLY, default=None),
        )
    )
    return wrapper


def kuai_builtin(
    function_or_name: Union[Callable[..., Any], str],
) -> Callable[..., Any]:
    if isinstance(function_or_name, str):
        if not function_or_name:
            raise TypeError("kuai builtin name must not be empty")

        def decorator(function: Callable[..., Any]) -> Callable[..., Any]:
            if not callable(function):
                raise TypeError("kuai_builtin expects a callable declaration")
            return _decorate(function, function_or_name)

        return decorator
    if not callable(function_or_name):
        raise TypeError("kuai_builtin expects a function or runtime name")
    return _decorate(function_or_name, function_or_name.__name__)
