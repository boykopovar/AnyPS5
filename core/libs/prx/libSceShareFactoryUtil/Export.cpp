#include "prx/libSceShareFactoryUtil/include/Api.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x81630002);
constexpr std::int32_t ERROR_NOT_LAUNCHED = static_cast<std::int32_t>(0x81630003);
constexpr std::int32_t ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x81630005);
constexpr std::size_t PARAM_SIZE = 0x1100;
constexpr std::size_t STATUS_SIZE = 0x114;

std::atomic<std::int32_t> g_initRefCount{0};

bool IsInitialized() {
    return g_initRefCount.load(std::memory_order_acquire) > 0;
}

bool CopyGuestParam(const void* param, std::size_t size) {
    if (param == nullptr || size == 0 || size > PARAM_SIZE) {
        return false;
    }
    std::array<std::uint8_t, PARAM_SIZE> copy{};
    const auto savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(copy.data(), param, size);
    errno = savedErrno;
    return ok;
}

bool CheckGuestStatusWritable(void* status) {
    if (status == nullptr) {
        return false;
    }
    const auto savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(status, STATUS_SIZE);
    errno = savedErrno;
    return ok;
}

}

extern "C" {

int APS5_VABI sceShareFactoryUtilInitialize(void) {
    std::int32_t current = g_initRefCount.load(std::memory_order_acquire);
    while (current < std::numeric_limits<std::int32_t>::max() &&
           !g_initRefCount.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
    return 0;
}

int APS5_VABI sceShareFactoryUtilTerminate(void) {
    std::int32_t current = g_initRefCount.load(std::memory_order_acquire);
    while (current > 0 && !g_initRefCount.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
    return 0;
}

int APS5_VABI sceShareFactoryUtilLaunchShareFactory(const void* param, std::uint32_t size) {
    if (!IsInitialized()) {
        return ERROR_NOT_INITIALIZED;
    }
    if (!CopyGuestParam(param, size)) {
        return ERROR_INVALID_PARAM;
    }
    return ERROR_NOT_LAUNCHED;
}

int APS5_VABI sceShareFactoryUtilGetLaunchStatus(void* status, const void* param) {
    if (status == nullptr || param == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    if (!IsInitialized()) {
        return ERROR_NOT_INITIALIZED;
    }
    if (!CopyGuestParam(param, PARAM_SIZE) || !CheckGuestStatusWritable(status)) {
        return ERROR_INVALID_PARAM;
    }
    return ERROR_NOT_LAUNCHED;
}

int APS5_VABI sceShareFactoryUtilBackToGameFromShareFactory(const void* param) {
    if (!IsInitialized()) {
        return ERROR_NOT_INITIALIZED;
    }
    if (!CopyGuestParam(param, PARAM_SIZE)) {
        return ERROR_INVALID_PARAM;
    }
    return ERROR_NOT_LAUNCHED;
}

int APS5_VABI sceShareFactoryUtilGetLaunchStatusOfShareFactory(void* status, const void* param) {
    if (status == nullptr || param == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    if (!IsInitialized()) {
        return ERROR_NOT_INITIALIZED;
    }
    if (!CopyGuestParam(param, PARAM_SIZE) || !CheckGuestStatusWritable(status)) {
        return ERROR_INVALID_PARAM;
    }
    return ERROR_NOT_LAUNCHED;
}

}
