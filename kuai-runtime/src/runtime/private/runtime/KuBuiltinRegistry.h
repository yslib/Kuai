#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/ku_builtin.h>

namespace kuai {

class KuBuiltinRegistry final {
    KU_DECL_IMPL()

public:
    KuBuiltinRegistry();
    ~KuBuiltinRegistry();

    KuBuiltinRegistry(const KuBuiltinRegistry &) = delete;
    KuBuiltinRegistry &operator=(const KuBuiltinRegistry &) = delete;

    [[nodiscard]] ku_builtin_builder_t          builder() noexcept;
    [[nodiscard]] std::vector<std::string_view> functionNames() const;
    [[nodiscard]] std::optional<ku_call_target_t>
    getCallTarget(std::string_view name) const noexcept;

private:
    static ku_status_t add(void *context, const ku_builtin_registration_t *registration) noexcept;
};

} // namespace kuai
