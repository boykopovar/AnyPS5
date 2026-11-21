#pragma once

#include "prx/libSceFreeTypeOt/include/Raccess.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestStreamOpenArgs {
    std::uint32_t flags;
    const std::uint8_t* memoryBase;
    std::int64_t memorySize;
    char* pathname;
    GuestStreamRec* stream;
    void* driver;
    std::int32_t numParams;
    void* params;
};

static_assert(sizeof(GuestStreamOpenArgs) == 64 && offsetof(GuestStreamOpenArgs, memoryBase) == 8 && offsetof(GuestStreamOpenArgs, memorySize) == 16);
static_assert(offsetof(GuestStreamOpenArgs, pathname) == 24 && offsetof(GuestStreamOpenArgs, stream) == 32 && offsetof(GuestStreamOpenArgs, numParams) == 48);

}

extern "C" {

int APS5_VABI FT_Stream_Open_nid_postfix(FreeType::GuestStreamRec* stream, const char* filepathname);
int APS5_VABI FT_Stream_New_nid_postfix(FreeType::GuestRaccessLibrary* library, const FreeType::GuestStreamOpenArgs* args, FreeType::GuestStreamRec** astream);

}
