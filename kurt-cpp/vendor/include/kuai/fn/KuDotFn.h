#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include <kuai/algorithm/linalg/product.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuScalar.h>
#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/vendor/KuObjectView.h>
#include <kuai/vendor/KuTypeVisit.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/fn/KuBinarySpanFn.h"

namespace kuai {

template <typename Vendor, typename TypeSeq = KuNonVoidPrimitiveTs>
class KuDotFn {
public:
    explicit KuDotFn(KuContext &context) : m_context(context), m_device(context) {
    }

    KuBuiltinResult operator()(KuObject &left, KuObject &right) {
        using R = KuBuiltinResult;
        return visitKuSpan<Vendor, Rs<0, 1, 2>, TypeSeq>(
                   left, right,
                   [&](auto leftSpan, auto rightSpan) -> R { return apply(leftSpan, rightSpan); })
            .mapOr([](std::string error) -> R { return R(std::move(error)); });
    }

private:
    template <typename LeftSpan, typename RightSpan>
    KuBuiltinResult apply(LeftSpan left, RightSpan right) {
        constexpr std::size_t LeftRank = LeftSpan::rank();
        constexpr std::size_t RightRank = RightSpan::rank();
        using LeftT = std::remove_cv_t<typename LeftSpan::value_type>;
        using RightT = std::remove_cv_t<typename RightSpan::value_type>;

        if constexpr (LeftRank == 0 || RightRank == 0) {
            if constexpr (LeftRank == 0 && RightRank == 0) {
                return multiplyScalars(left, right);
            } else {
                return KuBuiltinResult(
                    "dot does not multiply rank-zero values with vectors or matrices");
            }
        } else if constexpr (!std::is_invocable_v<ku_mul, LeftT, RightT>) {
            return KuBuiltinResult(std::string("dot does not support ") + toString<LeftT>()
                                   + " and " + toString<RightT>());
        } else {
            constexpr bool UsesMatrix = LeftRank == 2 || RightRank == 2;
            constexpr bool SupportedMatrixTypes =
                std::is_same_v<LeftT, RightT>
                && (std::is_same_v<LeftT, KuF32> || std::is_same_v<LeftT, KuF64>);
            if constexpr (UsesMatrix && !SupportedMatrixTypes) {
                return KuBuiltinResult("matrix products require matching float or double operands");
            } else {
                return applyProduct(left, right);
            }
        }
    }

    template <typename LeftSpan, typename RightSpan>
    KuBuiltinResult multiplyScalars(LeftSpan left, RightSpan right) {
        using LeftT = std::remove_cv_t<typename LeftSpan::value_type>;
        using RightT = std::remove_cv_t<typename RightSpan::value_type>;
        if constexpr (!std::is_invocable_v<ku_mul, LeftT, RightT>) {
            return KuBuiltinResult(std::string("dot does not support ") + toString<LeftT>()
                                   + " and " + toString<RightT>());
        } else {
            using OutputT = std::invoke_result_t<ku_mul, LeftT, RightT>;
            constexpr bool DeviceOutput =
                ku_is_device_accessor_v<typename LeftSpan::accessor_type>
                || ku_is_device_accessor_v<typename RightSpan::accessor_type>;
            auto multiply = KuBinarySpanFn<Vendor, ku_mul>(m_context, ku_mul{});
            if constexpr (DeviceOutput) {
                auto outputResult =
                    m_device.template createTensor<OutputT>(KuTensorDesc(KuDynamicExtents<0>{}));
                if (!outputResult.hasValue()) {
                    return KuBuiltinError(outputResult.error());
                }
                auto output = std::move(outputResult).value();
                multiply(left, right, makeKuSpan<Vendor, OutputT, 0>(*output));
                return KuBuiltinResult(std::move(output));
            } else {
                auto output = ku_make_sp<KuScalar>(ku_value_traits<OutputT>::dflt());
                multiply(left, right, makeKuSpan<OutputT>(*output));
                return KuBuiltinResult(std::move(output));
            }
        }
    }

    template <typename LeftSpan, typename RightSpan>
    KuBuiltinResult applyProduct(LeftSpan left, RightSpan right) {
        constexpr std::size_t LeftRank = LeftSpan::rank();
        constexpr std::size_t RightRank = RightSpan::rank();
        constexpr std::size_t OutputRank = LeftRank + RightRank - 2;
        using LeftT = std::remove_cv_t<typename LeftSpan::value_type>;
        using RightT = std::remove_cv_t<typename RightSpan::value_type>;
        using OutputT = std::invoke_result_t<ku_mul, LeftT, RightT>;

        static_assert(ku_is_device_accessor_v<typename LeftSpan::accessor_type>
                          && ku_is_device_accessor_v<typename RightSpan::accessor_type>,
                      "linalg products require device tensor operands");

        auto outputExtents = linalg::product_extents(left, right);
        if (!outputExtents) {
            return KuBuiltinResult("incompatible linalg product extents");
        }

        auto outputResult = m_device.template createTensor<OutputT>(KuTensorDesc(*outputExtents));
        if (!outputResult.hasValue()) {
            return KuBuiltinError(outputResult.error());
        }
        auto output = std::move(outputResult).value();
        auto outputSpan = makeKuSpan<Vendor, OutputT, OutputRank>(*output);
        if constexpr (LeftRank == 1 && RightRank == 1) {
            const auto status = linalg::dot(m_device, left, right, outputSpan);
            if (status != KU_STATUS_SUCCESS) {
                return KuBuiltinError(status);
            }
        } else if constexpr (LeftRank == 2 && RightRank == 1) {
            linalg::matrix_vector_product(m_device, left, right, outputSpan);
        } else if constexpr (LeftRank == 1 && RightRank == 2) {
            linalg::vector_matrix_product(m_device, left, right, outputSpan);
        } else {
            linalg::matrix_product(m_device, left, right, outputSpan);
        }
        return KuBuiltinResult(std::move(output));
    }

    KuContext              &m_context;
    KuVendorContext<Vendor> m_device;
};

} // namespace kuai
