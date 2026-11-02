#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceSdecCreateSwHevc();
int APS5_VABI sceSdecDestroySwHevc();
int APS5_VABI sceSdecExecuteSwHevc();
int APS5_VABI sceSdecGetFrameSwHevc();
void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySwHevc();
int APS5_VABI sceSdecResetSwHevc();

}
