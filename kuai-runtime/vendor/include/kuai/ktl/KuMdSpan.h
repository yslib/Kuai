#pragma once

#include <array>
#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuTemplateTraits.h>
namespace kuai {

inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

inline constexpr std::size_t ku_npos = static_cast<std::size_t>(-1);

struct KuFullExtent_t {
    KU_DEVICE_HOST explicit KuFullExtent_t() = default;
};
inline constexpr KuFullExtent_t ku_full_extent{};

template <class OffsetType, class ExtentType, class StrideType>
struct StridedSlice {
    OffsetType m_offset;
    ExtentType m_extent;
    StrideType m_stride;
    // begin: offset
    // end: offset + extent
    // step: stride
    KU_DEVICE_HOST StridedSlice(OffsetType off, ExtentType ext, StrideType str)
        : m_offset(off), m_extent(ext), m_stride(str) {
    }

    // The fowlloing methods are not standard as the c++26 implementation of
    // std::ranges::views::strided_slice just play a documenting role the number of selected
    // indeices : 1 + (extent - 1) / stride [offset, offset + extent) with step stride]
    OffsetType operator()(std::size_t i) const {
        // compute the i-th index in the strided slice
        return m_offset + i * m_stride;
    }

    size_t count() const {
        return 1 + (m_extent - 1) / m_stride;
    }

    OffsetType start() const {
        return m_offset;
    }

    OffsetType end() const {
        return m_offset + m_extent;
    }

    OffsetType step() const {
        return m_stride;
    }
};

// --------------------- Extents ---------------------
// Basic extents template that supports a fixed rank and a mix of static/dynamic
// extents. Usage: extents<std::size_t, 3, dynamic_extent, 5>  -> rank=3,
// extent(0)=3, extent(1)=runtime, extent(2)=5

template <typename IndexType = std::size_t, std::size_t... Extents>
class KuExtents {
public:
    using index_type = IndexType;
    KU_DEVICE_HOST static constexpr std::size_t rank() noexcept {
        return sizeof...(Extents);
    }

    KU_DEVICE_HOST static constexpr std::size_t rank_dynamic() noexcept {
        return count_dynamic();
    }

    using static_array_t = std::array<index_type, rank()>;

private:
    KU_DEVICE_HOST static constexpr static_array_t make_static_array() noexcept {
        return static_array_t{Extents...};
    }

    // Count dynamic rank
    KU_DEVICE_HOST static constexpr std::size_t count_dynamic() noexcept {
        std::size_t c = 0;
        for (std::size_t i = 0; i < rank(); ++i)
            if (make_static_array()[i] == dynamic_extent)
                ++c;
        return c;
    }

private:
    // storage for dynamic extents (in order of appearance)
    std::array<index_type, rank_dynamic()> m_dyn{};

public:
    // default ctor only works when no dynamic extents
    template <typename Dummy = void,
              typename = typename std::enable_if<rank_dynamic() == 0, Dummy>::type>
    KU_DEVICE_HOST constexpr KuExtents() noexcept {
    }

    // construct from runtime dynamic extents (number must match rank_dynamic)
    template <typename... Dyn, typename = std::enable_if_t<sizeof...(Dyn) == rank_dynamic()>>
    KU_DEVICE_HOST explicit KuExtents(Dyn... ds) {
        static_assert(sizeof...(Dyn) == rank_dynamic(), "wrong number of dynamic extents");
        m_dyn = std::array<index_type, rank_dynamic()>{static_cast<index_type>(ds)...};
    }

    KU_DEVICE_HOST explicit KuExtents(const std::array<index_type, rank_dynamic()> &ds)
        : m_dyn(ds) {
    }

    // extent(i) -- returns either static or dynamic value
    KU_DEVICE_HOST constexpr index_type extent(std::size_t i) const noexcept {
        // assert(i < rank());
        index_type s = make_static_array()[i];
        if (s != dynamic_extent)
            return s;
        // find which dynamic index it is
        std::size_t dyn_idx = 0;
        for (std::size_t j = 0; j < i; ++j)
            if (make_static_array()[j] == dynamic_extent)
                ++dyn_idx;
        return m_dyn[dyn_idx];
    }

    KU_DEVICE_HOST constexpr index_type fwd_prod_of_extents(std::size_t i) const noexcept {
        index_type     prod = 1;
        index_type     dyn_index = 0;
        constexpr auto staticIndex = make_static_array();
#pragma unroll
        for (std::size_t j = 0; j < i; ++j) {
            index_type s = staticIndex[j];
            if (s == dynamic_extent) {
                if (dyn_index < rank_dynamic())
                    s = m_dyn[dyn_index++];
                else
                    s = 1; // if no more dynamic extents, treat as 1
            }
            prod *= s;
        }
        return prod;
    }

    KU_DEVICE_HOST constexpr index_type prod() const noexcept {
        return fwd_prod_of_extents(rank());
    }

    // helper: extents as array
    KU_DEVICE_HOST std::array<index_type, rank()> as_array() const noexcept {
        std::array<index_type, rank()> out{};
        std::size_t                    dyn_idx = 0;
        for (std::size_t i = 0; i < rank(); ++i) {
            index_type s = make_static_array()[i];
            if (s != dynamic_extent)
                out[i] = s;
            else
                out[i] = m_dyn[dyn_idx++];
        }
        return out;
    }
};

template <typename IndexType>
KuExtents(const std::array<IndexType, 1> &) -> KuExtents<IndexType, dynamic_extent>;

template <typename IndexType>
KuExtents(const std::array<IndexType, 2> &) -> KuExtents<IndexType, dynamic_extent, dynamic_extent>;

namespace detail {

template <typename IndexType, typename Sequence>
struct ku_dynamic_extents;

template <typename IndexType, std::size_t... Indices>
struct ku_dynamic_extents<IndexType, std::index_sequence<Indices...>> {
    using type = KuExtents<IndexType, ((void)Indices, dynamic_extent)...>;
};

} // namespace detail

template <std::size_t Rank>
using KuDynamicExtents =
    typename detail::ku_dynamic_extents<std::size_t, std::make_index_sequence<Rank>>::type;

struct KuLayoutRight { // row-major layout
    template <class Extents>
    struct mapping {
        using extents_type = Extents;
        using index_type = typename Extents::index_type;

        KU_DEVICE_HOST mapping() = default;
        KU_DEVICE_HOST explicit mapping(const extents_type &e) : m_elems(e.as_array()) {
        }

        KU_DEVICE_HOST constexpr extents_type extents() const noexcept {
            // construct an extents object from stored values
            // since Extents may have static parts we need to pick a constructor path
            return extents_type(m_elems);
        }

        // multi-index operator: flatten in row-major (right-most index varies
        // fastest)
        template <typename... Idx>
        KU_DEVICE_HOST index_type operator()(Idx... idx) const noexcept {
            static_assert(sizeof...(Idx) == extents_type::rank(), "index count mismatch");
            std::array<index_type, extents_type::rank()> a = {static_cast<index_type>(idx)...};
            // compute offset
            index_type off = 0;
            index_type stride = 1;
            for (std::size_t i = extents_type::rank(); i-- > 0;) {
                off += a[i] * stride;
                stride *= m_elems[i];
            }
            return off;
        }

        // required_span_size (min linear extent)
        KU_DEVICE_HOST index_type required_span_size() const noexcept {
            index_type prod = 1;
            for (std::size_t i = 0; i < extents_type::rank(); ++i)
                prod *= m_elems[i];
            return prod;
        }

        KU_DEVICE_HOST static constexpr bool is_always_unique() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_always_exhaustive() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_always_strided() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_unique() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_exhaustive() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_strided() noexcept {
            return true;
        }

        KU_DEVICE_HOST constexpr extents_type strides() const noexcept {
            std::array<index_type, extents_type::rank()> strides{};
            index_type                                   stride = 1;
            for (std::size_t i = extents_type::rank(); i-- > 0;) {
                strides[i] = stride;
                stride *= m_elems[i];
            }
            return extents_type(strides);
        }

        KU_DEVICE_HOST constexpr void flat_to_multi_index(
            index_type                                    flat_idx,
            std::array<index_type, extents_type::rank()> &multi_idx) const noexcept {

#pragma unroll
            for (std::size_t i = extents_type::rank(); i-- > 0;) {
                multi_idx[i] = flat_idx % m_elems[i];
                flat_idx /= m_elems[i];
            }
        }

    protected:
        std::array<index_type, extents_type::rank()> m_elems{};
    };
};

struct KuLayoutLeft { //
    template <class Extents>
    struct mapping {
        using extents_type = Extents;
        using index_type = typename Extents::index_type;

        KU_DEVICE_HOST mapping() = default;
        KU_DEVICE_HOST explicit mapping(const extents_type &e) : m_exts(e) {
        }

        KU_DEVICE_HOST constexpr extents_type extents() const noexcept {
            // construct an extents object from stored values
            // since Extents may have static parts we need to pick a constructor path
            return m_exts;
        }

        // multi-index operator: flatten in col-major (left-most index varies
        // fastest)
        template <typename... Idx>
        KU_DEVICE_HOST constexpr index_type operator()(Idx... idx) const noexcept {
            static_assert(sizeof...(Idx) == extents_type::rank(), "index count mismatch");
            std::array<index_type, extents_type::rank()> a = {static_cast<index_type>(idx)...};
            // compute offset
            index_type off = 0;
            index_type stride = 1;
            const auto elems = m_exts.as_array();
            for (std::size_t i = 0; i < extents_type::rank(); ++i) {
                off += a[i] * stride;
                stride *= elems[i];
            }
            return off;
        }

        // required_span_size (min linear extent)
        KU_DEVICE_HOST constexpr index_type required_span_size() const noexcept {
            index_type prod = 1;
            const auto elems = m_exts.as_array();
            for (std::size_t i = 0; i < extents_type::rank(); ++i)
                prod *= elems[i];
            return prod;
        }

        KU_DEVICE_HOST static constexpr bool is_always_unique() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_always_exhaustive() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_always_strided() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_unique() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_exhaustive() noexcept {
            return true;
        }

        KU_DEVICE_HOST static constexpr bool is_strided() noexcept {
            return true;
        }

        KU_DEVICE_HOST constexpr extents_type strides() const noexcept {
            std::array<index_type, extents_type::rank()> strides{};
            index_type                                   stride = 1;
            const auto                                   elems = m_exts.as_array();
            for (std::size_t i = 0; i < extents_type::rank(); ++i) {
                strides[i] = stride;
                stride *= elems[i];
            }
            return extents_type(strides);
        }

        KU_DEVICE_HOST constexpr void flat_to_multi_index(
            index_type                                    flat_idx,
            std::array<index_type, extents_type::rank()> &multi_idx) const noexcept {
            const auto elems = m_exts.as_array();
#pragma unroll
            for (std::size_t i = 0; i < extents_type::rank(); ++i) {
                multi_idx[i] = flat_idx % elems[i];
                flat_idx /= elems[i];
            }
        }

    protected:
        // std::array<index_type, extents_type::rank()> m_elems{};
        extents_type m_exts;
    };
};

struct KuLayoutStride {
    template <typename Extents>
    struct mapping {

        using extents_type = Extents;
        using index_type = typename Extents::index_type;
        static constexpr index_type R = extents_type::rank();

        KU_DEVICE_HOST explicit mapping(const extents_type &e, const std::array<index_type, R> &s)
            : m_elems(e.as_array()), m_strides(s) {
        }

        KU_DEVICE_HOST constexpr extents_type extents() const noexcept {
            return m_elems;
        }

        template <typename... Idx>
        KU_DEVICE_HOST constexpr index_type operator()(Idx... idx) const noexcept {
            static_assert(sizeof...(Idx) == extents_type::rank(), "index count mismatch");
            std::array<index_type, extents_type::rank()> a{static_cast<index_type>(idx)...};
            index_type                                   off = 0;
            for (index_type i = 0; i < extents_type::rank(); ++i) {
                off += a[i] * m_strides.extent(i);
            }
            return off;
        }

        KU_DEVICE_HOST constexpr index_type required_span_size() const noexcept {
            index_type max_size = 1;
            for (index_type i = 0; i < extents_type::rank(); ++i) {
                max_size += m_elems.extent(i) * m_strides.extent(i);
            }
            return max_size;
        }

        KU_DEVICE_HOST constexpr extents_type strides() const noexcept {
            return m_strides;
        }

        KU_DEVICE_HOST constexpr void flat_to_multi_index(
            index_type                                    flat_idx,
            std::array<index_type, extents_type::rank()> &multi_idx) const noexcept {
#pragma unroll
            for (index_type i = 0; i < extents_type::rank(); ++i) {
                multi_idx[i] = flat_idx / m_strides.extent(i);
                flat_idx -= multi_idx[i] * m_strides.extent(i);
            }
        }

    protected:
        extents_type m_elems;
        extents_type m_strides;
    };
};

template <class ElementType>
struct KuDefaultAccessor {
    using element_type = ElementType;
    using data_handle_type = element_type *;
    using reference = element_type &;
    using const_reference = const element_type &;

    KU_DEVICE_HOST constexpr KuDefaultAccessor() noexcept = default;

    KU_DEVICE_HOST constexpr reference access(data_handle_type p, std::size_t i) noexcept {
        return p[i];
    }

    KU_DEVICE_HOST constexpr const_reference access(data_handle_type p,
                                                    std::size_t      i) const noexcept {
        return p[i];
    }

    KU_DEVICE_HOST constexpr data_handle_type offset(data_handle_type p,
                                                     std::size_t      i) const noexcept {
        return p + i;
    }
};

template <class ElementType,
          class Extents,
          class LayoutPolicy = KuLayoutRight,
          class AccessorPolicy = KuDefaultAccessor<ElementType>>
class KuMdSpan : private AccessorPolicy {
public:
    using extents_type = Extents;
    using layout_policy = LayoutPolicy;
    using accessor_type = AccessorPolicy;

    using data_handle_type = typename accessor_type::data_handle_type;
    using reference = typename accessor_type::reference;
    using const_reference = typename accessor_type::const_reference;
    using size_type = typename extents_type::index_type;

    using element_type = typename accessor_type::element_type;
    using value_type = element_type;

    using mapping_type = typename layout_policy::template mapping<extents_type>;

    KU_DEVICE_HOST KuMdSpan() = default;

    // construct from data handle + mapping
    KU_DEVICE_HOST
    KuMdSpan(data_handle_type p, const mapping_type &m, accessor_type a = accessor_type()) noexcept
        : accessor_type(a), m_handle(p), m_mapping(m) {
    }
    //
    // convenience constructor when mapping_type is constructible from extents
    // (e.g. layout_right/layout_left)
    template <
        typename M = mapping_type,
        typename = typename std::enable_if<std::is_constructible<M, extents_type>::value>::type>
    KU_DEVICE_HOST
    KuMdSpan(data_handle_type p, const extents_type &ex, accessor_type a = accessor_type()) noexcept
        : accessor_type(a), m_handle(p), m_mapping(ex) {
    }

    template <class... Idx>
    KU_DEVICE_HOST KuMdSpan(data_handle_type p, Idx... ids) noexcept
        : accessor_type(accessor_type()), m_handle(p),
          m_mapping(extents_type{static_cast<size_type>(ids)...}) {
    }

    // operator() for N indices
    template <typename... Idx>
    KU_DEVICE_HOST const_reference operator()(Idx... idx) const noexcept {
        static_assert(sizeof...(Idx) == extents_type::rank(),
                      "KuMdSpanNew: wrong number of indices");
        auto offset = m_mapping(static_cast<size_type>(idx)...);
        return this->access(m_handle, static_cast<std::size_t>(offset));
    }

    template <typename... Idx>
    KU_DEVICE_HOST reference operator()(Idx... idx) noexcept {
        static_assert(sizeof...(Idx) == extents_type::rank(),
                      "KuMdSpanNew: wrong number of indices");
        auto offset = m_mapping(static_cast<size_type>(idx)...);
        auto ext = m_mapping.extents();
        return this->access(m_handle, static_cast<std::size_t>(offset));
    }

    // extent queries
    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        return m_mapping.extents().extent(r);
    }

    KU_DEVICE_HOST static constexpr std::size_t rank() noexcept {
        return extents_type::rank();
    }

    KU_DEVICE_HOST constexpr size_type size() const noexcept {
        return m_mapping.extents().prod();
    }

    KU_DEVICE_HOST static constexpr bool is_always_unique() noexcept {
        return mapping_type::is_always_unique();
    }

    KU_DEVICE_HOST static constexpr bool is_always_exhaustive() noexcept {
        return mapping_type::is_always_exhaustive();
    }

    KU_DEVICE_HOST static constexpr bool is_always_strided() noexcept {
        return mapping_type::is_always_strided();
    }

    KU_DEVICE_HOST constexpr bool is_unique() const noexcept {
        return m_mapping.is_unique();
    }

    KU_DEVICE_HOST constexpr bool is_exhaustive() const noexcept {
        return m_mapping.is_exhaustive();
    }

    KU_DEVICE_HOST constexpr bool is_strided() const noexcept {
        return m_mapping.is_strided();
    }

    KU_DEVICE_HOST data_handle_type data_handle() const noexcept {
        return m_handle;
    }
    KU_DEVICE_HOST const mapping_type &mapping() const noexcept {
        return m_mapping;
    }
    KU_DEVICE_HOST const accessor_type &accessor() const noexcept {
        return *this;
    }

private:
    data_handle_type m_handle;
    mapping_type     m_mapping{};
};

template <class T, class MappingType>
KuMdSpan(T *, const MappingType &)
    -> KuMdSpan<T, typename MappingType::extents_type, typename MappingType::layout_policy>;

template <class T, class E, class L, class A, class... SliceArgs>
auto submdspan(const KuMdSpan<T, E, L, A> &span, SliceArgs... args) {
    // create a subspan with the same data handle and mapping
    using MdSpan = KuMdSpan<T, E, L, A>;
    using mapping_type = typename MdSpan::mapping_type;
    constexpr size_t rank = MdSpan::rank();
    static_assert(sizeof...(args) <= rank, "submdspan: too many slice arguments");

    auto                     old_mapping = span.mapping();
    auto                     old_strides = old_mapping.strides().as_array();
    auto                     old_extents = old_mapping.extents().as_array();
    std::array<size_t, rank> new_extents{};
    std::array<size_t, rank> new_strides = old_strides; // start with the old strides
    size_t                   ptr_offset = 0;

    size_t dim = 0;
    auto   process_dim = [&](auto spec) {
        using Spec = std::decay_t<decltype(spec)>;
        if constexpr (ku_is_instance_of_v<Spec, StridedSlice>) {
            size_t len =
                1 + (spec.m_extent - 1) / spec.m_stride; // compute the length of the strided slice
            ptr_offset +=
                spec.m_offset * old_strides[dim]; // compute the pointer offset based on the stride
            new_extents[dim] = len;
            new_strides[dim] *= spec.m_stride; // adjust the stride for the new slice
        } else if constexpr (std::is_same_v<Spec, KuFullExtent_t>) {
            new_extents[dim] = old_extents[dim];
        }
        ++dim;
    };

    (process_dim(args), ...);

    KuLayoutStride::mapping<E> mapping(
        E(new_extents),
        new_strides); // create a new mapping with the new extents and strides

    return KuMdSpan<T, E, KuLayoutStride, A>(span.data_handle() + ptr_offset, mapping,
                                             span.accessor());
}

} // namespace kuai
