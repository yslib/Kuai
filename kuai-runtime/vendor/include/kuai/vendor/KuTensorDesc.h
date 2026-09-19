#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuMdSpan.h>

namespace kuai {

class KuCapacity {
public:
    explicit constexpr KuCapacity(ku_size_t extent) noexcept : m_extent(extent) {
    }

    constexpr ku_size_t extent() const noexcept {
        return m_extent;
    }

private:
    ku_size_t m_extent;
};

template <typename Extents>
class KuTensorDesc {
public:
    using extents_type = Extents;

    static constexpr std::size_t rank() noexcept {
        return extents_type::rank();
    }

    explicit KuTensorDesc(extents_type extents)
        : m_extents(std::move(extents)), m_capacityExtent(defaultCapacityExtent(m_extents)) {
    }

    template <std::size_t Rank = rank(), typename = std::enable_if_t<(Rank > 0)>>
    KuTensorDesc(extents_type extents, KuCapacity capacity)
        : m_extents(std::move(extents)), m_capacityExtent(capacity.extent()) {
        if (m_capacityExtent < static_cast<ku_size_t>(m_extents.extent(rank() - 1))) {
            throw std::invalid_argument(
                "tensor capacity must not be smaller than the last shape extent");
        }
    }

    const extents_type &extents() const noexcept {
        return m_extents;
    }

    ku_size_t capacityExtent() const noexcept {
        return m_capacityExtent;
    }

    ku_size_t size() const noexcept {
        return static_cast<ku_size_t>(m_extents.prod());
    }

    ku_size_t capacity() const {
        if constexpr (rank() == 0) {
            return 1;
        } else {
            const auto prefix = static_cast<ku_size_t>(m_extents.fwd_prod_of_extents(rank() - 1));
            if (m_capacityExtent != 0
                && prefix > std::numeric_limits<ku_size_t>::max() / m_capacityExtent) {
                throw std::invalid_argument("tensor capacity overflows ku_size_t");
            }
            return prefix * m_capacityExtent;
        }
    }

private:
    static ku_size_t defaultCapacityExtent(const extents_type &extents) noexcept {
        if constexpr (rank() == 0) {
            return 1;
        } else {
            return static_cast<ku_size_t>(extents.extent(rank() - 1));
        }
    }

    extents_type m_extents;
    ku_size_t    m_capacityExtent;
};

template <typename Extents>
KuTensorDesc(Extents) -> KuTensorDesc<Extents>;

template <typename Extents>
KuTensorDesc(Extents, KuCapacity) -> KuTensorDesc<Extents>;

} // namespace kuai
