#pragma once

#include <cstdio>
#include <cstdlib>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

#if defined(__has_builtin)
#if __has_builtin(__builtin_trap)
#define KU_TRAP() __builtin_trap()
#endif
#if __has_builtin(__builtin_unreachable)
#define KU_BUILTIN_UNREACHABLE() __builtin_unreachable()
#endif
#endif

#ifndef KU_TRAP
#define KU_TRAP() std::abort()
#endif

#ifndef KU_BUILTIN_UNREACHABLE
#define KU_BUILTIN_UNREACHABLE() ((void)0)
#endif

namespace kuai::detail {

inline void kuWriteFailure(std::string_view            kind,
                           std::string_view            detail,
                           const std::source_location &location) noexcept {
    try {
#if defined(NDEBUG)
        const auto diagnostic = detail.empty()
                                    ? std::format("[kuai] {}\n", kind)
                                    : std::format("[kuai] {}\n  detail: {}\n", kind, detail);
#else
        const auto diagnostic =
            detail.empty()
                ? std::format("[kuai] {}\n  location: {}:{}\n  function: {}\n", kind,
                              location.file_name(), location.line(), location.function_name())
                : std::format("[kuai] {}\n  detail: {}\n  location: {}:{}\n  function: {}\n", kind,
                              detail, location.file_name(), location.line(),
                              location.function_name());
#endif
        std::fwrite(diagnostic.data(), sizeof(char), diagnostic.size(), stderr);
    } catch (...) {
        std::fprintf(stderr, "[kuai] failure while formatting %.*s\n",
                     static_cast<int>(kind.size()), kind.data());
    }
    std::fflush(stderr);
}

[[noreturn]] inline void
kuFailImpl(const char                 *kind,
           const char                 *detail,
           const std::source_location &location = std::source_location::current()) noexcept {
    kuWriteFailure(kind, detail == nullptr ? std::string_view{} : std::string_view{detail},
                   location);
    KU_TRAP();
    std::abort();
}

inline void
kuAssertImpl(bool                        condition,
             const char                 *expression,
             const char                 *message,
             const std::source_location &location = std::source_location::current()) noexcept {
    if (condition) {
        return;
    }
    if (message == nullptr || message[0] == '\0') {
        kuFailImpl("KU_ASSERT", expression, location);
    }
    try {
        const auto detail = std::format("{} | message: {}", expression, message);
        kuFailImpl("KU_ASSERT", detail.c_str(), location);
    } catch (...) {
        kuFailImpl("KU_ASSERT", expression, location);
    }
}

} // namespace kuai::detail

#define KU_FAIL_IMPL(kind, msg) \
    (void)::kuai::detail::kuFailImpl((kind), (msg), std::source_location::current())

#if defined(NDEBUG)
#define KU_ASSERT(...) ((void)0)
#else
#define KU_ASSERT_1(expr)                                            \
    ::kuai::detail::kuAssertImpl(static_cast<bool>(expr), #expr, "", \
                                 std::source_location::current())
#define KU_ASSERT_2(expr, msg)                                          \
    ::kuai::detail::kuAssertImpl(static_cast<bool>(expr), #expr, (msg), \
                                 std::source_location::current())
#define KU_GET_ASSERT_MACRO(_1, _2, NAME, ...) NAME
#define KU_ASSERT(...)                         KU_GET_ASSERT_MACRO(__VA_ARGS__, KU_ASSERT_2, KU_ASSERT_1)(__VA_ARGS__)
#endif

// Unlike KU_ASSERT, KU_VERIFY always evaluates expr. Use it when the expression
// performs work whose side effects are required in release builds.
#if defined(NDEBUG)
#define KU_VERIFY_1(expr)      ((void)(expr))
#define KU_VERIFY_2(expr, msg) ((void)(expr))
#else
#define KU_VERIFY_1(expr)      KU_ASSERT_1(expr)
#define KU_VERIFY_2(expr, msg) KU_ASSERT_2(expr, msg)
#endif
#define KU_GET_VERIFY_MACRO(_1, _2, NAME, ...) NAME
#define KU_VERIFY(...)                         KU_GET_VERIFY_MACRO(__VA_ARGS__, KU_VERIFY_2, KU_VERIFY_1)(__VA_ARGS__)

#define KU_UNREACHABLE()                                                                     \
    do {                                                                                     \
        KU_FAIL_IMPL("KU_UNREACHABLE", "control flow reached a path marked as unreachable"); \
        KU_BUILTIN_UNREACHABLE();                                                            \
    } while (0)

#define KU_NOT_IMPLEMENTED()                                                  \
    do {                                                                      \
        KU_FAIL_IMPL("KU_NOT_IMPLEMENTED", "feature is not implemented yet"); \
        KU_BUILTIN_UNREACHABLE();                                             \
    } while (0)

#if defined(NDEBUG)
#define KU_DEBUG(...) \
    do {              \
    } while (0)
#define KU_DEBUG_EXPR(expr) ((void)0)
#else
#define KU_DEBUG(...) \
    do {              \
        __VA_ARGS__;  \
    } while (0)
#define KU_DEBUG_EXPR(expr) (expr)
#endif
