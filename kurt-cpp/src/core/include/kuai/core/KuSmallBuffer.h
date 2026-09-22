#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace kuai {

template <typename Object, size_t N = 8>
class KuSmallBufferN {
public:
    using value_type = Object;
    template <typename... Args>
    explicit KuSmallBufferN(Args... args) : KuSmallBufferN() {
        static_assert((std::is_convertible<Args, value_type *>::value && ...),
                      "All arguments must be convertible to Object*");
        (push_back(args), ...);
    }

    KuSmallBufferN() : m_ptr(m_stack_buffer), m_count(0), m_capacity(N) {
    }

    ~KuSmallBufferN() {
        if (m_ptr != m_stack_buffer) {
            free(m_ptr);
        }
    }
    KuSmallBufferN(KuSmallBufferN &&other) noexcept {
        m_ptr = other.m_ptr;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        if (other.m_ptr == other.m_stack_buffer) {
            memcpy(m_stack_buffer, other.m_stack_buffer, m_count * sizeof(value_type));
            m_ptr = m_stack_buffer;
        }
        other.m_ptr = other.m_stack_buffer;
        other.m_count = 0;
        other.m_capacity = N;
    }
    KuSmallBufferN &operator=(KuSmallBufferN &&other) noexcept {
        if (this != &other) {
            release();
            m_ptr = other.m_ptr;
            m_count = other.m_count;
            m_capacity = other.m_capacity;
            if (other.m_ptr == other.m_stack_buffer) {
                memcpy(m_stack_buffer, other.m_stack_buffer, m_count * sizeof(value_type));
                m_ptr = m_stack_buffer;
            }
            other.m_ptr = other.m_stack_buffer;
            other.m_count = 0;
            other.m_capacity = N;
        }
    }
    KuSmallBufferN(const KuSmallBufferN &other) {
        m_ptr = other.m_ptr;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        if (other.m_ptr == other.m_stack_buffer) {
            memcpy(m_stack_buffer, other.m_stack_buffer, m_count * sizeof(value_type));
            m_ptr = m_stack_buffer;
        } else {
            m_ptr = static_cast<value_type *>(malloc(m_capacity * sizeof(value_type)));
            memcpy(m_ptr, other.m_ptr, m_count * sizeof(value_type));
        }
    }
    KuSmallBufferN &operator=(const KuSmallBufferN &other) {
        if (this != &other) {
            release();
            m_ptr = other.m_ptr;
            m_count = other.m_count;
            m_capacity = other.m_capacity;
            if (other.m_ptr == other.m_stack_buffer) {
                memcpy(m_stack_buffer, other.m_stack_buffer, m_count * sizeof(value_type));
                m_ptr = m_stack_buffer;
            } else {
                m_ptr = static_cast<value_type *>(malloc(m_capacity * sizeof(value_type)));
                memcpy(m_ptr, other.m_ptr, m_count * sizeof(value_type));
            }
        }
    }

    const value_type &operator[](size_t index) const {
        return m_ptr[index];
    }

    inline size_t size() const {
        return m_count;
    }

    inline bool empty() const {
        return m_count == 0;
    }

    value_type *begin() noexcept {
        return m_ptr;
    }

    const value_type *begin() const noexcept {
        return m_ptr;
    }

    value_type *end() noexcept {
        return m_ptr + m_count;
    }

    const value_type *end() const noexcept {
        return m_ptr + m_count;
    }

    inline void push_back(const value_type &obj) {
        if (m_count >= m_capacity) {
            grow();
        }
        m_ptr[m_count++] = obj;
    }

    value_type *data() noexcept {
        return m_ptr;
    }

    const value_type *data() const noexcept {
        return m_ptr;
    }

    std::span<value_type> view() noexcept {
        return {m_ptr, m_count};
    }

    std::span<const value_type> view() const noexcept {
        return {m_ptr, m_count};
    }

private:
    void grow() {
        size_t      new_capacity = m_capacity * 2;
        value_type *new_ptr;

        if (m_ptr == m_stack_buffer) {
            new_ptr = static_cast<value_type *>(malloc(new_capacity * sizeof(value_type)));
            memcpy(new_ptr, m_stack_buffer, m_count * sizeof(value_type));
        } else {
            new_ptr = static_cast<value_type *>(realloc(m_ptr, new_capacity * sizeof(value_type)));
        }
        m_ptr = new_ptr;
        m_capacity = new_capacity;
    }

    void release() {
        if (m_ptr != m_stack_buffer) {
            free(m_ptr);
            // m_ptr = m_stack_buffer;
            // m_count = 0;
            // m_capacity = N;
        }
    }

    value_type *m_ptr;
    size_t      m_count;
    size_t      m_capacity;
    value_type  m_stack_buffer[N];
};

template <typename T>
using KuSmallBuffer = KuSmallBufferN<T>;

} // namespace kuai
