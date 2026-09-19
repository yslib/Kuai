#pragma once

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <kuai/core/KuScalar.h>
#include <kuai/core/KuTensor.h>
#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/vendor/KuDeviceAccessor.h>
#include <kuai/vendor/KuTypeVisit.h>

namespace kuai {

template <typename To>
std::optional<To> readKuScalarAs(const KuScalar &scalar) {
    std::optional<To> result;
    visitKuTypeTs<KuAllPrimitiveTs>(scalar.getType(), [&]<typename From>() {
        if constexpr (std::is_invocable_v<ku_cast<To>, From>) {
            result = ku_cast<To>{}(scalar.template value<From>());
        }
    });
    return result;
}

template <typename T, typename LayoutPolicy = KuLayoutLeft>
auto makeKuSpan(KuScalar &scalar) {
    using Extents = KuExtents<std::size_t>;
    using Span = KuMdSpan<T, Extents, LayoutPolicy>;
    return Span(&scalar.template value<T>());
}

template <typename T, typename LayoutPolicy = KuLayoutLeft>
auto makeKuSpan(const KuScalar &scalar) {
    using Extents = KuExtents<std::size_t>;
    using Span = KuMdSpan<const T, Extents, LayoutPolicy>;
    return Span(&scalar.template value<T>());
}

template <typename Vendor, typename T, std::size_t Rank, typename LayoutPolicy = KuLayoutLeft>
auto makeKuSpan(KuTensor &tensor) {
    using Extents = KuDynamicExtents<Rank>;
    using Accessor = KuDeviceAccessor<T, Vendor>;
    using Span = KuMdSpan<T, Extents, LayoutPolicy, Accessor>;
    return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        return Span(tensor.template begin<T>(), Extents(tensor.extent(Indices)...), Accessor{});
    }(std::make_index_sequence<Rank>{});
}

namespace detail {

template <typename RankSeq>
struct KuFirstRank;

template <std::size_t First, std::size_t... Rest>
struct KuFirstRank<std::index_sequence<First, Rest...>>
    : std::integral_constant<std::size_t, First> {};

template <std::size_t Expected, std::size_t... Ranks>
inline constexpr bool ku_rank_sequence_contains_v = ((Expected == Ranks) || ...);

template <std::size_t Expected, typename RankSeq>
struct KuRankSequenceContains;

template <std::size_t Expected, std::size_t... Ranks>
struct KuRankSequenceContains<Expected, std::index_sequence<Ranks...>>
    : std::bool_constant<ku_rank_sequence_contains_v<Expected, Ranks...>> {};

template <std::size_t... Ranks>
bool kuRankSequenceContains(std::size_t rank, std::index_sequence<Ranks...>) {
    return ((rank == Ranks) || ...);
}

template <typename Result>
[[noreturn]] Result kuUnreachableSpanResult() {
    KU_UNREACHABLE();
}

template <typename TypeSeq>
std::size_t kuSpanPrimitiveTypeIndex(ku_primitive_type_t primitiveType) {
    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    const std::size_t     index = kuPrimitiveTypeIndex<TypeSeq>(primitiveType);
    KU_ASSERT(index < TypeCount, "span dtype is outside the declared type sequence");
    if (index >= TypeCount) {
        KU_UNREACHABLE();
    }
    return index;
}

template <typename RankSeq>
void kuAssertSpanRank(std::size_t rank) {
    const bool supported = kuRankSequenceContains(rank, RankSeq{});
    KU_ASSERT(supported, "span rank is outside the declared rank sequence");
    if (!supported) {
        KU_UNREACHABLE();
    }
}

template <typename Vendor, typename T, typename RankSeq, typename LayoutPolicy, typename Fn>
decltype(auto) visitKuContractTensorSpan(KuTensor &tensor, Fn &fn) {
    static constexpr std::size_t FirstRank = KuFirstRank<RankSeq>::value;
    using ProbeSpan =
        decltype(makeKuSpan<Vendor, T, FirstRank, LayoutPolicy>(std::declval<KuTensor &>()));
    using ResultType = std::invoke_result_t<Fn &, ProbeSpan>;

    kuAssertSpanRank<RankSeq>(tensor.rank());
    return KuStaticSwitch<KuTensor::MaxRank + 1>(
        tensor.rank(), [&]<std::size_t Rank>() -> decltype(auto) {
            if constexpr (KuRankSequenceContains<Rank, RankSeq>::value) {
                return std::invoke(fn, makeKuSpan<Vendor, T, Rank, LayoutPolicy>(tensor));
            } else {
                return kuUnreachableSpanResult<ResultType>();
            }
        });
}

template <typename T>
concept KuSpanObject = std::is_same_v<std::remove_cvref_t<T>, KuScalar>
                       || std::is_same_v<std::remove_cvref_t<T>, KuTensor>;

template <typename RankSeq>
void kuAssertSpanRank(KuScalar &) {
    constexpr bool SupportsScalar = KuRankSequenceContains<0, RankSeq>::value;
    KU_ASSERT(SupportsScalar, "scalar span visitation requires rank zero");
    if constexpr (!SupportsScalar) {
        KU_UNREACHABLE();
    }
}

template <typename RankSeq>
void kuAssertSpanRank(KuTensor &tensor) {
    kuAssertSpanRank<RankSeq>(tensor.rank());
}

template <typename Vendor, typename T, typename RankSeq, typename LayoutPolicy, typename Fn>
decltype(auto) visitKuContractSpan(KuScalar &scalar, Fn &fn) {
    kuAssertSpanRank<RankSeq>(scalar);
    return std::invoke(fn, makeKuSpan<T, LayoutPolicy>(std::as_const(scalar)));
}

template <typename Vendor, typename T, typename RankSeq, typename LayoutPolicy, typename Fn>
decltype(auto) visitKuContractSpan(KuTensor &tensor, Fn &fn) {
    return visitKuContractTensorSpan<Vendor, T, RankSeq, LayoutPolicy>(tensor, fn);
}

} // namespace detail

template <typename Vendor,
          typename RankSeq,
          typename TypeSeq,
          typename LayoutPolicy = KuLayoutLeft,
          typename Fn>
decltype(auto) visitKuSpan(KuScalar &scalar, Fn &&fn) {
    constexpr bool SupportsScalar = detail::KuRankSequenceContains<0, RankSeq>::value;
    KU_ASSERT(SupportsScalar, "scalar span visitation requires rank zero");
    if constexpr (!SupportsScalar) {
        KU_UNREACHABLE();
    }

    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    const std::size_t     index = detail::kuSpanPrimitiveTypeIndex<TypeSeq>(scalar.getType());
    return KuStaticSwitch<TypeCount>(index, [&]<std::size_t Index>() -> decltype(auto) {
        using T = std::tuple_element_t<Index, TypeSeq>;
        return std::invoke(fn, makeKuSpan<T, LayoutPolicy>(std::as_const(scalar)));
    });
}

template <typename Vendor,
          typename RankSeq,
          typename TypeSeq,
          typename LayoutPolicy = KuLayoutLeft,
          typename Fn>
decltype(auto) visitKuSpan(KuTensor &tensor, Fn &&fn) {
    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    const std::size_t     index = detail::kuSpanPrimitiveTypeIndex<TypeSeq>(tensor.getType());
    return KuStaticSwitch<TypeCount>(index, [&]<std::size_t Index>() -> decltype(auto) {
        using T = std::tuple_element_t<Index, TypeSeq>;
        return detail::visitKuContractTensorSpan<Vendor, T, RankSeq, LayoutPolicy>(tensor, fn);
    });
}

template <typename Vendor,
          typename RankSeq,
          typename TypeSeq,
          typename LayoutPolicy = KuLayoutLeft,
          typename Fn>
auto visitKuSpan(KuObject &object, Fn &&fn) {
    using ScalarResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuScalar &>(), std::declval<Fn &>()));
    using TensorResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuTensor &>(), std::declval<Fn &>()));
    static_assert(std::is_same_v<ScalarResultType, TensorResultType>,
                  "scalar and tensor span callbacks must return the same type");

    using ResultType = ScalarResultType;
    using Result = KuResult<ResultType, std::string>;

    const KuObjectKind kind = object.getKind();
    if (kind == kScalar) {
        return Result(
            visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(object.cast<KuScalar>(), fn));
    }
    if (kind == kTensor) {
        return Result(
            visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(object.cast<KuTensor>(), fn));
    }
    return Result(std::string(toString(kind)));
}

template <typename Vendor,
          typename RankSeq,
          typename TypeSeq,
          typename LayoutPolicy = KuLayoutLeft,
          typename Left,
          typename Right,
          typename Fn>
    requires detail::KuSpanObject<Left> && detail::KuSpanObject<Right>
decltype(auto) visitKuSpan(Left &left, Right &right, Fn &&fn) {
    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    constexpr std::size_t PairCount = TypeCount * TypeCount;
    static_assert(PairCount <= 512, "binary primitive dispatch exceeds KuStaticSwitch capacity");

    const std::size_t leftIndex = detail::kuSpanPrimitiveTypeIndex<TypeSeq>(left.getType());
    const std::size_t rightIndex = detail::kuSpanPrimitiveTypeIndex<TypeSeq>(right.getType());
    const std::size_t pairIndex = leftIndex * TypeCount + rightIndex;

    return KuStaticSwitch<PairCount>(pairIndex, [&]<std::size_t PairIndex>() -> decltype(auto) {
        constexpr std::size_t LeftIndex = PairIndex / TypeCount;
        constexpr std::size_t RightIndex = PairIndex % TypeCount;
        using LeftType = std::tuple_element_t<LeftIndex, TypeSeq>;
        using RightType = std::tuple_element_t<RightIndex, TypeSeq>;

        auto leftHandler = [&](auto &&leftSpan) -> decltype(auto) {
            auto rightHandler = [&](auto &&rightSpan) -> decltype(auto) {
                return std::invoke(fn, std::forward<decltype(leftSpan)>(leftSpan),
                                   std::forward<decltype(rightSpan)>(rightSpan));
            };
            return detail::visitKuContractSpan<Vendor, RightType, RankSeq, LayoutPolicy>(
                right, rightHandler);
        };
        return detail::visitKuContractSpan<Vendor, LeftType, RankSeq, LayoutPolicy>(left,
                                                                                    leftHandler);
    });
}

template <typename Vendor,
          typename RankSeq,
          typename TypeSeq,
          typename LayoutPolicy = KuLayoutLeft,
          typename Fn>
auto visitKuSpan(KuObject &left, KuObject &right, Fn &&fn) {
    using ResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuScalar &>(), std::declval<KuScalar &>(), std::declval<Fn &>()));
    using ScalarTensorResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuScalar &>(), std::declval<KuTensor &>(), std::declval<Fn &>()));
    using TensorScalarResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuTensor &>(), std::declval<KuScalar &>(), std::declval<Fn &>()));
    using TensorTensorResultType = decltype(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
        std::declval<KuTensor &>(), std::declval<KuTensor &>(), std::declval<Fn &>()));
    static_assert(std::is_same_v<ResultType, ScalarTensorResultType>
                      && std::is_same_v<ResultType, TensorScalarResultType>
                      && std::is_same_v<ResultType, TensorTensorResultType>,
                  "scalar and tensor span callbacks must return the same type");
    using Result = KuResult<ResultType, std::string>;

    const KuObjectKind leftKind = left.getKind();
    std::size_t        leftKindIndex;
    if (leftKind == kScalar) {
        leftKindIndex = 0;
    } else if (leftKind == kTensor) {
        leftKindIndex = 1;
    } else {
        return Result(std::string(toString(leftKind)));
    }

    const KuObjectKind rightKind = right.getKind();
    std::size_t        rightKindIndex;
    if (rightKind == kScalar) {
        rightKindIndex = 0;
    } else if (rightKind == kTensor) {
        rightKindIndex = 1;
    } else {
        return Result(std::string(toString(rightKind)));
    }

    const std::size_t kindIndex = leftKindIndex * 2 + rightKindIndex;
    return KuStaticSwitch<4>(kindIndex, [&]<std::size_t KindIndex>() -> Result {
        if constexpr (KindIndex == 0) {
            return Result(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
                left.cast<KuScalar>(), right.cast<KuScalar>(), fn));
        } else if constexpr (KindIndex == 1) {
            return Result(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
                left.cast<KuScalar>(), right.cast<KuTensor>(), fn));
        } else if constexpr (KindIndex == 2) {
            return Result(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
                left.cast<KuTensor>(), right.cast<KuScalar>(), fn));
        } else {
            return Result(visitKuSpan<Vendor, RankSeq, TypeSeq, LayoutPolicy>(
                left.cast<KuTensor>(), right.cast<KuTensor>(), fn));
        }
    });
}

} // namespace kuai
