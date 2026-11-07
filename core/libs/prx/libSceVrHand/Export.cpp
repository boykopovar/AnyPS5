#include "prx/libSceVrHand/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VR_HAND_ERROR_NOT_INITIALIZED = static_cast<int>(0x8a870001);
constexpr int SCE_VR_HAND_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8a870002);
constexpr int SCE_VR_HAND_ERROR_NOT_STARTED = static_cast<int>(0x8a870003);
constexpr int SCE_VR_HAND_ERROR_ALREADY_STARTED = static_cast<int>(0x8a870004);
constexpr int SCE_VR_HAND_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8a870006);

std::mutex g_mutex;
bool g_initialized = false;
bool g_started = false;

}

extern "C" {

int APS5_VABI sceVrHandFinalize() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    g_started = false;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceVrHandGetMesh() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    if (!g_started) return SCE_VR_HAND_ERROR_NOT_STARTED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetMeshSize() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    if (!g_started) return SCE_VR_HAND_ERROR_NOT_STARTED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetResult() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    if (!g_started) return SCE_VR_HAND_ERROR_NOT_STARTED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandInitialize() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_VR_HAND_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceVrHandQueryMemory(const void* param, void* result) {
    constexpr int errInvalidArg = static_cast<int>(0x8a870006);
    if (param == nullptr || result == nullptr) return errInvalidArg;
    std::uint32_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, param, sizeof(size))) return errInvalidArg;
    if (size != 0x20) return errInvalidArg;
    std::array<std::uint8_t, 0x20> input{};
    if (!GuestCopyIn_nid_no_patch(input.data(), param, sizeof(input))) return errInvalidArg;
    for (std::size_t index = 4; index < 0x20; ++index) if (input[index] != 0) return errInvalidArg;
    struct MemoryInfo {
        std::uint32_t memorySize{0x3c00000};
        std::uint32_t alignment{0x10000};
        std::uint8_t reserved[0x14]{};
    };
    static_assert(sizeof(MemoryInfo) == 28);
    const MemoryInfo info{};
    if (!GuestCopyOut_nid_no_patch(result, &info, sizeof(info))) return errInvalidArg;
    return 0;
}

int APS5_VABI sceVrHandStart() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    if (g_started) return SCE_VR_HAND_ERROR_ALREADY_STARTED;
    g_started = true;
    return 0;
}

int APS5_VABI sceVrHandStop() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_VR_HAND_ERROR_NOT_INITIALIZED;
    if (!g_started) return SCE_VR_HAND_ERROR_NOT_STARTED;
    g_started = false;
    return 0;
}

}
