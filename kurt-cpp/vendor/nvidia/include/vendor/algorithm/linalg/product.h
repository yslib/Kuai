#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cublas_v2.h>
#include <limits>
#include <type_traits>
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/algorithm/reduction.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/KuVendorStatus.h>
#include <vendor/KuVendorTraits.h>

namespace kuai::vendor::cuda {

namespace detail {

inline void checkCublas(cublasStatus_t status) {
    if (status == CUBLAS_STATUS_SUCCESS) {
        return;
    }
    std::fprintf(stderr, "[kuai] cuBLAS error: %s\n", cublasGetStatusString(status));
    std::fflush(stderr);
    throw ::kuai::detail::KuStatusError(toStatus(status));
}

template <typename Span>
using span_value_t = std::remove_cv_t<typename std::remove_cvref_t<Span>::value_type>;

template <typename Span>
inline constexpr bool is_layout_left_v =
    std::is_same_v<typename std::remove_cvref_t<Span>::layout_policy, KuLayoutLeft>;

template <typename Span>
inline constexpr bool is_layout_right_v =
    std::is_same_v<typename std::remove_cvref_t<Span>::layout_policy, KuLayoutRight>;

template <typename Span>
inline constexpr bool is_canonical_layout_v = is_layout_left_v<Span> || is_layout_right_v<Span>;

template <typename Value>
std::int64_t blasIndex(Value value) {
    using Limit = std::numeric_limits<std::int64_t>;
    if (value > static_cast<Value>(Limit::max())) {
        throw ::kuai::detail::KuStatusError(KU_STATUS_OUT_OF_RANGE);
    }
    return static_cast<std::int64_t>(value);
}

template <typename Matrix>
std::int64_t baseRows(const Matrix &matrix) {
    static_assert(is_canonical_layout_v<Matrix>,
                  "cuBLAS products require layout-left or layout-right matrices");
    const auto rows = is_layout_left_v<Matrix> ? matrix.extent(0) : matrix.extent(1);
    return blasIndex(rows);
}

template <typename Matrix>
std::int64_t baseColumns(const Matrix &matrix) {
    static_assert(is_canonical_layout_v<Matrix>,
                  "cuBLAS products require layout-left or layout-right matrices");
    const auto columns = is_layout_left_v<Matrix> ? matrix.extent(1) : matrix.extent(0);
    return blasIndex(columns);
}

template <typename Matrix>
std::int64_t leadingDimension(const Matrix &matrix) {
    return std::max<std::int64_t>(1, baseRows(matrix));
}

template <bool LogicalTranspose, typename Matrix>
constexpr cublasOperation_t matrixOperation() {
    static_assert(is_canonical_layout_v<Matrix>,
                  "cuBLAS products require layout-left or layout-right matrices");
    constexpr bool TransposeStorage = is_layout_right_v<Matrix> != LogicalTranspose;
    return TransposeStorage ? CUBLAS_OP_T : CUBLAS_OP_N;
}

template <typename Vector>
std::int64_t vectorStride(const Vector &vector) {
    static_assert(is_canonical_layout_v<Vector>,
                  "cuBLAS products require layout-left or layout-right vectors");
    return blasIndex(vector.mapping().strides().extent(0));
}

template <typename Result, typename LeftSpan, typename RightSpan>
struct DotAt {
    LeftSpan  m_left;
    RightSpan m_right;

    KU_DEVICE_HOST Result operator()(ku_size_t index) const {
        return static_cast<Result>(ku_mul{}(m_left(index), m_right(index)));
    }
};

template <typename Result>
struct DotAdd {
    KU_DEVICE_HOST Result operator()(const Result &left, const Result &right) const {
        return static_cast<Result>(ku_add{}(left, right));
    }
};

template <typename T>
void gemv(cublasHandle_t    handle,
          cublasOperation_t operation,
          std::int64_t      rows,
          std::int64_t      columns,
          const T          *matrix,
          std::int64_t      leadingDimension,
          const T          *input,
          std::int64_t      inputStride,
          T                *output,
          std::int64_t      outputStride) {
    const T alpha = T(1);
    const T beta = T(0);
    if constexpr (std::is_same_v<T, KuF32>) {
        checkCublas(cublasSgemv_64(handle, operation, rows, columns, &alpha, matrix,
                                   leadingDimension, input, inputStride, &beta, output,
                                   outputStride));
    } else {
        static_assert(std::is_same_v<T, KuF64>, "cuBLAS GEMV supports KuF32 or KuF64");
        checkCublas(cublasDgemv_64(handle, operation, rows, columns, &alpha, matrix,
                                   leadingDimension, input, inputStride, &beta, output,
                                   outputStride));
    }
}

template <typename T>
void gemm(cublasHandle_t    handle,
          cublasOperation_t leftOperation,
          cublasOperation_t rightOperation,
          std::int64_t      rows,
          std::int64_t      columns,
          std::int64_t      contracted,
          const T          *left,
          std::int64_t      leftLeadingDimension,
          const T          *right,
          std::int64_t      rightLeadingDimension,
          T                *output,
          std::int64_t      outputLeadingDimension) {
    const T alpha = T(1);
    const T beta = T(0);
    if constexpr (std::is_same_v<T, KuF32>) {
        checkCublas(cublasSgemm_64(handle, leftOperation, rightOperation, rows, columns, contracted,
                                   &alpha, left, leftLeadingDimension, right, rightLeadingDimension,
                                   &beta, output, outputLeadingDimension));
    } else {
        static_assert(std::is_same_v<T, KuF64>, "cuBLAS GEMM supports KuF32 or KuF64");
        checkCublas(cublasDgemm_64(handle, leftOperation, rightOperation, rows, columns, contracted,
                                   &alpha, left, leftLeadingDimension, right, rightLeadingDimension,
                                   &beta, output, outputLeadingDimension));
    }
}

} // namespace detail

template <typename Vendor, typename LeftVector, typename RightVector, typename OutputScalar>
ku_status_t dot(const KuVendorContext<Vendor> &context,
                LeftVector                     left,
                RightVector                    right,
                OutputScalar                   output) {
    using LeftT = detail::span_value_t<LeftVector>;
    using RightT = detail::span_value_t<RightVector>;
    using Result = std::remove_cv_t<typename OutputScalar::value_type>;
    using Product = std::invoke_result_t<ku_mul, LeftT, RightT>;
    static_assert(std::is_same_v<Result, Product>,
                  "dot output must use the multiplication result type");
    static_assert(detail::is_canonical_layout_v<LeftVector>
                      && detail::is_canonical_layout_v<RightVector>
                      && detail::is_canonical_layout_v<OutputScalar>,
                  "dot requires layout-left or layout-right spans");

    auto first = makeCountingIterator(ku_size_t(0));
    return algo::map_reduce(context, ku_value_traits<Result>::dflt(), first, first + left.extent(0),
                            output.data_handle(),
                            detail::DotAt<Result, LeftVector, RightVector>{left, right},
                            detail::DotAdd<Result>{});
}

template <typename Vendor, typename Matrix, typename InputVector, typename OutputVector>
void matrix_vector_product(const KuVendorContext<Vendor> &context,
                           Matrix                         matrix,
                           InputVector                    input,
                           OutputVector                   output) {
    using T = detail::span_value_t<Matrix>;
    static_assert(std::is_same_v<T, detail::span_value_t<InputVector>>
                      && std::is_same_v<T, detail::span_value_t<OutputVector>>,
                  "matrix-vector products require matching element types");
    if (output.size() == 0) {
        return;
    }

    auto handleResult =
        ku_vendor_traits<Vendor>::blas_handle(context.m_ctx, context.m_dev, context.m_stream);
    if (!handleResult) {
        throw ::kuai::detail::KuStatusError(handleResult.error());
    }
    auto handle = std::move(handleResult).value();
    detail::gemv(handle.get(), detail::matrixOperation<false, Matrix>(), detail::baseRows(matrix),
                 detail::baseColumns(matrix), matrix.data_handle(),
                 detail::leadingDimension(matrix), input.data_handle(), detail::vectorStride(input),
                 output.data_handle(), detail::vectorStride(output));
}

template <typename Vendor, typename InputVector, typename Matrix, typename OutputVector>
void vector_matrix_product(const KuVendorContext<Vendor> &context,
                           InputVector                    input,
                           Matrix                         matrix,
                           OutputVector                   output) {
    using T = detail::span_value_t<Matrix>;
    static_assert(std::is_same_v<T, detail::span_value_t<InputVector>>
                      && std::is_same_v<T, detail::span_value_t<OutputVector>>,
                  "vector-matrix products require matching element types");
    if (output.size() == 0) {
        return;
    }

    auto handleResult =
        ku_vendor_traits<Vendor>::blas_handle(context.m_ctx, context.m_dev, context.m_stream);
    if (!handleResult) {
        throw ::kuai::detail::KuStatusError(handleResult.error());
    }
    auto handle = std::move(handleResult).value();
    detail::gemv(handle.get(), detail::matrixOperation<true, Matrix>(), detail::baseRows(matrix),
                 detail::baseColumns(matrix), matrix.data_handle(),
                 detail::leadingDimension(matrix), input.data_handle(), detail::vectorStride(input),
                 output.data_handle(), detail::vectorStride(output));
}

template <typename Vendor, typename LeftMatrix, typename RightMatrix, typename OutputMatrix>
void matrix_product(const KuVendorContext<Vendor> &context,
                    LeftMatrix                     left,
                    RightMatrix                    right,
                    OutputMatrix                   output) {
    using T = detail::span_value_t<LeftMatrix>;
    static_assert(std::is_same_v<T, detail::span_value_t<RightMatrix>>
                      && std::is_same_v<T, detail::span_value_t<OutputMatrix>>,
                  "matrix products require matching element types");
    static_assert(detail::is_canonical_layout_v<OutputMatrix>,
                  "cuBLAS products require layout-left or layout-right outputs");
    if (output.size() == 0) {
        return;
    }

    auto handleResult =
        ku_vendor_traits<Vendor>::blas_handle(context.m_ctx, context.m_dev, context.m_stream);
    if (!handleResult) {
        throw ::kuai::detail::KuStatusError(handleResult.error());
    }
    auto handle = std::move(handleResult).value();
    if constexpr (detail::is_layout_left_v<OutputMatrix>) {
        detail::gemm(handle.get(), detail::matrixOperation<false, LeftMatrix>(),
                     detail::matrixOperation<false, RightMatrix>(),
                     detail::blasIndex(output.extent(0)), detail::blasIndex(output.extent(1)),
                     detail::blasIndex(left.extent(1)), left.data_handle(),
                     detail::leadingDimension(left), right.data_handle(),
                     detail::leadingDimension(right), output.data_handle(),
                     detail::leadingDimension(output));
    } else {
        detail::gemm(
            handle.get(), detail::matrixOperation<true, RightMatrix>(),
            detail::matrixOperation<true, LeftMatrix>(), detail::blasIndex(output.extent(1)),
            detail::blasIndex(output.extent(0)), detail::blasIndex(left.extent(1)),
            right.data_handle(), detail::leadingDimension(right), left.data_handle(),
            detail::leadingDimension(left), output.data_handle(), detail::leadingDimension(output));
    }
}

} // namespace kuai::vendor::cuda

KU_DEFINE_VENDOR(dot, cuda)
KU_DEFINE_VENDOR(matrix_vector_product, cuda)
KU_DEFINE_VENDOR(vector_matrix_product, cuda)
KU_DEFINE_VENDOR(matrix_product, cuda)
