#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

// Intrusive ownership primitives for kuai objects.
// Reference-count thread safety is selected by KuObject.

struct ku_adopt_ref_t {
    explicit constexpr ku_adopt_ref_t() = default;
};

inline constexpr ku_adopt_ref_t ku_adopt_ref{};

template <typename T>
class ku_sp {
public:
    using element_type = T;

    constexpr ku_sp() noexcept = default;

    constexpr ku_sp(std::nullptr_t) noexcept {
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    explicit ku_sp(Y *ptr) noexcept : m_ptr(ptr) {
        add_ref();
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    ku_sp(Y *ptr, ku_adopt_ref_t) noexcept : m_ptr(ptr) {
    }

    ku_sp(const ku_sp &other) noexcept : m_ptr(other.m_ptr) {
        add_ref();
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    ku_sp(const ku_sp<Y> &other) noexcept : m_ptr(other.m_ptr) {
        add_ref();
    }

    template <typename Y>
    ku_sp(const ku_sp<Y> &, T *ptr) noexcept : m_ptr(ptr) {
        add_ref();
    }

    ku_sp(ku_sp &&other) noexcept : m_ptr(other.detach()) {
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    ku_sp(ku_sp<Y> &&other) noexcept : m_ptr(other.detach()) {
    }

    ~ku_sp() {
        deref();
    }

    ku_sp &operator=(const ku_sp &other) noexcept {
        if (this != &other) {
            ku_sp tmp(other);
            swap(tmp);
        }
        return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    ku_sp &operator=(const ku_sp<Y> &other) noexcept {
        ku_sp tmp(other);
        swap(tmp);
        return *this;
    }

    ku_sp &operator=(ku_sp &&other) noexcept {
        if (this != &other) {
            ku_sp tmp(std::move(other));
            swap(tmp);
        }
        return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y *, T *>>>
    ku_sp &operator=(ku_sp<Y> &&other) noexcept {
        ku_sp tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    ku_sp &operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    void reset() noexcept {
        ku_sp().swap(*this);
    }

    template <typename Y>
    void reset(Y *ptr) noexcept {
        ku_sp(ptr).swap(*this);
    }

    template <typename Y>
    void reset(Y *ptr, ku_adopt_ref_t) noexcept {
        ku_sp(ptr, ku_adopt_ref).swap(*this);
    }

    void swap(ku_sp &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
    }

    T *get() const noexcept {
        return m_ptr;
    }

    T &operator*() const noexcept {
        return *m_ptr;
    }

    T *operator->() const noexcept {
        return m_ptr;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    long use_count() const noexcept {
        return m_ptr ? static_cast<long>(m_ptr->refCount()) : 0;
    }

    bool unique() const noexcept {
        return use_count() == 1;
    }

    T *detach() noexcept {
        auto *ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

private:
    template <typename>
    friend class ku_sp;

    void add_ref() noexcept {
        if (m_ptr) {
            m_ptr->ref();
        }
    }

    void deref() noexcept {
        auto *ptr = m_ptr;
        m_ptr = nullptr;
        if (ptr) {
            ptr->deref();
        }
    }

    T *m_ptr = nullptr;
};

template <typename T, typename... Args>
ku_sp<T> ku_make_sp(Args &&...args) {
    return ku_sp<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
ku_sp<T> ku_ref_sp(T *ptr) noexcept {
    return ku_sp<T>(ptr);
}

template <typename T, typename F>
ku_sp<T> ku_cast_sp(const ku_sp<F> &ptr) noexcept {
    return ku_sp<T>(ptr, static_cast<T *>(ptr.get()));
}

template <typename T, typename F>
ku_sp<T> ku_cast_sp(ku_sp<F> &&ptr) noexcept {
    return ku_sp<T>(static_cast<T *>(ptr.detach()), ku_adopt_ref);
}

namespace kuai {
template <typename To, typename From>
ku_sp<To> ku_dyn_cast_sp(const ku_sp<From> &ptr) noexcept {
    if (auto *casted = dynamic_cast<To *>(ptr.get())) {
        return ku_sp<To>(ptr, casted);
    }
    return {};
}
} // namespace kuai

template <typename T>
bool operator==(const ku_sp<T> &ptr, std::nullptr_t) noexcept {
    return !ptr;
}

template <typename T, typename U>
bool operator==(const ku_sp<T> &left, const ku_sp<U> &right) noexcept {
    return left.get() == right.get();
}

template <typename T>
bool operator==(std::nullptr_t, const ku_sp<T> &ptr) noexcept {
    return !ptr;
}

template <typename T>
bool operator!=(const ku_sp<T> &ptr, std::nullptr_t) noexcept {
    return static_cast<bool>(ptr);
}

template <typename T, typename U>
bool operator!=(const ku_sp<T> &left, const ku_sp<U> &right) noexcept {
    return !(left == right);
}

template <typename T>
bool operator!=(std::nullptr_t, const ku_sp<T> &ptr) noexcept {
    return static_cast<bool>(ptr);
}

template <typename T, typename D = std::default_delete<T>>
using ku_box = std::unique_ptr<T, D>;

template <typename T, typename... Args>
constexpr ku_box<T> ku_make_box(Args &&...args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
