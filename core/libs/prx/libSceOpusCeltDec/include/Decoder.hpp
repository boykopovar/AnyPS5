#pragma once

#include <cstdint>

namespace OpusCeltDec {

int Initialize_nid_no_patch(std::uint32_t* context);
int Terminate_nid_no_patch(std::uint32_t* context);
int GetSize_nid_no_patch(int channels);
int Create_nid_no_patch(std::uint32_t* context, void* state, int sampleRate, int channels);
int Decode_nid_no_patch(void* state, const std::uint8_t* packet, int packetBytes, std::int16_t* pcm, int capacityBytes);
int Destroy_nid_no_patch(void* state);

}
