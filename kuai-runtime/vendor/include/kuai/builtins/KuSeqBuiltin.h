#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuScalar.h>
#include <kuai/core/KuString.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/sequence.h"

namespace kuai {

namespace detail {

inline KuResult<KuI64, std::string> normalizeSeqInteger(const KuScalar  &value,
                                                        std::string_view parameterName) {
    switch (value.getType()) {
        case KU_PRIMITIVE_I32: {
            const auto result = value.value<KuI32>();
            if (ku_value_traits<KuI32>::isNull(result)) {
                return std::string("seq ") + std::string(parameterName) + " must not be null";
            }
            return static_cast<KuI64>(result);
        }
        case KU_PRIMITIVE_I64: {
            const auto result = value.value<KuI64>();
            if (ku_value_traits<KuI64>::isNull(result)) {
                return std::string("seq ") + std::string(parameterName) + " must not be null";
            }
            return result;
        }
        default:
            return std::string("seq ") + std::string(parameterName)
                   + " must be an i32 or i64 scalar";
    }
}

inline KuI64 defaultSeqStep(KuI64 begin, KuI64 end) noexcept {
    return begin > end ? KuI64{-1} : KuI64{1};
}

inline KuResult<ku_size_t, std::string> seqLength(KuI64 begin, KuI64 end, KuI64 step) {
    if (step == 0) {
        return std::string("seq step must not be zero");
    }
    if ((step > 0 && begin >= end) || (step < 0 && begin <= end)) {
        return ku_size_t{0};
    }

    using Unsigned = std::make_unsigned_t<KuI64>;
    const auto distance = step > 0 ? static_cast<Unsigned>(end) - static_cast<Unsigned>(begin)
                                   : static_cast<Unsigned>(begin) - static_cast<Unsigned>(end);
    const auto magnitude =
        step > 0 ? static_cast<Unsigned>(step) : Unsigned{0} - static_cast<Unsigned>(step);
    const auto length = distance / magnitude + static_cast<Unsigned>(distance % magnitude != 0);
    if (length > std::numeric_limits<ku_size_t>::max()) {
        return std::string("seq result length exceeds ku_size_t");
    }
    return static_cast<ku_size_t>(length);
}

template <typename Vendor, typename T>
KuBuiltinResult
generateSeq(KuVendorContext<Vendor> &device, KuI64 begin, KuI64 step, ku_size_t length) {
    auto outputResult = device.template createTensor<T>(KuTensorDesc(KuDynamicExtents<1>(length)));
    if (!outputResult.hasValue()) {
        return KuBuiltinError(outputResult.error());
    }
    auto output = std::move(outputResult).value();
    if (length != 0) {
        algo::sequence(device, output->template begin<T>(), output->template end<T>(), begin, step);
    }
    return KuBuiltinResult(std::move(output));
}

template <typename Vendor>
KuBuiltinResult
generateSeq(KuContext &context, KuI64 begin, KuI64 end, KuI64 step, const KuString &dtype) {
    auto length = seqLength(begin, end, step);
    if (!length) {
        return KuBuiltinError(std::move(length).error());
    }

    KuVendorContext<Vendor> device(context);
    if (dtype.value() == "i32") {
        return generateSeq<Vendor, KuI32>(device, begin, step, length.value());
    }
    if (dtype.value() == "i64") {
        return generateSeq<Vendor, KuI64>(device, begin, step, length.value());
    }
    return KuBuiltinResult("seq dtype must be \"i32\" or \"i64\"");
}

} // namespace detail

template <typename Vendor>
KuBuiltinResult seqEndBuiltin(KuContext &context, const KuScalar &endValue, const KuString &dtype) {
    auto end = detail::normalizeSeqInteger(endValue, "end");
    if (!end) {
        return KuBuiltinError(std::move(end).error());
    }
    constexpr KuI64 begin = 0;
    return detail::generateSeq<Vendor>(context, begin, end.value(),
                                       detail::defaultSeqStep(begin, end.value()), dtype);
}

template <typename Vendor>
KuBuiltinResult seqRangeBuiltin(KuContext      &context,
                                const KuScalar &beginValue,
                                const KuScalar &endValue,
                                const KuString &dtype) {
    auto begin = detail::normalizeSeqInteger(beginValue, "begin");
    if (!begin) {
        return KuBuiltinError(std::move(begin).error());
    }
    auto end = detail::normalizeSeqInteger(endValue, "end");
    if (!end) {
        return KuBuiltinError(std::move(end).error());
    }
    return detail::generateSeq<Vendor>(context, begin.value(), end.value(),
                                       detail::defaultSeqStep(begin.value(), end.value()), dtype);
}

template <typename Vendor>
KuBuiltinResult seqStepBuiltin(KuContext      &context,
                               const KuScalar &beginValue,
                               const KuScalar &endValue,
                               const KuScalar &stepValue,
                               const KuString &dtype) {
    auto begin = detail::normalizeSeqInteger(beginValue, "begin");
    if (!begin) {
        return KuBuiltinError(std::move(begin).error());
    }
    auto end = detail::normalizeSeqInteger(endValue, "end");
    if (!end) {
        return KuBuiltinError(std::move(end).error());
    }
    auto step = detail::normalizeSeqInteger(stepValue, "step");
    if (!step) {
        return KuBuiltinError(std::move(step).error());
    }
    return detail::generateSeq<Vendor>(context, begin.value(), end.value(), step.value(), dtype);
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_seq                           \
    KU_FUNC(m) {                                          \
        using namespace kuai;                             \
        m.def<&seqEndBuiltin<vendor::KuVendor>>("seq");   \
        m.def<&seqRangeBuiltin<vendor::KuVendor>>("seq"); \
        m.def<&seqStepBuiltin<vendor::KuVendor>>("seq");  \
    }
