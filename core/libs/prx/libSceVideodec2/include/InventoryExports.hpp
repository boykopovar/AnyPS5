#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideodec2DecodeFirstSlice();
int APS5_VABI sceVideodec2DecodeNextSlice();
int APS5_VABI sceVideodec2SyncAllSlice();

}
