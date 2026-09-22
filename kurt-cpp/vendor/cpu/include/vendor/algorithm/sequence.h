#pragma once
#include <algorithm>
#include <utility>

#include "kuai/algorithm/sequence.h"

namespace kuai {
namespace vendor::cpu {

template <typename Vendor, typename OutputIterator, typename T>
void sequence(
    const KuVendorContext<Vendor> &dc, OutputIterator begin, OutputIterator end, T init, T step) {
    (void)dc;
    std::generate(begin, end,
                  [valueAt = algo::detail::SequenceValueAt<T>{std::move(init), std::move(step)},
                   index = ku_size_t{0}]() mutable { return valueAt(index++); });
}
} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(sequence, cpu)
