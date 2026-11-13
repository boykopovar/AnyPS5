#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceSdecCreateSwHevc(const void* param);
int APS5_VABI sceSdecDestroySwHevc(void* handle);
int APS5_VABI sceSdecExecuteSwHevc(void* handle);
int APS5_VABI sceSdecGetFrameSwHevc(void* handle);
void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySwHevc();
int APS5_VABI sceSdecResetSwHevc(void* handle);

}
