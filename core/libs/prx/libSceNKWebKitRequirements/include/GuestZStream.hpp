#ifndef CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTZSTREAM_HPP
#define CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTZSTREAM_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKitRequirements {

using GuestAlloc = void* (APS5_VABI *)(void*, std::uint32_t, std::uint32_t);
using GuestFree = void (APS5_VABI *)(void*, void*);

struct GuestZStream {
    const std::uint8_t* next_in;
    std::uint32_t avail_in;
    std::uint64_t total_in;
    std::uint8_t* next_out;
    std::uint32_t avail_out;
    std::uint64_t total_out;
    const char* msg;
    void* state;
    GuestAlloc zalloc;
    GuestFree zfree;
    void* opaque;
    std::int32_t data_type;
    std::uint64_t adler;
    std::uint64_t reserved;
};

static_assert(offsetof(GuestZStream, avail_in) == 8 && offsetof(GuestZStream, total_in) == 16);
static_assert(offsetof(GuestZStream, next_out) == 24 && offsetof(GuestZStream, avail_out) == 32);
static_assert(offsetof(GuestZStream, total_out) == 40 && offsetof(GuestZStream, msg) == 48);
static_assert(offsetof(GuestZStream, state) == 56 && offsetof(GuestZStream, zalloc) == 64);
static_assert(offsetof(GuestZStream, zfree) == 72 && offsetof(GuestZStream, opaque) == 80);
static_assert(offsetof(GuestZStream, data_type) == 88 && offsetof(GuestZStream, adler) == 96);
static_assert(offsetof(GuestZStream, reserved) == 104 && sizeof(GuestZStream) == 112);

}

#endif
