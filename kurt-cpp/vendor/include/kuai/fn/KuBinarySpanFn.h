#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/vendor/KuSpanDispatch.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/transform.h"
namespace kuai {

template <typename Vendor, typename BinaryFn>
struct KuBinarySpanFn {
    using Fn = BinaryFn;
    KuBinarySpanFn(KuContext &context, BinaryFn fn) : m_device(context), m_fn(std::move(fn)) {
    }

    template <typename InputT1,
              typename InputExtents1,
              typename InputLayout1,
              typename InputAccessor1,
              typename InputT2,
              typename InputExtents2,
              typename InputLayout2,
              typename InputAccessor2,
              typename OutputT,
              typename OutputExtents,
              typename OutputLayout,
              typename OutputAccessor>
    void operator()(KuMdSpan<InputT1, InputExtents1, InputLayout1, InputAccessor1> span1,
                    KuMdSpan<InputT2, InputExtents2, InputLayout2, InputAccessor2> span2,
                    KuMdSpan<OutputT, OutputExtents, OutputLayout, OutputAccessor> output) const {
        apply(ku_output_side_t<decltype(output)>{}, span1, span2, output);
    }

private:
    template <typename LeftReader, typename RightReader>
    struct ApplyFn {
        LeftReader  m_left;
        RightReader m_right;
        BinaryFn    m_fn;

        KU_DEVICE_HOST auto operator()(ku_size_t index) const {
            return m_fn(m_left[index], m_right[index]);
        }
    };

    template <typename InputSpan1, typename InputSpan2, typename OutputSpan>
    void apply(KuHostOutputTag, InputSpan1 span1, InputSpan2 span2, OutputSpan output) const {
        static_assert(InputSpan1::rank() == 0 && InputSpan2::rank() == 0 && OutputSpan::rank() == 0,
                      "host binary computation requires rank-zero spans");
        static_assert(!ku_is_device_accessor_v<typename InputSpan1::accessor_type>
                          && !ku_is_device_accessor_v<typename InputSpan2::accessor_type>,
                      "host binary computation cannot read a device span");
        output() = m_fn(span1(), span2());
    }

    template <typename InputSpan1, typename InputSpan2, typename OutputSpan>
    void apply(KuDeviceOutputTag, InputSpan1 span1, InputSpan2 span2, OutputSpan output) const {
        static_assert(InputSpan1::is_always_unique() && InputSpan2::is_always_unique()
                          && OutputSpan::is_always_unique(),
                      "linear span algorithms require unique mappings");
        static_assert(InputSpan1::is_always_exhaustive() && InputSpan2::is_always_exhaustive()
                          && OutputSpan::is_always_exhaustive(),
                      "linear span algorithms require exhaustive mappings");
        constexpr size_t InputRank1 = InputSpan1::rank();
        constexpr size_t InputRank2 = InputSpan2::rank();
        constexpr size_t OutputRank = OutputSpan::rank();
        static_assert(OutputRank == std::max(InputRank1, InputRank2),
                      "OutputRank must be the max of InputRank1 and InputRank2");
        if (output.size() == 0) {
            return;
        }
        KU_ASSERT(span1.size() != 0 && span2.size() != 0);
        auto first = makeCountingIterator(ku_size_t(0));
        auto left = makeKuBroadcastReader(span1);
        auto right = makeKuBroadcastReader(span2);
        algo::transform(m_device, first, first + output.size(), output.data_handle(),
                        ApplyFn{left, right, m_fn});
    }

    KuVendorContext<Vendor> m_device;
    BinaryFn                m_fn;
};

template <typename BinaryFn>
KuBinarySpanFn(KuContext &, BinaryFn) -> KuBinarySpanFn<vendor::KuVendor, BinaryFn>;

} // namespace kuai
