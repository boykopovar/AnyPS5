#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libSceSysmodule/include/InventoryExports.hpp"
#include "prx/libSceSysmodule/ModuleTable.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr int SCE_SYSMODULE_ERROR_INVALID_VALUE = static_cast<int>(0x805A1000u);

constexpr std::array<std::uint32_t, 28> kRestrictedInternalIds = {
    0x80000001u, 0x80000002u, 0x80000009u, 0x8000000Au, 0x8000000Bu, 0x8000000Cu, 0x8000000Du,
    0x8000000Fu, 0x80000010u, 0x80000011u, 0x80000018u, 0x8000001Cu, 0x8000001Du, 0x8000001Eu,
    0x8000001Fu, 0x80000020u, 0x80000021u, 0x80000022u, 0x80000023u, 0x80000024u, 0x80000026u,
    0x80000078u, 0x8000007Eu, 0x80000080u, 0x8000008Cu, 0x8000008Du, 0x8000008Fu, 0x80000094u,
};

bool IsRestrictedInternalId(std::uint32_t id) {
    return std::binary_search(kRestrictedInternalIds.begin(), kRestrictedInternalIds.end(), id);
}

int ClampArgs(std::size_t args) {
    constexpr auto maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return args > maxInt ? std::numeric_limits<int>::max() : static_cast<int>(args);
}

} // namespace

extern "C" {

int APS5_VABI libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch(std::uint32_t id) {
    if (!IsRestrictedInternalId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleLoadModuleInternal(id);
}
APS5_EXPORT("5gs662NWKhE", libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(std::uint32_t id, std::size_t args, void* argp, std::uint32_t flags, int* res) {
    if ((id & 0x7fffffffu) == 0u || flags != 0u || kModuleTable.find(id) == kModuleTable.end()) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleUnloadModuleInternalWithArg(id, ClampArgs(args), argp, flags, res);
}
APS5_EXPORT("6XFxHG0KhBg", libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0u || !IsRestrictedInternalId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleUnloadModuleInternal(id);
}
APS5_EXPORT("9Q9uHJVfB4w", libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(std::uint32_t id, std::size_t args, void* argp, std::uint32_t flags, int* res) {
    if ((id & 0x7fffffffu) == 0u || flags != 0u || kModuleTable.find(id) == kModuleTable.end()) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleLoadModuleInternalWithArg(id, ClampArgs(args), argp, flags, res);
}
APS5_EXPORT("kMn8gDsKp9Y", libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch);

int APS5_VABI libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(std::uint32_t id) {
    if ((id & 0x7fffffffu) == 0u || !IsRestrictedInternalId(id)) {
        return SCE_SYSMODULE_ERROR_INVALID_VALUE;
    }
    return sceSysmoduleIsLoadedInternal(id);
}
APS5_EXPORT("uQqnJudN5fY", libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch);

}
