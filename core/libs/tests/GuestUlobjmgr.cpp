#include "prx/ulobjmgr/ulobjmgr.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

constexpr int enoent = 2;
constexpr int enomem = 12;
constexpr int efault = 14;
constexpr int ebusy = 16;
constexpr int einval = 22;
constexpr std::uint32_t idLimit = 0x3ffc;

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint32_t id = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject(0, 1, &id) == einval);
    Require(id == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject(0x1000, 0, &id) == einval);
    Require(id == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject(0x1000, 1, nullptr) == einval);
    errno = 71;
    Require(_sceUlobjmgrRegisterObject(0x1000, 1, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10))) == efault);
    Require(errno == 71);

    std::uint32_t id0 = 0xffffffffu;
    std::uint32_t id1 = 0xffffffffu;
    std::uint32_t id2 = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject(0x1000, -1, &id0) == 0);
    Require(errno == 71);
    Require(id0 == 0);
    Require(_sceUlobjmgrRegisterObject(0x2000, 2, &id1) == 0);
    Require(id1 == 1);
    Require(_sceUlobjmgrRegisterObject(0x3000, 3, &id2) == 0);
    Require(id2 == 2);

    Require(_sceUlobjmgrUnregisterObject(id1) == 0);
    std::uint32_t reused = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject(0x4000, 4, &reused) == 0);
    Require(reused == id1);

    Require(_sceUlobjmgrUnregisterObject(id0) == 0);
    Require(_sceUlobjmgrUnregisterObject(reused) == 0);
    Require(_sceUlobjmgrUnregisterObject(id2) == 0);

    Require(_sceUlobjmgrUnregisterObject(0x3ffb) == 0);
    Require(_sceUlobjmgrUnregisterObject(0x3ffc) == einval);
    Require(_sceUlobjmgrUnregisterObject(0x3fff) == einval);
    Require(_sceUlobjmgrUnregisterObject(0x4000) == einval);
    Require(_sceUlobjmgrUnregisterObject(0xffffffffu) == einval);

    std::uint32_t nidId = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0, 1, &nidId) == einval);
    Require(nidId == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, 0, &nidId) == einval);
    Require(nidId == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, 1, nullptr) == einval);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, 1, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10))) == efault);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, -1, &nidId) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(nidId) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x3ffb) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x3ffc) == einval);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x4000) == einval);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0xffffffffu) == einval);

    std::uint32_t pair[2] = {0x12345678u, 0xaaaaaaaau};
    Require(_sceUlobjmgrRegisterObject(0, 1, pair) == einval);
    Require(pair[0] == 0x12345678u && pair[1] == 0xaaaaaaaau);
    Require(_sceUlobjmgrRegisterObject(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int32_t>::min(), pair) == 0);
    Require(pair[1] == 0xaaaaaaaau);
    Require(_sceUlobjmgrUnregisterObject(pair[0]) == 0);

    std::uint32_t nidPair[2] = {0x87654321u, 0x55555555u};
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, 0, nidPair) == einval);
    Require(nidPair[0] == 0x87654321u && nidPair[1] == 0x55555555u);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int32_t>::max(), nidPair) == 0);
    Require(nidPair[1] == 0x55555555u);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(nidPair[0]) == 0);

    std::vector<std::uint32_t> activeIds;
    activeIds.reserve(idLimit);
    for (std::uint32_t i = 0; i < idLimit; ++i) {
        std::uint32_t allocatedId = 0xffffffffu;
        Require(_sceUlobjmgrRegisterObject(0x10000ull + i, 1, &allocatedId) == 0);
        Require(allocatedId < idLimit);
        activeIds.push_back(allocatedId);
    }

    std::uint32_t exhaustedId = 0x12345678u;
    Require(_sceUlobjmgrRegisterObject(0x99999ull, 1, &exhaustedId) == enomem);
    Require(exhaustedId == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject(0x99999ull, 1, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10))) == efault);

    const std::uint32_t freedSlot = activeIds.back();
    activeIds.pop_back();
    Require(_sceUlobjmgrUnregisterObject(freedSlot) == 0);
    std::uint32_t refilledId = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject(0x88888ull, 2, &refilledId) == 0);
    Require(refilledId == freedSlot);
    activeIds.push_back(refilledId);
    for (const std::uint32_t activeId : activeIds) {
        Require(_sceUlobjmgrUnregisterObject(activeId) == 0);
    }

    Require(ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch() == enoent);
    Require(ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(0) == einval);
    Require(ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(0x123456789abcdef0ull) == 0);
    Require(ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(0x2222222222222222ull) == ebusy);
    Require(ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch() == 0);
    Require(ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch() == enoent);
    Require(ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(0x3333333333333333ull) == 0);
    Require(ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch() == 0);
    return 0;
}
