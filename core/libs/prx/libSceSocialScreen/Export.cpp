#include "prx/libSceSocialScreen/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x81440001u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x81440002u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x81440003u);

std::atomic<uint32_t> g_state{0};

}

extern "C" {

int APS5_VABI sceSocialScreenCloseSeparateMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenConfigureSeparateMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenCrashDaemon() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenGetIgnoreSeparateMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenInitialize() {
    if (g_state.load() == 2) {
        return ErrorAlreadyInitialized;
    }
    g_state.store(2);
    return 0;
}

int APS5_VABI sceSocialScreenInitializeSeparateModeParameter(void* parameter) {
    if (parameter == nullptr) return ErrorInvalidArgument;
    alignas(4) std::uint8_t buffer[0x20] = {};
    const std::uint32_t at0 = 1;
    std::memcpy(buffer, &at0, sizeof(at0));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(parameter, buffer, sizeof(buffer));
    errno = savedErrno;
    if (!copied) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceSocialScreenOpenSeparateMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenReserveHighFrameRateVideoCaptureRequest() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetIgnoreSeparateMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetMode() {
    if (g_state.load() != 2) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenTerminate() {
    if (g_state.load() != 2) {
        return ErrorNotInitialized;
    }
    g_state.store(1);
    return 0;
}

}
