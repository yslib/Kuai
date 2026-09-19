#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>

#include <kuai/core/KuObject.h>
#include <kuai/core/KuPointer.h>
#include <kuai/core/KuTypes.h>
namespace kuai {
class KuDevice;
class KuDeviceData;
class KuTensor;

class KuDeviceData : public KuObject {
public:
    KU_RTTI_RANGE(KuDeviceData, KU_DEVICE_DATA_BEGIN, KU_DEVICE_DATA_END)

    ~KuDeviceData() override = default;

    [[nodiscard]] KuDevice &getDevice() const noexcept {
        return *m_device;
    }

protected:
    KuDeviceData(KuObjectKind type, KuDevice &device) : KuObject(type), m_device(&device) {
    }

    KuDeviceData(const KuDeviceData &) = default;
    KuDeviceData(KuDeviceData &&) noexcept = default;

private:
    // Non-owning. Instance-owned devices are stable for the instance lifetime.
    KuDevice *const m_device;
};

} // namespace kuai
