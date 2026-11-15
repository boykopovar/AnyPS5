#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>

namespace {

std::atomic<std::uint64_t> UiStatus{0};

}

int APS5_VABI sceShellCoreUtilGetUIStatus(std::uint64_t* status) {
    if (status == nullptr) {
        return SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    const std::uint64_t localStatus = UiStatus.load(std::memory_order_relaxed);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &localStatus, sizeof(localStatus));
    errno = savedErrno;
    if (!copied) {
        return SYSTEM_SERVICE_ERROR_PARAMETER;
    }
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceShellCoreUtilSetUIStatus(std::uint64_t status, std::uint64_t mask) {
    std::uint64_t current = UiStatus.load(std::memory_order_relaxed);
    while (!UiStatus.compare_exchange_weak(current, (current & ~mask) | (status & mask), std::memory_order_relaxed)) {
    }
    return SYSTEM_SERVICE_OK;
}
