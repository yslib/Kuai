#pragma once

#include <kuai/profiler/KuTracy.h>

#define KU_PROFILER_NULL nullptr

#if defined(KURT_ENABLE_TRACY) && defined(KURT_DEVICE_CUDA) && __CUDACC_VER_MAJOR__ == 12 \
    && __CUDACC_VER_MINOR__ == 9

#include <tracy/TracyCUDA.hpp>

using KuDeviceProfilerContext_t = tracy::CUDACtx *;
#define KU_CREATE_DEVICE_PROFILE_CONTEXT()     tracy::CUDACtx::Create();
#define KU_DESTROY_DEVICE_PROFILE_CONTEXT(ctx) tracy::CUDACtx::Destroy(ctx);
#define KU_PROFILE_DEVICE_FRAME_BEGIN(ctx)     TracyCUDAStartProfiling(ctx);
#define KU_PROFILE_DEVICE_FRAME_END(ctx)       TracyCUDAStopProfiling(ctx);

#else

using KuDeviceProfilerContext_t = void *;
#define KU_CREATE_DEVICE_PROFILE_CONTEXT() KU_PROFILER_NULL;
#define KU_DESTROY_DEVICE_PROFILE_CONTEXT(ctx)
#define KU_PROFILE_DEVICE_FRAME_BEGIN(ctx)
#define KU_PROFILE_DEVICE_FRAME_END(ctx)
#endif
