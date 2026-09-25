#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <kuai/bind/KuBind.h>

namespace kuai::bind {

class KuModule {
public:
    template <typename Fn, typename... Extra>
    KuModule &def(const char *name, Fn &&fn, Extra &&...extra) {
        static_assert((detail::ku_is_function_annotation_v<Extra> && ...),
                      "KuModule::def only accepts kuai argument annotations after the handler");
        if (name == nullptr || name[0] == '\0') {
            throw std::invalid_argument("KuModule::def requires a non-empty builtin name");
        }

        auto registration =
            detail::makeKuFunctionRegistration(std::forward<Fn>(fn), std::forward<Extra>(extra)...);
        auto &group = m_builtins[name];
        for (const auto &existing : group) {
            if (existing.get().overloadKey == registration.get().overloadKey) {
                throw std::invalid_argument("duplicate builtin overload");
            }
        }
        group.push_back(std::move(registration));
        return *this;
    }

    // Encodes a free-function address in the handler type, for example
    // module.def<&handler>("handler").
    template <auto Fn, typename... Extra>
    KuModule &def(const char *name, Extra &&...extra) {
        static_assert(std::is_pointer_v<decltype(Fn)>
                          && std::is_function_v<std::remove_pointer_t<decltype(Fn)>>,
                      "KuModule::def<Fn> requires a free-function pointer");
        return def(name, detail::ku_nttp_handler<Fn>{}, std::forward<Extra>(extra)...);
    }

    ku_status_t
    getBuiltinInfo(ku_builtin_info_t *out, ku_size_t capacity, ku_size_t *outCount) const noexcept {
        KU_ASSERT(outCount != nullptr, "getBuiltinInfo requires a non-null count output");
        KU_ASSERT(capacity == 0 || out != nullptr,
                  "getBuiltinInfo requires storage when capacity is nonzero");
        *outCount = m_builtins.size();
        if (capacity == 0) {
            return KU_STATUS_SUCCESS;
        }
        if (capacity < m_builtins.size()) {
            return KU_STATUS_BUFFER_TOO_SMALL;
        }
        ku_size_t index = 0;
        for (const auto &[name, group] : m_builtins) {
            out[index++].name = ku_string_view_t{.data = name.data(), .size = name.size()};
        }
        return KU_STATUS_SUCCESS;
    }

    ku_status_t getProcAddress(ku_string_view_t name, ku_call_t *out) noexcept {
        KU_ASSERT(out != nullptr, "getProcAddress requires a non-null output slot");
        const auto found = m_builtins.find(std::string_view(name.data, name.size));
        if (found == m_builtins.end() || found->second.empty()) {
            return KU_STATUS_NOT_FOUND;
        }
        // unordered_map keeps group addresses stable across rehashing.
        out->kind = KU_CALL_CALLABLE;
        out->value.closure = ku_closure_t{.capture = &found->second, .ffi = &dispatchBuiltinGroup};
        return KU_STATUS_SUCCESS;
    }

private:
    using BuiltinGroup = std::vector<detail::KuFunctionRegistration>;

    struct StringHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    static ku_status_t dispatchBuiltinGroup(void *capture, ku_frame_t *frame) noexcept {
        if (capture == nullptr || frame == nullptr) {
            if (frame != nullptr) {
                frame->result_count = 0;
            }
            return KU_STATUS_INVALID_ARGUMENT;
        }

        const auto                     &group = *static_cast<const BuiltinGroup *>(capture);
        const detail::KuFunctionRecord *selected = nullptr;
        int                             selectedRank = -1;
        bool                            ambiguous = false;
        for (const auto &entry : group) {
            const auto &record = entry.get();
            const auto  rank = record.match(record.state, frame);
            if (rank < 0) {
                continue;
            }
            if (selected == nullptr || rank > selectedRank) {
                selected = &record;
                selectedRank = rank;
                ambiguous = false;
            } else if (rank == selectedRank) {
                ambiguous = true;
            }
        }
        if (selected == nullptr || ambiguous) {
            frame->result_count = 0;
            return KU_STATUS_INVALID_ARGUMENT;
        }

        const auto &target = selected->target;
        if (target.kind == KU_CALL_FFI) {
            return target.value.ffi(frame);
        }
        return target.value.closure.ffi(target.value.closure.capture, frame);
    }

    std::unordered_map<std::string, BuiltinGroup, StringHash, std::equal_to<>> m_builtins;
};

} // namespace kuai::bind
