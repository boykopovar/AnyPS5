#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id);

int APS5_VABI _sceUlobjmgrRegisterObject_nid_postfix(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject_nid_postfix(std::uint32_t id);

}
