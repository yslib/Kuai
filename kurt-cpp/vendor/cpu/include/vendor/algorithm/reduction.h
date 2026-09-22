#pragma once
#include <functional>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <utility>

#include <kuai/core/KuDeviceData.h>
#include <kuai/ktl/functional/KuCast.h>

#include "kuai/algorithm/reduction.h"

namespace kuai {
namespace vendor::cpu {

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &dc,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    (void)dc;
    *output = std::accumulate(
        first, last, std::forward<InitValueType>(initValue), [&](auto accumulator, auto &&value) {
            return std::invoke(reduceFn, std::move(accumulator),
                               std::invoke(mapFn, std::forward<decltype(value)>(value)));
        });
    return KU_STATUS_SUCCESS;
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &dc,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       ku_size_t                      segmentedLength,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    KU_ASSERT(segmentedLength > 0);
    using init_type = std::remove_cvref_t<InitValueType>;
    const init_type initial(std::forward<InitValueType>(initValue));

    while (first != last) {
        auto segmentLast = first;
        for (ku_size_t i = 0; i < segmentedLength && segmentLast != last; ++i) {
            ++segmentLast;
        }
        const auto status =
            map_reduce(dc, init_type(initial), first, segmentLast, output, mapFn, reduceFn);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        ++output;
        first = segmentLast;
    }
    return KU_STATUS_SUCCESS;
}

template <typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename MapFn,
          typename ReduceBinaryFn>
void map_reduce_by_key_impl(KeyIterator            keyFirst,
                            KeyIterator            keyLast,
                            InputIterator          valueFirst,
                            OutputIterator         outputFirst,
                            RunCountOutputIterator runCountOutput,
                            MapFn                &&mapFn,
                            ReduceBinaryFn       &&reduceFn) {
    ku_size_t runCount = 0;
    if (keyFirst == keyLast) {
        *runCountOutput = runCount;
        return;
    }

    using key_type = typename std::iterator_traits<KeyIterator>::value_type;
    key_type currentKey = *keyFirst;
    auto     accumulated = std::invoke(mapFn, *valueFirst);
    ++keyFirst;
    ++valueFirst;

    for (; keyFirst != keyLast; ++keyFirst, ++valueFirst) {
        if (*keyFirst == currentKey) {
            accumulated =
                std::invoke(reduceFn, std::move(accumulated), std::invoke(mapFn, *valueFirst));
            continue;
        }

        *outputFirst = std::move(accumulated);
        ++outputFirst;
        ++runCount;
        currentKey = *keyFirst;
        accumulated = std::invoke(mapFn, *valueFirst);
    }
    *outputFirst = std::move(accumulated);
    *runCountOutput = runCount + 1;
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce_by_key(const KuVendorContext<Vendor> &dc,
                              KeyIterator                    keyFirst,
                              KeyIterator                    keyLast,
                              InputIterator                  valueFirst,
                              OutputIterator                 outputFirst,
                              RunCountOutputIterator         runCountOutput,
                              MapFn                        &&mapFn,
                              ReduceBinaryFn               &&reduceFn) {
    (void)dc;
    map_reduce_by_key_impl(keyFirst, keyLast, valueFirst, outputFirst, runCountOutput,
                           std::forward<MapFn>(mapFn), std::forward<ReduceBinaryFn>(reduceFn));
    return KU_STATUS_SUCCESS;
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename BinaryFn>
ku_status_t reduce_by_key(const KuVendorContext<Vendor> &dc,
                          KeyIterator                    keyFirst,
                          KeyIterator                    keyLast,
                          InputIterator                  valueFirst,
                          OutputIterator                 outputFirst,
                          RunCountOutputIterator         runCountOutput,
                          BinaryFn                     &&reduceFn) {
    auto identity = [](auto &&value) -> decltype(auto) {
        return std::forward<decltype(value)>(value);
    };
    return map_reduce_by_key(dc, keyFirst, keyLast, valueFirst, outputFirst, runCountOutput,
                             identity, std::forward<BinaryFn>(reduceFn));
}

} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(map_reduce, cpu)

KU_DEFINE_VENDOR(map_reduce_by_key, cpu)

KU_DEFINE_VENDOR(reduce_by_key, cpu)
