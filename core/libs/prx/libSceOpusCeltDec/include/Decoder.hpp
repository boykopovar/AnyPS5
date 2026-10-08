#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace OpusCeltDec {

int APS5_VABI Initialize_nid_no_patch(std::uint32_t* context);
int APS5_VABI Terminate_nid_no_patch(std::uint32_t* context);
int APS5_VABI GetSize_nid_no_patch(int channels);
int APS5_VABI Create_nid_no_patch(std::uint32_t* context, void* state, int sampleRate, int channels);
int APS5_VABI Decode_nid_no_patch(void* state, const std::uint8_t* packet, int packetBytes, std::int16_t* pcm, int capacityBytes);
int APS5_VABI Destroy_nid_no_patch(void* state);

}
