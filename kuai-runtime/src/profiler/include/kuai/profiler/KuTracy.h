#pragma once
inline constexpr int KURT_KERNEL_COLOR = 0xff6a6a;           // IndianRed1
inline constexpr int HOST_CODE_COLOR = 0x00bfff;             // DeepSkyBlue1
inline constexpr int KURT_DEVICE_COLOR = 0x228b22;           // ForestGreen
inline constexpr int MEMCPY_D2H_COLOR = 0xcd661d;            // Chocolate3
inline constexpr int MEMCPY_H2D_COLOR = 0xffff00;            // Yellow
inline constexpr int KURT_KERNEL_COLOR_UNALIGNED = 0x7d26cd; // Purple3

#ifdef KURT_ENABLE_TRACY
#include <common/TracyColor.hpp>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

#define KU_PROFILE_FRAME_BEGIN
#define KU_PROFILE_FRAME_END FrameMark

#define KU_ZONE_SCOPED                 ZoneScopedC(HOST_CODE_COLOR)
#define KU_ZONE_SCOPED_N(name)         ZoneScopedNC(name, HOST_CODE_COLOR)
#define KU_ZONE_SCOPED_NC(name, color) ZoneScopedNC(name, color)

#define KU_ZONE_NAME(txt, size) ZoneName(txt, size)
#define KU_ZONE_COLOR(c)        ZoneColor(c)

#define KU_TRACE_MEM_ALLOC(ptr, bytes)         TracyAlloc(ptr, bytes)
#define KU_TRACE_MEM_FREE(ptr)                 TracyFree(ptr)
#define KU_TRACE_MEM_ALLOC_N(ptr, bytes, name) TracyAllocN(ptr, bytes, name)
#define KU_TRACE_MEM_FREE_N(ptr, name)         TracyFreeN(ptr, name)
#define KU_IIF(cond, trueVal, falseVal)        ((cond) ? (trueVal) : (falseVal))

#define KU_KERNEL_CALL_ZONE_SCOPED(name) KU_ZONE_SCOPED_NC(name, KURT_DEVICE_COLOR)

#else
#define KU_PROFILE_FRAME_BEGIN
#define KU_PROFILE_FRAME_END

#define KU_ZONE_SCOPED
#define KU_ZONE_COLOR(c)
#define KU_ZONE_SCOPED_N(name)
#define KU_ZONE_SCOPED_NC(name, color)
#define KU_ZONE_NAME(txt, size)

#define KU_TRACE_MEM_ALLOC(ptr, bytes)
#define KU_TRACE_MEM_FREE(ptr)
#define KU_TRACE_MEM_ALLOC_N(ptr, bytes, name)
#define KU_TRACE_MEM_FREE_N(ptr, name)
#define KU_IIF(cond, trueVal, falseVal)
#define KU_KERNEL_CALL_ZONE_SCOPED(name)

#endif

#define KU_IIF_ZONE_COLOR(cond, trueColor, falseColor) \
    KU_IIF(cond, KU_ZONE_COLOR(trueColor), KU_ZONE_COLOR(falseColor))
#define KU_IIF_ZONE_NAME(cond, trueName, trueNameSize, falseName, falseNameSize) \
    KU_IIF(cond, KU_ZONE_NAME(trueName, trueNameSize), KU_ZONE_NAME(falseName, falseNameSize))
