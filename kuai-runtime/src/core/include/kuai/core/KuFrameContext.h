#pragma once

#include <utility>

#include <kuai/core/KuContext.h>

namespace kuai {

// Device-bound context storage owned independently from one FFI invocation.
class KuFrameContext final {
public:
    KuFrameContext(const KuFrameContext &) = delete;
    KuFrameContext &operator=(const KuFrameContext &) = delete;
    KuFrameContext(KuFrameContext &&) = delete;
    KuFrameContext &operator=(KuFrameContext &&) = delete;

    explicit KuFrameContext(KuDevice &device) noexcept : m_context(device) {
    }

    explicit KuFrameContext(KuContext context) noexcept : m_context(std::move(context)) {
    }

    [[nodiscard]] KuContext *context() noexcept {
        return &m_context;
    }

    [[nodiscard]] const KuContext *context() const noexcept {
        return &m_context;
    }

    [[nodiscard]] KuDevice *getDevice() const noexcept {
        return &m_context.getDevice();
    }

private:
    KuContext m_context;
};

} // namespace kuai
