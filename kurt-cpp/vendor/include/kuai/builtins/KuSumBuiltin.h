#pragma once

#include <string>
#include <type_traits>

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/fn/KuReduceFn.h"

namespace kuai {

struct SumReduceFn {
    template <typename T1, typename T2>
    KU_DEVICE_HOST auto operator()(const T1 &a, const T2 &b) const {
        static_assert(std::is_same_v<T1, T2>, "T1 and T2 must be the same type");
        if (ku_value_traits<decltype(a)>::isNull(a) || ku_value_traits<decltype(b)>::isNull(b)) {
            return ku_value_traits<decltype(a)>::isNull(a) ? b : a;
        } else {
            // we don't use ku_add
            return ku_value_traits<T1>::repr(a) + ku_value_traits<T2>::repr(b);
        }
    }
};

struct SumMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        using ResultType = ku_accum_result_type_t<T>;
        // return ku_value_traits<decltype(a)>::isNull(a) ? ku_value_traits<ResultType>::dflt() :
        // ku_cast<ResultType>()(a);
        return ku_cast<ResultType>()(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        using AccumT = ku_accum_result_type_t<T>;
        return ku_value_traits<AccumT>::null();
    }
};
template <typename Vendor>
KuBuiltinResult KuSumBuiltin(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>()(context, *arg, SumMapFn(), SumReduceFn());
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_sum                        \
    KU_FUNC(m) {                                       \
        using namespace kuai;                          \
        m.def<&KuSumBuiltin<vendor::KuVendor>>("sum"); \
    }
