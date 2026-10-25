#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSdecCreateSwHevc();
int APS5_VABI sceSdecDestroySwHevc();
int APS5_VABI sceSdecExecuteSwHevc();
int APS5_VABI sceSdecGetFrameSwHevc();
int APS5_VABI sceSdecGetVersionSwHevc();
int APS5_VABI sceSdecQueryMemorySwHevc();
int APS5_VABI sceSdecResetSwHevc();

}
