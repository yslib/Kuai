#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuContext.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/core/KuScalar.h>
#include <kuai/core/KuTensor.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/vendor/KuObjectView.h>
#include <kuai/vendor/KuTypeVisit.h>
#include <kuai/vendor/KuVendorContext.h>
namespace kuai {
template <typename Vendor, typename TypeSeq, typename Layout = KuLayoutLeft>
struct KuUnaryFn {
public:
    template <typename UnarySpanFn>
    KuBuiltinResult operator()(KuContext &context, KuObject &arg, UnarySpanFn &&spanFn) {
        using UnaryOp = typename std::remove_cvref_t<UnarySpanFn>::Fn;
        using R = KuBuiltinResult;
        KuVendorContext<Vendor> device(context);
        auto                    SpanHandler = [&](auto &&span) -> R {
            using InputSpan = std::remove_cvref_t<decltype(span)>;
            using T = std::remove_cv_t<typename InputSpan::value_type>;
            constexpr std::size_t Rank = InputSpan::rank();
            if constexpr (!std::is_invocable_v<UnaryOp, T>) {
                return R(KuBuiltinError("don't support this type " + std::string(toString<T>())));
            } else {
                using ResultType = std::invoke_result_t<UnaryOp, T>;
                if constexpr (!ku_is_device_accessor_v<typename InputSpan::accessor_type>) {
                    static_assert(Rank == 0, "host unary inputs must have rank zero");
                    auto output = ku_make_sp<KuScalar>(ku_value_traits<ResultType>::dflt());
                    spanFn(span, makeKuSpan<ResultType, Layout>(*output));
                    return KuBuiltinResult(std::move(output));
                } else {
                    auto outputResult = device.template createTensor<ResultType>(
                        KuTensorDesc(span.mapping().extents()));
                    if (!outputResult.hasValue()) {
                        return KuBuiltinError(outputResult.error());
                    }
                    auto output = std::move(outputResult).value();
                    if (span.size() != 0) {
                        spanFn(span, makeKuSpan<Vendor, ResultType, Rank, Layout>(*output));
                    }
                    return KuBuiltinResult(std::move(output));
                }
            }
        };
        return visitKuSpan<Vendor, Rs<0, 1, 2>, TypeSeq, Layout>(arg, SpanHandler)
            .mapOr([](std::string error) -> R { return R(std::move(error)); });
    }
};

} // namespace kuai
