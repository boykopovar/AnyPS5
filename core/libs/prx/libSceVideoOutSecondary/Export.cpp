#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVideoOutSecondary/include/InventoryExports.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int VideoOutBusy = static_cast<int>(0x80290009u);
constexpr int VideoOutInvalidHandle = static_cast<int>(0x8029000Bu);

struct SecondarySlot {
    std::int32_t handle = -1;
    std::int32_t state = 0;
    std::array<std::uint8_t, 32> data{};
};

std::mutex SlotsMutex;
std::array<SecondarySlot, 4> Slots{};

bool TryRead(const void* src, void* dst, std::size_t size) {
    if (src == nullptr) {
        return false;
    }
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = savedErrno;
    return ok;
}

}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

void* APS5_VABI libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch(std::int32_t handle, const void* config) {
    std::array<std::uint8_t, 32> localConfig{};
    if (config != nullptr && !TryRead(config, localConfig.data(), localConfig.size())) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(SlotsMutex);
    int freeIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (Slots[i].handle >= 0 && Slots[i].handle == handle) {
            return nullptr;
        }
        if (freeIndex < 0 && Slots[i].handle < 0) {
            freeIndex = i;
        }
    }
    if (freeIndex < 0) {
        return nullptr;
    }
    Slots[freeIndex].handle = handle;
    Slots[freeIndex].state = 0;
    Slots[freeIndex].data = localConfig;
    return &Slots[freeIndex];
}
APS5_EXPORT("KJWkt9y7YC4", libSceVideoOutSecondaryUnknown_KJWkt9y7YC4_nid_no_patch);

int APS5_VABI libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch(const std::int32_t* handle) {
    std::int32_t targetHandle = 0;
    if (!TryRead(handle, &targetHandle, sizeof(targetHandle))) {
        return VideoOutInvalidHandle;
    }
    std::lock_guard<std::mutex> lock(SlotsMutex);
    int foundIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (Slots[i].handle >= 0 && Slots[i].handle == targetHandle) {
            foundIndex = i;
            break;
        }
    }
    if (foundIndex < 0) {
        return VideoOutInvalidHandle;
    }
    if (targetHandle >= 0 && targetHandle <= 3 && Slots[targetHandle].state != 0) {
        return VideoOutBusy;
    }
    if (targetHandle < 0) {
        return VideoOutInvalidHandle;
    }
    Slots[foundIndex].handle = -1;
    return 0;
}
APS5_EXPORT("s+isXKLpwS0", libSceVideoOutSecondaryUnknown_s_plus_isXKLpwS0_nid_no_patch);

}
