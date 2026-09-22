#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <kuai/core/KuDeviceData.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/ktl/iterator/KuTransformIterator.h>
#include <kuai/vendor/KuSpanDispatch.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/fill.h"
#include "kuai/algorithm/reduction.h"
#include "kuai/algorithm/transform.h"

namespace kuai {

namespace detail {

struct KuNoReduceFinalizer {};

template <std::size_t Axis, typename InputSpan, std::size_t... Indices>
auto kuReductionOutputExtentsImpl(const InputSpan &input, std::index_sequence<Indices...>) {
    return KuDynamicExtents<InputSpan::rank() - 1>(
        input.extent(Indices < Axis ? Indices : Indices + 1)...);
}

template <std::size_t Axis, typename InputSpan>
auto kuReductionOutputExtents(const InputSpan &input) {
    static_assert(InputSpan::rank() > 0, "rank-zero reduction preserves rank zero");
    static_assert(Axis < InputSpan::rank(), "reduction axis must be smaller than the input rank");
    return kuReductionOutputExtentsImpl<Axis>(input,
                                              std::make_index_sequence<InputSpan::rank() - 1>{});
}

template <typename InputSpan,
          typename OutputSpan,
          std::size_t Axis,
          bool Direct = Axis == 0 && std::is_same_v<typename InputSpan::layout_policy, KuLayoutLeft>
                        && std::is_same_v<typename OutputSpan::layout_policy, KuLayoutLeft>>
class KuAxisReductionReader;

template <typename InputSpan, typename OutputSpan, std::size_t Axis>
class KuAxisReductionReader<InputSpan, OutputSpan, Axis, false> {
public:
    using size_type = typename InputSpan::size_type;

    static_assert(InputSpan::rank() > 0, "axis reduction requires a positive-rank input");
    static_assert(Axis < InputSpan::rank(), "reduction axis must be smaller than the input rank");
    static_assert(OutputSpan::rank() + 1 == InputSpan::rank(),
                  "axis reduction output rank must be one less than its input rank");
    static_assert(InputSpan::is_always_strided(),
                  "axis reduction requires a strided input mapping");

    KU_DEVICE_HOST KuAxisReductionReader(InputSpan input, OutputSpan output)
        : m_input(std::move(input)), m_output(std::move(output)) {
    }

    KU_DEVICE_HOST decltype(auto) operator()(size_type logicalIndex) const noexcept {
        const size_type axisExtent = m_input.extent(Axis);
        const size_type axisIndex = logicalIndex % axisExtent;
        const size_type outputFlatIndex = logicalIndex / axisExtent;

        std::array<size_type, OutputSpan::rank()> outputIndex{};
        m_output.mapping().flat_to_multi_index(outputFlatIndex, outputIndex);

        const auto strides = m_input.mapping().strides();
        size_type  inputOffset = axisIndex * strides.extent(Axis);
        for (std::size_t inputAxis = 0; inputAxis < InputSpan::rank(); ++inputAxis) {
            if (inputAxis == Axis) {
                continue;
            }
            const std::size_t outputAxis = inputAxis < Axis ? inputAxis : inputAxis - 1;
            inputOffset += outputIndex[outputAxis] * strides.extent(inputAxis);
        }
        return m_input.accessor().access(m_input.data_handle(), inputOffset);
    }

    KU_DEVICE_HOST decltype(auto) operator[](size_type logicalIndex) const noexcept {
        return (*this)(logicalIndex);
    }

private:
    InputSpan  m_input;
    OutputSpan m_output;
};

template <typename InputSpan, typename OutputSpan, std::size_t Axis>
class KuAxisReductionReader<InputSpan, OutputSpan, Axis, true> {
public:
    using size_type = typename InputSpan::size_type;

    static_assert(InputSpan::rank() > 0, "axis reduction requires a positive-rank input");
    static_assert(Axis < InputSpan::rank(), "reduction axis must be smaller than the input rank");
    static_assert(OutputSpan::rank() + 1 == InputSpan::rank(),
                  "axis reduction output rank must be one less than its input rank");

    KU_DEVICE_HOST KuAxisReductionReader(InputSpan input, OutputSpan) : m_input(std::move(input)) {
    }

    KU_DEVICE_HOST decltype(auto) operator()(size_type logicalIndex) const noexcept {
        return m_input.accessor().access(m_input.data_handle(), logicalIndex);
    }

    KU_DEVICE_HOST decltype(auto) operator[](size_type logicalIndex) const noexcept {
        return (*this)(logicalIndex);
    }

private:
    InputSpan m_input;
};

template <std::size_t Axis, typename InputSpan, typename OutputSpan>
KU_DEVICE_HOST auto makeKuAxisReductionReader(InputSpan input, OutputSpan output) {
    return KuAxisReductionReader<InputSpan, OutputSpan, Axis>(std::move(input), std::move(output));
}

template <typename Vendor, typename MapFn, typename ReduceFn, std::size_t Axis = 0>
class KuReduceSpanCore {
public:
    KuReduceSpanCore(KuContext &context, MapFn mapFn, ReduceFn reduceFn)
        : m_device(context), m_mapFn(std::move(mapFn)), m_reduceFn(std::move(reduceFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t apply(InputSpan input, OutputSpan output) const {
        return apply(ku_output_side_t<OutputSpan>{}, input, output, KuNoReduceFinalizer{});
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t apply(InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        return apply(ku_output_side_t<OutputSpan>{}, input, output, resultFn);
    }

private:
    template <typename Reader>
    struct MapAtFn {
        Reader m_input;
        MapFn  m_map;

        KU_DEVICE_HOST auto operator()(ku_size_t index) const {
            return m_map(m_input[index]);
        }
    };

    template <typename Reader, typename FinalFn>
    struct MapAndFinalizeAtFn {
        Reader  m_input;
        MapFn   m_map;
        FinalFn m_finalize;

        KU_DEVICE_HOST auto operator()(ku_size_t index) const {
            return m_finalize(m_map(m_input[index]));
        }
    };

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
        requires(InputSpan::rank() == 0 && OutputSpan::rank() == 0)
    ku_status_t
    apply(KuHostOutputTag, InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        static_assert(!ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "host reduction cannot read a device span");
        return mapScalar(input, output, resultFn);
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
        requires(InputSpan::rank() == 0 && OutputSpan::rank() == 0)
    ku_status_t
    apply(KuDeviceOutputTag, InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        static_assert(InputSpan::is_always_unique() && OutputSpan::is_always_unique(),
                      "linear span algorithms require unique mappings");
        static_assert(InputSpan::is_always_exhaustive() && OutputSpan::is_always_exhaustive(),
                      "linear span algorithms require exhaustive mappings");
        static_assert(ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "device reduction requires a device source span");
        static_assert(ku_is_device_accessor_v<typename OutputSpan::accessor_type>,
                      "device reduction requires a device destination span");
        return mapScalar(input, output, resultFn);
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
        requires(InputSpan::rank() > 0)
    ku_status_t
    apply(KuDeviceOutputTag, InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        static_assert(InputSpan::is_always_unique() && OutputSpan::is_always_unique(),
                      "linear span algorithms require unique mappings");
        static_assert(InputSpan::is_always_exhaustive() && OutputSpan::is_always_exhaustive(),
                      "linear span algorithms require exhaustive mappings");
        static_assert(InputSpan::is_always_strided(),
                      "axis reduction requires a strided input mapping");
        static_assert(ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "device reduction requires a device source span");
        static_assert(ku_is_device_accessor_v<typename OutputSpan::accessor_type>,
                      "device reduction requires a device destination span");
        static_assert(Axis < InputSpan::rank(),
                      "reduction axis must be smaller than the input rank");
        static_assert(OutputSpan::rank() + 1 == InputSpan::rank(),
                      "axis reduction output rank must be one less than its input rank");

        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        if (input.extent(Axis) == 0) {
            algo::fill_n(m_device, output.data_handle(), output.size(),
                         ku_value_traits<OutputT>::null());
            return KU_STATUS_SUCCESS;
        }

        auto first = makeTransformIterator(makeCountingIterator(ku_size_t(0)),
                                           makeKuAxisReductionReader<Axis>(input, output));
        return reduceSegments(first, first + input.size(), input.extent(Axis), output.data_handle(),
                              output.size(), resultFn);
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t mapScalar(InputSpan input, OutputSpan output, KuNoReduceFinalizer) const {
        using T = std::remove_cv_t<typename InputSpan::value_type>;
        using Accumulator = decltype(MapFn::template initValue<T>());
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(std::is_same_v<Accumulator, OutputT>,
                      "a reduction without a finalizer must publish its accumulator type");

        if constexpr (ku_is_device_accessor_v<typename OutputSpan::accessor_type>) {
            auto first = makeCountingIterator(ku_size_t(0));
            auto reader = makeKuInputReader(input);
            algo::transform(m_device, first, first + 1, output.data_handle(),
                            MapAtFn{reader, m_mapFn});
        } else {
            output() = m_mapFn(input());
        }
        return KU_STATUS_SUCCESS;
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t mapScalar(InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        using T = std::remove_cv_t<typename InputSpan::value_type>;
        using Accumulator = decltype(MapFn::template initValue<T>());
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(std::is_same_v<std::invoke_result_t<const ResultFn &, Accumulator>, OutputT>,
                      "a finalized reduction must publish its finalizer result type");

        if constexpr (ku_is_device_accessor_v<typename OutputSpan::accessor_type>) {
            auto first = makeCountingIterator(ku_size_t(0));
            auto reader = makeKuInputReader(input);
            algo::transform(m_device, first, first + 1, output.data_handle(),
                            MapAndFinalizeAtFn{reader, m_mapFn, resultFn});
        } else {
            output() = resultFn(m_mapFn(input()));
        }
        return KU_STATUS_SUCCESS;
    }

    template <typename InputIterator, typename OutputIterator>
    ku_status_t reduceSegments(InputIterator  first,
                               InputIterator  last,
                               ku_size_t      segmentLength,
                               OutputIterator output,
                               ku_size_t,
                               KuNoReduceFinalizer) const {
        using T = std::remove_cv_t<typename std::iterator_traits<InputIterator>::value_type>;
        using Accumulator = decltype(MapFn::template initValue<T>());
        using OutputT = std::remove_cv_t<typename std::iterator_traits<OutputIterator>::value_type>;
        static_assert(std::is_same_v<Accumulator, OutputT>,
                      "a reduction without a finalizer must publish its accumulator type");
        return reduceSegmentsTo(first, last, segmentLength, output);
    }

    template <typename InputIterator, typename OutputIterator, typename ResultFn>
    ku_status_t reduceSegments(InputIterator   first,
                               InputIterator   last,
                               ku_size_t       segmentLength,
                               OutputIterator  output,
                               ku_size_t       outputSize,
                               const ResultFn &resultFn) const {
        using T = std::remove_cv_t<typename std::iterator_traits<InputIterator>::value_type>;
        using Accumulator = decltype(MapFn::template initValue<T>());
        using OutputT = std::remove_cv_t<typename std::iterator_traits<OutputIterator>::value_type>;
        static_assert(std::is_same_v<std::invoke_result_t<const ResultFn &, Accumulator>, OutputT>,
                      "a finalized reduction must publish its finalizer result type");

        return finalizeReduction<Accumulator>(output, outputSize, resultFn, [&](auto *temporary) {
            return reduceSegmentsTo(first, last, segmentLength, temporary);
        });
    }

    template <typename InputIterator, typename OutputIterator>
    ku_status_t reduceSegmentsTo(InputIterator  first,
                                 InputIterator  last,
                                 ku_size_t      segmentLength,
                                 OutputIterator output) const {
        using T = std::remove_cv_t<typename std::iterator_traits<InputIterator>::value_type>;
        return algo::map_reduce(m_device, MapFn::template initValue<T>(), first, last,
                                segmentLength, output, m_mapFn, m_reduceFn);
    }

    template <typename Accumulator, typename OutputIterator, typename ResultFn, typename Reduction>
    ku_status_t finalizeReduction(OutputIterator  output,
                                  ku_size_t       outputSize,
                                  const ResultFn &resultFn,
                                  Reduction     &&reduction) const {
        auto temporaryResult = m_device.template createStorage<Accumulator>(outputSize);
        if (!temporaryResult.hasValue()) {
            return temporaryResult.error();
        }
        auto       temporary = std::move(temporaryResult).value();
        const auto reductionStatus = std::forward<Reduction>(reduction)(temporary.data());
        if (reductionStatus == KU_STATUS_SUCCESS) {
            algo::transform(m_device, temporary.data(), temporary.data() + outputSize, output,
                            resultFn);
        }
        return reductionStatus;
    }

    KuVendorContext<Vendor> m_device;
    MapFn                   m_mapFn;
    ReduceFn                m_reduceFn;
};

} // namespace detail

template <typename Vendor,
          typename MapFn,
          typename ReduceFn,
          typename ResultFn = void,
          std::size_t Axis = 0>
struct KuReduceSpanFn {
    template <typename T>
    using accumulator_type = decltype(MapFn::template initValue<T>());

    template <typename T>
    using output_type = std::invoke_result_t<const ResultFn &, accumulator_type<T>>;

    template <typename T>
    static constexpr bool supports =
        requires(T value, const MapFn map, const ReduceFn reduce, const ResultFn result) {
            MapFn::template initValue<T>();
            map(value);
            reduce(MapFn::template initValue<T>(), MapFn::template initValue<T>());
            result(MapFn::template initValue<T>());
        };

    KuReduceSpanFn(KuContext &context, MapFn mapFn, ReduceFn reduceFn, ResultFn resultFn)
        : m_core(context, std::move(mapFn), std::move(reduceFn)), m_resultFn(std::move(resultFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t operator()(InputSpan input, OutputSpan output) const {
        return m_core.apply(input, output, m_resultFn);
    }

private:
    detail::KuReduceSpanCore<Vendor, MapFn, ReduceFn, Axis> m_core;
    ResultFn                                                m_resultFn;
};

template <typename Vendor, typename MapFn, typename ReduceFn, std::size_t Axis>
struct KuReduceSpanFn<Vendor, MapFn, ReduceFn, void, Axis> {
    template <typename T>
    using accumulator_type = decltype(MapFn::template initValue<T>());

    template <typename T>
    using output_type = accumulator_type<T>;

    template <typename T>
    static constexpr bool supports = requires(T value, const MapFn map, const ReduceFn reduce) {
        MapFn::template initValue<T>();
        map(value);
        reduce(MapFn::template initValue<T>(), MapFn::template initValue<T>());
    };

    KuReduceSpanFn(KuContext &context, MapFn mapFn, ReduceFn reduceFn)
        : m_core(context, std::move(mapFn), std::move(reduceFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t operator()(InputSpan input, OutputSpan output) const {
        return m_core.apply(input, output);
    }

private:
    detail::KuReduceSpanCore<Vendor, MapFn, ReduceFn, Axis> m_core;
};

template <typename MapFn, typename ReduceFn, typename ResultFn>
KuReduceSpanFn(KuContext &, MapFn, ReduceFn, ResultFn)
    -> KuReduceSpanFn<vendor::KuVendor, MapFn, ReduceFn, ResultFn>;

template <typename MapFn, typename ReduceFn>
KuReduceSpanFn(KuContext &, MapFn, ReduceFn) -> KuReduceSpanFn<vendor::KuVendor, MapFn, ReduceFn>;

} // namespace kuai
