#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libSceSysmodule/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int kInvalidValue = static_cast<int>(0x805A1000);
constexpr int kUnloaded = static_cast<int>(0x805A1001);

void Require(bool value) { if (!value) std::abort(); }

void CheckInternalIdRejected(std::uint32_t id) {
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
    Require(sceSysmoduleLoadModuleInternal(id) == kInvalidValue);
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
    Require(sceSysmoduleUnloadModuleInternal(id) == kInvalidValue);
}

void CheckRestrictedInventoryRejected(std::uint32_t id) {
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(id) == kInvalidValue);
    Require(libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch(id) == kInvalidValue);
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(id) == kInvalidValue);
    Require(libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch(id) == kInvalidValue);
}

void CheckWithArgInventory() {
    int res = -1;
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0x80000000u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x80000000u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0x80000001u, 0u, nullptr, 1u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x80000001u, 0u, nullptr, 1u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0x8000ffffu, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x8000ffffu, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0x80ffff00u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x80ffff00u, 0u, nullptr, 0u, &res) == kInvalidValue);
    Require(res == -1);
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x80000001u, 0u, nullptr, 0u, &res) == kUnloaded);
    Require(libSceSysmoduleUnknown_kMn8gDsKp9Y_nid_no_patch(0x80000001u, 0u, nullptr, 0u, &res) == 0);
    Require(res == 0);
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(0x80000001u) == 0);
    res = -1;
    Require(libSceSysmoduleUnknown_6XFxHG0KhBg_nid_no_patch(0x80000001u, 0u, nullptr, 0u, &res) == 0);
    Require(res == 0);
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(0x80000001u) == kUnloaded);
}

} // namespace

int main() {
    CheckInternalIdRejected(0x80ffff00);
    CheckInternalIdRejected(0x8000ffff);
    Require(sceSysmoduleIsLoadedInternal(0x00ffffff) == kInvalidValue);

    for (std::uint32_t bad : {0u, 0x80000000u, 0x80000003u, 0x80000095u, 0x8000ffffu, 0x80ffff00u, 0x00000001u, 0x00ffffffu}) {
        CheckRestrictedInventoryRejected(bad);
    }
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(0x80000001u) == kUnloaded);
    Require(libSceSysmoduleUnknown_5gs662NWKhE_nid_no_patch(0x80000001u) == 0);
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(0x80000001u) == 0);
    Require(libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch(0x80000001u) == 0);
    Require(libSceSysmoduleUnknown_uQqnJudN5fY_nid_no_patch(0x80000001u) == kUnloaded);
    Require(libSceSysmoduleUnknown_9Q9uHJVfB4w_nid_no_patch(0x80000001u) == kUnloaded);

    CheckWithArgInventory();
}
