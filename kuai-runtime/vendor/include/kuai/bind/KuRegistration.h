#pragma once

#include <new>
#include <stdexcept>

#include <kuai/bind/KuModule.h>
#include <kuai/core/KuCore.h>
#include <kuai/ktl/KuDeviceMarker.h>

#define KU_VENDOR_BUILTINS_RECORD_START __start_ku_vendor_builtins_record
#define KU_VENDOR_BUILTINS_RECORD_STOP  __stop_ku_vendor_builtins_record
#define KU_VENDOR_BUILTINS_RECORD_NAME  "ku_vendor_builtins_record"

extern "C" {
extern const ku_vendor_builtin_record_t KU_VENDOR_BUILTINS_RECORD_START[];
extern const ku_vendor_builtin_record_t KU_VENDOR_BUILTINS_RECORD_STOP[];
}

namespace kuai::vendor {

template <void (*Loader)(bind::KuModule &)>
ku_status_t __ku_vendor_builtin_loader(const ku_builtin_builder_t *builder) noexcept {
    if (builder == nullptr || builder->add == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    try {
        bind::KuModule module(*builder);
        Loader(module);
        return KU_STATUS_SUCCESS;
    } catch (const ::kuai::detail::KuStatusError &error) {
        return error.status();
    } catch (const bind::detail::KuRegistrationError &error) {
        return error.status();
    } catch (const std::invalid_argument &) {
        return KU_STATUS_INVALID_ARGUMENT;
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

inline constexpr ku_vendor_builtin_record_t
__ku_vendor_make_builtin_record(const char *name, ku_vendor_builtin_loader_t loader) {
    return ku_vendor_builtin_record_t{.name = name, .loader = loader};
}

} // namespace kuai::vendor

// The linker section is host metadata. Emitting it during a device pass would
// place a host loader function pointer in device constant memory.
#if KU_DEVICE_COMPILE_PASS
#define KU_VENDOR_REGISTER_BUILTIN_RECORD(name, func)
#else
#define KU_VENDOR_REGISTER_BUILTIN_RECORD(name, func)                       \
    [[gnu::used, gnu::section(KU_VENDOR_BUILTINS_RECORD_NAME),              \
      gnu::aligned(alignof(ku_vendor_builtin_record_t))]] static const auto \
        KU_UNIQUE_NAME(_kuai_vendor_builtin_record_) =                      \
            ::kuai::vendor::__ku_vendor_make_builtin_record(                \
                #name, &::kuai::vendor::__ku_vendor_builtin_loader<func>);
#endif

#define __KU_VENDOR_MODULE_IN_SECTION(ident, variable)   \
    static void ident(::kuai::bind::KuModule &variable); \
    KU_VENDOR_REGISTER_BUILTIN_RECORD(kuvendor, ident)   \
    static void ident(::kuai::bind::KuModule &variable)

#define KU_FUNC(variable) \
    __KU_VENDOR_MODULE_IN_SECTION(KU_UNIQUE_NAME(_ku_vendor_builtin_init_), variable)
