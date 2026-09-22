#pragma once

#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/functional/KuDivide.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/functional/KuSub.h>
namespace kuai {

template <typename T>
struct MeanStats {
    using AccumT = typename std::conditional<std::is_same_v<T, KuF32>, KuF32, KuF64>::type;
    using CountT = KuI64;
    KU_DEVICE_HOST MeanStats() {
        m_mean = kuai::ku_value_traits<AccumT>::null();
        m_count = 0;
    }

    KU_DEVICE_HOST MeanStats(const AccumT &mean, const CountT &count)
        : m_mean(mean), m_count(count) {
    }

    KU_DEVICE_HOST MeanStats(const T &val) {
        if (kuai::ku_value_traits<decltype(val)>::isNull(val)) {
            m_mean = ku_value_traits<AccumT>::null();
            m_count = 0;
        } else {
            m_mean = ku_cast<AccumT>()(val);
            m_count = 1;
        }
    }

    KU_DEVICE_HOST MeanStats operator+(const MeanStats &a) const {
        if (isNull()) {
            return a;
        } else if (a.isNull()) {
            return *this;
        } else {
            auto delta = ku_sub()(a.m_mean, m_mean);
            auto n = m_count + a.m_count;
            auto newMean = ku_add()(m_mean, ku_divide()(ku_mul()(delta, a.m_count), n));
            return MeanStats(newMean, n);
        }
    }

    KU_DEVICE_HOST bool isNull() const noexcept {
        return kuai::ku_value_traits<decltype(m_mean)>::isNull(m_mean);
    }
    AccumT m_mean;
    CountT m_count;
};

struct MeanResultCastFn {
    template <typename T>
    KU_DEVICE_HOST KuF64 operator()(const MeanStats<T> &a) const {
        return ku_cast<KuF64>()(a.m_mean);
    }
};

struct MeanReduceFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const MeanStats<T> &a, const MeanStats<T> &b) const {
        return a + b;
    }
};

struct MeanMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        return MeanStats<T>(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        return MeanStats<T>();
    }
};
} // namespace kuai
