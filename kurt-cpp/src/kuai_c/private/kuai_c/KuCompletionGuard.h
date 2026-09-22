#pragma once

#include <kuai/kuai_c/ku_completion.h>

namespace kuai {

// Internal move-only RAII owner for one ku_completion_t reference. Destruction releases
// the reference but does not wait for the asynchronous operation.
class KuCompletionGuard {
public:
    KuCompletionGuard() noexcept = default;

    // Adopts one existing owned reference.
    explicit KuCompletionGuard(ku_completion_t completion) noexcept : m_completion(completion) {
    }

    KuCompletionGuard(const KuCompletionGuard &) = delete;
    KuCompletionGuard &operator=(const KuCompletionGuard &) = delete;

    KuCompletionGuard(KuCompletionGuard &&other) noexcept : m_completion(other.release()) {
    }

    KuCompletionGuard &operator=(KuCompletionGuard &&other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~KuCompletionGuard() {
        reset();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_completion != nullptr;
    }

    [[nodiscard]] ku_completion_t get() const noexcept {
        return m_completion;
    }

    // Releases the current reference and exposes an empty output slot.
    [[nodiscard]] ku_completion_t *put() noexcept {
        reset();
        return &m_completion;
    }

    // Creates another independently owned reference to the same completion.
    [[nodiscard]] KuCompletionGuard retain() const noexcept {
        if (m_completion == nullptr) {
            return {};
        }
        ku_completion_retain(m_completion);
        return KuCompletionGuard(m_completion);
    }

    [[nodiscard]] ku_completion_t release() noexcept {
        auto completion = m_completion;
        m_completion = nullptr;
        return completion;
    }

    void reset(ku_completion_t completion = nullptr) noexcept {
        if (m_completion != nullptr) {
            ku_completion_release(m_completion);
        }
        m_completion = completion;
    }

    [[nodiscard]] ku_status_t wait() const noexcept {
        return ku_completion_wait(m_completion);
    }

private:
    ku_completion_t m_completion = nullptr;
};

} // namespace kuai
