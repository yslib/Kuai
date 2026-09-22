#pragma once

#include <atomic>
#include <cstddef>

#include <kuai/core/KuCore.h>

namespace kuai {

class KuRefCounted {
public:
    // Intrusive ownership is opt-in per instance. Stack/static/uniquely owned
    // objects must not use ref()/deref() unless their owner establishes that
    // contract.
    void ref() const noexcept {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void deref() const noexcept {
        const auto previous = m_refCount.fetch_sub(1, std::memory_order_acq_rel);
        KU_ASSERT(previous > 0, "cannot deref an unreferenced kuai ref-counted object");
        if (previous == 1) {
            delete this;
        }
    }

    [[nodiscard]] std::size_t refCount() const noexcept {
        return m_refCount.load(std::memory_order_relaxed);
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
    mutable std::atomic_size_t m_refCount = 0;
};

} // namespace kuai
