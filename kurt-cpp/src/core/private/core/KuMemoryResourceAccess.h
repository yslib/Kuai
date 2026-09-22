#pragma once

#include <kuai/core/KuMemoryResource.h>

namespace kuai::detail {

class KuMemoryResourceAccess final {
public:
    static ku_status_t shutdown(KuMemoryResource &resource) noexcept {
        return resource.shutdown();
    }
};

} // namespace kuai::detail
