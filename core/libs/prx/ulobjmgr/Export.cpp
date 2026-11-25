#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/ulobjmgr/include/InventoryExports.hpp"
#include "prx/ulobjmgr/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ULOBJMGR_ENOENT = 2;
constexpr int ULOBJMGR_ENOMEM = 12;
constexpr int ULOBJMGR_EFAULT = 14;
constexpr int ULOBJMGR_EBUSY = 16;
constexpr int ULOBJMGR_EINVAL = 22;
constexpr std::uint32_t ULOBJMGR_ID_LIMIT = 0x3ffc;
constexpr std::uint32_t ULOBJMGR_FREE_END = 0x3fff;

struct UlobjmgrState {
    std::mutex mutex;
    bool initialized = false;
    std::uint32_t freeHead = 0;
    std::array<std::uint32_t, ULOBJMGR_ID_LIMIT> nextFree{};
    std::array<std::uint64_t, ULOBJMGR_ID_LIMIT> entries{};
    std::array<bool, ULOBJMGR_ID_LIMIT> allocated{};
    std::uint64_t globalSlot = 0;
};

UlobjmgrState& State() {
    static UlobjmgrState state;
    return state;
}

void EnsureInitialized(UlobjmgrState& state) {
    if (state.initialized) return;
    for (std::uint32_t i = 0; i + 1 < ULOBJMGR_ID_LIMIT; ++i) {
        state.nextFree[i] = i + 1;
    }
    state.nextFree[ULOBJMGR_ID_LIMIT - 1] = ULOBJMGR_FREE_END;
    state.freeHead = 0;
    state.initialized = true;
}

bool CopyOutId(std::uint32_t* id, std::uint32_t value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(id, &value, sizeof(value));
    errno = savedErrno;
    return copied;
}

}

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id) {
    if (object == 0 || kind == 0 || id == nullptr) return ULOBJMGR_EINVAL;
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized(state);
    if (state.freeHead >= ULOBJMGR_ID_LIMIT) {
        if (!CopyOutId(id, 0xffffffffu)) return ULOBJMGR_EFAULT;
        return ULOBJMGR_ENOMEM;
    }
    const std::uint32_t slot = state.freeHead;
    state.freeHead = state.nextFree[slot];
    if (!CopyOutId(id, slot)) {
        state.nextFree[slot] = state.freeHead;
        state.freeHead = slot;
        return ULOBJMGR_EFAULT;
    }
    state.entries[slot] = (object << 16) | static_cast<std::uint16_t>(kind);
    state.allocated[slot] = true;
    return 0;
}

int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id) {
    if (id >= ULOBJMGR_ID_LIMIT) return ULOBJMGR_EINVAL;
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitialized(state);
    state.entries[id] &= ~std::uint64_t{0xffff};
    if (state.allocated[id]) {
        state.allocated[id] = false;
        state.nextFree[id] = state.freeHead;
        state.freeHead = id;
    }
    return 0;
}

int APS5_VABI _sceUlobjmgrRegisterObject_nid_postfix(std::uint64_t object, std::int32_t kind, std::uint32_t* id) {
    return _sceUlobjmgrRegisterObject(object, kind, id);
}

int APS5_VABI _sceUlobjmgrUnregisterObject_nid_postfix(std::uint32_t id) {
    return _sceUlobjmgrUnregisterObject(id);
}

}

extern "C" {

int APS5_VABI ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch() {
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.globalSlot == 0) return ULOBJMGR_ENOENT;
    state.globalSlot = 0;
    return 0;
}
APS5_EXPORT("HZ9Q2c+4BU4", ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch);

int APS5_VABI ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(std::uint64_t value) {
    if (value == 0) return ULOBJMGR_EINVAL;
    auto& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.globalSlot != 0) return ULOBJMGR_EBUSY;
    state.globalSlot = value;
    return 0;
}
APS5_EXPORT("SweJO7t3pkk", ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch);

}
