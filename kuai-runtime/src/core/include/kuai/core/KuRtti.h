#pragma once

#include <compare>
#include <cstddef>
#include <type_traits>

#define KU_RTTI_REQUIRED(T) \
    static_assert(::kuai::ku_is_rtti_registered_v<T>, "Type " #T " is not registered for RTTI")

#define KU_RTTI_LEAF(CLASS, KIND)                                     \
    struct ku_rtti_descriptor {                                       \
        using object_type = CLASS;                                    \
        using kind_type = typename CLASS::rtti_kind_type;             \
        static constexpr kind_type kind_v = KIND;                     \
        static constexpr bool      matches(kind_type kind) noexcept { \
            return kind == kind_v;                               \
        }                                                             \
    };

#define KU_RTTI_RANGE(CLASS, BEGIN_KIND, END_KIND)                    \
    struct ku_rtti_descriptor {                                       \
        using object_type = CLASS;                                    \
        using kind_type = typename CLASS::rtti_kind_type;             \
        static constexpr kind_type begin_v = BEGIN_KIND;              \
        static constexpr kind_type end_v = END_KIND;                  \
        static constexpr bool      matches(kind_type kind) noexcept { \
            return begin_v < kind && kind < end_v;               \
        }                                                             \
    };

namespace kuai {

template <typename Tag>
struct ku_rtti_kind_traits;

template <typename Tag>
class KuRttiKind {
public:
    using value_type = typename ku_rtti_kind_traits<Tag>::value_type;

    constexpr explicit KuRttiKind(value_type value) noexcept : m_value(value) {
    }

    [[nodiscard]] constexpr value_type value() const noexcept {
        return m_value;
    }

    friend constexpr bool operator==(const KuRttiKind &, const KuRttiKind &) = default;
    friend constexpr auto operator<=>(const KuRttiKind &, const KuRttiKind &) = default;

private:
    value_type m_value;
};

template <typename T, typename = void>
struct ku_rtti_traits {
    using object_type = std::remove_cv_t<T>;
    using kind_type = void;

    static constexpr bool registered_v = false;
};

template <typename T>
struct ku_rtti_traits<T, std::void_t<typename std::remove_cv_t<T>::ku_rtti_descriptor>> {
    using object_type = std::remove_cv_t<T>;
    using descriptor_type = typename object_type::ku_rtti_descriptor;
    using kind_type = typename descriptor_type::kind_type;

    static constexpr bool registered_v =
        std::is_same_v<object_type, typename descriptor_type::object_type>;

    static constexpr bool matches(kind_type kind) noexcept {
        if constexpr (registered_v) {
            return descriptor_type::matches(kind);
        }
        return false;
    }
};

template <typename T>
struct ku_is_rtti_registered : std::bool_constant<ku_rtti_traits<T>::registered_v> {};

template <typename T>
inline constexpr bool ku_is_rtti_registered_v = ku_is_rtti_registered<T>::value;

template <typename Kind>
class KuRtti {
public:
    using rtti_kind_type = Kind;

    constexpr KuRtti(const KuRtti &) noexcept = default;
    constexpr KuRtti(KuRtti &&) noexcept = default;
    constexpr KuRtti &operator=(const KuRtti &) noexcept = default;
    constexpr KuRtti &operator=(KuRtti &&) noexcept = default;

    [[nodiscard]] constexpr Kind getKind() const noexcept {
        return m_kind;
    }

    template <typename T>
    [[nodiscard]] constexpr T *as() noexcept {
        using target_type = std::remove_cv_t<T>;
        using target_traits = ku_rtti_traits<target_type>;
        constexpr bool registered = target_traits::registered_v;
        constexpr bool sameKind = std::is_same_v<Kind, typename target_traits::kind_type>;
        constexpr bool related = std::is_base_of_v<KuRtti<Kind>, target_type>;

        static_assert(registered, "RTTI target type is not registered");
        static_assert(!registered || sameKind, "RTTI target type belongs to another kind domain");
        static_assert(!registered || related,
                      "RTTI target type does not derive from this RTTI domain");

        if constexpr (registered && sameKind && related) {
            return target_traits::matches(m_kind) ? static_cast<T *>(this) : nullptr;
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] constexpr const T *as() const noexcept {
        using target_type = std::remove_cv_t<T>;
        using target_traits = ku_rtti_traits<target_type>;
        constexpr bool registered = target_traits::registered_v;
        constexpr bool sameKind = std::is_same_v<Kind, typename target_traits::kind_type>;
        constexpr bool related = std::is_base_of_v<KuRtti<Kind>, target_type>;

        static_assert(registered, "RTTI target type is not registered");
        static_assert(!registered || sameKind, "RTTI target type belongs to another kind domain");
        static_assert(!registered || related,
                      "RTTI target type does not derive from this RTTI domain");

        if constexpr (registered && sameKind && related) {
            return target_traits::matches(m_kind) ? static_cast<const T *>(this) : nullptr;
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] constexpr T &cast() noexcept {
        static_assert(std::is_base_of_v<KuRtti<Kind>, std::remove_cv_t<T>>,
                      "cast target type does not derive from this RTTI domain");
        return static_cast<T &>(*this);
    }

    template <typename T>
    [[nodiscard]] constexpr const T &cast() const noexcept {
        static_assert(std::is_base_of_v<KuRtti<Kind>, std::remove_cv_t<T>>,
                      "cast target type does not derive from this RTTI domain");
        return static_cast<const T &>(*this);
    }

protected:
    constexpr explicit KuRtti(Kind kind) noexcept : m_kind(kind) {
    }

    ~KuRtti() = default;

private:
    Kind m_kind;
};

template <typename T, typename From>
[[nodiscard]] constexpr bool ku_isa(const From *object) noexcept {
    return object != nullptr && object->template as<T>() != nullptr;
}

template <typename T>
[[nodiscard]] constexpr bool ku_isa(std::nullptr_t) noexcept {
    KU_RTTI_REQUIRED(T);
    return false;
}

} // namespace kuai
