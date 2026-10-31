#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/ulobjmgr/include/InventoryExports.hpp"
#include "prx/ulobjmgr/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ULOBJMGR_EINVAL = 22;
constexpr std::uint32_t ULOBJMGR_ID_LIMIT = 0x4000;

}

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id) {
 if (object == 0 || kind == 0 || id == nullptr) return ULOBJMGR_EINVAL;
 *id = 0;
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
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HZ9Q2c+4BU4", ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch);

int APS5_VABI ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SweJO7t3pkk", ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch);

}
