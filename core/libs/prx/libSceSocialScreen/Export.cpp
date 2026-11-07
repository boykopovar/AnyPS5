#include "prx/libSceSocialScreen/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED = static_cast<int>(0x81440001);
constexpr int SCE_SOCIAL_SCREEN_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81440002);
constexpr int SCE_SOCIAL_SCREEN_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81440003);

std::mutex g_mutex;
bool g_initialized = false;

}

extern "C" {

int APS5_VABI sceSocialScreenCloseSeparateMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenConfigureSeparateMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenCrashDaemon() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenGetIgnoreSeparateMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenInitialize() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_SOCIAL_SCREEN_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceSocialScreenInitializeSeparateModeParameter(void* parameter) {
    if (parameter == nullptr) return SCE_SOCIAL_SCREEN_ERROR_INVALID_ARGUMENT;
    alignas(4) std::uint8_t buffer[0x20] = {};
    const std::uint32_t at0 = 1;
    std::memcpy(buffer, &at0, sizeof(at0));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(parameter, buffer, sizeof(buffer));
    errno = savedErrno;
    if (!copied) return SCE_SOCIAL_SCREEN_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSocialScreenOpenSeparateMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenReserveHighFrameRateVideoCaptureRequest() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetIgnoreSeparateMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetMode() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenTerminate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SOCIAL_SCREEN_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    return 0;
}

}
