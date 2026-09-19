#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/vendor/KuDeviceAccessor.h>

namespace kuai {

struct KuHostOutputTag {};
struct KuDeviceOutputTag {};

template <typename DestinationSpan>
using ku_output_side_t = std::conditional_t<
    ku_is_device_accessor_v<typename std::remove_cvref_t<DestinationSpan>::accessor_type>,
    KuDeviceOutputTag,
    KuHostOutputTag>;

template <typename T>
struct KuHostScalarReader {
    using value_type = T;
    T m_value;

    KU_DEVICE_HOST T operator[](std::size_t) const noexcept {
        return m_value;
    }
};

template <typename Iterator, bool Broadcast>
struct KuDeviceSpanReader {
    using value_type = std::remove_cv_t<typename std::iterator_traits<Iterator>::value_type>;
    Iterator m_data;

    KU_DEVICE_HOST decltype(auto) operator[](std::size_t index) const noexcept {
        return m_data[Broadcast ? 0 : index];
    }
};

template <typename Span>
KU_HOST auto makeKuInputReader(Span span) {
    using SourceSpan = std::remove_cvref_t<Span>;
    using T = std::remove_cv_t<typename SourceSpan::value_type>;
    using Accessor = typename SourceSpan::accessor_type;
    if constexpr (ku_is_device_accessor_v<Accessor>) {
        return KuDeviceSpanReader<typename SourceSpan::data_handle_type, SourceSpan::rank() == 0>{
            span.data_handle()};
    } else {
        static_assert(SourceSpan::rank() == 0, "host span inputs must have rank zero");
        return KuHostScalarReader<T>{span()};
    }
}

template <typename Reader>
struct KuBroadcastReader {
    Reader    m_reader;
    ku_size_t m_size;

    KU_DEVICE_HOST decltype(auto) operator[](ku_size_t index) const noexcept {
        return m_reader[index % m_size];
    }
};

template <typename Span>
KU_HOST auto makeKuBroadcastReader(Span span) {
    return KuBroadcastReader<decltype(makeKuInputReader(span))>{makeKuInputReader(span),
                                                                span.size()};
}

} // namespace kuai
