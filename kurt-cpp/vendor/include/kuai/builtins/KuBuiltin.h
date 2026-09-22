#pragma once
#include <string>
#include <utility>

#include <kuai/core/KuContext.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuObject.h>
#include <kuai/ktl/KuResult.h>

namespace kuai {

class KuBuiltinError {
public:
    KuBuiltinError(const char *message) : m_status(KU_STATUS_BUILTIN_ERROR), m_message(message) {
    }

    KuBuiltinError(std::string message)
        : m_status(KU_STATUS_BUILTIN_ERROR), m_message(std::move(message)) {
    }

    explicit KuBuiltinError(ku_status_t status) noexcept : m_status(status) {
    }

    KuBuiltinError(ku_status_t status, std::string message)
        : m_status(status), m_message(std::move(message)) {
    }

    [[nodiscard]] ku_status_t status() const noexcept {
        return m_status;
    }

    [[nodiscard]] const std::string &toString() const noexcept {
        return m_message;
    }

private:
    ku_status_t m_status = KU_STATUS_BUILTIN_ERROR;
    std::string m_message;
};

using KuBuiltinResult = KuResult<ku_sp<KuObject>, KuBuiltinError>;

} // namespace kuai
