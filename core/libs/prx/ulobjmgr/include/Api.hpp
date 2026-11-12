#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject(std::uint32_t id);

int APS5_VABI _sceUlobjmgrRegisterObject_nid_postfix(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject_nid_postfix(std::uint32_t id);

int APS5_VABI ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch(void);
int APS5_VABI ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch(void);

}
