#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>

namespace kuai {

template <typename T>
struct KurtosisStats {
    using AccumT = std::conditional_t<std::is_same_v<T, KuF32>, KuF32, KuF64>;
    using CountT = KuI64;
    KU_DEVICE_HOST KurtosisStats() {
        m_mean = ku_value_traits<AccumT>::dflt();
        m_m2 = ku_value_traits<AccumT>::null();
        m_m3 = ku_value_traits<AccumT>::dflt();
        m_m4 = ku_value_traits<AccumT>::dflt();
        m_count = 0;
    }

    KU_DEVICE_HOST KurtosisStats(const AccumT &mean,
                                 const AccumT &m2,
                                 const AccumT &m3,
                                 const AccumT &m4,
                                 const CountT &count)
        : m_mean(mean), m_m2(m2), m_m3(m3), m_m4(m4), m_count(count) {
    }

    KU_DEVICE_HOST KurtosisStats(const T &val) {
        if (kuai::ku_value_traits<decltype(val)>::isNull(val)) {
            m_mean = ku_value_traits<AccumT>::dflt();
            m_m2 = ku_value_traits<AccumT>::null();
            m_m3 = ku_value_traits<AccumT>::dflt();
            m_m4 = ku_value_traits<AccumT>::dflt();
            m_count = 0;
        } else {
            m_mean = ku_cast<AccumT>()(val);
            m_m2 = ku_value_traits<AccumT>::dflt();
            m_m3 = ku_value_traits<AccumT>::dflt();
            m_m4 = ku_value_traits<AccumT>::dflt();
            m_count = 1;
        }
    }

    KU_DEVICE_HOST KurtosisStats operator+(const KurtosisStats &y) const {
        if (this->isNull()) {
            return y;
        } else if (y.isNull()) {
            return *this;
        }
        auto delta = y.m_mean - this->m_mean;
        auto delta2 = delta * delta;
        auto delta3 = delta2 * delta;
        auto delta4 = delta3 * delta;

        const KuI64 count = this->m_count + y.m_count;
        auto        count2 = count * count;
        auto        count3 = count2 * count;
        auto        mean = this->m_mean + delta * y.m_count / count;

        auto m2 = this->m_m2 + y.m_m2;
        m2 += delta2 * this->m_count * y.m_count / count;

        auto m3 = this->m_m3 + y.m_m3;
        m3 += delta3 * m_count * y.m_count * (m_count - y.m_count) / count2;
        m3 += (AccumT)3.0 * delta * (this->m_count * y.m_m2 - y.m_count * this->m_m2) / count;

        auto m4 = this->m_m4 + y.m_m4;
        m4 += delta4 * m_count * y.m_count
              * (this->m_count * this->m_count - this->m_count * y.m_count + y.m_count * y.m_count)
              / count3;
        m4 += (AccumT)6.0 * delta2
              * (this->m_count * this->m_count * y.m_m2 + y.m_count * y.m_count * this->m_m2)
              / count2;
        m4 += (AccumT)4.0 * delta * (this->m_count * y.m_m3 - y.m_count * this->m_m3) / count;
        return KurtosisStats(mean, m2, m3, m4, count);
    }

    KU_DEVICE_HOST bool isNull() const noexcept {
        // only m2 indicates if the value is null
        return kuai::ku_value_traits<decltype(m_m2)>::isNull(m_m2);
    }

    KU_DEVICE_HOST constexpr auto count() const noexcept {
        return m_count;
    }

    AccumT m_mean;
    AccumT m_m2;
    AccumT m_m3;
    AccumT m_m4;
    CountT m_count;
};

struct UnbiasedKurtosisResultFn {
    template <typename T>
    KU_DEVICE_HOST KuF64 operator()(const KurtosisStats<T> &a) const {
        if (a.m_count <= 3 || a.isNull()) {
            return ku_value_traits<KuF64>::null();
        }
        if (a.m_m2 == 0) {
            return ku_value_traits<KuF64>::null();
        }
        auto n = a.m_count;
        auto k1 = KuF64(n * a.m_m4 / (a.m_m2 * a.m_m2));
        return KuF64(n - 1) / ((KuF64)1.0 * (n - 2) * (n - 3)) * ((n + 1) * k1 - 3 * (n - 1)) + 3;
    }
};

struct KurtosisReduceFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const KurtosisStats<T> &a, const KurtosisStats<T> &b) const {
        return a + b;
    }
};

struct KurtosisMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        return KurtosisStats<T>(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        return KurtosisStats<T>();
    }
};
} // namespace kuai
