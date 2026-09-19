#pragma once

#include <array>
#include <cstdint>

#include <kuai/core/KuTensor.h>
#include <kuai/kuai_c/ku_tensor.h>

namespace kuai {

class KuDevice;

class KuDLPackBuilder final {
public:
    KuDLPackBuilder() = delete;

    static ku_status_t toDLPack(KuTensor &tensor, DLManagedTensorVersioned **out) noexcept;

private:
    struct ExportContext {
        DLManagedTensorVersioned               m_managed{};
        ku_sp<KuTensor>                        m_tensor;
        std::array<int64_t, KuTensor::MaxRank> m_shape{};
        std::array<int64_t, KuTensor::MaxRank> m_strides{};
    };

    static bool        toDataType(ku_primitive_type_t type, DLDataType &out) noexcept;
    static bool        fillDevice(const KuTensor &tensor, DLDevice &out) noexcept;
    static void        deleteExport(DLManagedTensorVersioned *managed) noexcept;
    static ku_status_t fillExport(ExportContext &context) noexcept;
};

class KuTensorViewBuilder final {
public:
    KuTensorViewBuilder() = delete;

    static ku_status_t
    fromDLPack(KuDevice &device, DLManagedTensorVersioned *managed, ku_sp<KuTensor> &out) noexcept;

private:
    class ImportOwner final {
    public:
        void adopt(DLManagedTensorVersioned *managed) noexcept {
            m_managed = managed;
        }

        ~ImportOwner() {
            if (m_managed != nullptr && m_managed->deleter != nullptr) {
                m_managed->deleter(m_managed);
            }
        }

    private:
        DLManagedTensorVersioned *m_managed = nullptr;
    };

    static bool  fromDataType(const DLDataType &type, ku_primitive_type_t &out) noexcept;
    static bool  requiresExplicitStrides(const DLPackVersion &version) noexcept;
    static bool  checkedExtent(int64_t extent, ku_size_t &out) noexcept;
    static void *offsetData(const DLTensor &tensor) noexcept;
    static ku_status_t
    importTensor(KuDevice &device, DLManagedTensorVersioned *managed, ku_sp<KuTensor> &out);
};

class KuTensorCreateBuilder final {
public:
    KuTensorCreateBuilder() = delete;

    static ku_status_t
    create(KuDevice &device, const ku_tensor_create_desc_t &desc, ku_sp<KuTensor> &out) noexcept;

private:
    using Shape = std::array<ku_size_t, KuTensor::MaxRank>;

    static bool
    checkedShape(const ku_tensor_create_desc_t &desc, Shape &out, ku_size_t &size) noexcept;
    static bool hasSupportedLayout(const ku_tensor_create_desc_t &desc,
                                   const Shape                   &shape) noexcept;
};

} // namespace kuai
