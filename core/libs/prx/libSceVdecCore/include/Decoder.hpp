#pragma once

#include <cstdint>

extern "C" {

std::uint32_t VdecCoreFrameBufferSlots_nid_no_patch(const void* decoder, void** slots, std::uint32_t capacity);
std::uint64_t VdecCoreMaxAccessUnitSize_nid_no_patch();
std::int32_t VdecCoreAccessUnitLength_nid_no_patch(std::uint64_t auSize);

}
