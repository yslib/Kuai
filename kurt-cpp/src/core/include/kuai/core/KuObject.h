#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuObjectDefs.h>
#include <kuai/core/KuPointer.h>
#include <kuai/core/KuRefCounted.h>
#include <kuai/core/KuRtti.h>
#include <kuai/core/KuTypes.h>

namespace kuai {

// clang-format off
using ku_object_type_t = std::int32_t;

struct KuObjectKindTag;

template <>
struct ku_rtti_kind_traits<KuObjectKindTag> {
    using value_type = ku_object_type_t;
};

using KuObjectKind = KuRttiKind<KuObjectKindTag>;

inline constexpr KuObjectKind KU_UNKNOWN{0};

#define KU_DECLARE_SECTION(_sec_, _parent_, _begin_, _user_begin_, _end_, _items_, _cls_name_) \
    inline constexpr KuObjectKind KU_PP_CAT3(KU_, _sec_, _BEGIN){_begin_};                       \
    _items_(KU_DECLARE_KIND_ITEMS)                                                               \
    inline constexpr KuObjectKind KU_PP_CAT3(KU_, _sec_, _USER_BEGIN){_user_begin_};             \
    inline constexpr KuObjectKind KU_PP_CAT3(KU_, _sec_, _END){_end_};

#define KU_DECLARE_KIND_ITEMS(_kind_, _value_) inline constexpr KuObjectKind KU_PP_CAT(k, _kind_){_value_};
KU_OBJECT_SECTIONS(KU_DECLARE_SECTION)
#undef KU_DECLARE_KIND_ITEMS
#undef KU_DECLARE_SECTION

#define KU_DECLARE_OTHERS_ITEMS(_item_, _value_) inline constexpr KuObjectKind KU_PP_CAT(k, _item_){_value_};
KU_OTHER_ITEMS(KU_DECLARE_OTHERS_ITEMS)
#undef KU_DECLARE_OTHERS_ITEMS

inline constexpr KuObjectKind KU_OBJECT_USER_DEFINE_BEGIN{KU_OBJECT_USER_DEFINE_BEGIN_VALUE};

// clang-format on

inline std::string_view toString(KuObjectKind type) noexcept {
    switch (type.value()) {
#define KU_DECLARE_SECTION(_sec_, _parent_, _begin_, _user_begin_, _end_, _items_, _cls_name_) \
    _items_(KU_DECLARE_KIND_ITEMS)

#define KU_DECLARE_KIND_ITEMS(_kind_, _value_) \
    case KU_PP_CAT(k, _kind_).value():         \
        return #_kind_;
        KU_OBJECT_SECTIONS(KU_DECLARE_SECTION)
#undef KU_DECLARE_KIND_ITEMS
#undef KU_DECLARE_SECTION

#define KU_DECLARE_OTHERS_ITEMS(_item_, _value_) \
    case KU_PP_CAT(k, _item_).value():           \
        return #_item_;
        KU_OTHER_ITEMS(KU_DECLARE_OTHERS_ITEMS)
#undef KU_DECLARE_OTHERS_ITEMS
        default:
            return "KU_UNKNOWN";
    };
}

// clang-format on

class KuObject : public KuRtti<KuObjectKind>, public KuRefCounted {
public:
    KU_RTTI_LEAF(KuObject, KU_UNKNOWN)

protected:
    KuObject(KuObjectKind type) : KuRtti<KuObjectKind>(type) {
    }

    KuObject(const KuObject &other) noexcept : KuRtti<KuObjectKind>(other), KuRefCounted(other) {
    }

    KuObject(KuObject &&other) noexcept
        : KuRtti<KuObjectKind>(std::move(other)), KuRefCounted(std::move(other)) {
    }

public:
    KuObject &operator=(const KuObject &other) noexcept {
        KuRtti<KuObjectKind>::operator=(other);
        KuRefCounted::operator=(other);
        return *this;
    }

    KuObject &operator=(KuObject &&other) noexcept {
        KuRtti<KuObjectKind>::operator=(std::move(other));
        KuRefCounted::operator=(std::move(other));
        return *this;
    }

    virtual ~KuObject() = default;
};

} // namespace kuai
