#include "runtime/KuBuiltinRegistry.h"

#include <functional>
#include <new>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

namespace kuai {
namespace {

struct KuStringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};

class KuRegisteredBuiltin final {
public:
    explicit KuRegisteredBuiltin(const ku_builtin_registration_t &registration) noexcept
        : m_registration(registration) {
    }

    KuRegisteredBuiltin(KuRegisteredBuiltin &&other) noexcept
        : m_registration(other.m_registration),
          m_ownsState(std::exchange(other.m_ownsState, false)) {
    }

    KuRegisteredBuiltin &operator=(KuRegisteredBuiltin &&other) noexcept {
        if (this != &other) {
            destroy();
            m_registration = other.m_registration;
            m_ownsState = std::exchange(other.m_ownsState, false);
        }
        return *this;
    }

    ~KuRegisteredBuiltin() {
        destroy();
    }

    KuRegisteredBuiltin(const KuRegisteredBuiltin &) = delete;
    KuRegisteredBuiltin &operator=(const KuRegisteredBuiltin &) = delete;

    void adoptState() noexcept {
        m_ownsState = m_registration.state != nullptr;
    }

    [[nodiscard]] const ku_builtin_registration_t &registration() const noexcept {
        return m_registration;
    }

private:
    void destroy() noexcept {
        if (!m_ownsState) {
            return;
        }
        m_ownsState = false;
        try {
            m_registration.destroy(m_registration.state);
        } catch (...) {
        }
    }

    ku_builtin_registration_t m_registration{};
    bool                      m_ownsState{false};
};

struct KuBuiltinGroup {
    std::vector<KuRegisteredBuiltin> m_records;
};

bool isValidTarget(const ku_call_target_t &target) noexcept {
    switch (target.kind) {
        case KU_CALL_TARGET_FFI:
            return target.value.ffi != nullptr;
        case KU_CALL_TARGET_CALLABLE:
            return target.value.closure.ffi != nullptr;
        default:
            return false;
    }
}

ku_status_t invokeCallTarget(const ku_call_target_t &target, ku_frame_t *frame) {
    if (frame == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    switch (target.kind) {
        case KU_CALL_TARGET_FFI:
            if (target.value.ffi != nullptr) {
                return target.value.ffi(frame);
            }
            break;
        case KU_CALL_TARGET_CALLABLE:
            if (target.value.closure.ffi != nullptr) {
                return target.value.closure.ffi(target.value.closure.capture, frame);
            }
            break;
        default:
            break;
    }

    frame->result_count = 0;
    return KU_STATUS_INVALID_ARGUMENT;
}

ku_status_t dispatchBuiltinGroup(void *capture, ku_frame_t *frame) noexcept {
    if (capture == nullptr || frame == nullptr) {
        if (frame != nullptr) {
            frame->result_count = 0;
        }
        return KU_STATUS_INVALID_ARGUMENT;
    }

    const auto                &group = *static_cast<const KuBuiltinGroup *>(capture);
    const KuRegisteredBuiltin *selected = nullptr;
    ku_builtin_match_rank_t    selectedRank = KU_BUILTIN_NO_MATCH;
    bool                       ambiguous = false;
    for (const auto &record : group.m_records) {
        const auto &registration = record.registration();
        const auto  rank = registration.match(registration.state, frame);
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
    return invokeCallTarget(selected->registration().target, frame);
}

} // namespace

class KuBuiltinRegistry::Impl {
    KU_DECL_API(KuBuiltinRegistry)

public:
    using GroupMap = std::
        unordered_map<std::string, std::unique_ptr<KuBuiltinGroup>, KuStringHash, std::equal_to<>>;

    explicit Impl(KuBuiltinRegistry *api) : q_ptr(api) {
    }

    ku_status_t add(const ku_builtin_registration_t &registration) noexcept {
        if (registration.name.data == nullptr || registration.name.size == 0
            || registration.overload_key == nullptr || registration.match == nullptr
            || !isValidTarget(registration.target)
            || (registration.state == nullptr) != (registration.destroy == nullptr)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }

        try {
            const std::string_view name(registration.name.data, registration.name.size);
            const auto             found = m_groups.find(name);
            if (found != m_groups.end()) {
                for (const auto &record : found->second->m_records) {
                    if (record.registration().overload_key == registration.overload_key) {
                        return KU_STATUS_INVALID_ARGUMENT;
                    }
                }
                found->second->m_records.emplace_back(registration);
                found->second->m_records.back().adoptState();
                return KU_STATUS_SUCCESS;
            }

            auto group = std::make_unique<KuBuiltinGroup>();
            group->m_records.emplace_back(registration);
            auto [position, inserted] = m_groups.emplace(std::string(name), std::move(group));
            if (!inserted) {
                return KU_STATUS_INVALID_ARGUMENT;
            }
            position->second->m_records.back().adoptState();
            return KU_STATUS_SUCCESS;
        } catch (const std::bad_alloc &) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            return KU_STATUS_INTERNAL_ERROR;
        }
    }

    GroupMap m_groups;
};

KuBuiltinRegistry::KuBuiltinRegistry() : d_ptr(std::make_unique<Impl>(this)) {
}

KuBuiltinRegistry::~KuBuiltinRegistry() = default;

ku_builtin_builder_t KuBuiltinRegistry::builder() noexcept {
    return ku_builtin_builder_t{.context = this, .add = &KuBuiltinRegistry::add};
}

std::vector<std::string_view> KuBuiltinRegistry::functionNames() const {
    return d_ptr->m_groups | std::views::keys | std::ranges::to<std::vector<std::string_view>>();
}

std::optional<ku_call_target_t>
KuBuiltinRegistry::getCallTarget(std::string_view name) const noexcept {
    const auto found = d_ptr->m_groups.find(name);
    if (found == d_ptr->m_groups.end() || found->second->m_records.empty()) {
        return std::nullopt;
    }

    ku_call_target_t target{};
    target.kind = KU_CALL_TARGET_CALLABLE;
    target.value.closure =
        ku_closure_t{.capture = found->second.get(), .ffi = &dispatchBuiltinGroup};
    return target;
}

ku_status_t KuBuiltinRegistry::add(void                            *context,
                                   const ku_builtin_registration_t *registration) noexcept {
    if (context == nullptr || registration == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    return static_cast<KuBuiltinRegistry *>(context)->d_ptr->add(*registration);
}

} // namespace kuai
