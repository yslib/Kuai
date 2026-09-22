#pragma once

#include <kuai/core/KuObject.h>
#include <kuai/kuai_c/ku_slice.h>

namespace kuai {

class KuSlice final : public KuObject {
public:
    KU_RTTI_LEAF(KuSlice, kSlice)

    explicit KuSlice(ku_slice_desc_t value);

    [[nodiscard]] const ku_slice_desc_t &value() const noexcept;

private:
    ku_slice_desc_t m_value;
};

} // namespace kuai
