#pragma once

#include <type_traits>
#include <utility>

#include <kuai/core/KuContext.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/vendor/KuSpanDispatch.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/transform.h"
namespace kuai {

template <typename Vendor, typename UnaryFn>
struct KuUnarySpanFn {
    using Fn = UnaryFn;
    KuUnarySpanFn(KuContext &context, UnaryFn fn) : m_device(context), m_fn(std::move(fn)) {
    }

    template <typename InputT,
              typename InputExtents,
              typename InputLayout,
              typename InputAccessor,
              typename OutputT,
              typename OutputExtents,
              typename OutputLayout,
              typename OutputAccessor>
    void operator()(KuMdSpan<InputT, InputExtents, InputLayout, InputAccessor>     input,
                    KuMdSpan<OutputT, OutputExtents, OutputLayout, OutputAccessor> output) const {
        apply(ku_output_side_t<decltype(output)>{}, input, output);
    }

private:
    template <typename Reader>
    struct ApplyFn {
        Reader  m_input;
        UnaryFn m_fn;

        KU_DEVICE_HOST auto operator()(ku_size_t index) const {
            return m_fn(m_input[index]);
        }
    };

    template <typename InputSpan, typename OutputSpan>
    void apply(KuHostOutputTag, InputSpan input, OutputSpan output) const {
        static_assert(InputSpan::rank() == 0 && OutputSpan::rank() == 0,
                      "host unary computation requires rank-zero spans");
        static_assert(!ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "host unary computation cannot read a device span");
        output() = m_fn(input());
    }

    template <typename InputSpan, typename OutputSpan>
    void apply(KuDeviceOutputTag, InputSpan input, OutputSpan output) const {
        static_assert(InputSpan::is_always_unique() && OutputSpan::is_always_unique(),
                      "linear span algorithms require unique mappings");
        static_assert(InputSpan::is_always_exhaustive() && OutputSpan::is_always_exhaustive(),
                      "linear span algorithms require exhaustive mappings");
        static_assert(InputSpan::rank() == 0 || InputSpan::rank() == OutputSpan::rank(),
                      "unary input and output ranks must match");
        if (output.size() == 0) {
            return;
        }
        KU_ASSERT(input.size() == 1 || input.size() == output.size());
        auto first = makeCountingIterator(ku_size_t(0));
        auto inputReader = makeKuBroadcastReader(input);
        algo::transform(m_device, first, first + output.size(), output.data_handle(),
                        ApplyFn{inputReader, m_fn});
    }

    KuVendorContext<Vendor> m_device;
    UnaryFn                 m_fn;
};

template <typename UnaryFn>
KuUnarySpanFn(KuContext &, UnaryFn) -> KuUnarySpanFn<vendor::KuVendor, UnaryFn>;

} // namespace kuai
