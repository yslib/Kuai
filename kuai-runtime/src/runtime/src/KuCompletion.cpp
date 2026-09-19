#include <utility>

#include <kuai/runtime/KuCompletion.h>

namespace kuai {

ku_status_t KuCompletion::wait() {
    std::unique_lock lock(m_mutex);
    m_ready.wait(lock, [this]() { return m_completed; });
    return m_status;
}

ku_status_t KuCompletion::onCompletion(ku_completion_callback_t newCallback, void *newUserData) {
    if (newCallback == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    ku_status_t completionStatus = KU_STATUS_INTERNAL_ERROR;
    bool        invokeNow = false;
    {
        std::lock_guard lock(m_mutex);
        if (m_callbackRegistered) {
            return KU_STATUS_INVALID_STATE;
        }
        m_callbackRegistered = true;
        if (m_completed) {
            completionStatus = m_status;
            invokeNow = true;
        } else {
            m_callback = newCallback;
            m_userData = newUserData;
        }
    }

    if (invokeNow) {
        ref();
        newCallback(newUserData, completionStatus);
        deref();
    }
    return KU_STATUS_SUCCESS;
}

void KuCompletion::retainUntilCompletion(ku_sp<KuRefCounted> object) noexcept {
    if (!object) {
        return;
    }

    std::lock_guard lock(m_mutex);
    if (m_acceptRetainedObject) {
        KU_ASSERT(!m_retainedObject, "a completion may retain only one internal object");
        m_retainedObject = std::move(object);
    }
}

void KuCompletion::prepareToSignal() noexcept {
    ku_sp<KuRefCounted> retainedObject;
    {
        std::lock_guard lock(m_mutex);
        if (!m_acceptRetainedObject) {
            return;
        }
        m_acceptRetainedObject = false;
        retainedObject = std::move(m_retainedObject);
    }
}

void KuCompletion::signal(ku_status_t status) noexcept {
    prepareToSignal();

    ku_completion_callback_t callback = nullptr;
    void                    *userData = nullptr;
    {
        std::lock_guard lock(m_mutex);
        KU_ASSERT(!m_completed, "a completion may only be signaled once");
        if (m_completed) {
            return;
        }
        m_completed = true;
        m_status = status;
        callback = m_callback;
        userData = m_userData;
    }
    m_ready.notify_all();
    if (callback != nullptr) {
        ref();
        callback(userData, status);
        deref();
    }
}

} // namespace kuai
