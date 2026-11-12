#ifndef CORE_LIBS_PRX_LIBSCEJXR_STREAM_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_STREAM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace Jxr {

struct Stream {
    std::array<std::uintptr_t, 4> state;
    std::int32_t memory;
    std::int64_t (APS5_VABI* close)(Stream**);
    std::int32_t (APS5_VABI* end)(Stream*);
    std::int64_t (APS5_VABI* read)(Stream*, void*, std::size_t);
    std::int64_t (APS5_VABI* write)(Stream*, const void*, std::size_t);
    std::int64_t (APS5_VABI* setPosition)(Stream*, std::size_t);
    std::int64_t (APS5_VABI* getPosition)(Stream*, std::size_t*);
};

struct BitIo {
    std::uint32_t shadow;
    std::uint32_t accumulator;
    std::uint32_t bitsUsed;
    std::int32_t mask;
    std::uint8_t* start;
    std::uint8_t* current;
    Stream* stream;
    std::size_t referenceOffset;
};

struct SimpleBitIo {
    Stream* stream;
    std::uint32_t bytesRead;
    std::uint8_t accumulator;
    std::uint32_t bitsLeft;
};

static_assert(sizeof(Stream) == 88 && offsetof(Stream, read) == 56 && offsetof(Stream, setPosition) == 72);
static_assert(sizeof(BitIo) == 48 && offsetof(BitIo, start) == 16 && offsetof(BitIo, referenceOffset) == 40);
static_assert(sizeof(SimpleBitIo) == 24 && offsetof(SimpleBitIo, bytesRead) == 8 && offsetof(SimpleBitIo, accumulator) == 12 && offsetof(SimpleBitIo, bitsLeft) == 16);

} // namespace Jxr

#endif
