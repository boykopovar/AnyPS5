#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSdecCreateSw();
int APS5_VABI sceSdecDestroySw();
int APS5_VABI sceSdecGetVersionSw();
int APS5_VABI sceSdecQueryMemorySw();

}
