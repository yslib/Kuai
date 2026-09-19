#pragma once

#include <memory>

#include <kuai/core/KuHostTransfer.h>

namespace kuai {

class KuDevice;

namespace detail {

ku_status_t createDefaultHostTransfer(const ku_instance_capabilities_t &capabilities,
                                      KuDevice                         &device,
                                      std::unique_ptr<KuHostTransfer>  &out) noexcept;

} // namespace detail
} // namespace kuai
