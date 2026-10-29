#pragma once

#include "SceTypes.hpp"
#include "prx/libSceJxr/GuestPixelInfo.h"
#include "prx/libSceJxr/Stream.hpp"

extern "C" {

std::int64_t APS5_VABI PKFree_nid_postfix(void** memory);
std::int64_t APS5_VABI PixelFormatLookup_nid_postfix(JxrPixelInfo* value, std::uint8_t mode);
std::int64_t APS5_VABI readIS_nid_postfix(void* codec, Jxr::BitIo* io);
std::int64_t APS5_VABI writeIS_nid_postfix(void* codec, Jxr::BitIo* io);

}
