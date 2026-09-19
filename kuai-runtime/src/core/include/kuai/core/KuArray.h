#pragma once
#include <string>
#include <utility>
#include <vector>

#include <kuai/core/KuObject.h>
namespace kuai {

class KuArray : public KuObject {
public:
    KU_RTTI_LEAF(KuArray, kArray)

    KuArray() : KuObject(kArray) {
    }
    KuArray(ku_size_t count) : KuObject(kArray), m_data(count) {
    }

    void append(ku_sp<KuObject> data) {
        m_data.emplace_back(std::move(data));
    }

    // Borrowed access. The returned object remains valid only while this array keeps it alive.
    KuObject *get(ku_size_t i) noexcept {
        return m_data[i].get();
    }

    const KuObject *get(ku_size_t i) const noexcept {
        return m_data[i].get();
    }

    // Owning access. The returned pointer keeps the element alive independently of this array.
    ku_sp<KuObject> retain(ku_size_t i) const noexcept {
        return m_data[i];
    }

    ku_size_t rows() const {
        return size();
    }

    void set(ku_size_t i, ku_sp<KuObject> data) noexcept {
        m_data[i] = std::move(data);
    }

    // return the count of elements in the array
    ku_size_t size() const {
        return m_data.size();
    }

    // resize the array
    void resize(ku_size_t newSize) {
        m_data.resize(newSize);
    }

    // reserve the array
    void reserve(ku_size_t newCapacity) {
        m_data.reserve(newCapacity);
    }

    ku_size_t capacity() const {
        return m_data.capacity();
    }

private:
    std::vector<ku_sp<KuObject>> m_data;
};

} // namespace kuai
