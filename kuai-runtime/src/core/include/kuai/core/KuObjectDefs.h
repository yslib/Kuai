#pragma once

#define KU_PP_CAT_IMPL(a, b) a##b
#define KU_PP_CAT(a, b)      KU_PP_CAT_IMPL(a, b)

#define KU_PP_CAT3_IMPL(a, b, c) a##b##c
#define KU_PP_CAT3(a, b, c)      KU_PP_CAT3_IMPL(a, b, c)

#define KU_NODE_ITEMS(X)

#define KU_CALLABLE_ITEMS(X) X(KuCallableGraphKernel, 1)

#define KU_DEVICE_DATA_ITEMS(X) X(Tensor, 1001)

#define KU_OBJECT_USER_DEFINE_BEGIN_VALUE 4000

// (SECTION, PARENT_SECTION, BEGIN_VALUE, USER_BEGIN_VALUE, END_VALUE, ITEMS, CLASS_NAME)
#define KU_OBJECT_SECTIONS(SECTION)                                                \
    SECTION(CALLABLE, ROOT, 0, 500, 1000, KU_CALLABLE_ITEMS, Callable)             \
    SECTION(DEVICE_DATA, ROOT, 1000, 1500, 2000, KU_DEVICE_DATA_ITEMS, DeviceData) \
    SECTION(NODE, ROOT, 2000, 2500, 3000, KU_NODE_ITEMS, Node)

#define KU_OTHER_ITEMS(X) \
    X(Scalar, 3001)       \
    X(Array, 3002)        \
    X(Iterator, 3003)     \
    X(FrameContext, 3004) \
    X(KuInstance, 3005)   \
    X(String, 3006)       \
    X(Slice, 3007)
