#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/ktl/iterator/KuTabulateOutputIterator.h>
#include <kuai/ktl/iterator/KuTransformIterator.h>
#include <kuai/ktl/iterator/KuTransformOutputIterator.h>
#include <kuai/vendor/KuSpanDispatch.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/scan.h"

namespace kuai {

namespace detail {

struct KuNoScanFinalizer {};

template <std::size_t Axis, typename Span>
KU_DEVICE_HOST auto kuAxisScanOffset(const Span              &span,
                                     typename Span::size_type logicalIndex) noexcept {
    static_assert(Span::rank() > 0, "axis scan requires a positive-rank span");
    static_assert(Axis < Span::rank(), "scan axis must be smaller than the span rank");
    static_assert(Span::is_always_strided(), "axis scan requires a strided span mapping");

    const auto axisExtent = span.extent(Axis);
    const auto axisIndex = logicalIndex % axisExtent;
    auto       segmentIndex = logicalIndex / axisExtent;
    const auto strides = span.mapping().strides();
    auto       offset = axisIndex * strides.extent(Axis);

    for (std::size_t dimension = 0; dimension < Span::rank(); ++dimension) {
        if (dimension == Axis) {
            continue;
        }
        const auto coordinate = segmentIndex % span.extent(dimension);
        segmentIndex /= span.extent(dimension);
        offset += coordinate * strides.extent(dimension);
    }
    return offset;
}

template <typename Span,
          std::size_t Axis,
          bool Direct = Axis == 0 && std::is_same_v<typename Span::layout_policy, KuLayoutLeft>>
class KuAxisScanReader;

template <typename Span, std::size_t Axis>
class KuAxisScanReader<Span, Axis, false> {
public:
    using size_type = typename Span::size_type;

    KU_DEVICE_HOST explicit KuAxisScanReader(Span input) : m_input(std::move(input)) {
    }

    KU_DEVICE_HOST decltype(auto) operator()(size_type logicalIndex) const noexcept {
        const auto offset = kuAxisScanOffset<Axis>(m_input, logicalIndex);
        return m_input.accessor().access(m_input.data_handle(), offset);
    }

    KU_DEVICE_HOST decltype(auto) operator[](size_type logicalIndex) const noexcept {
        return (*this)(logicalIndex);
    }

private:
    Span m_input;
};

template <typename Span, std::size_t Axis>
class KuAxisScanReader<Span, Axis, true> {
public:
    using size_type = typename Span::size_type;

    KU_DEVICE_HOST explicit KuAxisScanReader(Span input) : m_input(std::move(input)) {
    }

    KU_DEVICE_HOST decltype(auto) operator()(size_type logicalIndex) const noexcept {
        return m_input.accessor().access(m_input.data_handle(), logicalIndex);
    }

    KU_DEVICE_HOST decltype(auto) operator[](size_type logicalIndex) const noexcept {
        return (*this)(logicalIndex);
    }

private:
    Span m_input;
};

template <std::size_t Axis, typename Span>
KU_DEVICE_HOST auto makeKuAxisScanReader(Span input) {
    return KuAxisScanReader<Span, Axis>(std::move(input));
}

template <typename Span, std::size_t Axis>
struct KuAxisScanWriteAtFn {
    Span m_output;

    template <typename T>
    KU_DEVICE_HOST void operator()(std::size_t logicalIndex, const T &value) const {
        const auto offset =
            kuAxisScanOffset<Axis>(m_output, static_cast<typename Span::size_type>(logicalIndex));
        auto accessor = m_output.accessor();
        accessor.access(m_output.data_handle(), offset) = value;
    }
};

template <std::size_t Axis, typename Span>
KU_DEVICE_HOST auto makeKuAxisScanOutputIterator(Span output) {
    if constexpr (Axis == 0 && std::is_same_v<typename Span::layout_policy, KuLayoutLeft>) {
        return output.data_handle();
    } else {
        return makeKuTabulateOutputIterator(KuAxisScanWriteAtFn<Span, Axis>{std::move(output)});
    }
}

template <typename Vendor, typename MapFn, typename ReduceFn, std::size_t Axis = 0>
class KuScanSpanCore {
public:
    KuScanSpanCore(KuContext &context, MapFn mapFn, ReduceFn reduceFn)
        : m_device(context), m_mapFn(std::move(mapFn)), m_reduceFn(std::move(reduceFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t apply(InputSpan input, OutputSpan output) const {
        return apply(ku_output_side_t<OutputSpan>{}, input, output, KuNoScanFinalizer{});
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t apply(InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        return apply(ku_output_side_t<OutputSpan>{}, input, output, resultFn);
    }

private:
    template <typename InputSpan>
    using accumulator_type =
        decltype(MapFn::template initValue<std::remove_cv_t<typename InputSpan::value_type>>());

    template <typename Reader>
    struct ReadAtFn {
        Reader m_input;

        KU_DEVICE_HOST decltype(auto) operator()(ku_size_t index) const {
            return m_input[index];
        }
    };

    template <typename InputSpan, typename OutputSpan>
    ku_status_t
    apply(KuHostOutputTag, InputSpan input, OutputSpan output, KuNoScanFinalizer) const {
        static_assert(InputSpan::rank() == 0 && OutputSpan::rank() == 0,
                      "host scan requires rank-zero spans");
        static_assert(!ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "host scan cannot read a device span");
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(std::is_same_v<accumulator_type<InputSpan>, OutputT>,
                      "a scan without a finalizer must publish its accumulator type");
        output() = m_mapFn(input());
        return KU_STATUS_SUCCESS;
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t
    apply(KuHostOutputTag, InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        static_assert(InputSpan::rank() == 0 && OutputSpan::rank() == 0,
                      "host scan requires rank-zero spans");
        static_assert(!ku_is_device_accessor_v<typename InputSpan::accessor_type>,
                      "host scan cannot read a device span");
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(
            std::is_same_v<std::invoke_result_t<const ResultFn &, accumulator_type<InputSpan>>,
                           OutputT>,
            "a finalized scan must publish its finalizer result type");
        output() = resultFn(m_mapFn(input()));
        return KU_STATUS_SUCCESS;
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t
    apply(KuDeviceOutputTag, InputSpan input, OutputSpan output, KuNoScanFinalizer) const {
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(std::is_same_v<accumulator_type<InputSpan>, OutputT>,
                      "a scan without a finalizer must publish its accumulator type");
        return scanDevice(input, output, KuNoScanFinalizer{});
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t
    apply(KuDeviceOutputTag, InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        using OutputT = std::remove_cv_t<typename OutputSpan::value_type>;
        static_assert(
            std::is_same_v<std::invoke_result_t<const ResultFn &, accumulator_type<InputSpan>>,
                           OutputT>,
            "a finalized scan must publish its finalizer result type");
        return scanDevice(input, output, resultFn);
    }

    template <typename OutputIterator>
    static auto makeOutputIterator(OutputIterator output, KuNoScanFinalizer) {
        return output;
    }

    template <typename OutputIterator, typename ResultFn>
    static auto makeOutputIterator(OutputIterator output, const ResultFn &resultFn) {
        return makeKuTransformOutputIterator(std::move(output), resultFn);
    }

    template <typename InputSpan, typename OutputSpan, typename ResultFn>
    ku_status_t scanDevice(InputSpan input, OutputSpan output, const ResultFn &resultFn) const {
        static_assert(InputSpan::is_always_unique() && OutputSpan::is_always_unique(),
                      "linear span algorithms require unique mappings");
        static_assert(InputSpan::is_always_exhaustive() && OutputSpan::is_always_exhaustive(),
                      "linear span algorithms require exhaustive mappings");
        constexpr std::size_t InputRank = InputSpan::rank();
        constexpr std::size_t OutputRank = OutputSpan::rank();
        static_assert((InputRank == 0 && OutputRank == 1) || (InputRank == OutputRank),
                      "scan output rank must follow its operation contract");
        if (output.size() == 0) {
            return KU_STATUS_SUCCESS;
        }

        auto count = makeCountingIterator(ku_size_t(0));
        if constexpr (InputRank == 0) {
            auto inputReader = makeKuInputReader(input);
            auto first = makeTransformIterator(count, ReadAtFn{inputReader});
            auto outputFirst = makeOutputIterator(output.data_handle(), resultFn);
            return algo::inclusive_scan(m_device, first, first + output.size(), outputFirst,
                                        m_mapFn, m_reduceFn);
        } else {
            static_assert(Axis < InputRank, "scan axis must be smaller than the input rank");
            static_assert(OutputRank == InputRank,
                          "positive-rank scans must preserve the input rank");
            static_assert(InputSpan::is_always_strided() && OutputSpan::is_always_strided(),
                          "axis scan requires strided span mappings");

            auto first = makeTransformIterator(count, makeKuAxisScanReader<Axis>(input));
            auto outputFirst =
                makeOutputIterator(makeKuAxisScanOutputIterator<Axis>(output), resultFn);
            if constexpr (InputRank == 1) {
                return algo::inclusive_scan(m_device, first, first + input.size(), outputFirst,
                                            m_mapFn, m_reduceFn);
            } else {
                return algo::segmented_inclusive_scan(m_device, first, first + input.size(),
                                                      input.extent(Axis), outputFirst, m_mapFn,
                                                      m_reduceFn);
            }
        }
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
struct KuScanSpanFn {
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

    KuScanSpanFn(KuContext &context, MapFn mapFn, ReduceFn reduceFn, ResultFn resultFn)
        : m_core(context, std::move(mapFn), std::move(reduceFn)), m_resultFn(std::move(resultFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t operator()(InputSpan input, OutputSpan output) const {
        return m_core.apply(input, output, m_resultFn);
    }

private:
    detail::KuScanSpanCore<Vendor, MapFn, ReduceFn, Axis> m_core;
    ResultFn                                              m_resultFn;
};

template <typename Vendor, typename MapFn, typename ReduceFn, std::size_t Axis>
struct KuScanSpanFn<Vendor, MapFn, ReduceFn, void, Axis> {
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

    KuScanSpanFn(KuContext &context, MapFn mapFn, ReduceFn reduceFn)
        : m_core(context, std::move(mapFn), std::move(reduceFn)) {
    }

    template <typename InputSpan, typename OutputSpan>
    ku_status_t operator()(InputSpan input, OutputSpan output) const {
        return m_core.apply(input, output);
    }

private:
    detail::KuScanSpanCore<Vendor, MapFn, ReduceFn, Axis> m_core;
};

template <typename MapFn, typename ReduceFn, typename ResultFn>
KuScanSpanFn(KuContext &, MapFn, ReduceFn, ResultFn)
    -> KuScanSpanFn<vendor::KuVendor, MapFn, ReduceFn, ResultFn>;

template <typename MapFn, typename ReduceFn>
KuScanSpanFn(KuContext &, MapFn, ReduceFn) -> KuScanSpanFn<vendor::KuVendor, MapFn, ReduceFn>;

} // namespace kuai
