#pragma once

#include <kuai/core/KuObject.h>
#include <kuai/kuai_c/ku_builtin.h>
namespace kuai {

class KuCallable : public KuObject {
public:
    KU_RTTI_RANGE(KuCallable, KU_CALLABLE_BEGIN, KU_CALLABLE_END)

    // The caller supplies a complete invocation frame, including its borrowed
    // runtime context and result storage.
    virtual ku_status_t call(ku_frame_t *frame) noexcept = 0;
    virtual ~KuCallable() = default;

protected:
    explicit KuCallable(KuObjectKind type) : KuObject(type) {
    }
};
} // namespace kuai
