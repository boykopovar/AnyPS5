#pragma once

#include "SceTypes.hpp"

struct FcConfig;

extern "C" {

FcConfig* APS5_VABI FcConfigGetCurrent();
int APS5_VABI FcInitBringUptoDate();

}
