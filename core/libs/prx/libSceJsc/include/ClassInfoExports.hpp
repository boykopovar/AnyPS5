#pragma once

#include "prx/libSceJsc/include/ClassInfoAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

bool APS5_VABI _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(const Jsc::JscAbi::ClassInfoPrefix* self);

}
