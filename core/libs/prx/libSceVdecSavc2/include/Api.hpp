#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceSdecCreateSw2();
int APS5_VABI sceSdecDestroySw2();
void APS5_VABI sceSdecGetVersionSw2(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySw2();

}
