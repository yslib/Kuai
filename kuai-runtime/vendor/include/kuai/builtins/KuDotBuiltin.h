#pragma once

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/fn/KuDotFn.h"

namespace kuai {

template <typename Vendor>
KuBuiltinResult dotBuiltin(KuContext &context, KuObject &left, KuObject &right) {
    return KuDotFn<Vendor>(context)(left, right);
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_dot                      \
    KU_FUNC(m) {                                     \
        using namespace kuai;                        \
        m.def<&dotBuiltin<vendor::KuVendor>>("dot"); \
    }
