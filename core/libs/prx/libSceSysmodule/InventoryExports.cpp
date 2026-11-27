#include "SceTypes.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libSceSysmodule/include/InventoryExports.hpp"
#include "prx/libSceSysmodule/ModuleTable.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr int SCE_SYSMODULE_ERROR_INVALID_VALUE = static_cast<int>(0x805A1000u);

int ClampArgs(std::size_t args) {
    constexpr auto maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return args > maxInt ? std::numeric_limits<int>::max() : static_cast<int>(args);
}

} // namespace

extern "C" {

int APS5_VABI libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5gs662NWKhE", libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(std::uint32_t id, std::size_t args, void* argp, std::uint32_t flags, int* res) {
    if ((id & 0x7fffffffu) == 0u || flags != 0u || kModuleTable.find(id) == kModuleTable.end()) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleUnloadModuleInternalWithArg(id, ClampArgs(args), argp, flags, res);
}
APS5_EXPORT("6XFxHG0KhBg", libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9Q9uHJVfB4w", libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(std::uint32_t id, std::size_t args, void* argp, std::uint32_t flags, int* res) {
    if ((id & 0x7fffffffu) == 0u || flags != 0u || kModuleTable.find(id) == kModuleTable.end()) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleLoadModuleInternalWithArg(id, ClampArgs(args), argp, flags, res);
}
APS5_EXPORT("kMn8gDsKp9Y", libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uQqnJudN5fY", libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch);

}
