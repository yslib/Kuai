#pragma once
#include <type_traits>
#include <utility>

#include <kuai/core/KuObject.h>
#include <kuai/core/KuTypes.h>
namespace kuai {

class KuScalar : public KuObject {
public:
    KU_RTTI_LEAF(KuScalar, kScalar)

    template <typename T>
        requires ku_is_primitive_type_v<T>
    KuScalar(T value) : KuObject(kScalar), m_value(KuTypeTraits<T>::make(value)) {
    }

    explicit KuScalar(ku_union_t value) : KuObject(kScalar), m_value(value) {
    }

    void *data() {
        return const_cast<void *>(std::as_const(*this).data());
    }

    const void *data() const {
        switch (getType()) {
#define X(name, enum_value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                                  \
        return &KuTypeTraits<payload_type>::get(m_value);
            KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
            default:
                KU_ASSERT(false && "Unsupported scalar data type");
        }
        KU_UNREACHABLE();
    }

    ku_primitive_type_t getType() const {
        return m_value.tag;
    }

    ku_size_t elementBytes() const {
        return ku_primitive_type_size(getType());
    }

    ku_size_t bytes() const {
        return elementBytes();
    }

    template <typename T>
        requires ku_is_primitive_type_v<T>
    T &value() noexcept {
        return KuTypeTraits<T>::get(m_value);
    }

    template <typename T>
        requires ku_is_primitive_type_v<T>
    const T &value() const noexcept {
        return KuTypeTraits<T>::get(m_value);
    }

    template <typename T>
        requires ku_is_primitive_type_v<T>
    T *getIf() noexcept {
        return const_cast<T *>(std::as_const(*this).template getIf<T>());
    }

    template <typename T>
        requires ku_is_primitive_type_v<T>
    const T *getIf() const noexcept {
        if (KuTypeTraits<T>::primitiveType == getType()) {
            return &KuTypeTraits<T>::get(m_value);
        }
        return nullptr;
    }

    template <typename T>
        requires ku_is_primitive_type_v<T>
    void setValue(T value) {
        m_value = KuTypeTraits<T>::make(value);
    }

private:
    ku_union_t m_value;
};

} // namespace kuai
