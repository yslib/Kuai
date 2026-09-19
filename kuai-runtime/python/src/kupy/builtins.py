from ._builtin import (
    _kuai_builtin_overload,
    _kuai_builtin_variadic_axes,
    kuai_builtin,
)


@kuai_builtin
def abs(x):
    pass


@kuai_builtin
def acos(x):
    pass


@kuai_builtin
def acosh(x):
    pass


@kuai_builtin
def asin(x):
    pass


@kuai_builtin
def asinh(x):
    pass


@kuai_builtin
def atan(x):
    pass


@kuai_builtin
def atanh(x):
    pass


@kuai_builtin
def cbrt(x):
    pass


@kuai_builtin
def cos(x):
    pass


@kuai_builtin
def cosh(x):
    pass


@kuai_builtin
def exp(x):
    pass


@kuai_builtin
def log(x):
    pass


@kuai_builtin
def neg(x):
    pass


@kuai_builtin
def reciprocal(x):
    pass


@kuai_builtin
def sin(x):
    pass


@kuai_builtin
def sinh(x):
    pass


@kuai_builtin
def sqrt(x):
    pass


@kuai_builtin
def tan(x):
    pass


@kuai_builtin
def tanh(x):
    pass


@kuai_builtin
def add(x, y):
    pass


@kuai_builtin
def sub(x, y):
    pass


@kuai_builtin
def mul(x, y):
    pass


@kuai_builtin
def div(x, y):
    pass


@kuai_builtin
def ratio(x, y):
    pass


@kuai_builtin
def mod(x, y):
    pass


@kuai_builtin
def xor(x, y):
    pass


@kuai_builtin("bitAnd")
def bit_and(x, y):
    pass


@kuai_builtin("bitOr")
def bit_or(x, y):
    pass


@kuai_builtin("bitXor")
def bit_xor(x, y):
    pass


@kuai_builtin
def eq(x, y):
    pass


@kuai_builtin
def ne(x, y):
    pass


@kuai_builtin
def ge(x, y):
    pass


@kuai_builtin
def gt(x, y):
    pass


@kuai_builtin
def le(x, y):
    pass


@kuai_builtin
def lt(x, y):
    pass


@kuai_builtin
def pow(x, y):
    pass


@kuai_builtin
def dot(x, y):
    pass


@kuai_builtin
def bool(x):
    pass


@kuai_builtin
def char(x):
    pass


@kuai_builtin
def short(x):
    pass


@kuai_builtin
def int(x):
    pass


@kuai_builtin
def long(x):
    pass


@kuai_builtin
def float(x):
    pass


@kuai_builtin
def double(x):
    pass


@kuai_builtin
def cumsum(x):
    pass


@kuai_builtin
def sum(x):
    pass


@kuai_builtin
def avg(x):
    pass


@kuai_builtin
def std(x):
    pass


@kuai_builtin
def var(x):
    pass


@kuai_builtin
def skew(x):
    pass


@kuai_builtin
def kurtosis(x):
    pass


min = _kuai_builtin_overload("min", (1, 2))
max = _kuai_builtin_overload("max", (1, 2))
seq = _kuai_builtin_overload("seq", (2, 3, 4))
for _overload in (min, max, seq):
    _overload.__module__ = __name__


@kuai_builtin
def normal(mean, standard_deviation, count, dtype):
    pass


at = _kuai_builtin_variadic_axes("at", 1, 8)
at.__module__ = __name__


@kuai_builtin
def mask(source, stencil):
    pass


__all__ = [
    "abs",
    "acos",
    "acosh",
    "asin",
    "asinh",
    "atan",
    "atanh",
    "cbrt",
    "cos",
    "cosh",
    "exp",
    "log",
    "neg",
    "reciprocal",
    "sin",
    "sinh",
    "sqrt",
    "tan",
    "tanh",
    "add",
    "sub",
    "mul",
    "div",
    "ratio",
    "mod",
    "xor",
    "bit_and",
    "bit_or",
    "bit_xor",
    "eq",
    "ne",
    "ge",
    "gt",
    "le",
    "lt",
    "pow",
    "dot",
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "cumsum",
    "sum",
    "avg",
    "std",
    "var",
    "skew",
    "kurtosis",
    "min",
    "max",
    "seq",
    "normal",
    "at",
    "mask",
]
