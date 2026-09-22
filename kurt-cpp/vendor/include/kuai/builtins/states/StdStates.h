#pragma once

#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/functional/KuSqrt.h>
#include <kuai/ktl/functional/KuSub.h>
namespace kuai {

template <typename T>
struct StdStats {
    using AccumT = std::conditional_t<std::is_same_v<T, KuF32>, KuF32, KuF64>;
    using CountT = KuI32;
    KU_DEVICE_HOST StdStats() {
        m_mean = ku_value_traits<AccumT>::null();
        m_ssr = ku_value_traits<AccumT>::null();
        m_count = 0;
    }

    KU_DEVICE_HOST StdStats(const AccumT &mean, const AccumT &ssr, const CountT &count)
        : m_mean(mean), m_ssr(ssr), m_count(count) {
    }

    KU_DEVICE_HOST StdStats(const T &val) {
        if (kuai::ku_value_traits<decltype(val)>::isNull(val)) {
            m_mean = kuai::ku_value_traits<AccumT>::null();
            m_ssr = kuai::ku_value_traits<AccumT>::null();
            m_count = 0;
        } else {
            m_mean = ku_cast<AccumT>()(val);
            m_ssr = ku_value_traits<AccumT>::dflt();
            m_count = 1;
        }
    }

    KU_DEVICE_HOST StdStats operator+(const StdStats &a) const {
        auto   subOp = ku_sub();
        auto   addOp = ku_add();
        auto   mulOp = ku_mul();
        auto   castOp = ku_cast<AccumT>();
        AccumT delta = subOp(m_mean, a.m_mean);
        AccumT delta2 = mulOp(delta, delta);
        CountT n = m_count + a.m_count;

        AccumT mean = addOp(a.m_mean, mulOp(delta, m_count * 1.0 / n));
        AccumT ssr = addOp(m_ssr, a.m_ssr);
        AccumT tmp = mulOp(m_count * a.m_count * 1.0 / n, delta2);
        ssr = addOp(ssr, tmp);
        return StdStats(mean, ssr, n);
    }

    KU_DEVICE_HOST bool isNull() const noexcept {
        return kuai::ku_value_traits<decltype(m_mean)>::isNull(m_mean);
    };

    KU_DEVICE_HOST CountT count() const {
        return m_count;
    }

    AccumT m_mean;
    AccumT m_ssr;
    CountT m_count;
};

struct StdResultCastFn {
    template <typename T>
    KU_DEVICE_HOST KuF64 operator()(const StdStats<T> &a) const {
        if (a.m_count < 2 || a.isNull()) {
            return ku_value_traits<KuF64>::null();
        }
        auto r =
            ku_sqrt()(ku_value_traits<decltype(a.m_ssr)>::repr(a.m_ssr) * 1.0 / (a.m_count - 1));
        return ku_cast<KuF64>()(r);
    }
};

struct PopulationStdResultCastFn {
    template <typename T>
    KU_DEVICE_HOST KuF64 operator()(const StdStats<T> &a) const {
        if (a.isNull()) {
            return ku_value_traits<KuF64>::null();
        }
        auto r = ku_sqrt()(ku_value_traits<decltype(a.m_ssr)>::repr(a.m_ssr) * 1.0 / a.m_count);
        return ku_cast<KuF64>()(r);
    }
};

struct StdReduceFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const StdStats<T> &a, const StdStats<T> &b) const {
        if (a.isNull())
            return b;
        if (b.isNull())
            return a;
        return a + b;
    }
};

struct StdMapFn {
    template <typename T>
    KU_DEVICE_HOST auto operator()(const T &a) const {
        return StdStats<T>(a);
    }

    template <typename T>
    KU_DEVICE_HOST static auto initValue() {
        return StdStats<T>();
    }
};
} // namespace kuai
