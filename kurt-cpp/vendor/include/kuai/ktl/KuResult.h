#pragma once
#include <functional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace kuai {

template <typename T, typename E>
class KuResult {
public:
    using ValueType = T;
    using ErrorType = E;
    // Constructors for success and error
    constexpr KuResult(const T &value) : m_expected(value) {
    }
    constexpr KuResult(T &&value) : m_expected(std::move(value)) {
    }
    constexpr KuResult(const E &error) : m_expected(error) {
    }
    constexpr KuResult(E &&error) : m_expected(std::move(error)) {
    }
    constexpr KuResult() : m_expected(E()) {
    }

    KuResult &operator=(const KuResult &other) noexcept {
        m_expected = other.m_expected;
        return *this;
    }
    KuResult &operator=(KuResult &&other) noexcept {
        m_expected = std::move(other.m_expected);
        return *this;
    }
    KuResult(const KuResult &other) : m_expected(other.m_expected) {
    }
    KuResult(KuResult &&other) : m_expected(std::move(other.m_expected)) {
    }

    // Check if the result is a success
    bool hasValue() const noexcept {
        return std::holds_alternative<T>(m_expected);
    }

    operator bool() const noexcept {
        return hasValue();
    }
    constexpr const T *operator->() const noexcept {
        return &value();
    }
    constexpr T *operator->() noexcept {
        return &value();
    }

    constexpr const T &operator*() const & noexcept {
        return value();
    }
    constexpr T &operator*() & noexcept {
        return value();
    }
    constexpr T &&operator*() && noexcept {
        return std::move(*this).value();
    }

    // Get the value, throws if it's an error
    T &value() & {
        if (!hasValue()) {
            mayPanic();
        }
        return std::get<T>(m_expected);
    }

    const T &value() const & {
        if (!hasValue()) {
            mayPanic();
        }
        return std::get<T>(m_expected);
    }

    T &&value() && {
        if (!hasValue()) {
            mayPanic();
        }
        return std::move(std::get<T>(m_expected));
    }

    E &error() & {
        if (hasValue()) {
            mayPanic();
        }
        return std::get<E>(m_expected);
    }

    const E &error() const & {
        if (hasValue()) {
            mayPanic();
        }
        return std::get<E>(m_expected);
    }

    E &&error() && {
        if (hasValue()) {
            mayPanic();
        }
        return std::move(std::get<E>(m_expected));
    }

    T valueOr(const T &default_value) const & {
        if (hasValue()) {
            return std::get<T>(m_expected);
        }
        return default_value;
    }

    T valueOr(const T &default_value) && {
        if (hasValue()) {
            return std::move(std::get<T>(m_expected));
        }
        return default_value;
    }

    E errorOr(const E &default_error) const & {
        if (!hasValue()) {
            return std::get<E>(m_expected);
        }
        return default_error;
    }

    E errorOr(const E &default_error) && {
        if (!hasValue()) {
            return std::move(std::get<E>(m_expected));
        }
        return default_error;
    }

    // Monadic operation: and_then
    template <typename F>
    auto andThen(F &&func) const & -> KuResult<decltype(func(std::declval<T>())), E> {
        if (hasValue()) {
            return func(std::get<T>(m_expected));
        }
        return std::get<E>(m_expected);
    }

    template <typename F>
    auto andThen(F &&func) && -> KuResult<decltype(func(std::declval<T>())), E> {
        if (hasValue()) {
            return std::forward<F>(func)(std::move(std::get<T>(m_expected)));
        }
        return std::move(std::get<E>(m_expected));
    }

    // Monadic operation: or_else
    template <typename F>
    auto orElse(F &&func) const & -> KuResult<T, decltype(func(std::declval<E>()))> {
        if (!hasValue()) {
            return func(std::get<E>(m_expected));
        }
        return std::get<T>(m_expected);
    }

    template <typename F>
    auto orElse(F &&func) && -> KuResult<T, decltype(func(std::declval<E>()))> {
        if (!hasValue()) {
            return std::forward<F>(func)(std::move(std::get<E>(m_expected)));
        }
        return std::move(std::get<T>(m_expected));
    }

    // Monadic operation: map
    template <typename F>
    auto map(F &&func) const & -> KuResult<decltype(func(std::declval<T>())), E> {
        if (hasValue()) {
            return std::forward<F>(func)(std::get<T>(m_expected));
        }
        return std::get<E>(m_expected);
    }

    template <typename F>
    auto map(F &&func) && -> KuResult<decltype(func(std::declval<T>())), E> {
        if (hasValue()) {
            return std::forward<F>(func)(std::move(std::get<T>(m_expected)));
        }
        return std::move(std::get<E>(m_expected));
    }

    template <typename F>
    auto mapError(F &&func) const & -> KuResult<T, decltype(func(std::declval<E>()))> {
        if (!hasValue()) {
            return std::forward<F>(func)(std::get<E>(m_expected));
        }
        return std::get<T>(m_expected);
    }

    template <typename F>
    auto mapError(F &&func) && -> KuResult<T, decltype(func(std::declval<E>()))> {
        if (!hasValue()) {
            return std::forward<F>(func)(std::move(std::get<E>(m_expected)));
        }
        return std::move(std::get<T>(m_expected));
    }

    // Monadic operation: map_or_else
    template <typename F, typename G>
    auto mapOrElse(F &&func, G &&error_func) const & -> decltype(func(std::declval<T>())) {
        if (hasValue()) {
            return std::forward<F>(func)(std::get<T>(m_expected));
        }
        return std::forward<G>(error_func)(std::get<E>(m_expected));
    }

    template <typename F, typename G>
    auto mapOrElse(F &&func, G &&error_func) && -> decltype(func(std::declval<T>())) {
        if (hasValue()) {
            return std::forward<F>(func)(std::move(std::get<T>(m_expected)));
        }
        return std::forward<G>(error_func)(std::move(std::get<E>(m_expected)));
    }

    // Monadic operation: map_or
    template <typename F>
    auto mapOr(F &&default_func) const & -> decltype(default_func(std::declval<E>())) {
        if (hasValue()) {
            return std::get<T>(m_expected);
        }
        return std::forward<F>(default_func)(std::get<E>(m_expected));
    }

    template <typename F>
    auto mapOr(F &&default_func) && -> decltype(default_func(std::declval<E>())) {
        if (hasValue()) {
            return std::move(std::get<T>(m_expected));
        }
        return std::forward<F>(default_func)(std::move(std::get<E>(m_expected)));
    }

private:
    std::variant<T, E> m_expected;

    void mayPanic() const {
        if constexpr (std::is_same_v<E, std::string>) {
            throw std::runtime_error(std::get<std::string>(m_expected));
        } else {
            throw std::runtime_error("Called value() on an error result");
        }
    }
};

template <typename T>
struct KuIsResultType : std::false_type {};

template <typename T, typename E>
struct KuIsResultType<KuResult<T, E>> : std::true_type {};

template <typename T>
inline constexpr bool KuIsResultType_v = KuIsResultType<T>::value;

template <typename T>
using KuResultInfo = KuResult<T, std::string>;

} // namespace kuai
