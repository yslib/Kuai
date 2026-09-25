#include <kuai/core/KuObject.h>
#include <kuai/kuai_c/KuCHandle.h>
#include <kuai/kuai_c/ku_completion.h>
#include <kuai/kuai_c/ku_object.h>
#include <kuai/runtime/KuCompletion.h>

#include "kuai_c/KuCApiMethod.h"
#include "kuai_c/KuCHandleImpl.h"

KU_C_REF_COUNTED_LIFETIME_IMPL(ku_completion_retain, ku_completion_release, ku_completion_t)
KU_C_REF_COUNTED_LIFETIME_IMPL(ku_object_retain, ku_object_release, ku_object_t)

extern "C" ku_status_t ku_completion_wait(ku_completion_t completion) {
    KU_ASSERT(completion != nullptr, "ku_completion_wait requires a non-null completion");
    return kuai::capi::invokeBoundary([&] { return kuai::capi::fromHandle(completion)->wait(); });
}

extern "C" ku_status_t ku_completion_on_completion(ku_completion_t          completion,
                                                   ku_completion_callback_t callback,
                                                   void                    *userData) {
    KU_ASSERT(completion != nullptr, "ku_completion_on_completion requires a non-null completion");
    KU_ASSERT(callback != nullptr, "ku_completion_on_completion requires a non-null callback");
    return kuai::capi::invokeBoundary(
        [&] { return kuai::capi::fromHandle(completion)->onCompletion(callback, userData); });
}
