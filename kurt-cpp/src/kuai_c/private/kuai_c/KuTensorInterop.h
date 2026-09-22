#pragma once

#include <array>
#include <cstdint>

#include <kuai/core/KuTensor.h>
#include <kuai/kuai_c/ku_tensor.h>

namespace kuai {

class KuDevice;

class KuTensorCreateBuilder final {
public:
    KuTensorCreateBuilder() = delete;

    static ku_status_t
    create(KuDevice &device, const ku_tensor_create_desc_t &desc, ku_sp<KuTensor> &out) noexcept;

private:
    using Shape = std::array<ku_size_t, KuTensor::MaxRank>;

    static bool
    checkedShape(const ku_tensor_create_desc_t &desc, Shape &out, ku_size_t &size) noexcept;
    static bool hasSupportedLayout(const ku_tensor_create_desc_t &desc,
                                   const Shape                   &shape) noexcept;
};

} // namespace kuai
