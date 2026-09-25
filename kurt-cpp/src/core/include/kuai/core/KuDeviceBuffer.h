#pragma once

#include <cstddef>
#include <limits>
#include <utility>

#include <kuai/core/KuMemoryResource.h>

namespace kuai {

template <typename T>
class KuDeviceBuffer {
public:
    using value_type = T;
    using size_type = ku_size_t;

    KuDeviceBuffer() noexcept = default;

    KuDeviceBuffer(const KuDeviceBuffer &) = delete;
    KuDeviceBuffer &operator=(const KuDeviceBuffer &) = delete;

    KuDeviceBuffer(KuDeviceBuffer &&other) noexcept
        : m_resource(std::exchange(other.m_resource, nullptr)),
          m_stream(std::exchange(other.m_stream, nullptr)),
          m_data(std::exchange(other.m_data, nullptr)), m_size(std::exchange(other.m_size, 0)) {
    }

    KuDeviceBuffer &operator=(KuDeviceBuffer &&other) noexcept {
        if (this != &other) {
            release();
            m_resource = std::exchange(other.m_resource, nullptr);
            m_stream = std::exchange(other.m_stream, nullptr);
            m_data = std::exchange(other.m_data, nullptr);
            m_size = std::exchange(other.m_size, 0);
        }
        return *this;
    }

    ~KuDeviceBuffer() noexcept {
        release();
    }

    static ku_status_t allocate(KuMemoryResource &resource,
                                ku_stream_t       stream,
                                size_type         size,
                                KuDeviceBuffer   &out) noexcept {
        KU_ASSERT(stream != nullptr, "device buffer allocation requires a non-null stream");
        out = KuDeviceBuffer();
        if (size == 0) {
            return KU_STATUS_SUCCESS;
        }
        constexpr auto alignmentSlack = KuMemoryResource::DefaultAlignment - 1;
        constexpr auto maxBytes = std::numeric_limits<std::size_t>::max() - alignmentSlack;
        if (size > maxBytes / sizeof(T)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }

        void      *data = nullptr;
        const auto status =
            resource.allocate(static_cast<std::size_t>(size) * sizeof(T), stream, &data);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        out.m_resource = &resource;
        out.m_stream = stream;
        out.m_data = static_cast<T *>(data);
        out.m_size = size;
        return KU_STATUS_SUCCESS;
    }

    [[nodiscard]] T *data() noexcept {
        return m_data;
    }

    [[nodiscard]] const T *data() const noexcept {
        return m_data;
    }

    [[nodiscard]] size_type size() const noexcept {
        return m_size;
    }

    [[nodiscard]] bool empty() const noexcept {
        return m_size == 0;
    }

private:
    void release() noexcept {
        if (m_data != nullptr) {
            m_resource->deallocate(m_data, static_cast<std::size_t>(m_size) * sizeof(T), m_stream);
        }
        m_resource = nullptr;
        m_stream = nullptr;
        m_data = nullptr;
        m_size = 0;
    }

    KuMemoryResource *m_resource = nullptr;
    ku_stream_t       m_stream = nullptr;
    T                *m_data = nullptr;
    size_type         m_size = 0;
};

} // namespace kuai
