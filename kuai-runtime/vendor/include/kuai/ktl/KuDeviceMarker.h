#pragma once

// Internal compiler-dialect normalization. kuai vendor code should use
// KU_DEVICE, KU_HOST, KU_GLOBAL, and KU_DEVICE_HOST rather than spelling a
// backend compiler's attributes directly.
#ifndef KU_DETAIL_DEVICE_COMPILER_CUDA
#if defined(__CUDACC__)
#define KU_DETAIL_DEVICE_COMPILER_CUDA 1
#else
#define KU_DETAIL_DEVICE_COMPILER_CUDA 0
#endif
#endif

// These value macros describe the compiler pass currently parsing this code,
// not the selected backend or the eventual execution location of a function.
#if defined(__CUDA_ARCH__)
#define KU_DEVICE_COMPILE_PASS 1
#define KU_HOST_COMPILE_PASS   0
#else
#define KU_DEVICE_COMPILE_PASS 0
#define KU_HOST_COMPILE_PASS   1
#endif

#if KU_DETAIL_DEVICE_COMPILER_CUDA
#define KU_DEVICE      __device__
#define KU_HOST        __host__
#define KU_GLOBAL      __global__
#define KU_DEVICE_HOST KU_HOST KU_DEVICE
#else
#define KU_DEVICE
#define KU_HOST
#define KU_GLOBAL
#define KU_DEVICE_HOST
#endif
