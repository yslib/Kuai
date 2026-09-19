#pragma once

#include <kuai/kuai_c/ku_object.h>

namespace kuai::bind {

// Input conversion is borrowed and must provide bool load(...) noexcept. Output conversion
// transfers an already-owned result and must provide void store(Storage *) noexcept.
template <typename Storage, typename Value>
struct KuArgCaster;

// Opaque kuai handles lower without changing ownership. A returned handle must
// already carry the +1 reference required by the ku_frame_t result contract.
template <>
struct KuArgCaster<ku_object_t, ku_object_t> {
    using storage_type = ku_object_t;
    using value_type = ku_object_t;
    value_type m_value{nullptr};

    bool load(const storage_type object) noexcept {
        m_value = object;
        return true;
    }

    void store(storage_type *object) noexcept {
        *object = m_value;
    }
};

} // namespace kuai::bind
