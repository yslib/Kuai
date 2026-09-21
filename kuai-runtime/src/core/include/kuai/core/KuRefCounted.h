#pragma once

#include <atomic>
#include <cstddef>

#include <kuai/core/KuCore.h>

#ifndef KURT_ATOMIC_REF_COUNT
#define KURT_ATOMIC_REF_COUNT 1
#endif

#if defined(KURT_REQUIRE_ATOMIC_REF_COUNT) && KURT_REQUIRE_ATOMIC_REF_COUNT \
    && !KURT_ATOMIC_REF_COUNT
#error "KURT_REQUIRE_ATOMIC_REF_COUNT requires KURT_ATOMIC_REF_COUNT"
#endif

namespace kuai {

class KuRefCounted {
public:
    // Intrusive ownership is opt-in per instance. Stack/static/uniquely owned
    // objects must not use ref()/deref() unless their owner establishes that
    // contract.
    void ref() const noexcept {
#if KURT_ATOMIC_REF_COUNT
        m_refCount.fetch_add(1, std::memory_order_relaxed);
#else
        ++m_refCount;
#endif
    }

    void deref() const noexcept {
#if KURT_ATOMIC_REF_COUNT
        const auto previous = m_refCount.fetch_sub(1, std::memory_order_acq_rel);
        KU_ASSERT(previous > 0, "cannot deref an unreferenced kuai ref-counted object");
        if (previous == 1) {
            delete this;
        }
#else
        KU_ASSERT(m_refCount > 0, "cannot deref an unreferenced kuai ref-counted object");
        if (--m_refCount == 0) {
            delete this;
        }
#endif
    }

    [[nodiscard]] std::size_t refCount() const noexcept {
#if KURT_ATOMIC_REF_COUNT
        return m_refCount.load(std::memory_order_relaxed);
#else
        return m_refCount;
#endif
    }

protected:
    KuRefCounted() = default;

    KuRefCounted(const KuRefCounted &) noexcept {
    }

    KuRefCounted(KuRefCounted &&) noexcept {
    }

    KuRefCounted &operator=(const KuRefCounted &) noexcept {
        return *this;
    }

    KuRefCounted &operator=(KuRefCounted &&) noexcept {
        return *this;
    }

    virtual ~KuRefCounted() = default;

private:
#if KURT_ATOMIC_REF_COUNT
    mutable std::atomic_size_t m_refCount = 0;
#else
    mutable std::size_t m_refCount = 0;
#endif
};

} // namespace kuai
