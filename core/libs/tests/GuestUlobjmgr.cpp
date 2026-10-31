#include "prx/ulobjmgr/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

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
    Require(_sceUlobjmgrRegisterObject_nid_postfix(0x1000, -1, &nidId) == 0);
    Require(nidId == 0);

    Require(_sceUlobjmgrUnregisterObject_nid_postfix(nidId) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x3fff) == 0);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0x4000) == einval);
    Require(_sceUlobjmgrUnregisterObject_nid_postfix(0xffffffffu) == einval);
}
