#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <tuple>
#include <type_traits>

#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/KuView.h>
namespace kuai {

template <typename View>
struct KuViewIteratorFacade {
public:
    static constexpr size_t Rank = View::rank();
    using value_type = typename View::value_type;
    using data_handle_type = typename View::data_handle_type;
    using extents_type = typename View::extents_type;
    using size_type = typename View::size_type;
    using index_type = size_type;
    using difference_type = std::ptrdiff_t;
    using const_reference = typename View::const_reference;
    using reference = const_reference;
    using pointer = value_type *;
    using iterator_category = std::forward_iterator_tag;

    KU_DEVICE_HOST KuViewIteratorFacade(const View &view, size_type idx = 0)
        : m_exts(view.extents()), m_data(view), m_idx(idx) {
    }

    KU_DEVICE_HOST constexpr size_type index() const noexcept {
        return m_idx;
    }

    KU_DEVICE_HOST KuViewIteratorFacade(const KuViewIteratorFacade &other)
        : m_exts(other.m_exts), m_data(other.m_data), m_idx(other.m_idx) {
    }

    KU_DEVICE_HOST KuViewIteratorFacade &operator=(const KuViewIteratorFacade &other) {
        if (this != &other) {
            m_exts = other.m_exts;
            m_data = other.m_data;
            m_idx = other.m_idx;
        }
        return *this;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade &operator++() {
        m_idx++;
        return *this;
    }
    KU_DEVICE_HOST constexpr KuViewIteratorFacade operator++(int) {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade &operator+=(difference_type n) {
        m_idx += n;
        return *this;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade operator+(difference_type n) const {
        auto tmp = *this;
        tmp += n;
        return tmp;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade &operator--() {
        m_idx--;
        return *this;
    }
    KU_DEVICE_HOST constexpr KuViewIteratorFacade operator--(int) {
        auto tmp = *this;
        --(*this);
        return tmp;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade &operator-=(difference_type n) {
        m_idx -= n;
        return *this;
    }

    KU_DEVICE_HOST constexpr KuViewIteratorFacade operator-(difference_type n) const {
        auto tmp = *this;
        tmp -= n;
        return tmp;
    }

    KU_DEVICE_HOST difference_type operator-(const KuViewIteratorFacade &other) const {
        return static_cast<difference_type>(m_idx) - static_cast<difference_type>(other.m_idx);
    }

    KU_DEVICE_HOST bool operator==(const KuViewIteratorFacade &other) const {
        return m_idx == other.m_idx;
    }
    KU_DEVICE_HOST bool operator!=(const KuViewIteratorFacade &other) const {
        return !(*this == other);
    }

    KU_DEVICE_HOST const_reference operator*() const noexcept {
        std::array<size_type, Rank> multi_idx;
        map(m_idx, multi_idx);
        return std::apply(m_data, multi_idx);
    }

    KU_DEVICE_HOST const_reference operator[](difference_type n) const noexcept {
        return *(*this + n);
    }

    // protected:
    KU_DEVICE_HOST constexpr void
    map(index_type                                    flat_idx,
        std::array<index_type, extents_type::rank()> &multi_idx) const noexcept {
#pragma unroll
        for (std::size_t i = 0; i < extents_type::rank(); ++i) {
            multi_idx[i] = flat_idx % m_exts.extent(i);
            flat_idx /= m_exts.extent(i);
        }
    }
    extents_type m_exts;
    View         m_data;
    size_type    m_idx;
};

template <typename View>
struct KuViewInputIterator : public KuViewIteratorFacade<View> {
public:
    using T = typename View::value_type;
    static constexpr size_t Rank = View::rank();
    using value_type = T;
    using data_handle_type = typename View::data_handle_type;
    using extents_type = typename View::extents_type;
    using size_type = typename View::size_type;
    using difference_type = std::ptrdiff_t;
    using reference = typename View::const_reference;
    using const_reference = reference;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator_category = std::forward_iterator_tag;

public:
    KuViewInputIterator(const View &view, size_type idx = 0)
        : KuViewIteratorFacade<View>(view, idx) {
    }
};

template <typename View>
KU_DEVICE_HOST auto makeKuViewInputIterator(const View &view, ku_size_t idx = 0) {
    return KuViewInputIterator<View>(view, idx);
}

template <typename ViewIterator>
struct ViewOutputIteratorProxy {
    using value_type = typename ViewIterator::value_type;
    using size_type = typename ViewIterator::size_type;
    using index_type = size_type;
    using extents_type = typename ViewIterator::extents_type;
    static constexpr size_t Rank = ViewIterator::Rank;

    KU_DEVICE_HOST ViewOutputIteratorProxy(ViewIterator view) : m_viewIter(view) {
    }

    KU_DEVICE_HOST ViewOutputIteratorProxy &operator=(const value_type &lhs) {
        std::array<size_type, Rank> multi_idx{};
        m_viewIter.map(m_viewIter.index(), multi_idx);
        auto &val = std::apply(m_viewIter.m_data, multi_idx);
        auto  ok = !ku_value_traits<decltype(val)>::isNull(val);
        if (ok) {
            val = lhs;
        }
        return *this;
    }

    KU_DEVICE_HOST bool operator==(const value_type &rhs) const {
        std::array<size_type, Rank> multi_idx{};
        m_viewIter.map(m_viewIter.index(), multi_idx);
        const auto val = std::apply(m_viewIter.m_data, multi_idx);
        return val == rhs;
    }

private:
    ViewIterator m_viewIter;
};

template <typename View>
struct KuViewOutputIterator : public KuViewIteratorFacade<View> {

    template <typename Span, typename... Args>
    friend class KuView;

public:
    using T = typename View::value_type;
    static constexpr size_t Rank = View::rank();
    using value_type = T;
    using data_handle_type = typename View::data_handle_type;
    using extents_type = typename View::extents_type;
    using size_type = typename View::size_type;
    using difference_type = std::ptrdiff_t;
    using reference = ViewOutputIteratorProxy<KuViewOutputIterator<View>>;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator_category = std::forward_iterator_tag;

    KuViewOutputIterator(const View &view, size_type idx = 0)
        : KuViewIteratorFacade<View>(view, idx) {
    }

    KU_DEVICE_HOST reference operator*() {
        return reference(*this);
    }

    KU_DEVICE_HOST constexpr auto operator+(difference_type n) const {
        auto tmp = *this;
        tmp += n;
        return tmp;
    }

    KU_DEVICE_HOST reference operator[](difference_type n) {
        return *(*this + n);
    }
};

template <typename View>
KU_DEVICE_HOST auto makeKuViewOutputIterator(const View &view, ku_size_t idx = 0) {
    return KuViewOutputIterator<View>(view, idx);
}
} // namespace kuai
