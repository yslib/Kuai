#pragma once

#include <string>
#include <string_view>

#include <kuai/core/KuObject.h>

namespace kuai {

class KuString final : public KuObject {
public:
    KU_RTTI_LEAF(KuString, kString)

    explicit KuString(std::string_view value);

    [[nodiscard]] std::string_view value() const noexcept;

private:
    std::string m_value;
};

} // namespace kuai
