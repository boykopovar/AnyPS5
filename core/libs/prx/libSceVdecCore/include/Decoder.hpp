#pragma once

#include <cstdint>

extern "C" {

std::uint32_t VdecCoreFrameBufferSlots_nid_no_patch(const void* decoder, void** slots, std::uint32_t capacity);

}
