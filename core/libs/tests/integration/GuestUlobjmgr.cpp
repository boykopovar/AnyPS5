#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

extern "C" {
int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int einval = 22;
constexpr std::uint32_t unsetId = 0xffffffffu;

const Case nullObject{"RegisterObject_NullObject_ReturnsEinvalAndKeepsId", [] {
    std::uint32_t id = unsetId;
    RequireEqual(_sceUlobjmgrRegisterObject(0, 1, &id), einval, "null object");
    RequireEqual(id, unsetId, "id untouched");
}};

const Case zeroKind{"RegisterObject_ZeroKind_ReturnsEinvalAndKeepsId", [] {
    std::uint32_t id = unsetId;
    RequireEqual(_sceUlobjmgrRegisterObject(0x1000, 0, &id), einval, "zero kind");
    RequireEqual(id, unsetId, "id untouched");
}};

const Case nullId{"RegisterObject_NullIdPointer_ReturnsEinval", [] {
    RequireEqual(_sceUlobjmgrRegisterObject(0x1000, 1, nullptr), einval, "null id pointer");
}};

const Case validObject{"RegisterObject_ValidObject_SucceedsWithIdZero", [] {
    std::uint32_t id = unsetId;
    RequireEqual(_sceUlobjmgrRegisterObject(0x1000, -1, &id), 0, "register");
    RequireEqual(id, 0u, "assigned id");
    RequireEqual(_sceUlobjmgrUnregisterObject(id), 0, "unregister the assigned id");
}};

const Case idsBelowLimit{"UnregisterObject_IdBelowLimit_Succeeds", [] {
    RequireEqual(_sceUlobjmgrUnregisterObject(0), 0, "id 0");
    RequireEqual(_sceUlobjmgrUnregisterObject(0x3fff), 0, "id 0x3fff");
}};

const Case idsAtOrAboveLimit{"UnregisterObject_IdAtOrAboveLimit_ReturnsEinval", [] {
    for (const std::uint32_t id : {0x4000u, 0xffffffffu}) {
        RequireEqual(_sceUlobjmgrUnregisterObject(id), einval, "id " + std::to_string(id));
    }
}};

} // namespace
