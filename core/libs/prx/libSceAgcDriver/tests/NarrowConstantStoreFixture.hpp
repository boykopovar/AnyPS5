#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_NARROWCONSTANTSTOREFIXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_NARROWCONSTANTSTOREFIXTURE_HPP

#include <array>
#include <cstdint>

namespace NarrowConstantStoreFixture {

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t LaneBytes = 16;
constexpr std::uint8_t Fill = 0xcd;

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020084, 0x7e040280, 0x7e0602ff, 0x00a10200, 0x7e0802ff, 0x0000fec3,
    0xdc608000, 0x00000201,
    0xdc688002, 0x00000401,
    0xdc708008, 0x00000301,
    0xbf810000,
};

constexpr std::array<std::uint8_t, LaneBytes> ExpectedLane{
    0x00, Fill, 0xc3, 0xfe, Fill, Fill, Fill, Fill,
    0x00, 0x02, 0xa1, 0x00, Fill, Fill, Fill, Fill,
};

}

#endif
