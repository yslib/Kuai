#pragma once

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <kuai/core/KuScalar.h>
#include <kuai/core/KuTensor.h>
#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuResult.h>
#include <kuai/profiler/KuTracy.h>
#include <kuai/vendor/KuStaticSwitch.h>

namespace kuai {

template <typename... Args>
using Ts = std::tuple<Args...>;

template <std::size_t... Ranks>
using Rs = std::index_sequence<Ranks...>;

using KuNonVoidPrimitiveTs = Ts<KuBool8, KuChar8, KuI16, KuI32, KuI64, KuF32, KuF64>;
using KuAllPrimitiveTs = Ts<KuVoid8, KuBool8, KuChar8, KuI16, KuI32, KuI64, KuF32, KuF64>;

template <typename TypeSeq, typename Fn>
auto visitKuTypeTs(ku_primitive_type_t dataType, Fn &&fn);

namespace detail {

template <typename TypeSeq, std::size_t... Indices>
consteval bool kuPrimitiveTypeSequenceIsDense(std::index_sequence<Indices...>) {
    using FirstType = std::tuple_element_t<0, TypeSeq>;
    constexpr ku_primitive_type_t FirstTypeCode = KuTypeTraits<FirstType>::primitiveType;
    return ((KuTypeTraits<std::tuple_element_t<Indices, TypeSeq>>::primitiveType
             == FirstTypeCode + static_cast<ku_primitive_type_t>(Indices))
            && ...);
}

template <typename TypeSeq>
std::size_t kuPrimitiveTypeIndex(ku_primitive_type_t dataType) {
    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    static_assert(TypeCount > 0, "primitive type sequence must not be empty");
    static_assert(kuPrimitiveTypeSequenceIsDense<TypeSeq>(std::make_index_sequence<TypeCount>{}),
                  "primitive type sequence must use dense, ordered codes");

    using FirstType = std::tuple_element_t<0, TypeSeq>;
    constexpr ku_primitive_type_t FirstTypeCode = KuTypeTraits<FirstType>::primitiveType;
    if (dataType < FirstTypeCode) {
        return TypeCount;
    }
    const auto index = static_cast<std::size_t>(dataType - FirstTypeCode);
    return index < TypeCount ? index : TypeCount;
}

} // namespace detail

template <typename TypeSeq, typename Fn>
auto visitKuTypeTs(ku_primitive_type_t dataType, Fn &&fn) {
    KU_ZONE_SCOPED_N("visitKuTypeTs");
    constexpr std::size_t TypeCount = std::tuple_size_v<TypeSeq>;
    using FirstType = std::tuple_element_t<0, TypeSeq>;
    using ResultType = decltype(std::declval<Fn>().template operator()<FirstType>());

    const std::size_t index = detail::kuPrimitiveTypeIndex<TypeSeq>(dataType);
    if constexpr (std::is_void_v<ResultType>) {
        if (index == TypeCount) {
            return;
        }
        KuStaticSwitch<TypeCount>(index, [&]<std::size_t Index>() {
            using T = std::tuple_element_t<Index, TypeSeq>;
            std::forward<Fn>(fn).template operator()<T>();
        });
    } else {
        using Result = KuResult<ResultType, std::string>;
        if (index == TypeCount) {
            return Result(std::string(kuPrimitiveTypeName(dataType)));
        }
        return KuStaticSwitch<TypeCount>(index, [&]<std::size_t Index>() -> Result {
            using T = std::tuple_element_t<Index, TypeSeq>;
            return Result(std::forward<Fn>(fn).template operator()<T>());
        });
    }
}

template <typename... Args, typename Fn>
auto visitKuType(ku_primitive_type_t dataType, Fn &&fn) {
    return visitKuTypeTs<Ts<Args...>>(dataType, std::forward<Fn>(fn));
}

template <typename TypeSeq, typename Fn>
auto visitKuScalar(KuObject &object, Fn &&fn) {
    using FirstType = std::tuple_element_t<0, TypeSeq>;
    using ResultType = std::invoke_result_t<Fn, FirstType>;
    using Result = KuResult<ResultType, std::string>;

    auto *scalar = object.as<KuScalar>();
    if (scalar == nullptr) {
        return Result(std::string(toString(object.getKind())));
    }

    return visitKuTypeTs<TypeSeq>(scalar->getType(),
                                  [&]<typename T>() -> Result {
                                      return Result(
                                          std::forward<Fn>(fn)(scalar->template value<T>()));
                                  })
        .mapOr([](std::string error) { return Result(std::move(error)); });
}

} // namespace kuai
