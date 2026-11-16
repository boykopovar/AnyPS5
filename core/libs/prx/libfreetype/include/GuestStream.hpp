#pragma once

#include "prx/libSceFreeType/include/GuestList.hpp"
#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace LegacyFreeType {

struct GuestStreamRec;

union GuestStreamDesc {
    std::int64_t value;
    void* pointer;
};

struct GuestStreamRec {
    unsigned char* base;
    std::uint64_t size;
    std::uint64_t pos;
    GuestStreamDesc descriptor;
    GuestStreamDesc pathname;
    std::uint64_t (APS5_VABI *read)(GuestStreamRec*, std::uint64_t, unsigned char*, std::uint64_t);
    void (APS5_VABI *close)(GuestStreamRec*);
    FreeType::GuestMemoryRec* memory;
    unsigned char* cursor;
    unsigned char* limit;
};

static_assert(sizeof(GuestStreamRec) == 80);
static_assert(offsetof(GuestStreamRec, descriptor) == 24 && offsetof(GuestStreamRec, read) == 40);
static_assert(offsetof(GuestStreamRec, memory) == 56 && offsetof(GuestStreamRec, limit) == 72);

}
