#include <kuai/core/KuString.h>

namespace kuai {

KuString::KuString(std::string_view value) : KuObject(kString), m_value(value) {
}

std::string_view KuString::value() const noexcept {
    return m_value;
}

} // namespace kuai
