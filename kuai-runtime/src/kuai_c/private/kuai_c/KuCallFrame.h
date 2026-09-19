#pragma once

#include <cstddef>
#include <span>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuFrameContext.h>
#include <kuai/core/KuObject.h>
#include <kuai/core/KuSmallBuffer.h>
#include <kuai/kuai_c/ku_builtin.h>

#include "kuai_c/KuCHandle.h"

namespace kuai {

// Stack-oriented owner for a C++-initiated FFI call. Arguments and context are
// borrowed; non-null results remain owned here until releaseResult() transfers them.
template <size_t ResultCapacity = 1, size_t InlineArgumentCapacity = 8>
class KuCallFrame final {
    static_assert(ResultCapacity > 0, "a kuai call frame must provide at least one result slot");
    static_assert(InlineArgumentCapacity > 0,
                  "the inline argument capacity must be greater than zero");

public:
    KuCallFrame(ku_frame_ctx_t context, std::span<const ku_object_t> positionalArguments) {
        initialize(context, positionalArguments, ResultCapacity);
    }

    KuCallFrame(ku_frame_ctx_t               context,
                std::span<const ku_object_t> positionalArguments,
                size_t                       resultCapacity) {
        initialize(context, positionalArguments, resultCapacity);
    }

    KuCallFrame(KuFrameContext &context, std::span<const ku_object_t> positionalArguments)
        : KuCallFrame(capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments) {
    }

    KuCallFrame(KuFrameContext              &context,
                std::span<const ku_object_t> positionalArguments,
                size_t                       resultCapacity)
        : KuCallFrame(
              capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments, resultCapacity) {
    }

    KuCallFrame(ku_frame_ctx_t context, std::span<KuObject *const> positionalArguments) {
        for (auto *argument : positionalArguments) {
            m_arguments.push_back(capi::toHandle<ku_object_t>(argument));
        }
        initialize(context, m_arguments.view(), ResultCapacity);
    }

    KuCallFrame(ku_frame_ctx_t             context,
                std::span<KuObject *const> positionalArguments,
                size_t                     resultCapacity) {
        for (auto *argument : positionalArguments) {
            m_arguments.push_back(capi::toHandle<ku_object_t>(argument));
        }
        initialize(context, m_arguments.view(), resultCapacity);
    }

    KuCallFrame(KuFrameContext &context, std::span<KuObject *const> positionalArguments)
        : KuCallFrame(capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments) {
    }

    KuCallFrame(KuFrameContext            &context,
                std::span<KuObject *const> positionalArguments,
                size_t                     resultCapacity)
        : KuCallFrame(
              capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments, resultCapacity) {
    }

    KuCallFrame(ku_frame_ctx_t context, std::span<const KuObject *> positionalArguments) {
        for (const auto *argument : positionalArguments) {
            m_arguments.push_back(capi::toHandle<ku_object_t>(argument));
        }
        initialize(context, m_arguments.view(), ResultCapacity);
    }

    KuCallFrame(ku_frame_ctx_t              context,
                std::span<const KuObject *> positionalArguments,
                size_t                      resultCapacity) {
        for (const auto *argument : positionalArguments) {
            m_arguments.push_back(capi::toHandle<ku_object_t>(argument));
        }
        initialize(context, m_arguments.view(), resultCapacity);
    }

    KuCallFrame(KuFrameContext &context, std::span<const KuObject *> positionalArguments)
        : KuCallFrame(capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments) {
    }

    KuCallFrame(KuFrameContext             &context,
                std::span<const KuObject *> positionalArguments,
                size_t                      resultCapacity)
        : KuCallFrame(
              capi::toHandle<ku_frame_ctx_t>(&context), positionalArguments, resultCapacity) {
    }

    ~KuCallFrame() noexcept {
        for (auto &result : m_results) {
            auto *object = capi::fromHandle(std::exchange(result, nullptr));
            if (object != nullptr) {
                object->deref();
            }
        }
    }

    KuCallFrame(const KuCallFrame &) = delete;
    KuCallFrame &operator=(const KuCallFrame &) = delete;
    KuCallFrame(KuCallFrame &&) = delete;
    KuCallFrame &operator=(KuCallFrame &&) = delete;

    ku_frame_t *get() noexcept {
        return &m_frame;
    }

    const ku_frame_t *get() const noexcept {
        return &m_frame;
    }

    size_t resultCount() const noexcept {
        return m_frame.result_count;
    }

    ku_object_t releaseResult(size_t index = 0) noexcept {
        KU_ASSERT(index < m_results.size(), "result index exceeds the call frame capacity");
        KU_ASSERT(index < m_frame.result_count, "result index exceeds the returned result count");
        return std::exchange(m_results.data()[index], nullptr);
    }

private:
    void initialize(ku_frame_ctx_t               context,
                    std::span<const ku_object_t> positionalArguments,
                    size_t                       resultCapacity) {
        // Empty spans are allowed to have a null data pointer, while the C frame
        // contract requires argv to remain non-null even for zero arguments.
        const auto *argv =
            positionalArguments.empty() ? m_arguments.data() : positionalArguments.data();
        const size_t physicalResultCapacity = resultCapacity == 0 ? 1 : resultCapacity;
        for (size_t i = 0; i < physicalResultCapacity; ++i) {
            m_results.push_back(nullptr);
        }
        m_frame = ku_frame_t{.ctx = context,
                             .argv = argv,
                             .pargn = positionalArguments.size(),
                             .knames = nullptr,
                             .kargn = 0,
                             .results = m_results.data(),
                             .result_capacity = physicalResultCapacity,
                             .result_count = 0};
    }

    KuSmallBufferN<ku_object_t, InlineArgumentCapacity> m_arguments;
    KuSmallBufferN<ku_object_t, ResultCapacity>         m_results;
    ku_frame_t                                          m_frame{};
};

} // namespace kuai
