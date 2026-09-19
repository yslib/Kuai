#pragma once

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuContext.h>
#include <kuai/core/KuTensor.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/vendor/KuObjectView.h>
#include <kuai/vendor/KuTypeVisit.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/fn/KuScanSpanFn.h"

namespace kuai {

template <typename Vendor, typename TypeSeq, std::size_t Axis = 0>
struct KuScanFn {
    template <typename MapFn, typename ReduceFn>
    KuBuiltinResult
    operator()(KuContext &context, KuObject &arg, MapFn mapFn, ReduceFn reduceFn) const {
        return invoke(context, arg,
                      KuScanSpanFn<Vendor, MapFn, ReduceFn, void, Axis>(context, std::move(mapFn),
                                                                        std::move(reduceFn)));
    }

    template <typename MapFn, typename ReduceFn, typename ResultFn>
    KuBuiltinResult operator()(KuContext &context,
                               KuObject  &arg,
                               MapFn      mapFn,
                               ReduceFn   reduceFn,
                               ResultFn   resultFn) const {
        return invoke(context, arg,
                      KuScanSpanFn<Vendor, MapFn, ReduceFn, ResultFn, Axis>(
                          context, std::move(mapFn), std::move(reduceFn), std::move(resultFn)));
    }

private:
    template <typename SpanFn>
    KuBuiltinResult invoke(KuContext &context, KuObject &arg, SpanFn spanFn) const {
        using R = KuBuiltinResult;
        KuVendorContext<Vendor> device(context);

        auto spanHandler = [&, spanFn = std::move(spanFn)](auto input) -> R {
            using InputSpan = std::remove_cvref_t<decltype(input)>;
            using T = std::remove_cv_t<typename InputSpan::value_type>;
            constexpr std::size_t Rank = InputSpan::rank();
            if constexpr (!SpanFn::template supports<T>) {
                return R("don't support this type " + std::string(toString<T>()));
            } else {
                using OutputT = typename SpanFn::template output_type<T>;

                if constexpr (Rank == 0) {
                    auto outputResult = device.template createTensor<OutputT>(
                        KuTensorDesc(KuDynamicExtents<1>(ku_size_t(1))));
                    if (!outputResult.hasValue()) {
                        return KuBuiltinError(outputResult.error());
                    }
                    auto       output = std::move(outputResult).value();
                    const auto status = spanFn(input, makeKuSpan<Vendor, OutputT, 1>(*output));
                    if (status != KU_STATUS_SUCCESS) {
                        return R(KuBuiltinError(status));
                    }
                    return R(std::move(output));
                } else if constexpr (Axis >= Rank) {
                    return R("The scan axis must be smaller than the input rank");
                } else {
                    auto outputResult = device.template createTensor<OutputT>(
                        KuTensorDesc(input.mapping().extents()));
                    if (!outputResult.hasValue()) {
                        return KuBuiltinError(outputResult.error());
                    }
                    auto output = std::move(outputResult).value();
                    if (input.size() != 0) {
                        const auto status =
                            spanFn(input, makeKuSpan<Vendor, OutputT, Rank>(*output));
                        if (status != KU_STATUS_SUCCESS) {
                            return R(KuBuiltinError(status));
                        }
                    }
                    return R(std::move(output));
                }
            }
        };

        return visitKuSpan<Vendor, Rs<0, 1, 2>, TypeSeq>(arg, spanHandler)
            .mapOr([](std::string error) -> R { return R(std::move(error)); });
    }
};

} // namespace kuai
