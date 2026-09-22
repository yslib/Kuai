#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>
#include <utility>

#include <kuai/ktl/KuResult.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

template <typename HandleType, typename CreateFn, typename DestroyFn>
class KuHandlePool {
    struct Key {
        ku_device_id_t  m_device;
        ku_stream_t     m_stream;
        std::thread::id m_thread;

        bool operator==(const Key &) const noexcept = default;
    };

    struct KeyHash {
        std::size_t operator()(const Key &key) const noexcept {
            auto hash = std::hash<ku_device_id_t>{}(key.m_device);
            hash ^=
                std::hash<ku_stream_t>{}(key.m_stream) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
            hash ^= std::hash<std::thread::id>{}(key.m_thread) + 0x9e3779b9U + (hash << 6U)
                    + (hash >> 2U);
            return hash;
        }
    };

    struct Entry {
        explicit Entry(HandleType value) noexcept : m_handle(value) {
        }

        HandleType m_handle;
        std::mutex m_mutex;
    };

public:
    class Lease {
    public:
        explicit Lease(Entry &entry) : m_entry(&entry), m_lock(entry.m_mutex) {
        }

        Lease(const Lease &) = delete;
        Lease &operator=(const Lease &) = delete;
        Lease(Lease &&) noexcept = default;
        Lease &operator=(Lease &&) noexcept = default;

        [[nodiscard]] HandleType get() const noexcept {
            return m_entry->m_handle;
        }

    private:
        Entry                       *m_entry;
        std::unique_lock<std::mutex> m_lock;
    };

    KuHandlePool(CreateFn create, DestroyFn destroy)
        : m_create(std::move(create)), m_destroy(std::move(destroy)) {
    }

    KuHandlePool(const KuHandlePool &) = delete;
    KuHandlePool &operator=(const KuHandlePool &) = delete;
    KuHandlePool(KuHandlePool &&) = delete;
    KuHandlePool &operator=(KuHandlePool &&) = delete;

    ~KuHandlePool() {
        for (auto &[key, entry] : m_entries) {
            std::lock_guard entryLock(entry->m_mutex);
            std::invoke(m_destroy, key.m_device, entry->m_handle);
        }
    }

    [[nodiscard]] KuResult<Lease, ku_status_t> acquire(ku_device_id_t device, ku_stream_t stream) {
        const Key key{device, stream, std::this_thread::get_id()};
        Entry    *entry = nullptr;
        {
            std::lock_guard poolLock(m_mutex);
            const auto      found = m_entries.find(key);
            if (found != m_entries.end()) {
                entry = found->second.get();
            } else {
                HandleType handle{};
                const auto status = std::invoke(m_create, device, stream, &handle);
                if (status != KU_STATUS_SUCCESS) {
                    return status;
                }
                try {
                    auto insertedEntry = std::make_unique<Entry>(handle);
                    entry = insertedEntry.get();
                    m_entries.emplace(key, std::move(insertedEntry));
                } catch (const std::bad_alloc &) {
                    std::invoke(m_destroy, device, handle);
                    return KU_STATUS_OUT_OF_HOST_MEMORY;
                } catch (...) {
                    std::invoke(m_destroy, device, handle);
                    return KU_STATUS_INTERNAL_ERROR;
                }
            }
        }
        return Lease(*entry);
    }

private:
    std::unordered_map<Key, std::unique_ptr<Entry>, KeyHash> m_entries;
    std::mutex                                               m_mutex;
    CreateFn                                                 m_create;
    DestroyFn                                                m_destroy;
};

} // namespace kuai
