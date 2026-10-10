#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t StorageResults = 4;
constexpr std::uint32_t SampleResults = 16;
constexpr std::uint32_t StorageWidth = 32;
constexpr std::uint32_t StorageHeight = 8;
constexpr std::uint32_t SampleWidth = 64;
constexpr std::uint32_t SampleHeight = 2;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::uint32_t, Threads * (Inputs + Results)> Buffer{};
alignas(4096) std::array<std::uint32_t, 4096> StorageTexels{};
alignas(4096) std::array<std::uint32_t, 4096> SampleTexels{};

alignas(256) constexpr std::array<std::uint32_t, 47> StorageCode{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xbe93030f, 0xbe92030e, 0xbe91030d,
    0xbe90030c, 0xbe8f030b, 0xbe8e030a, 0xbe8d0309, 0xbe8c0308, 0xbe8b0307, 0xbe8a0306, 0xbe890305,
    0xbe880304, 0x7e0a02ff, 0x5a7a0005, 0x7e0e02ff, 0x5a7a0007, 0x7e100300, 0x7e120280, 0xf0210108,
    0x00020408, 0xbbfd0000, 0x7e140280, 0xf0442108, 0x00020a08, 0xbf8c3f70, 0x7e120281, 0xf0230108,
    0x00020608, 0xbbfd0000, 0x7e160280, 0xf0442108, 0x00020b08, 0xbf8c3f70, 0xe0701200, 0x80000a03,
    0xe0701204, 0x80000b03, 0xe0701208, 0x80000503, 0xe070120c, 0x80000703, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 99> SampleCode{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xbe93030f, 0xbe92030e, 0xbe91030d,
    0xbe90030c, 0xbe8f030b, 0xbe8e030a, 0xbe8d0309, 0xbe8c0308, 0xbe8b0307, 0xbe8a0306, 0xbe890305,
    0xbe880304, 0x7e1402ff, 0x5a7a000a, 0x7e1602ff, 0x5a7a000b, 0x7e1802ff, 0x5a7a000c, 0x7e1a02ff,
    0x5a7a000d, 0x7e1c02ff, 0x5a7a000e, 0x7e1e02ff, 0x5a7a000f, 0x7e2002ff, 0x5a7a0010, 0x7e2202ff,
    0x5a7a0011, 0x7e2402ff, 0x5a7a0012, 0x7e2602ff, 0x5a7a0013, 0x7e2802ff, 0x5a7a0014, 0x7e2a02ff,
    0x5a7a0015, 0x7e2c02ff, 0x5a7a0016, 0x7e2e02ff, 0x5a7a0017, 0x7e3002ff, 0x5a7a0018, 0x7e3202ff,
    0x5a7a0019, 0xf0830108, 0x00820a04, 0xf1030108, 0x00820c04, 0x7e540280, 0xf03b0308, 0x0002112a,
    0xf0910108, 0x00821404, 0x34500081, 0x7e520281, 0xf0030108, 0x00021628, 0x7e300280, 0xf0010108,
    0x80021828, 0xbf8c3f70, 0xe0701200, 0x80000a03, 0xe0701204, 0x80000b03, 0xe0701208, 0x80000c03,
    0xe070120c, 0x80000d03, 0xe0701210, 0x80000e03, 0xe0701214, 0x80000f03, 0xe0701218, 0x80001003,
    0xe070121c, 0x80001103, 0xe0701220, 0x80001203, 0xe0701224, 0x80001303, 0xe0701228, 0x80001403,
    0xe070122c, 0x80001503, 0xe0701230, 0x80001603, 0xe0701234, 0x80001703, 0xe0701238, 0x80001803,
    0xe070123c, 0x80001903, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<std::uint32_t, 3>, 3> RefusedCode{{
    {0xf1820308u, 0x00820a04u, 0xbf810000u},
    {0xf0452108u, 0x00020a08u, 0xbf810000u},
    {0xf0010108u, 0x0002ff08u, 0xbf810000u},
}};

constexpr std::array<std::array<std::uint32_t, 3>, Threads> StorageInputs{{
    {0x00000000u, 0x00000000u, 0xb3c4904au},
    {0x00000000u, 0x00000005u, 0x00000000u},
    {0x00000005u, 0x00000000u, 0x7be90291u},
    {0xffffffffu, 0x00000001u, 0xffffffffu},
    {0x00000001u, 0xffffffffu, 0x9e5c77e7u},
    {0x7fffffffu, 0x00000001u, 0x7fffffffu},
    {0x80000000u, 0x80000000u, 0x4285a9afu},
    {0xffffffffu, 0xffffffffu, 0xffffffffu},
    {0x7df64fcfu, 0xf6453aa7u, 0x92191efbu},
    {0xd9895985u, 0x68518d38u, 0xd9895985u},
    {0xcf273511u, 0x138c146fu, 0x57100d0au},
    {0x2bb1b013u, 0xd6e649d7u, 0x2bb1b013u},
    {0xa95d2364u, 0x3976bad3u, 0x5bc5a782u},
    {0x4bcdebb2u, 0xa6480378u, 0x4bcdebb2u},
    {0xcbad05bau, 0xa433bde1u, 0xd1c28862u},
    {0x8f217978u, 0x860b0f2au, 0x8f217978u},
    {0x871aba07u, 0xb551a6ccu, 0x661dc63eu},
    {0x9a7f8d63u, 0x6e7133fbu, 0x9a7f8d63u},
    {0xce07ea31u, 0xe31b5068u, 0x9da9bf21u},
    {0x92b8b3e9u, 0xe399ac38u, 0x92b8b3e9u},
    {0x1b9abec0u, 0x1cbb3115u, 0x8d8433aeu},
    {0x613db786u, 0x7f5a7218u, 0x613db786u},
    {0x2084fc98u, 0xa69ea4ecu, 0x74859b95u},
    {0xb5b4acfcu, 0x1191b75bu, 0xb5b4acfcu},
    {0x8bec71e7u, 0xe0f3debeu, 0x77f93ffbu},
    {0x9f1254f3u, 0x48de1c56u, 0x9f1254f3u},
    {0xef1e9ff4u, 0xc7a1d8dbu, 0xb8ab9e91u},
    {0x9bcffc0fu, 0xc7bb053fu, 0x9bcffc0fu},
    {0x3c796f72u, 0xb5943014u, 0xf08eac42u},
    {0x436b6cdeu, 0x8333e5a8u, 0x436b6cdeu},
    {0x4457feb1u, 0xeaf89aefu, 0xfdcec3b5u},
    {0xdb26bcebu, 0x5d63880eu, 0xdb26bcebu},
}};

constexpr std::array<std::array<std::uint32_t, StorageResults>, Threads> StorageExpected{{
    {0x00000000u, 0xb3c4904au, 0x5a7a0005u, 0x5a7a0007u},
    {0x00000000u, 0x00000000u, 0x5a7a0005u, 0x5a7a0007u},
    {0x00000005u, 0x7be90291u, 0x5a7a0005u, 0x5a7a0007u},
    {0xffffffffu, 0xffffffffu, 0x5a7a0005u, 0x5a7a0007u},
    {0x00000001u, 0x9e5c77e7u, 0x5a7a0005u, 0x5a7a0007u},
    {0x7fffffffu, 0x7fffffffu, 0x5a7a0005u, 0x5a7a0007u},
    {0x80000000u, 0x4285a9afu, 0x5a7a0005u, 0x5a7a0007u},
    {0xffffffffu, 0xffffffffu, 0x5a7a0005u, 0x5a7a0007u},
    {0x7df64fcfu, 0x92191efbu, 0x5a7a0005u, 0x5a7a0007u},
    {0xd9895985u, 0xd9895985u, 0x5a7a0005u, 0x5a7a0007u},
    {0xcf273511u, 0x57100d0au, 0x5a7a0005u, 0x5a7a0007u},
    {0x2bb1b013u, 0x2bb1b013u, 0x5a7a0005u, 0x5a7a0007u},
    {0xa95d2364u, 0x5bc5a782u, 0x5a7a0005u, 0x5a7a0007u},
    {0x4bcdebb2u, 0x4bcdebb2u, 0x5a7a0005u, 0x5a7a0007u},
    {0xcbad05bau, 0xd1c28862u, 0x5a7a0005u, 0x5a7a0007u},
    {0x8f217978u, 0x8f217978u, 0x5a7a0005u, 0x5a7a0007u},
    {0x871aba07u, 0x661dc63eu, 0x5a7a0005u, 0x5a7a0007u},
    {0x9a7f8d63u, 0x9a7f8d63u, 0x5a7a0005u, 0x5a7a0007u},
    {0xce07ea31u, 0x9da9bf21u, 0x5a7a0005u, 0x5a7a0007u},
    {0x92b8b3e9u, 0x92b8b3e9u, 0x5a7a0005u, 0x5a7a0007u},
    {0x1b9abec0u, 0x8d8433aeu, 0x5a7a0005u, 0x5a7a0007u},
    {0x613db786u, 0x613db786u, 0x5a7a0005u, 0x5a7a0007u},
    {0x2084fc98u, 0x74859b95u, 0x5a7a0005u, 0x5a7a0007u},
    {0xb5b4acfcu, 0xb5b4acfcu, 0x5a7a0005u, 0x5a7a0007u},
    {0x8bec71e7u, 0x77f93ffbu, 0x5a7a0005u, 0x5a7a0007u},
    {0x9f1254f3u, 0x9f1254f3u, 0x5a7a0005u, 0x5a7a0007u},
    {0xef1e9ff4u, 0xb8ab9e91u, 0x5a7a0005u, 0x5a7a0007u},
    {0x9bcffc0fu, 0x9bcffc0fu, 0x5a7a0005u, 0x5a7a0007u},
    {0x3c796f72u, 0xf08eac42u, 0x5a7a0005u, 0x5a7a0007u},
    {0x436b6cdeu, 0x436b6cdeu, 0x5a7a0005u, 0x5a7a0007u},
    {0x4457feb1u, 0xfdcec3b5u, 0x5a7a0005u, 0x5a7a0007u},
    {0xdb26bcebu, 0xdb26bcebu, 0x5a7a0005u, 0x5a7a0007u},
}};

constexpr std::array<std::array<std::uint32_t, 3>, Threads> SampleInputs{{
    {0x3c000000u, 0x3e800000u, 0x00000000u},
    {0x3d200000u, 0x3e800000u, 0x00000000u},
    {0x3d900000u, 0x3e800000u, 0x00000000u},
    {0x3dd00000u, 0x3e800000u, 0x00000000u},
    {0x3e080000u, 0x3e800000u, 0x00000000u},
    {0x3e280000u, 0x3e800000u, 0x00000000u},
    {0x3e480000u, 0x3e800000u, 0x00000000u},
    {0x3e680000u, 0x3e800000u, 0x00000000u},
    {0x3e840000u, 0x3e800000u, 0x00000000u},
    {0x3e940000u, 0x3e800000u, 0x00000000u},
    {0x3ea40000u, 0x3e800000u, 0x00000000u},
    {0x3eb40000u, 0x3e800000u, 0x00000000u},
    {0x3ec40000u, 0x3e800000u, 0x00000000u},
    {0x3ed40000u, 0x3e800000u, 0x00000000u},
    {0x3ee40000u, 0x3e800000u, 0x00000000u},
    {0x3ef40000u, 0x3e800000u, 0x00000000u},
    {0x3f020000u, 0x3e800000u, 0x00000000u},
    {0x3f0a0000u, 0x3e800000u, 0x00000000u},
    {0x3f120000u, 0x3e800000u, 0x00000000u},
    {0x3f1a0000u, 0x3e800000u, 0x00000000u},
    {0x3f220000u, 0x3e800000u, 0x00000000u},
    {0x3f2a0000u, 0x3e800000u, 0x00000000u},
    {0x3f320000u, 0x3e800000u, 0x00000000u},
    {0x3f3a0000u, 0x3e800000u, 0x00000000u},
    {0x3f420000u, 0x3e800000u, 0x00000000u},
    {0x3f4a0000u, 0x3e800000u, 0x00000000u},
    {0x3f520000u, 0x3e800000u, 0x00000000u},
    {0x3f5a0000u, 0x3e800000u, 0x00000000u},
    {0x3f620000u, 0x3e800000u, 0x00000000u},
    {0x3f6a0000u, 0x3e800000u, 0x00000000u},
    {0x3f720000u, 0x3e800000u, 0x00000000u},
    {0x3f7a0000u, 0x3e800000u, 0x00000000u},
}};

constexpr std::array<std::array<std::uint32_t, SampleResults>, Threads> SampleExpected{{
    {0x00000000u, 0x5a7a000bu, 0x42c80000u, 0x42ca0000u, 0x3f800000u, 0x00000000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x00000000u, 0x5a7a0015u, 0x42c80000u, 0x5a7a0017u, 0x00005640u, 0x5a7a0019u},
    {0x40000000u, 0x5a7a000bu, 0x42cc0000u, 0x42ce0000u, 0x40400000u, 0x40000000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x40000000u, 0x5a7a0015u, 0x42cc0000u, 0x5a7a0017u, 0x00005660u, 0x5a7a0019u},
    {0x40800000u, 0x5a7a000bu, 0x42d00000u, 0x42d20000u, 0x40a00000u, 0x40800000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x40800000u, 0x5a7a0015u, 0x42d00000u, 0x5a7a0017u, 0x00005680u, 0x5a7a0019u},
    {0x40c00000u, 0x5a7a000bu, 0x42d40000u, 0x42d60000u, 0x40e00000u, 0x40c00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x40c00000u, 0x5a7a0015u, 0x42d40000u, 0x5a7a0017u, 0x000056a0u, 0x5a7a0019u},
    {0x41000000u, 0x5a7a000bu, 0x42d80000u, 0x42da0000u, 0x41100000u, 0x41000000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41000000u, 0x5a7a0015u, 0x42d80000u, 0x5a7a0017u, 0x000056c0u, 0x5a7a0019u},
    {0x41200000u, 0x5a7a000bu, 0x42dc0000u, 0x42de0000u, 0x41300000u, 0x41200000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41200000u, 0x5a7a0015u, 0x42dc0000u, 0x5a7a0017u, 0x000056e0u, 0x5a7a0019u},
    {0x41400000u, 0x5a7a000bu, 0x42e00000u, 0x42e20000u, 0x41500000u, 0x41400000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41400000u, 0x5a7a0015u, 0x42e00000u, 0x5a7a0017u, 0x00005700u, 0x5a7a0019u},
    {0x41600000u, 0x5a7a000bu, 0x42e40000u, 0x42e60000u, 0x41700000u, 0x41600000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41600000u, 0x5a7a0015u, 0x42e40000u, 0x5a7a0017u, 0x00005720u, 0x5a7a0019u},
    {0x41800000u, 0x5a7a000bu, 0x42e80000u, 0x42ea0000u, 0x41880000u, 0x41800000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41800000u, 0x5a7a0015u, 0x42e80000u, 0x5a7a0017u, 0x00005740u, 0x5a7a0019u},
    {0x41900000u, 0x5a7a000bu, 0x42ec0000u, 0x42ee0000u, 0x41980000u, 0x41900000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41900000u, 0x5a7a0015u, 0x42ec0000u, 0x5a7a0017u, 0x00005760u, 0x5a7a0019u},
    {0x41a00000u, 0x5a7a000bu, 0x42f00000u, 0x42f20000u, 0x41a80000u, 0x41a00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41a00000u, 0x5a7a0015u, 0x42f00000u, 0x5a7a0017u, 0x00005780u, 0x5a7a0019u},
    {0x41b00000u, 0x5a7a000bu, 0x42f40000u, 0x42f60000u, 0x41b80000u, 0x41b00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41b00000u, 0x5a7a0015u, 0x42f40000u, 0x5a7a0017u, 0x000057a0u, 0x5a7a0019u},
    {0x41c00000u, 0x5a7a000bu, 0x42f80000u, 0x42fa0000u, 0x41c80000u, 0x41c00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41c00000u, 0x5a7a0015u, 0x42f80000u, 0x5a7a0017u, 0x000057c0u, 0x5a7a0019u},
    {0x41d00000u, 0x5a7a000bu, 0x42fc0000u, 0x42fe0000u, 0x41d80000u, 0x41d00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41d00000u, 0x5a7a0015u, 0x42fc0000u, 0x5a7a0017u, 0x000057e0u, 0x5a7a0019u},
    {0x41e00000u, 0x5a7a000bu, 0x43000000u, 0x43010000u, 0x41e80000u, 0x41e00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41e00000u, 0x5a7a0015u, 0x43000000u, 0x5a7a0017u, 0x00005800u, 0x5a7a0019u},
    {0x41f00000u, 0x5a7a000bu, 0x43020000u, 0x43030000u, 0x41f80000u, 0x41f00000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x41f00000u, 0x5a7a0015u, 0x43020000u, 0x5a7a0017u, 0x00005810u, 0x5a7a0019u},
    {0x42000000u, 0x5a7a000bu, 0x43040000u, 0x43050000u, 0x42040000u, 0x42000000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42000000u, 0x5a7a0015u, 0x43040000u, 0x5a7a0017u, 0x00005820u, 0x5a7a0019u},
    {0x42080000u, 0x5a7a000bu, 0x43060000u, 0x43070000u, 0x420c0000u, 0x42080000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42080000u, 0x5a7a0015u, 0x43060000u, 0x5a7a0017u, 0x00005830u, 0x5a7a0019u},
    {0x42100000u, 0x5a7a000bu, 0x43080000u, 0x43090000u, 0x42140000u, 0x42100000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42100000u, 0x5a7a0015u, 0x43080000u, 0x5a7a0017u, 0x00005840u, 0x5a7a0019u},
    {0x42180000u, 0x5a7a000bu, 0x430a0000u, 0x430b0000u, 0x421c0000u, 0x42180000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42180000u, 0x5a7a0015u, 0x430a0000u, 0x5a7a0017u, 0x00005850u, 0x5a7a0019u},
    {0x42200000u, 0x5a7a000bu, 0x430c0000u, 0x430d0000u, 0x42240000u, 0x42200000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42200000u, 0x5a7a0015u, 0x430c0000u, 0x5a7a0017u, 0x00005860u, 0x5a7a0019u},
    {0x42280000u, 0x5a7a000bu, 0x430e0000u, 0x430f0000u, 0x422c0000u, 0x42280000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42280000u, 0x5a7a0015u, 0x430e0000u, 0x5a7a0017u, 0x00005870u, 0x5a7a0019u},
    {0x42300000u, 0x5a7a000bu, 0x43100000u, 0x43110000u, 0x42340000u, 0x42300000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42300000u, 0x5a7a0015u, 0x43100000u, 0x5a7a0017u, 0x00005880u, 0x5a7a0019u},
    {0x42380000u, 0x5a7a000bu, 0x43120000u, 0x43130000u, 0x423c0000u, 0x42380000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42380000u, 0x5a7a0015u, 0x43120000u, 0x5a7a0017u, 0x00005890u, 0x5a7a0019u},
    {0x42400000u, 0x5a7a000bu, 0x43140000u, 0x43150000u, 0x42440000u, 0x42400000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42400000u, 0x5a7a0015u, 0x43140000u, 0x5a7a0017u, 0x000058a0u, 0x5a7a0019u},
    {0x42480000u, 0x5a7a000bu, 0x43160000u, 0x43170000u, 0x424c0000u, 0x42480000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42480000u, 0x5a7a0015u, 0x43160000u, 0x5a7a0017u, 0x000058b0u, 0x5a7a0019u},
    {0x42500000u, 0x5a7a000bu, 0x43180000u, 0x43190000u, 0x42540000u, 0x42500000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42500000u, 0x5a7a0015u, 0x43180000u, 0x5a7a0017u, 0x000058c0u, 0x5a7a0019u},
    {0x42580000u, 0x5a7a000bu, 0x431a0000u, 0x431b0000u, 0x425c0000u, 0x42580000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42580000u, 0x5a7a0015u, 0x431a0000u, 0x5a7a0017u, 0x000058d0u, 0x5a7a0019u},
    {0x42600000u, 0x5a7a000bu, 0x431c0000u, 0x431d0000u, 0x42640000u, 0x42600000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42600000u, 0x5a7a0015u, 0x431c0000u, 0x5a7a0017u, 0x000058e0u, 0x5a7a0019u},
    {0x42680000u, 0x5a7a000bu, 0x431e0000u, 0x431f0000u, 0x426c0000u, 0x42680000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42680000u, 0x5a7a0015u, 0x431e0000u, 0x5a7a0017u, 0x000058f0u, 0x5a7a0019u},
    {0x42700000u, 0x5a7a000bu, 0x43200000u, 0x43210000u, 0x42740000u, 0x42700000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42700000u, 0x5a7a0015u, 0x43200000u, 0x5a7a0017u, 0x00005900u, 0x5a7a0019u},
    {0x42780000u, 0x5a7a000bu, 0x43220000u, 0x43230000u, 0x427c0000u, 0x42780000u, 0x5a7a0010u, 0x00000040u, 0x00000002u, 0x5a7a0013u, 0x42780000u, 0x5a7a0015u, 0x43220000u, 0x5a7a0017u, 0x00005910u, 0x5a7a0019u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t width, std::uint32_t height) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void FillBuffer(const std::array<std::array<std::uint32_t, 3>, Threads>& inputs) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::copy(inputs[tid].begin(), inputs[tid].end(), Buffer.begin() + tid * Inputs);
        Buffer[tid * Inputs + 3u] = 0u;
    }
}

void RunStorage(AgcDriver::VulkanDevice& device) {
    FillBuffer(StorageInputs);
    StorageTexels.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(StorageTexels.data(), Format32UInt, StorageWidth, StorageHeight);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    Dispatch(device, StorageCode, userData);
}

void RunSample(AgcDriver::VulkanDevice& device) {
    FillBuffer(SampleInputs);
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format32Float, SampleWidth, SampleHeight, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= SampleTexels.size() * 4u, "image texel status: the texture does not fit the texel storage");
    SampleTexels.fill(0u);
    for (std::uint32_t y = 0; y < SampleHeight; ++y) {
        for (std::uint32_t x = 0; x < SampleWidth; ++x) {
            SampleTexels[(mips[0].tiledOffset + static_cast<std::uint64_t>(y) * mips[0].pitchBytes) / 4u + x] = std::bit_cast<std::uint32_t>(static_cast<float>(x + 100u * y));
        }
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(SampleTexels.data(), Format32Float, SampleWidth, SampleHeight);
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    Dispatch(device, SampleCode, userData);
}

void CheckRefused(AgcDriver::VulkanDevice& device) {
    constexpr std::array<const char*, 3> names{"image_get_lod lwe", "image_atomic_add tfe", "image_load tfe status past v255"};
    std::vector<std::uint32_t> userData(16, 0u);
    for (std::uint32_t i = 0; i < RefusedCode.size(); ++i) {
        const std::span<const std::uint32_t> code(RefusedCode[i]);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device.Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        bool refused = false;
        try {
            (void)ShaderRecompiler::Recompile(request);
        } catch (const std::exception& error) {
            refused = std::string(error.what()).find("MIMG") != std::string::npos;
        }
        Require(refused, std::string(names[i]) + " was not refused");
    }
}

void Expect(const char* name, std::uint32_t tid, std::uint32_t actual, std::uint32_t expected) {
    char message[160];
    std::snprintf(message, sizeof(message), "%s: thread %u is 0x%08x, expected 0x%08x", name, tid, actual, expected);
    Require(actual == expected, message);
}

void CheckStorage() {
    constexpr std::array<const char*, StorageResults> names{"image_store tfe", "image_store tfe lwe", "image_store tfe data status", "image_store tfe lwe data status"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t j = 0; j < StorageResults; ++j) {
            Expect(names[j], tid, Buffer[Threads * Inputs + tid * Results + j], StorageExpected[tid][j]);
        }
    }
}

void CheckSample() {
    constexpr std::array<const char*, SampleResults> names{"image_sample tfe lwe", "image_sample tfe lwe status", "image_gather4 tfe lwe x", "image_gather4 tfe lwe y", "image_gather4 tfe lwe z", "image_gather4 tfe lwe w", "image_gather4 tfe lwe status", "image_get_resinfo tfe lwe width", "image_get_resinfo tfe lwe height", "image_get_resinfo tfe lwe status", "image_sample_l tfe", "image_sample_l tfe status", "image_load tfe lwe", "image_load tfe lwe status", "image_load tfe d16", "image_load tfe d16 status"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t j = 0; j < SampleResults; ++j) {
            Expect(names[j], tid, Buffer[Threads * Inputs + tid * Results + j], SampleExpected[tid][j]);
        }
    }
}


const Testing::Case storageStatus{"ImageTexelStatus_StorageStoresWithTfeAndLwe_ReportExpectedStatus", [] {
    auto& device = SharedVulkanTestDevice();
    RunStorage(device);
    CheckStorage();
}};

const Testing::Case sampleStatus{"ImageTexelStatus_SamplesWithTfeAndLwe_ReturnExpectedDataAndStatus", [] {
    auto& device = SharedVulkanTestDevice();
    RunSample(device);
    CheckSample();
}};

const Testing::Case unsupportedRefused{"ImageTexelStatus_UnsupportedStatusForms_AreRefused", [] {
    auto& device = SharedVulkanTestDevice();
    CheckRefused(device);
}};

} // namespace
