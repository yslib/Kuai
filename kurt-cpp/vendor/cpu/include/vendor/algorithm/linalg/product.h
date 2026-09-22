#pragma once

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/KuVendor.h>

namespace kuai::vendor::cpu {

template <typename Vendor, typename LeftVector, typename RightVector, typename OutputScalar>
ku_status_t dot(const KuVendorContext<Vendor> &context,
                LeftVector                     left,
                RightVector                    right,
                OutputScalar                   output) {
    (void)context;
    (void)left;
    (void)right;
    (void)output;
    KU_THROW_VENDOR_NOT_IMPELEMENT;
}

template <typename Vendor, typename Matrix, typename InputVector, typename OutputVector>
void matrix_vector_product(const KuVendorContext<Vendor> &context,
                           Matrix                         matrix,
                           InputVector                    input,
                           OutputVector                   output) {
    (void)context;
    (void)matrix;
    (void)input;
    (void)output;
    KU_THROW_VENDOR_NOT_IMPELEMENT;
}

template <typename Vendor, typename InputVector, typename Matrix, typename OutputVector>
void vector_matrix_product(const KuVendorContext<Vendor> &context,
                           InputVector                    input,
                           Matrix                         matrix,
                           OutputVector                   output) {
    (void)context;
    (void)input;
    (void)matrix;
    (void)output;
    KU_THROW_VENDOR_NOT_IMPELEMENT;
}

template <typename Vendor, typename LeftMatrix, typename RightMatrix, typename OutputMatrix>
void matrix_product(const KuVendorContext<Vendor> &context,
                    LeftMatrix                     left,
                    RightMatrix                    right,
                    OutputMatrix                   output) {
    (void)context;
    (void)left;
    (void)right;
    (void)output;
    KU_THROW_VENDOR_NOT_IMPELEMENT;
}

} // namespace kuai::vendor::cpu

KU_DEFINE_VENDOR(dot, cpu)
KU_DEFINE_VENDOR(matrix_vector_product, cpu)
KU_DEFINE_VENDOR(vector_matrix_product, cpu)
KU_DEFINE_VENDOR(matrix_product, cpu)
