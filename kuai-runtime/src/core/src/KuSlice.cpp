#include <kuai/core/KuSlice.h>

namespace kuai {

KuSlice::KuSlice(ku_slice_desc_t value) : KuObject(kSlice), m_value(value) {
}

const ku_slice_desc_t &KuSlice::value() const noexcept {
    return m_value;
}

} // namespace kuai
