#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void);
int APS5_VABI sceNpEAAccessTerminate(void);
int APS5_VABI sceNpHasEAAccessSubscription(int userId, bool* result);

}
