#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/ulobjmgr/include/InventoryExports.hpp"
#include "prx/ulobjmgr/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ULOBJMGR_ENOENT = 2;
constexpr int ULOBJMGR_EFAULT = 14;
constexpr int ULOBJMGR_EBUSY = 16;
constexpr int ULOBJMGR_EINVAL = 22;
constexpr std::uint32_t ULOBJMGR_ID_LIMIT = 0x4000;

std::atomic<std::uint64_t> tableSlot{0};

}

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id) {
    if (object == 0 || kind == 0 || id == nullptr) return ULOBJMGR_EINVAL;
    constexpr std::uint32_t zero = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(id, &zero, sizeof(zero));
    errno = savedErrno;
    if (!copied) return ULOBJMGR_EFAULT;
    return 0;
}

int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id) {
 if (id >= ULOBJMGR_ID_LIMIT) return ULOBJMGR_EINVAL;
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
    return tableSlot.exchange(0) == 0 ? ULOBJMGR_ENOENT : 0;
}
APS5_EXPORT("HZ9Q2c+4BU4", ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch);

int APS5_VABI ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(std::uint64_t value) {
    if (value == 0) return ULOBJMGR_EINVAL;
    std::uint64_t expected = 0;
    if (!tableSlot.compare_exchange_strong(expected, value)) return ULOBJMGR_EBUSY;
    return 0;
}
APS5_EXPORT("SweJO7t3pkk", ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch);

}
