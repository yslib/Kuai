#pragma once

#include <utility>

#include <kuai/kuai_c/ku_object.h>

namespace kuai {

class KuPyHandle {
public:
    KuPyHandle() noexcept = default;

    ~KuPyHandle() noexcept {
        reset();
    }

    KuPyHandle(const KuPyHandle &) = delete;
    KuPyHandle &operator=(const KuPyHandle &) = delete;

    KuPyHandle(KuPyHandle &&other) noexcept : m_handle(other.detach()) {
    }

    KuPyHandle &operator=(KuPyHandle &&other) noexcept {
        if (this != &other) {
            reset(other.detach());
        }
        return *this;
    }

    [[nodiscard]] static KuPyHandle adopt(ku_object_t handle) noexcept {
        return KuPyHandle(handle);
    }

    [[nodiscard]] static ku_status_t retain(ku_object_t handle, KuPyHandle *out) noexcept {
        out->reset();
        if (handle == nullptr) {
            return KU_STATUS_SUCCESS;
        }
        const ku_status_t status = ku_object_retain(handle);
        if (status == KU_STATUS_SUCCESS) {
            out->m_handle = handle;
        }
        return status;
    }

    [[nodiscard]] ku_object_t get() const noexcept {
        return m_handle;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_handle != nullptr;
    }

    [[nodiscard]] ku_object_t detach() noexcept {
        return std::exchange(m_handle, nullptr);
    }

    void reset(ku_object_t handle = nullptr) noexcept {
        if (m_handle != nullptr) {
            (void)ku_object_release(m_handle);
        }
        m_handle = handle;
    }

private:
    explicit KuPyHandle(ku_object_t handle) noexcept : m_handle(handle) {
    }

    ku_object_t m_handle = nullptr;
};

} // namespace kuai
