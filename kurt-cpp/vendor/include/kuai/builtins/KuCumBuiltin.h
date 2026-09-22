#pragma once

#include <type_traits>

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/fn/KuScanFn.h"
#include "kuai/fn/KuScanSpanFn.h"

namespace kuai {

template <typename Vendor, typename MapFn, typename ReduceFn>
KuBuiltinResult KuCumUnaryBuiltin(KuContext &context, KuObject *in) {
    auto reduceFn = ReduceFn();
    auto mapFn = MapFn();
    return KuScanFn<Vendor, KuNonVoidPrimitiveTs>()(context, *in, mapFn, reduceFn);
}

template <typename ReduceFn>
struct CumReduce {
    template <typename T1, typename T2>
    KU_DEVICE_HOST auto operator()(const T1 &a, const T2 &b) const noexcept {
        static_assert(std::is_same_v<T1, T2>, "T1 and T2 must be the same type");
        if (ku_value_traits<decltype(a)>::isNull(a) || ku_value_traits<decltype(b)>::isNull(b)) {
            return ku_value_traits<decltype(a)>::isNull(a) ? b : a;
        } else {
            return ReduceFn()(a, b);
        }
    }
};

template <template <typename...> class ResultType>
struct CumMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        return ku_cast<typename ResultType<T>::type>()(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        return ku_value_traits<typename ResultType<T>::type>::null();
    }
};

} // namespace kuai

#define KU_GENERATE_BUILTIN_cumsum                                                 \
    KU_FUNC(m) {                                                                   \
        using namespace kuai;                                                      \
        m.def<&KuCumUnaryBuiltin<vendor::KuVendor, CumMapFn<ku_accum_result_type>, \
                                 CumReduce<ku_add>>>("cumsum");                    \
    }
