#pragma once

#include <optional>
#include <type_traits>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

namespace kuai::linalg {

template <typename LeftSpan, typename RightSpan>
struct product_extents_type {
    static constexpr std::size_t LeftRank = std::remove_cvref_t<LeftSpan>::rank();
    static constexpr std::size_t RightRank = std::remove_cvref_t<RightSpan>::rank();

    static_assert((LeftRank == 1 || LeftRank == 2) && (RightRank == 1 || RightRank == 2),
                  "linalg products require rank-one or rank-two operands");

    using type = KuDynamicExtents<LeftRank + RightRank - 2>;
};

template <typename LeftSpan, typename RightSpan>
using product_extents_t = typename product_extents_type<LeftSpan, RightSpan>::type;

template <typename LeftSpan, typename RightSpan>
std::optional<product_extents_t<LeftSpan, RightSpan>> product_extents(const LeftSpan  &left,
                                                                      const RightSpan &right) {
    using Left = std::remove_cvref_t<LeftSpan>;
    using Right = std::remove_cvref_t<RightSpan>;
    using Extents = product_extents_t<Left, Right>;
    constexpr std::size_t LeftRank = Left::rank();
    constexpr std::size_t RightRank = Right::rank();

    if constexpr (LeftRank == 1 && RightRank == 1) {
        if (left.extent(0) != right.extent(0)) {
            return std::nullopt;
        }
        return Extents{};
    } else if constexpr (LeftRank == 2 && RightRank == 1) {
        if (left.extent(1) != right.extent(0)) {
            return std::nullopt;
        }
        return Extents(left.extent(0));
    } else if constexpr (LeftRank == 1 && RightRank == 2) {
        if (left.extent(0) != right.extent(0)) {
            return std::nullopt;
        }
        return Extents(right.extent(1));
    } else {
        if (left.extent(1) != right.extent(0)) {
            return std::nullopt;
        }
        return Extents(left.extent(0), right.extent(1));
    }
}

} // namespace kuai::linalg

#include <vendor/algorithm/linalg/product.h>

namespace kuai::linalg {

template <typename Vendor, typename LeftVector, typename RightVector, typename OutputScalar>
ku_status_t dot(const KuVendorContext<Vendor> &context,
                LeftVector                     left,
                RightVector                    right,
                OutputScalar                   output) {
    static_assert(LeftVector::rank() == 1 && RightVector::rank() == 1,
                  "dot requires rank-one inputs");
    static_assert(OutputScalar::rank() == 0, "dot requires a rank-zero output");
    KU_KERNEL_CALL_ZONE_SCOPED("linalg::dot");
    return KU_CALL_VENDOR(dot, context, left, right, output);
}

template <typename Vendor, typename Matrix, typename InputVector, typename OutputVector>
void matrix_vector_product(const KuVendorContext<Vendor> &context,
                           Matrix                         matrix,
                           InputVector                    input,
                           OutputVector                   output) {
    static_assert(Matrix::rank() == 2 && InputVector::rank() == 1,
                  "matrix_vector_product requires a matrix and a vector");
    static_assert(OutputVector::rank() == 1, "matrix_vector_product requires a rank-one output");
    KU_KERNEL_CALL_ZONE_SCOPED("linalg::matrix_vector_product");
    KU_CALL_VENDOR(matrix_vector_product, context, matrix, input, output);
}

template <typename Vendor, typename InputVector, typename Matrix, typename OutputVector>
void vector_matrix_product(const KuVendorContext<Vendor> &context,
                           InputVector                    input,
                           Matrix                         matrix,
                           OutputVector                   output) {
    static_assert(InputVector::rank() == 1 && Matrix::rank() == 2,
                  "vector_matrix_product requires a vector and a matrix");
    static_assert(OutputVector::rank() == 1, "vector_matrix_product requires a rank-one output");
    KU_KERNEL_CALL_ZONE_SCOPED("linalg::vector_matrix_product");
    KU_CALL_VENDOR(vector_matrix_product, context, input, matrix, output);
}

template <typename Vendor, typename LeftMatrix, typename RightMatrix, typename OutputMatrix>
void matrix_product(const KuVendorContext<Vendor> &context,
                    LeftMatrix                     left,
                    RightMatrix                    right,
                    OutputMatrix                   output) {
    static_assert(LeftMatrix::rank() == 2 && RightMatrix::rank() == 2,
                  "matrix_product requires rank-two inputs");
    static_assert(OutputMatrix::rank() == 2, "matrix_product requires a rank-two output");
    KU_KERNEL_CALL_ZONE_SCOPED("linalg::matrix_product");
    KU_CALL_VENDOR(matrix_product, context, left, right, output);
}

} // namespace kuai::linalg
