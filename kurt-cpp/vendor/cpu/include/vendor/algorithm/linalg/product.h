#pragma once

#include <type_traits>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/algorithm/reduction.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/KuVendor.h>

namespace kuai::vendor::cpu {

template <typename Vendor, typename LeftVector, typename RightVector, typename OutputScalar>
ku_status_t dot(const KuVendorContext<Vendor> &context,
                LeftVector                     left,
                RightVector                    right,
                OutputScalar                   output) {
    using Result = std::remove_cv_t<typename OutputScalar::value_type>;
    auto first = makeCountingIterator(ku_size_t{0});
    return algo::map_reduce(
        context, ku_value_traits<Result>::dflt(), first, first + left.extent(0),
        output.data_handle(),
        [&](ku_size_t index) { return ku_cast<Result>{}(ku_mul{}(left(index), right(index))); },
        [](Result accumulated, Result value) {
            // Addition can widen small integers; preserve NULL when narrowing back.
            return ku_cast<Result>{}(ku_add{}(accumulated, value));
        });
}

template <typename Vendor, typename Matrix, typename InputVector, typename OutputVector>
void matrix_vector_product(const KuVendorContext<Vendor> &context,
                           Matrix                         matrix,
                           InputVector                    input,
                           OutputVector                   output) {
    (void)context;
    using T = std::remove_cv_t<typename OutputVector::value_type>;
    for (ku_size_t row = 0; row < output.extent(0); ++row) {
        T sum{};
        for (ku_size_t k = 0; k < matrix.extent(1); ++k) {
            sum += matrix(row, k) * input(k);
        }
        output(row) = sum;
    }
}

template <typename Vendor, typename InputVector, typename Matrix, typename OutputVector>
void vector_matrix_product(const KuVendorContext<Vendor> &context,
                           InputVector                    input,
                           Matrix                         matrix,
                           OutputVector                   output) {
    (void)context;
    using T = std::remove_cv_t<typename OutputVector::value_type>;
    for (ku_size_t column = 0; column < output.extent(0); ++column) {
        T sum{};
        for (ku_size_t k = 0; k < matrix.extent(0); ++k) {
            sum += input(k) * matrix(k, column);
        }
        output(column) = sum;
    }
}

template <typename Vendor, typename LeftMatrix, typename RightMatrix, typename OutputMatrix>
void matrix_product(const KuVendorContext<Vendor> &context,
                    LeftMatrix                     left,
                    RightMatrix                    right,
                    OutputMatrix                   output) {
    (void)context;
    using T = std::remove_cv_t<typename OutputMatrix::value_type>;
    for (ku_size_t column = 0; column < output.extent(1); ++column) {
        for (ku_size_t row = 0; row < output.extent(0); ++row) {
            T sum{};
            for (ku_size_t k = 0; k < left.extent(1); ++k) {
                sum += left(row, k) * right(k, column);
            }
            output(row, column) = sum;
        }
    }
}

} // namespace kuai::vendor::cpu

KU_DEFINE_VENDOR(dot, cpu)
KU_DEFINE_VENDOR(matrix_vector_product, cpu)
KU_DEFINE_VENDOR(vector_matrix_product, cpu)
KU_DEFINE_VENDOR(matrix_product, cpu)
