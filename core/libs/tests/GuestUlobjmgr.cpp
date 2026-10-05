#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id);
}

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
}
