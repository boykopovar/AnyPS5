#include "prx/libSceEditMp4/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <cerrno>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_EDIT_MP4_ERROR_NOT_INITIALIZED = static_cast<int>(0x80f30001);
constexpr int SCE_EDIT_MP4_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80f30002);

std::mutex g_mutex;
bool g_initialized = false;

}

extern "C" {

int APS5_VABI sceEditMp4Abort() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Edit() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4GetStatus() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Initialize() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_EDIT_MP4_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    return 0;
}

void APS5_VABI sceEditMp4InitializeParam(void* param) {
    if (param == nullptr) return;
    std::array<std::uint8_t, 0x1c> initial{};
    const std::uint64_t at10 = 0x3f;
    const std::uint32_t at18 = 700;
    std::memcpy(initial.data() + 0x10, &at10, sizeof(at10));
    std::memcpy(initial.data() + 0x18, &at18, sizeof(at18));
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(param, initial.data(), initial.size());
    errno = savedErrno;
}

std::uint64_t APS5_VABI sceEditMp4QueryMemSize() {
    return 0x387d30;
}

int APS5_VABI sceEditMp4SetFinishCallback() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Terminate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceEditMp4Unite() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_EDIT_MP4_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
