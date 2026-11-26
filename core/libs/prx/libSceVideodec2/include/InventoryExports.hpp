#pragma once

#include "SceTypes.hpp"
#include "prx/libSceVideodec2/include/Api.hpp"

extern "C" {

int APS5_VABI sceVideodec2DecodeFirstSlice(std::uint64_t decoder, const void* input, SceVideodec2::FrameBuffer* frameBuffer);
int APS5_VABI sceVideodec2DecodeNextSlice(std::uint64_t decoder, const void* input);
int APS5_VABI sceVideodec2SyncAllSlice(std::uint64_t decoder, SceVideodec2::OutputInfo* output, const void* syncResult);

}
