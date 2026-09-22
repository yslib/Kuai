#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuContext.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuScalar.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/vendor/KuObjectView.h>
#include <kuai/vendor/KuVendorContext.h>

namespace kuai {

template <typename Vendor, typename TypeSeq>
struct KuBinaryFn {
    explicit KuBinaryFn(KuContext &context) : m_device(context) {
    }

    KuBinaryFn(const KuBinaryFn &) = delete;
    KuBinaryFn &operator=(const KuBinaryFn &) = delete;
    KuBinaryFn(KuBinaryFn &&) = delete;
    KuBinaryFn &operator=(KuBinaryFn &&) = delete;

    template <typename Extents1, typename Extents2>
    static bool checkShapeCompatible(const Extents1 &extents1, const Extents2 &extents2) {
        constexpr std::size_t Rank1 = Extents1::rank();
        constexpr std::size_t Rank2 = Extents2::rank();
        if constexpr (Rank1 == Rank2) {
            return extents1.as_array() == extents2.as_array();
        } else {
            for (std::size_t i = Rank2; i < Rank1; ++i) {
                if (extents1.fwd_prod_of_extents(i) == extents2.prod()) {
                    return true;
                }
            }
            return false;
        }
    }

    template <typename SpanFn>
    KuBuiltinResult operator()(KuObject &arg1, KuObject &arg2, SpanFn &&spanFn) {
        using BinaryFn = typename std::decay_t<SpanFn>::Fn;
        using R = KuBuiltinResult;
        return visitKuSpan<Vendor, Rs<0, 1, 2>, TypeSeq>(
                   arg1, arg2,
                   [&](auto &&span1, auto &&span2) -> R {
                       using Span1 = std::decay_t<decltype(span1)>;
                       using T1 = std::remove_cv_t<typename Span1::value_type>;
                       constexpr std::size_t Rank1 = Span1::rank();
                       using Span2 = std::decay_t<decltype(span2)>;
                       using T2 = std::remove_cv_t<typename Span2::value_type>;
                       constexpr std::size_t Rank2 = Span2::rank();
                       constexpr bool        DeviceOutput =
                           ku_is_device_accessor_v<typename Span1::accessor_type>
                           || ku_is_device_accessor_v<typename Span2::accessor_type>;
                       if constexpr (!std::is_invocable_v<BinaryFn, T1, T2>) {
                           return R(std::string("This operator don't support for ") + toString<T1>()
                                    + " and " + toString<T2>());
                       } else {
                           using OpRet = std::invoke_result_t<BinaryFn, T1, T2>;
                           if constexpr (Rank1 == Rank2) {
                               if (span1.mapping().extents().as_array()
                                   == span2.mapping().extents().as_array()) {
                                   if constexpr (Rank1 == 0) {
                                       if constexpr (DeviceOutput) {
                                           auto outputResult =
                                               m_device.template createTensor<OpRet>(
                                                   KuTensorDesc(KuDynamicExtents<0>{}));
                                           if (!outputResult.hasValue()) {
                                               return R(KuBuiltinError(outputResult.error()));
                                           }
                                           auto output = std::move(outputResult).value();
                                           spanFn(span1, span2,
                                                  makeKuSpan<Vendor, OpRet, 0>(*output));
                                           return R(KuBuiltinResult(std::move(output)));
                                       } else {
                                           auto output =
                                               ku_make_sp<KuScalar>(ku_value_traits<OpRet>::dflt());
                                           spanFn(span1, span2, makeKuSpan<OpRet>(*output));
                                           return R(KuBuiltinResult(std::move(output)));
                                       }
                                   } else {
                                       auto outputResult = m_device.template createTensor<OpRet>(
                                           KuTensorDesc(span1.mapping().extents()));
                                       if (!outputResult.hasValue()) {
                                           return R(KuBuiltinError(outputResult.error()));
                                       }
                                       auto output = std::move(outputResult).value();
                                       if (span1.size() != 0) {
                                           spanFn(span1, span2,
                                                  makeKuSpan<Vendor, OpRet, Rank1>(*output));
                                       }
                                       return R(KuBuiltinResult(std::move(output)));
                                   }
                               }
                               if constexpr (Rank1 == 2) {
                                   if (span1.extent(1) == 1) {
                                       auto outputResult = m_device.template createTensor<OpRet>(
                                           KuTensorDesc(span2.mapping().extents()));
                                       if (!outputResult.hasValue()) {
                                           return R(KuBuiltinError(outputResult.error()));
                                       }
                                       auto output = std::move(outputResult).value();
                                       if (span2.size() != 0) {
                                           spanFn(span1, span2,
                                                  makeKuSpan<Vendor, OpRet, Rank2>(*output));
                                       }
                                       return R(KuBuiltinResult(std::move(output)));
                                   }
                                   if (span2.extent(1) == 1) {
                                       auto outputResult = m_device.template createTensor<OpRet>(
                                           KuTensorDesc(span1.mapping().extents()));
                                       if (!outputResult.hasValue()) {
                                           return R(KuBuiltinError(outputResult.error()));
                                       }
                                       auto output = std::move(outputResult).value();
                                       if (span1.size() != 0) {
                                           spanFn(span1, span2,
                                                  makeKuSpan<Vendor, OpRet, Rank1>(*output));
                                       }
                                       return R(KuBuiltinResult(std::move(output)));
                                   }
                               }
                               return R("incompatible tensor size");
                           } else if constexpr (Rank1 > Rank2) {
                               if (!checkShapeCompatible(span1.mapping().extents(),
                                                         span2.mapping().extents())) {
                                   return R("incompatible tensor size");
                               }
                               auto outputResult = m_device.template createTensor<OpRet>(
                                   KuTensorDesc(span1.mapping().extents()));
                               if (!outputResult.hasValue()) {
                                   return R(KuBuiltinError(outputResult.error()));
                               }
                               auto output = std::move(outputResult).value();
                               spanFn(span1, span2, makeKuSpan<Vendor, OpRet, Rank1>(*output));
                               return R(KuBuiltinResult(std::move(output)));
                           } else {
                               if (!checkShapeCompatible(span2.mapping().extents(),
                                                         span1.mapping().extents())) {
                                   return R("incompatible tensor size");
                               }
                               auto outputResult = m_device.template createTensor<OpRet>(
                                   KuTensorDesc(span2.mapping().extents()));
                               if (!outputResult.hasValue()) {
                                   return R(KuBuiltinError(outputResult.error()));
                               }
                               auto output = std::move(outputResult).value();
                               spanFn(span1, span2, makeKuSpan<Vendor, OpRet, Rank2>(*output));
                               return R(KuBuiltinResult(std::move(output)));
                           }
                       }
                   })
            .mapOr([](std::string error) -> R { return R(std::move(error)); });
    }

    KuVendorContext<Vendor> m_device;
};

} // namespace kuai
