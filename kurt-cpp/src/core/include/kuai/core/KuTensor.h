#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/core/KuTypes.h>

namespace kuai {

using KuTensorOwner = std::shared_ptr<void>;
class KuDevice;

class KuTensor final : public KuDeviceData {
public:
    inline static constexpr std::size_t MaxRank = 8;

    KU_RTTI_LEAF(KuTensor, kTensor)

    static constexpr std::size_t maxRank() noexcept {
        return MaxRank;
    }

    KuTensor(KuDevice                  &device,
             ku_primitive_type_t        dataType,
             void                      *data,
             std::span<const ku_size_t> shape,
             ku_size_t                  capacity,
             KuTensorOwner              owner = {})
        : KuDeviceData(kTensor, device), m_data(data), m_capacity(capacity),
          m_owner(std::move(owner)), m_dataType(dataType) {
        initializeShape(shape);
        initializeCanonicalStrides();
    }

    KuTensor(KuDevice                  &device,
             ku_primitive_type_t        dataType,
             void                      *data,
             std::span<const ku_size_t> shape,
             std::span<const ku_size_t> strides,
             ku_size_t                  capacity,
             KuTensorOwner              owner = {})
        : KuDeviceData(kTensor, device), m_data(data), m_capacity(capacity),
          m_owner(std::move(owner)), m_dataType(dataType) {
        initializeShape(shape);
        if (strides.size() != m_rank) {
            throw std::invalid_argument("tensor stride rank must match shape rank");
        }
        std::copy(strides.begin(), strides.end(), m_strides.begin());
    }

    ku_size_t rank() const noexcept {
        return m_rank;
    }

    std::span<const ku_size_t> shape() const noexcept {
        return {m_shape.data(), static_cast<std::size_t>(m_rank)};
    }

    std::span<const ku_size_t> strides() const noexcept {
        return {m_strides.data(), static_cast<std::size_t>(m_rank)};
    }

    ku_size_t extent(std::size_t index) const {
        if (index >= m_rank) {
            throw std::out_of_range("tensor extent index exceeds rank");
        }
        return m_shape[index];
    }

    ku_size_t stride(std::size_t index) const {
        if (index >= m_rank) {
            throw std::out_of_range("tensor stride index exceeds rank");
        }
        return m_strides[index];
    }

    ku_size_t size() const noexcept {
        return m_size;
    }

    ku_size_t capacity() const noexcept {
        return m_capacity;
    }

    template <typename T>
    T *begin() noexcept {
        return static_cast<T *>(data());
    }

    template <typename T>
    const T *begin() const noexcept {
        return static_cast<const T *>(data());
    }

    template <typename T>
    T *end() noexcept {
        return begin<T>() + size();
    }

    template <typename T>
    const T *end() const noexcept {
        return begin<T>() + size();
    }

    ku_primitive_type_t getType() const {
        return m_dataType;
    }

    ku_size_t elementBytes() const {
        return ku_primitive_type_size(getType());
    }

    ku_size_t bytes() const {
        return size() * elementBytes();
    }

    void *data() noexcept {
        return m_data;
    }

    const void *data() const noexcept {
        return m_data;
    }

    KuTensorOwner takeOwner() noexcept {
        return std::move(m_owner);
    }

    ~KuTensor() override = default;

private:
    void initializeShape(std::span<const ku_size_t> shape) {
        if (shape.size() > maxRank()) {
            throw std::invalid_argument("tensor rank exceeds the supported maximum");
        }
        m_rank = static_cast<ku_size_t>(shape.size());
        std::copy(shape.begin(), shape.end(), m_shape.begin());
        m_size = 1;
        for (const auto extentValue : shape) {
            if (extentValue != 0 && m_size > std::numeric_limits<ku_size_t>::max() / extentValue) {
                throw std::invalid_argument("tensor shape product overflows ku_size_t");
            }
            m_size *= extentValue;
        }
        if (m_size != 0 && m_data == nullptr) {
            throw std::invalid_argument("non-empty tensor requires a data pointer");
        }
        if (m_capacity < m_size) {
            throw std::invalid_argument("tensor capacity must not be smaller than its size");
        }
        const auto elementSize = ku_primitive_type_size(m_dataType);
        if (m_capacity > std::numeric_limits<ku_size_t>::max() / elementSize) {
            throw std::invalid_argument("tensor byte size overflows ku_size_t");
        }
    }

    void initializeCanonicalStrides() {
        if (m_rank == 0) {
            return;
        }
        m_strides[0] = 1;
        for (std::size_t i = 1; i < static_cast<std::size_t>(m_rank); ++i) {
            const auto previousExtent = m_shape[i - 1];
            const auto previousStride = m_strides[i - 1];
            if (previousExtent != 0
                && previousStride > std::numeric_limits<ku_size_t>::max() / previousExtent) {
                throw std::invalid_argument("tensor stride overflows ku_size_t");
            }
            m_strides[i] = previousStride * previousExtent;
        }
    }

    void                          *m_data = nullptr;
    std::array<ku_size_t, MaxRank> m_shape{};
    std::array<ku_size_t, MaxRank> m_strides{};
    ku_size_t                      m_rank = 0;
    ku_size_t                      m_size = 1;
    ku_size_t                      m_capacity = 1;
    KuTensorOwner                  m_owner;
    ku_primitive_type_t            m_dataType;
};

} // namespace kuai
