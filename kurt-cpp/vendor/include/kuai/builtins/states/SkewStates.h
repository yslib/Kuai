#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/functional/KuDivide.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/functional/KuPow.h>
#include <kuai/ktl/functional/KuSqrt.h>
#include <kuai/ktl/functional/KuSub.h>

namespace kuai {

template <typename T>
struct SkewStats {
    using AccumT = std::conditional_t<std::is_same_v<T, KuF32>, KuF32, KuF64>;
    using CountT = KuI32;
    KU_DEVICE_HOST SkewStats() {
        m_mean = ku_value_traits<AccumT>::null();
        m_m2 = ku_value_traits<AccumT>::null();
        m_m3 = ku_value_traits<AccumT>::null();
        m_count = 0;
    }

    KU_DEVICE_HOST
    SkewStats(const AccumT &mean, const AccumT &m2, const AccumT &m3, const CountT &count)
        : m_mean(mean), m_m2(m2), m_m3(m3), m_count(count) {
    }

    KU_DEVICE_HOST SkewStats(const T &val) {
        if (kuai::ku_value_traits<decltype(val)>::isNull(val)) {
            m_mean = ku_value_traits<AccumT>::null();
            m_m2 = ku_value_traits<AccumT>::null();
            m_m3 = ku_value_traits<AccumT>::null();
            m_count = 0;
        } else {
            m_mean = ku_cast<AccumT>()(val);
            m_m2 = ku_value_traits<AccumT>::dflt();
            m_m3 = ku_value_traits<AccumT>::dflt();
            m_count = 1;
        }
    }

    KU_DEVICE_HOST SkewStats operator+(const SkewStats &y) const {
        auto delta = y.m_mean - this->m_mean;
        auto delta2 = delta * delta;
        auto delta3 = delta2 * delta;
        auto delta4 = delta3 * delta;

        const KuI64 count = this->m_count + y.m_count;
        auto        count2 = count * count;
        auto        mean = this->m_mean + delta * y.m_count / count;

        auto M2 = this->m_m2 + y.m_m2;
        M2 += delta2 * this->m_count * y.m_count / count;

        auto M3 = this->m_m3 + y.m_m3;
        M3 += delta3 * m_count * y.m_count * (m_count - y.m_count) / count2;
        M3 += (AccumT)3.0 * delta * (this->m_count * y.m_m2 - y.m_count * this->m_m2) / count;

        return SkewStats(mean, M2, M3, count);
    }

    KU_DEVICE_HOST bool isNull() const noexcept {
        return kuai::ku_value_traits<decltype(m_mean)>::isNull(m_mean);
    }

    KU_DEVICE_HOST constexpr CountT count() const noexcept {
        return m_count;
    }

    AccumT m_mean;
    AccumT m_m2;
    AccumT m_m3;
    CountT m_count;
};

struct SkewReduceFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const SkewStats<T> &a, const SkewStats<T> &b) const {
        if (a.isNull())
            return b;
        if (b.isNull())
            return a;
        return a + b;
    }
};

struct SkewMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        return SkewStats<T>(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        return SkewStats<T>();
    }
};

struct SkewResultWithUnbiasCastFn {
    template <typename T>
    KU_DEVICE_HOST KuF64 operator()(const SkewStats<T> &a) const {
        if (a.m_count < 3 || a.isNull()) {
            return ku_value_traits<KuF64>::null();
        }
        if (a.m_m2 == 0) {
            return ku_value_traits<KuF64>::null();
        }
        const auto val = ku_sqrt()(a.m_count) * ku_value_traits<decltype(a.m_m3)>::repr(a.m_m3)
                         / ku_pow()(a.m_m2, 1.5);
        return ku_cast<KuF64>()(ku_sqrt()(a.m_count * (a.m_count - 1)) / (a.m_count - 2) * val);
    }
};
} // namespace kuai
