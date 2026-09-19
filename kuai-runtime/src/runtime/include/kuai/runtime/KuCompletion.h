#pragma once

#include <condition_variable>
#include <mutex>

#include <kuai/core/KuPointer.h>
#include <kuai/core/KuRefCounted.h>
#include <kuai/kuai_c/ku_completion.h>

namespace kuai {

class KuCompletion final : public KuRefCounted {
public:
    KuCompletion() = default;

    KuCompletion(const KuCompletion &) = delete;
    KuCompletion &operator=(const KuCompletion &) = delete;

    ku_status_t wait();
    ku_status_t onCompletion(ku_completion_callback_t newCallback, void *newUserData);
    void        retainUntilCompletion(ku_sp<KuRefCounted> object) noexcept;
    // Producer-side phase boundary: release protected objects before the
    // operation becomes externally observable or its task is declared idle.
    void prepareToSignal() noexcept;
    void signal(ku_status_t status) noexcept;

    ~KuCompletion() override = default;

private:
    std::mutex               m_mutex;
    std::condition_variable  m_ready;
    ku_status_t              m_status = KU_STATUS_INTERNAL_ERROR;
    ku_completion_callback_t m_callback = nullptr;
    void                    *m_userData = nullptr;
    ku_sp<KuRefCounted>      m_retainedObject;
    bool                     m_acceptRetainedObject = true;
    bool                     m_completed = false;
    bool                     m_callbackRegistered = false;
};

} // namespace kuai
