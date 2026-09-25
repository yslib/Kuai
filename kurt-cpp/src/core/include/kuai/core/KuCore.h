#pragma once

#include <memory>
#include <stdexcept>
#include <string_view>

#include <kuai/core/KuDebug.h>
#include <kuai/kuai_c/ku_types.h>

#define KU_STRINGIFY(x) #x
#define KU_TOSTRING(x)  KU_STRINGIFY(x)

// Vendor entry visibility. Windows exports are specified by the linker.
#if defined(_WIN32)
#define KU_EXPORT
#elif defined(__GNUC__) || defined(__clang__)
#define KU_EXPORT __attribute__((visibility("default")))
#else
#define KU_EXPORT
#endif

#define KU_DECL_IMPL()                           \
    class Impl;                                  \
    inline Impl *d_func() noexcept {             \
        return d_ptr.get();                      \
    }                                            \
    inline const Impl *d_func() const noexcept { \
        return d_ptr.get();                      \
    }                                            \
    std::unique_ptr<Impl> const d_ptr;

#define KU_DECL_API(Class)                        \
    inline Class *q_func() {                      \
        return static_cast<Class *>(q_ptr);       \
    }                                             \
    inline const Class *q_func() const {          \
        return static_cast<const Class *>(q_ptr); \
    }                                             \
    friend class Class;                           \
    Class *const q_ptr = nullptr;

#define KU_IMPL() auto *const _ = d_func();
#define KU_API    (Class) Class *const _ = q_func();

namespace kuai {

inline std::string_view kuStatusToString(ku_status_t status) {
    switch (status) {
#define X(name, value, text) \
    case KU_STATUS_##name:   \
        return text;
        KU_STATUS_DEFS(X)
#undef X
        default:
            return "unknown status";
    }
}

namespace detail {

class KuStatusError final : public std::exception {
public:
    explicit KuStatusError(ku_status_t status) noexcept : m_status(status) {
    }

    [[nodiscard]] ku_status_t status() const noexcept {
        return m_status;
    }

    [[nodiscard]] const char *what() const noexcept override {
        return "kuai classified status error";
    }

private:
    ku_status_t m_status;
};

} // namespace detail

} // namespace kuai

#define _CONCAT_IMPL(a, b)   a##b
#define KU_CONCAT(a, b)      _CONCAT_IMPL(a, b)
#define KU_UNIQUE_NAME(base) KU_CONCAT(base, __COUNTER__)
#define KU_UNUSED(expr)      static_cast<void>(expr)

#if defined(_MSC_VER)
#define KU_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define KU_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define KU_ALWAYS_INLINE inline
#endif

#define KU_STATUS_CHECK(expr)                          \
    do {                                               \
        const auto _res = (expr);                      \
        if (_res != KU_STATUS_SUCCESS) {               \
            throw ::kuai::detail::KuStatusError(_res); \
        }                                              \
    } while (0)

#define KU_DEPRECATED(msg) [[deprecated(msg)]]
