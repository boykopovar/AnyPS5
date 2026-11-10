#pragma once

#include "prx/libSceFreeTypeOt/include/Raccess.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestOpenArgs {
    std::uint32_t flags;
    const std::uint8_t* memoryBase;
    std::int64_t memorySize;
    char* pathname;
    GuestStreamRec* stream;
    void* driver;
    std::int32_t numParams;
    void* params;
};

static_assert(sizeof(GuestOpenArgs) == 64 && offsetof(GuestOpenArgs, memoryBase) == 8 && offsetof(GuestOpenArgs, memorySize) == 16);
static_assert(offsetof(GuestOpenArgs, pathname) == 24 && offsetof(GuestOpenArgs, stream) == 32 && offsetof(GuestOpenArgs, numParams) == 48);

}

extern "C" {

int APS5_VABI FT_Stream_Open_nid_postfix(FreeType::GuestStreamRec* stream, const char* filepathname);
int APS5_VABI FT_Stream_New_nid_postfix(FreeType::GuestRaccessLibrary* library, const FreeType::GuestOpenArgs* args, FreeType::GuestStreamRec** astream);

}
