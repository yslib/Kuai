#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/ku_types.h>
namespace kuai {

using KuVoid = ::ku_void_t;
using KuBool = ::ku_bool_t;

static_assert(std::is_standard_layout_v<KuVoid>);
static_assert(std::is_trivially_copyable_v<KuVoid>);
static_assert(std::is_standard_layout_v<KuBool>);
static_assert(std::is_trivially_copyable_v<KuBool>);

// type for kuai container definition
using KuVoid8 = KuVoid;
using KuBool8 = KuBool;
using KuChar8 = ku_char8_t;
using KuI16 = ku_i16_t;
using KuI32 = ku_i32_t;
using KuI64 = ku_i64_t;
using KuF32 = ku_f32_t;
using KuF64 = ku_f64_t;

inline const char *kuPrimitiveTypeName(ku_primitive_type_t type) {
    switch (type) {
#define X(name, enum_value, display_name, ...) \
    case KU_PRIMITIVE_##name:                  \
        return display_name;
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            return "<unknown>";
    }
}

template <typename>
struct ku_primitive_storage_traits {
    static constexpr bool isPrimitive = false;
};

#define X(name, enum_value, display_name, payload_type, field)                         \
    template <>                                                                        \
    struct ku_primitive_storage_traits<payload_type> {                                 \
        using type = payload_type;                                                     \
        static constexpr bool                isPrimitive = true;                       \
        static constexpr ku_primitive_type_t primitiveType = KU_PRIMITIVE_##name;      \
        static constexpr const char         *displayName = display_name;               \
        static constexpr std::size_t         size = sizeof(payload_type);              \
        static constexpr payload_type       &get(ku_union_t &storage) noexcept {       \
            return storage.field;                                                \
        }                                                                              \
        static constexpr const payload_type &get(const ku_union_t &storage) noexcept { \
            return storage.field;                                                      \
        }                                                                              \
        static constexpr ku_union_t make(payload_type value) noexcept {                \
            ku_union_t storage{};                                                      \
            storage.tag = primitiveType;                                               \
            get(storage) = value;                                                      \
            return storage;                                                            \
        }                                                                              \
    };
KU_PRIMITIVE_TYPE_DEFS(X)
#undef X

template <class T>
struct KuTypeTraits : ku_primitive_storage_traits<T> {
    static constexpr const char *name = "Unknown";
    static constexpr bool        isNumber = false;
    static constexpr bool        isIntegral = false;
    static constexpr bool        isFloatingPoint = false;
    static constexpr bool        isBool = false;
};

#define KU_PRIMITIVE_TYPE_TRAIT_DEFS(X)              \
    X(KuVoid8, 0, KuChar8, false, false, false, 'n') \
    X(KuBool8, 1, KuChar8, false, false, true, 'b')  \
    X(KuChar8, 2, KuI16, true, false, false, 'c')    \
    X(KuI16, 3, KuI32, true, false, false, 's')      \
    X(KuI32, 4, KuI32, true, false, false, 'i')      \
    X(KuI64, 5, KuI64, true, false, false, 'l')      \
    X(KuF32, 6, KuF32, false, true, false, 'f')      \
    X(KuF64, 7, KuF64, false, true, false, 'd')

#define X(type_, level, promoted_type, integral, floating_point, boolean, tag_name) \
    template <>                                                                     \
    struct KuTypeTraits<type_> : ku_primitive_storage_traits<type_>,                \
                                 std::integral_constant<int, level> {               \
        using type = type_;                                                         \
        using next = promoted_type;                                                 \
        static constexpr const char *name = #type_;                                 \
        static constexpr bool        isNumber = integral || floating_point;         \
        static constexpr bool        isIntegral = integral;                         \
        static constexpr bool        isFloatingPoint = floating_point;              \
        static constexpr bool        isBool = boolean;                              \
        static constexpr char        tag = tag_name;                                \
    };
KU_PRIMITIVE_TYPE_TRAIT_DEFS(X)
#undef X

// for non basic types, we don't define the next type and the level, we force non-basic type don't
// go through the default deduction process, you need to implement it for it.

template <typename T>
inline constexpr bool ku_is_primitive_type_v = KuTypeTraits<T>::isPrimitive;

template <typename T>
struct ku_is_number : std::bool_constant<KuTypeTraits<T>::isNumber> {};

template <typename T>
inline constexpr bool ku_is_number_v = ku_is_number<T>::value;

template <typename T>
struct ku_is_float : std::bool_constant<KuTypeTraits<T>::isFloatingPoint> {};

template <typename T>
inline constexpr bool ku_is_float_v = ku_is_float<T>::value;

template <typename T>
struct ku_is_integral : std::bool_constant<KuTypeTraits<T>::isIntegral> {};

template <typename T>
inline constexpr bool ku_is_integral_v = ku_is_integral<T>::value;

template <typename T>
struct ku_is_bool : std::bool_constant<KuTypeTraits<T>::isBool> {};

template <typename T>
inline constexpr bool ku_is_bool_v = ku_is_bool<T>::value;

inline size_t ku_primitive_type_size(ku_primitive_type_t type) {
    switch (type) {
#define X(name, enum_value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                                  \
        return KuTypeTraits<payload_type>::size;
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            KU_ASSERT(false && "Unsupported data type");
    }
    KU_UNREACHABLE();
}

template <typename T>
inline const char *toString() {
    return KuTypeTraits<T>::name;
}

template <class Lhs, class Rhs>
struct ku_promotion_type {
    using type =
        std::conditional_t<(KuTypeTraits<Lhs>::value < KuTypeTraits<Rhs>::value), Rhs, Lhs>;
};

template <class Lhs, class Rhs>
using ku_promotion_type_t = typename ku_promotion_type<Lhs, Rhs>::type;

template <typename T1, typename T2>
struct ku_bit_op_type {
    using type = std::conditional_t<(ku_is_bool_v<T1> && ku_is_bool_v<T2>),
                                    KuBool8,
                                    std::conditional_t<ku_is_integral_v<T1> && ku_is_integral_v<T2>,
                                                       ku_promotion_type_t<T1, T2>,
                                                       void>>;
};

template <typename T1, typename T2>
using ku_bit_op_type_t = typename ku_bit_op_type<T1, T2>::type;

template <class Lhs, class Rhs>
struct ku_has_void_type : public std::false_type {};
template <class R>
struct ku_has_void_type<KuVoid8, R> : public std::true_type {};
template <class L>
struct ku_has_void_type<L, KuVoid8> : public std::true_type {};
template <>
struct ku_has_void_type<KuVoid8, KuVoid8> : public std::true_type {};

template <typename T1, typename T2>
struct ku_both_void_type : public std::false_type {};

template <>
struct ku_both_void_type<KuVoid8, KuVoid8> : public std::true_type {};

template <typename T1, typename T2>
inline constexpr bool ku_both_void_type_v = ku_both_void_type<T1, T2>::value;

template <typename T1, typename T2>
inline constexpr bool ku_has_void_type_v = ku_has_void_type<T1, T2>::value;

template <class T, typename = std::enable_if_t<!std::is_same_v<T, KuVoid8>, T>>
struct ku_accum_result_type {
    using type =
        std::conditional_t<(KuTypeTraits<T>::value <= KuTypeTraits<KuI64>::value), KuI64, KuF64>;
};

template <class T>
using ku_accum_result_type_t = typename ku_accum_result_type<T>::type;

template <typename T1, typename T2>
struct ku_ratio_result_type {
    static constexpr size_t value = std::max(KuTypeTraits<T1>::value, KuTypeTraits<T2>::value);
    using type = std::conditional_t<(value == KuTypeTraits<KuF32>::value), KuF32, KuF64>;
};

template <typename T1, typename T2>
using ku_ratio_result_type_t = typename ku_ratio_result_type<T1, T2>::type;

template <typename T>
struct ku_floating_result_type {
    using type = std::conditional_t<std::is_same_v<T, KuF32>, KuF32, KuF64>;
};

template <typename T>
using ku_floating_result_type_t = typename ku_floating_result_type<T>::type;

template <class Lhs, class Rhs>
struct ku_binary_result_type {
private:
    using promotion_type = ku_promotion_type_t<Lhs, Rhs>;

public:
    // bool + bool -> char
    // char + char -> short
    // short + short -> int
    // int + int -> int
    using type =
        std::conditional_t<(KuTypeTraits<promotion_type>::value < KuTypeTraits<KuI32>::value),
                           typename KuTypeTraits<promotion_type>::next,
                           promotion_type>;
};

template <class Lhs, class Rhs>
using ku_binary_result_type_t = typename ku_binary_result_type<Lhs, Rhs>::type;

} // namespace kuai
