#pragma once

#include <cstddef>
#include <utility>

#include <kuai/core/KuDeviceBuffer.h>
#include <kuai/vendor/KuVendorContext.h>

namespace kuai::vendor::cuda::detail {

struct KuCubIdentity {
    template <typename T>
    KU_DEVICE_HOST constexpr T operator()(T value) const {
        return value;
    }
};

template <typename Vendor, typename Operation>
ku_status_t invokeCub(const KuVendorContext<Vendor> &context, Operation &&operation) {
    std::size_t temporaryBytes = 0;
    auto        nativeStatus = operation(nullptr, temporaryBytes);
    const auto  queryStatus = Vendor::toStatus(nativeStatus);
    if (queryStatus != KU_STATUS_SUCCESS) {
        return queryStatus;
    }

    KuDeviceBuffer<std::byte> temporary;
    const auto allocationSize = temporaryBytes == 0 ? std::size_t{1} : temporaryBytes;
    const auto allocationStatus = KuDeviceBuffer<std::byte>::allocate(
        *context.m_resource, context.m_stream, allocationSize, temporary);
    if (allocationStatus != KU_STATUS_SUCCESS) {
        return allocationStatus;
    }

    return Vendor::toStatus(operation(temporary.data(), temporaryBytes));
}

} // namespace kuai::vendor::cuda::detail
