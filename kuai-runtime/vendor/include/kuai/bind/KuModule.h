#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>

#include <kuai/bind/KuBind.h>

namespace kuai::bind {

namespace detail {

class KuRegistrationError final : public std::runtime_error {
public:
    explicit KuRegistrationError(ku_status_t status)
        : std::runtime_error("failed to register kuai builtin"), m_status(status) {
    }

    [[nodiscard]] ku_status_t status() const noexcept {
        return m_status;
    }

private:
    ku_status_t m_status;
};

} // namespace detail

class KuModule {
public:
    explicit KuModule(ku_builtin_builder_t builder) : m_builder(builder) {
        if (m_builder.add == nullptr) {
            throw std::invalid_argument("KuModule requires a valid builtin builder");
        }
    }

    template <typename Fn, typename... Extra>
    KuModule &def(const char *name, Fn &&fn, Extra &&...extra) {
        static_assert((detail::ku_is_function_annotation_v<Extra> && ...),
                      "KuModule::def only accepts kuai argument annotations after the handler");
        if (name == nullptr || name[0] == '\0') {
            throw std::invalid_argument("KuModule::def requires a non-empty builtin name");
        }

        auto       registration = detail::makeKuFunctionRegistration(name, std::forward<Fn>(fn),
                                                                     std::forward<Extra>(extra)...);
        const auto status = m_builder.add(m_builder.context, &registration.get());
        if (status != KU_STATUS_SUCCESS) {
            throw detail::KuRegistrationError(status);
        }
        registration.releaseState();
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

private:
    ku_builtin_builder_t m_builder;
};
} // namespace kuai::bind
