#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRnpsBundleInit();
int APS5_VABI sceRnpsBundleInitEx();
int APS5_VABI sceRnpsBundleTerm();
int APS5_VABI sceRnpsBundleVerifyAndDecrypt();
int APS5_VABI sceRnpsBundleVerifyChain();

}
