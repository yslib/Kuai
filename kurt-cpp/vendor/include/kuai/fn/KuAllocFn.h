#pragma once
#include <kuai/core/KuContext.h>
#include <kuai/core/KuDeviceBuffer.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/ktl/KuView.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/fill.h"
namespace kuai {
template <typename Vendor, typename T>
class KuAllocFn {
public:
    using Storage = KuDeviceBuffer<T>;

    KuAllocFn(KuContext &context) : m_device(context) {
    }

    explicit KuAllocFn(const KuVendorContext<Vendor> &device) : m_device(device) {
    }

    auto operator()(ku_size_t size) {
        return storage(size);
    }

    auto operator()(ku_size_t size, ku_size_t capacity) {
        return storage(size, capacity);
    }

    template <typename IndexType, IndexType... StaticExts>
    auto operator()(KuExtents<IndexType, StaticExts...> exts) {
        auto size = exts.prod();
        return storage(size);
    }

    template <typename IndexType, IndexType... StaticExts>
    auto operator()(KuExtents<IndexType, StaticExts...> exts, ku_size_t lastDimCap)
        -> KuResult<KuDeviceBuffer<T>, ku_status_t> {
        auto size = exts.prod();
        auto capacity = storageCapacity(exts, lastDimCap);
        return storage(size, capacity);
    }

    template <typename IndexType, IndexType... StaticExts>
    auto operator()(KuExtents<IndexType, StaticExts...> exts, const T &value) {
        auto size = exts.prod();
        return filledStorage(size, size, value);
    }

    template <typename IndexType, IndexType... StaticExts>
    auto operator()(KuExtents<IndexType, StaticExts...> exts, ku_size_t lastDimCap, const T &value)
        -> KuResult<KuDeviceBuffer<T>, ku_status_t> {
        auto size = exts.prod();
        auto capacity = storageCapacity(exts, lastDimCap);
        return filledStorage(size, capacity, value);
    }

private:
    template <typename IndexType, IndexType... StaticExts>
    ku_size_t storageCapacity(KuExtents<IndexType, StaticExts...> exts, ku_size_t lastDimCap) {
        return exts.prod() / exts.extent(sizeof...(StaticExts) - 1) * lastDimCap;
    }

    KuResult<Storage, ku_status_t> storage(ku_size_t size) {
        return m_device.template createStorage<T>(size);
    }

    KuResult<Storage, ku_status_t> storage(ku_size_t size, ku_size_t capacity) {
        return m_device.template createStorage<T>(size, capacity);
    }

    KuResult<Storage, ku_status_t>
    filledStorage(ku_size_t size, ku_size_t capacity, const T &value) {
        auto result = storage(size, capacity);
        if (!result.hasValue()) {
            return result.error();
        }
        algo::fill_n(m_device, result->data(), size, value);
        return result;
    }

    KuVendorContext<Vendor> m_device;
};

} // namespace kuai
