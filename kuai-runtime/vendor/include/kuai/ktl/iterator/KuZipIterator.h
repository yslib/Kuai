#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuTuple.h>
namespace kuai {
template <typename... Iterators>
class KuZipIterator {
public:
    template <typename Iterator>
    using element_type = typename std::iterator_traits<Iterator>::value_type;

    using IteratorTuple = KuTuple<Iterators...>;
    using value_type = KuTuple<element_type<Iterators>...>;
    using reference = value_type; // Return by value to avoid dangling references
    using pointer = void;         // pointer type is not well-defined for zip iterator
    using difference_type =
        std::common_type_t<typename std::iterator_traits<Iterators>::difference_type...>;
    using iterator_category = std::random_access_iterator_tag;

    KU_DEVICE_HOST KuZipIterator() = default;

    KU_DEVICE_HOST KuZipIterator(Iterators... iterators)
        : m_iteratorsTuple(std::move(iterators)...) {
    }

    KU_DEVICE_HOST KuZipIterator(const KuZipIterator &other)
        : m_iteratorsTuple(other.m_iteratorsTuple) {
    }

    KU_DEVICE_HOST KuZipIterator &operator++() {
        increment(std::index_sequence_for<Iterators...>{});
        return *this;
    }

    KU_DEVICE_HOST KuZipIterator operator++(int) {
        KuZipIterator tmp = *this;
        increment(std::index_sequence_for<Iterators...>{});
        return tmp;
    }

    KU_DEVICE_HOST KuZipIterator &operator--() {
        decrement(std::index_sequence_for<Iterators...>{});
        return *this;
    }

    KU_DEVICE_HOST KuZipIterator operator--(int) {
        KuZipIterator tmp = *this;
        decrement(std::index_sequence_for<Iterators...>{});
        return tmp;
    }

    KU_DEVICE_HOST KuZipIterator operator+(difference_type n) const {
        KuZipIterator tmp = *this;
        tmp += n;
        return tmp;
    }

    friend KU_DEVICE_HOST KuZipIterator operator+(difference_type n, const KuZipIterator &it) {
        return it + n;
    }

    KU_DEVICE_HOST KuZipIterator &operator+=(difference_type n) {
        advance(n, std::index_sequence_for<Iterators...>{});
        return *this;
    }

    KU_DEVICE_HOST KuZipIterator operator-(difference_type n) const {
        KuZipIterator tmp = *this;
        tmp -= n;
        return tmp;
    }

    KU_DEVICE_HOST KuZipIterator &operator-=(difference_type n) {
        advance(-n, std::index_sequence_for<Iterators...>{});
        return *this;
    }

    KU_DEVICE_HOST difference_type operator-(const KuZipIterator &other) const {
        return iterator<0>() - other.template iterator<0>();
    }

    KU_DEVICE_HOST reference operator*() const {
        return dereference(std::index_sequence_for<Iterators...>{});
    }

    KU_DEVICE_HOST reference operator[](difference_type n) const {
        return *(*this + n);
    }

    KU_DEVICE_HOST bool operator!=(const KuZipIterator &other) const {
        return !(*this == other);
    }

    KU_DEVICE_HOST bool operator==(const KuZipIterator &other) const {
        return equal(other, std::index_sequence_for<Iterators...>{});
    }

    KU_DEVICE_HOST bool operator<(const KuZipIterator &other) const {
        return (*this - other) < 0;
    }

    KU_DEVICE_HOST bool operator<=(const KuZipIterator &other) const {
        return !(*this > other);
    }

    KU_DEVICE_HOST bool operator>(const KuZipIterator &other) const {
        return other < *this;
    }

    KU_DEVICE_HOST bool operator>=(const KuZipIterator &other) const {
        return !(*this < other);
    }

private:
    IteratorTuple m_iteratorsTuple;

    template <std::size_t I>
    KU_DEVICE_HOST auto &iterator() {
        return get<I>(m_iteratorsTuple);
    }

    template <std::size_t I>
    KU_DEVICE_HOST const auto &iterator() const {
        return get<I>(m_iteratorsTuple);
    }

    template <std::size_t... Is>
    KU_DEVICE_HOST void increment(std::index_sequence<Is...>) {
        (..., ++iterator<Is>());
    }

    template <std::size_t... Is>
    KU_DEVICE_HOST void decrement(std::index_sequence<Is...>) {
        (..., --iterator<Is>());
    }

    template <std::size_t... Is>
    KU_DEVICE_HOST void advance(difference_type n, std::index_sequence<Is...>) {
        (..., (iterator<Is>() = iterator<Is>() + n));
    }

    template <std::size_t... Is>
    KU_DEVICE_HOST reference dereference(std::index_sequence<Is...>) const {
        return reference(element_type<Iterators>(*iterator<Is>())...);
    }

    template <std::size_t... Is>
    KU_DEVICE_HOST bool equal(const KuZipIterator &other, std::index_sequence<Is...>) const {
        return (... && (iterator<Is>() == other.template iterator<Is>()));
    }
};

template <typename... Iterators>
KU_DEVICE_HOST auto makeZipIterator(Iterators... iterators) {
    return KuZipIterator<Iterators...>(std::move(iterators)...);
}
} // namespace kuai
