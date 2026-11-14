#include "prx/ulobjmgr/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

constexpr int efault = 14;
constexpr int einval = 22;

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint32_t id = 0xffffffffu;
    Require(_sceUlobjmgrRegisterObject(0, 1, &id) == einval);
    Require(id == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject(0x1000, 0, &id) == einval);
    Require(id == 0xffffffffu);
    Require(_sceUlobjmgrRegisterObject(0x1000, 1, nullptr) == einval);
    Require(_sceUlobjmgrRegisterObject(0x1000, 1, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10))) == efault);
    Require(_sceUlobjmgrRegisterObject(0x1000, -1, &id) == 0);
    Require(id == 0);

    Require(_sceUlobjmgrUnregisterObject(id) == 0);
    Require(_sceUlobjmgrUnregisterObject(0x3fff) == 0);
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
    Require(nidId == 0);

    Require(_sceUlobjmgrUnregisterObject_nid_postfix(nidId) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x3fff) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x4000) == einval);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0xffffffffu) == einval);

    std::uint32_t pair[2] = {0x12345678u, 0xaaaaaaaau};
    Require(_sceUlobjmgrRegisterObject(0, 1, pair) == einval);
    Require(pair[0] == 0x12345678u && pair[1] == 0xaaaaaaaau);
    Require(_sceUlobjmgrRegisterObject(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int32_t>::min(), pair) == 0);
    Require(pair[0] == 0 && pair[1] == 0xaaaaaaaau);
    Require(_sceUlobjmgrUnregisterObject(pair[0]) == 0);

    std::uint32_t nidPair[2] = {0x87654321u, 0x55555555u};
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, 0, nidPair) == einval);
    Require(nidPair[0] == 0x87654321u && nidPair[1] == 0x55555555u);
    Require(_sceUlobjmgrRegisterObject_nid_postfix(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int32_t>::max(), nidPair) == 0);
    Require(nidPair[0] == 0 && nidPair[1] == 0x55555555u);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(nidPair[0]) == 0);
}
