#pragma once

#include "prx/libSceFreeTypeOt/include/Stream.hpp"

#include <cstdint>

namespace FreeType {

struct GuestRaccessLibrary {
    GuestMemoryRec* memory;
};

}

extern "C" {

int APS5_VABI FT_Raccess_Get_HeaderInfo_nid_postfix(FreeType::GuestRaccessLibrary* library, FreeType::GuestStreamRec* stream, std::int64_t rforkOffset,
                                                    std::int64_t* mapOffset, std::int64_t* rdataPos);
int APS5_VABI FT_Raccess_Get_DataOffsets_nid_postfix(FreeType::GuestRaccessLibrary* library, FreeType::GuestStreamRec* stream, std::int64_t mapOffset,
                                                     std::int64_t rdataPos, std::int64_t tag, std::int64_t** offsets, std::int64_t* count);

}
