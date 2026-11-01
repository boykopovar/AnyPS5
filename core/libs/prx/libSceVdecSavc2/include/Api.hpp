#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSdecCreateSw2();
int APS5_VABI sceSdecDestroySw2();
int APS5_VABI sceSdecGetVersionSw2();
int APS5_VABI sceSdecQueryMemorySw2();

}
