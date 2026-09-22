#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/reduction.h>

namespace kuai {
namespace algo {

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &exec,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    KU_KERNEL_CALL_ZONE_SCOPED("map_reduce_1");
    return KU_CALL_VENDOR(map_reduce, exec, std::forward<InitValueType>(initValue), first, last,
                          output, std::forward<MapFn>(mapFn),
                          std::forward<ReduceBinaryFn>(reduceFn));
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &exec,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       ku_size_t                      segmentedLength,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    KU_KERNEL_CALL_ZONE_SCOPED("map_reduce_2");
    return KU_CALL_VENDOR(map_reduce, exec, std::forward<InitValueType>(initValue), first, last,
                          segmentedLength, output, std::forward<MapFn>(mapFn),
                          std::forward<ReduceBinaryFn>(reduceFn));
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce_by_key(const KuVendorContext<Vendor> &exec,
                              KeyIterator                    keyFirst,
                              KeyIterator                    keyLast,
                              InputIterator                  valueFirst,
                              OutputIterator                 outputFirst,
                              RunCountOutputIterator         runCountOutput,
                              MapFn                        &&mapFn,
                              ReduceBinaryFn               &&reduceFn) {
    KU_KERNEL_CALL_ZONE_SCOPED("map_reduce_by_key");
    return KU_CALL_VENDOR(map_reduce_by_key, exec, keyFirst, keyLast, valueFirst, outputFirst,
                          runCountOutput, std::forward<MapFn>(mapFn),
                          std::forward<ReduceBinaryFn>(reduceFn));
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename BinaryFn>
ku_status_t reduce_by_key(const KuVendorContext<Vendor> &exec,
                          KeyIterator                    keyFirst,
                          KeyIterator                    keyLast,
                          InputIterator                  valueFirst,
                          OutputIterator                 outputFirst,
                          RunCountOutputIterator         runCountOutput,
                          BinaryFn                     &&reduceFn) {
    KU_KERNEL_CALL_ZONE_SCOPED("reduce_by_key");
    return KU_CALL_VENDOR(reduce_by_key, exec, keyFirst, keyLast, valueFirst, outputFirst,
                          runCountOutput, std::forward<BinaryFn>(reduceFn));
}

} // namespace algo
} // namespace kuai
