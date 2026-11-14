#pragma once

#include "SceTypes.hpp"

struct FcConfig;

extern "C" {

FcConfig* APS5_VABI FcConfigCreate();
FcConfig* APS5_VABI FcConfigReference(FcConfig* config);
void APS5_VABI FcConfigDestroy(FcConfig* config);

}
