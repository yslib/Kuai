#pragma once

#include <cstddef>

#include <kuai/core/KuPointer.h>
#include <kuai/core/KuTypes.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

class KuDevice;
class KuCompletion;

struct KuHostTransferInfo {
    std::size_t m_parallelism = 0;
    std::size_t m_stagingBytesPerLane = 0;
    std::size_t m_copyStreamCount = 0;
};

class KuHostTransfer {
public:
    KuHostTransfer(const KuHostTransfer &) = delete;
    KuHostTransfer &operator=(const KuHostTransfer &) = delete;
    KuHostTransfer(KuHostTransfer &&) = delete;
    KuHostTransfer &operator=(KuHostTransfer &&) = delete;

    virtual ~KuHostTransfer();

    ku_status_t copyAsync(void                *dst,
                          const void          *src,
                          ku_size_t            bytes,
                          ku_memcpy_kind_t     kind,
                          ku_stream_t          dependencyStream,
                          ku_sp<KuCompletion> &outCompletion) noexcept;

    [[nodiscard]] KuHostTransferInfo info() const noexcept {
        return m_info;
    }

    [[nodiscard]] KuDevice &getDevice() const noexcept {
        return m_device;
    }

    void waitIdle() noexcept {
        onWaitIdle();
    }

protected:
    KuHostTransfer(KuDevice &device, KuHostTransferInfo info) noexcept;

    virtual ku_status_t onCopyAsync(void            *dst,
                                    const void      *src,
                                    ku_size_t        bytes,
                                    ku_memcpy_kind_t kind,
                                    ku_stream_t      dependencyStream,
                                    KuCompletion    &completion) noexcept = 0;
    virtual void        onWaitIdle() noexcept = 0;

private:
    KuDevice          &m_device;
    KuHostTransferInfo m_info;
};

} // namespace kuai
