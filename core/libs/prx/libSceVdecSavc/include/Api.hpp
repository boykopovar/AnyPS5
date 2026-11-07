#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceSdecCreateSw();
int APS5_VABI sceSdecDestroySw();
void APS5_VABI sceSdecGetVersionSw(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySw();

}
