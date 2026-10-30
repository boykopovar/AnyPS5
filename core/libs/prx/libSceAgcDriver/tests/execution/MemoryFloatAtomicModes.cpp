#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 12;
constexpr std::uint32_t Results = 32;
constexpr std::uint32_t TargetDwords = 512;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 4096;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results + TargetDwords> Output{};
alignas(4096) std::array<std::uint32_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 112> BufferCode{
    0x160200b0u, 0x34180087u, 0xe0381000u, 0x80000201u, 0xe0381010u, 0x80000601u, 0xe0341020u, 0x80000a01u,
    0xbf8c3f70u, 0x341a0082u, 0x4a1a1affu, 0x00001000u, 0x341c0083u, 0x4a1c1cffu, 0x00001200u, 0xe0701000u,
    0x8001020du, 0xbbfd0000u, 0x7e200303u, 0x7e220304u, 0xe0fc5000u, 0x8001100du, 0xbf8c3f70u, 0xe0305000u,
    0x8001120du, 0xbf8c3f70u, 0xe0701000u, 0x8001100cu, 0xe0701004u, 0x8001120cu, 0xe0701080u, 0x8001020du,
    0xbbfd0000u, 0x7e200303u, 0x7e220304u, 0xe1005080u, 0x8001100du, 0xbf8c3f70u, 0xe0305080u, 0x8001120du,
    0xbf8c3f70u, 0xe0701008u, 0x8001100cu, 0xe070100cu, 0x8001120cu, 0xe0701100u, 0x8001020du, 0xbbfd0000u,
    0x7e200303u, 0x7e220304u, 0xe0f85100u, 0x8001100du, 0xbf8c3f70u, 0xe0305100u, 0x8001120du, 0xbf8c3f70u,
    0xe0701010u, 0x8001100cu, 0xe0701014u, 0x8001120cu, 0xe0741000u, 0x8001060eu, 0xbbfd0000u, 0x7e280308u,
    0x7e2a0309u, 0x7e2c030au, 0x7e2e030bu, 0xe17c5000u, 0x8001140eu, 0xbf8c3f70u, 0xe0345000u, 0x8001180eu,
    0xbf8c3f70u, 0xe0741018u, 0x8001140cu, 0xe0741020u, 0x8001180cu, 0xe0741100u, 0x8001060eu, 0xbbfd0000u,
    0x7e280308u, 0x7e2a0309u, 0x7e2c030au, 0x7e2e030bu, 0xe1805100u, 0x8001140eu, 0xbf8c3f70u, 0xe0345100u,
    0x8001180eu, 0xbf8c3f70u, 0xe0741028u, 0x8001140cu, 0xe0741030u, 0x8001180cu, 0xe0741200u, 0x8001060eu,
    0xbbfd0000u, 0x7e280308u, 0x7e2a0309u, 0x7e2c030au, 0x7e2e030bu, 0xe1785200u, 0x8001140eu, 0xbf8c3f70u,
    0xe0345200u, 0x8001180eu, 0xbf8c3f70u, 0xe0741038u, 0x8001140cu, 0xe0741040u, 0x8001180cu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 59> ImageCode{
    0x160200b0u, 0x34180087u, 0xe0381000u, 0x80000201u, 0xe0381010u, 0x80000601u, 0xe0341020u, 0x80000a01u,
    0xbf8c3f70u, 0x7e3c0300u, 0x7e3e0280u, 0x7e260302u, 0xf03c0108u, 0x0002131eu, 0x7e200303u, 0x7e220304u,
    0xf0782108u, 0x0002101eu, 0x7e240280u, 0xf0442108u, 0x0002121eu, 0xbf8c3f70u, 0xe0701000u, 0x8001100cu,
    0xe0701004u, 0x8001120cu, 0x7e3e0281u, 0x7e260302u, 0xf03c0108u, 0x0002131eu, 0x7e200303u, 0x7e220304u,
    0xf07c2108u, 0x0002101eu, 0x7e240280u, 0xf0442108u, 0x0002121eu, 0xbf8c3f70u, 0xe0701008u, 0x8001100cu,
    0xe070100cu, 0x8001120cu, 0x7e3e0282u, 0x7e260302u, 0xf03c0108u, 0x0002131eu, 0x7e200303u, 0x7e220304u,
    0xf0742308u, 0x0002101eu, 0x7e240280u, 0xf0442108u, 0x0002121eu, 0xbf8c3f70u, 0xe0701010u, 0x8001100cu,
    0xe0701014u, 0x8001120cu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 52> GlobalCode{
    0x160200b0u, 0x34180087u, 0xe0381000u, 0x80000201u, 0xe0381010u, 0x80000601u, 0xe0341020u, 0x80000a01u,
    0xbf8c3f70u, 0x34340082u, 0xd70f6a1au, 0x00023408u, 0x7e360209u, 0x50363680u, 0x34380083u, 0xd70f6a1cu,
    0x00023808u, 0x7e3a0209u, 0x503a3a80u, 0xdc708000u, 0x007d021au, 0xbbfd0000u, 0x7e200303u, 0x7e220304u,
    0xdcfd8000u, 0x137d101au, 0xbf8c3f70u, 0xdc318000u, 0x127d001au, 0xbf8c3f70u, 0xe0701000u, 0x8001130cu,
    0xe0701004u, 0x8001120cu, 0xdc748200u, 0x007d061cu, 0xbbfd0000u, 0x7e280308u, 0x7e2a0309u, 0x7e2c030au,
    0x7e2e030bu, 0xdd7d8200u, 0x207d141cu, 0xbf8c3f70u, 0xdc358200u, 0x187d001cu, 0xbf8c3f70u, 0xe0741018u,
    0x8001200cu, 0xe0741020u, 0x8001180cu, 0xbf810000u,
};

constexpr std::uint32_t Rows32[Threads][3] = {
    {0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x80000000u},
    {0x80000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000001u, 0x00000000u},
    {0x00000000u, 0x80000001u, 0x80000001u},
    {0x00000001u, 0x00000000u, 0x00000001u},
    {0x00000001u, 0x3f800000u, 0x3f800000u},
    {0x80000001u, 0x80000000u, 0x80000000u},
    {0x00000001u, 0x80000001u, 0x80000001u},
    {0x007fffffu, 0x00800000u, 0x007fffffu},
    {0x807fffffu, 0xbf800000u, 0x807fffffu},
    {0x7fc00000u, 0x00000001u, 0x7fc00000u},
    {0x00000001u, 0x7fc00000u, 0x00000001u},
    {0x7f800001u, 0x00000001u, 0x7f800001u},
    {0x00000001u, 0x7f800001u, 0x00000001u},
    {0x3f800000u, 0xbf800000u, 0x3f800000u},
    {0x7f800000u, 0xff800000u, 0xff800000u},
    {0x80000000u, 0x00000001u, 0x80000000u},
    {0x7fc12345u, 0x7fc00000u, 0x7fc12345u},
    {0xffc00000u, 0x3f800000u, 0x3f800000u},
    {0x7fa00000u, 0x3f800000u, 0x3f800000u},
    {0x40490fdbu, 0x007fffffu, 0x40490fdbu},
    {0x807fffffu, 0x80000001u, 0x80000001u},
    {0x00800000u, 0x007fffffu, 0x00800000u},
    {0x00000000u, 0xff800001u, 0xff800001u},
    {0x80000001u, 0x00000001u, 0x80000001u},
    {0x7f7fffffu, 0xff7fffffu, 0xff7fffffu},
    {0x00000001u, 0x00000001u, 0x00000001u},
    {0x007fffffu, 0x80000000u, 0x80000000u},
    {0xff800000u, 0x80000001u, 0x80000001u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x3f800000u, 0x00000001u, 0x3f800000u}
};

constexpr std::uint64_t Rows64[Threads][3] = {
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
    {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x8000000000000001ull, 0x8000000000000001ull},
    {0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x0000000000000001ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {0x8000000000000001ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {0x0000000000000001ull, 0x8000000000000001ull, 0x8000000000000001ull},
    {0x000fffffffffffffull, 0x0010000000000000ull, 0x000fffffffffffffull},
    {0x800fffffffffffffull, 0xbff0000000000000ull, 0x800fffffffffffffull},
    {0x7ff8000000000000ull, 0x0000000000000001ull, 0x7ff8000000000000ull},
    {0x0000000000000001ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0x7ff0000000000001ull, 0x0000000000000001ull, 0x7ff0000000000001ull},
    {0x0000000000000001ull, 0x7ff0000000000001ull, 0x7ff0000000000001ull},
    {0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff0000000000000ull},
    {0x8000000000000000ull, 0x0000000000000001ull, 0x8000000000000000ull},
    {0x7ff8000000012345ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0xfff8000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {0xfff0000000000001ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {0x400921fb54442d18ull, 0x000fffffffffffffull, 0x400921fb54442d18ull},
    {0x800fffffffffffffull, 0x8000000000000001ull, 0x8000000000000001ull},
    {0x0010000000000000ull, 0x000fffffffffffffull, 0x0010000000000000ull},
    {0x0000000000000000ull, 0xfff0000000000001ull, 0x0000000000000000ull},
    {0x8000000000000001ull, 0x0000000000000001ull, 0x8000000000000001ull},
    {0x7fefffffffffffffull, 0xfff0000000000000ull, 0x7fefffffffffffffull},
    {0x0000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x000fffffffffffffull, 0x8000000000000000ull, 0x8000000000000000ull},
    {0xfff0000000000000ull, 0x8000000000000001ull, 0x8000000000000001ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0x3ff0000000000000ull, 0x0000000000000001ull, 0x3ff0000000000000ull}
};

constexpr std::uint32_t Flushed32[Threads][3] = {
    {0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000001u},
    {0x80000000u, 0x00000000u, 0x80000001u},
    {0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x3f800000u, 0x00000001u},
    {0x80000000u, 0x80000000u, 0x80000000u},
    {0x80000000u, 0x00000000u, 0x80000001u},
    {0x00000000u, 0x00800000u, 0x00800000u},
    {0xbf800000u, 0x80000000u, 0xbf800000u},
    {0x00000000u, 0x00000000u, 0x7fc00000u},
    {0x00000000u, 0x00000000u, 0x7fc00000u},
    {0x7fc00001u, 0x7fc00001u, 0x7f800001u},
    {0x7fc00001u, 0x7fc00001u, 0x7f800001u},
    {0xbf800000u, 0x3f800000u, 0xbf800000u},
    {0xff800000u, 0x7f800000u, 0x7f800000u},
    {0x80000000u, 0x00000000u, 0x00000001u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc12345u},
    {0x3f800000u, 0x3f800000u, 0xffc00000u},
    {0x7fe00000u, 0x7fe00000u, 0x7fa00000u},
    {0x00000000u, 0x40490fdbu, 0x007fffffu},
    {0x80000000u, 0x80000000u, 0x80000001u},
    {0x00000000u, 0x00800000u, 0x007fffffu},
    {0xffc00001u, 0xffc00001u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x00000001u},
    {0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu},
    {0x00000000u, 0x00000000u, 0x00000001u},
    {0x80000000u, 0x00000000u, 0x80000000u},
    {0xff800000u, 0x80000000u, 0xff800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x00000000u, 0x3f800000u, 0x00000001u}
};

constexpr std::uint32_t Kept32[Threads][3] = {
    {0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000001u, 0x00000001u},
    {0x80000001u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000001u, 0x00000000u},
    {0x00000001u, 0x3f800000u, 0x00000001u},
    {0x80000001u, 0x80000000u, 0x80000001u},
    {0x80000001u, 0x00000001u, 0x00000001u},
    {0x007fffffu, 0x00800000u, 0x00800000u},
    {0xbf800000u, 0x807fffffu, 0xbf800000u},
    {0x00000001u, 0x00000001u, 0x7fc00000u},
    {0x00000001u, 0x00000001u, 0x7fc00000u},
    {0x7fc00001u, 0x7fc00001u, 0x7f800001u},
    {0x7fc00001u, 0x7fc00001u, 0x7f800001u},
    {0xbf800000u, 0x3f800000u, 0xbf800000u},
    {0xff800000u, 0x7f800000u, 0x7f800000u},
    {0x80000000u, 0x00000001u, 0x00000001u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc12345u},
    {0x3f800000u, 0x3f800000u, 0xffc00000u},
    {0x7fe00000u, 0x7fe00000u, 0x7fa00000u},
    {0x007fffffu, 0x40490fdbu, 0x007fffffu},
    {0x807fffffu, 0x80000001u, 0x807fffffu},
    {0x007fffffu, 0x00800000u, 0x007fffffu},
    {0xffc00001u, 0xffc00001u, 0x00000000u},
    {0x80000001u, 0x00000001u, 0x00000001u},
    {0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu},
    {0x00000001u, 0x00000001u, 0x00000001u},
    {0x80000000u, 0x007fffffu, 0x007fffffu},
    {0xff800000u, 0x80000001u, 0xff800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x00000001u, 0x3f800000u, 0x00000001u}
};

constexpr std::uint64_t Flushed64[Threads][3] = {
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000001ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x3ff0000000000000ull, 0x0000000000000001ull},
    {0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000001ull},
    {0x0000000000000000ull, 0x0010000000000000ull, 0x0010000000000000ull},
    {0xbff0000000000000ull, 0x8000000000000000ull, 0xbff0000000000000ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x7ff8000000000000ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0x7ff0000000000001ull},
    {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0x0000000000000001ull},
    {0xbff0000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {0xfff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000012345ull},
    {0x3ff0000000000000ull, 0x3ff0000000000000ull, 0xfff8000000000000ull},
    {0xfff8000000000001ull, 0xfff8000000000001ull, 0xfff0000000000001ull},
    {0x0000000000000000ull, 0x400921fb54442d18ull, 0x000fffffffffffffull},
    {0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000001ull},
    {0x0000000000000000ull, 0x0010000000000000ull, 0x000fffffffffffffull},
    {0xfff8000000000001ull, 0xfff8000000000001ull, 0xfff0000000000001ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0xfff0000000000000ull, 0x7fefffffffffffffull, 0xfff0000000000000ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
    {0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0x0000000000000000ull, 0x3ff0000000000000ull, 0x0000000000000001ull}
};

constexpr std::uint64_t Kept64[Threads][3] = {
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
    {0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x8000000000000001ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull},
    {0x0000000000000001ull, 0x3ff0000000000000ull, 0x0000000000000001ull},
    {0x8000000000000001ull, 0x8000000000000000ull, 0x8000000000000001ull},
    {0x8000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x000fffffffffffffull, 0x0010000000000000ull, 0x0010000000000000ull},
    {0xbff0000000000000ull, 0x800fffffffffffffull, 0xbff0000000000000ull},
    {0x0000000000000001ull, 0x0000000000000001ull, 0x7ff8000000000000ull},
    {0x0000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0x7ff0000000000001ull},
    {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0x0000000000000001ull},
    {0xbff0000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {0xfff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull},
    {0x8000000000000000ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000012345ull},
    {0x3ff0000000000000ull, 0x3ff0000000000000ull, 0xfff8000000000000ull},
    {0xfff8000000000001ull, 0xfff8000000000001ull, 0xfff0000000000001ull},
    {0x000fffffffffffffull, 0x400921fb54442d18ull, 0x000fffffffffffffull},
    {0x800fffffffffffffull, 0x8000000000000001ull, 0x800fffffffffffffull},
    {0x000fffffffffffffull, 0x0010000000000000ull, 0x000fffffffffffffull},
    {0xfff8000000000001ull, 0xfff8000000000001ull, 0xfff0000000000001ull},
    {0x8000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0xfff0000000000000ull, 0x7fefffffffffffffull, 0xfff0000000000000ull},
    {0x0000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull},
    {0x8000000000000000ull, 0x000fffffffffffffull, 0x000fffffffffffffull},
    {0xfff0000000000000ull, 0x8000000000000001ull, 0xfff0000000000000ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0x0000000000000001ull, 0x3ff0000000000000ull, 0x0000000000000001ull}
};

constexpr std::array<const char*, 3> Names{"fmin", "fmax", "fcmpswap"};

enum class Kind { Buffer, Image, Global };

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "memory float atomic modes: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() { return block; }
private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Threads - 1u) & 3u) << 30u),
        ((Threads - 1u) >> 2u) | (2u << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::string Hex(std::uint64_t value, int digits) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%0*llx", digits, static_cast<unsigned long long>(value));
    return text;
}

const char* KindName(Kind kind) {
    return kind == Kind::Buffer ? "buffer" : kind == Kind::Image ? "image" : "global";
}

void FillInput() {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* row = &Input[tid * Inputs];
        row[0] = Rows32[tid][0];
        row[1] = Rows32[tid][1];
        row[2] = Rows32[tid][2];
        for (std::uint32_t i = 0; i < 3u; ++i) {
            row[4u + 2u * i] = static_cast<std::uint32_t>(Rows64[tid][i]);
            row[5u + 2u * i] = static_cast<std::uint32_t>(Rows64[tid][i] >> 32u);
        }
    }
}

void Run(AgcDriver::VulkanDevice& device, Kind kind, GuestBlock& guest, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    Output.fill(0xdeadbeefu);
    Texels.fill(0xdeadbeefu);
    std::memset(guest.Data(), 0xcd, BlockBytes);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    if (kind == Kind::Image) {
        const auto texture = TextureDescriptor(Texels.data());
        std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    } else if (kind == Kind::Global) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
        userData[8] = static_cast<std::uint32_t>(address);
        userData[9] = static_cast<std::uint32_t>(address >> 32u);
    }
    const std::span<const std::uint32_t> code = kind == Kind::Buffer ? std::span<const std::uint32_t>(BufferCode) : kind == Kind::Image ? std::span<const std::uint32_t>(ImageCode) : std::span<const std::uint32_t>(GlobalCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(Kind kind, const char* mode, bool flush32, bool flush64) {
    const auto where = std::string("memory float atomic modes: ") + KindName(kind) + ", " + mode + ", lane ";
    const std::uint32_t ops = kind == Kind::Global ? 1u : 3u;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t op = 0; op < ops; ++op) {
            const std::uint32_t expected = flush32 ? Flushed32[tid][op] : Kept32[tid][op];
            Require(out[2u * op] == Rows32[tid][0], where + std::to_string(tid) + " " + Names[op] + " returned " + Hex(out[2u * op], 8) + ", expected " + Hex(Rows32[tid][0], 8));
            Require(out[2u * op + 1u] == expected, where + std::to_string(tid) + " " + Names[op] + " stored " + Hex(out[2u * op + 1u], 8) + ", expected " + Hex(expected, 8));
        }
        if (kind == Kind::Image) continue;
        for (std::uint32_t op = 0; op < ops; ++op) {
            const std::uint64_t returned = out[6u + 4u * op] | (static_cast<std::uint64_t>(out[7u + 4u * op]) << 32u);
            const std::uint64_t stored = out[8u + 4u * op] | (static_cast<std::uint64_t>(out[9u + 4u * op]) << 32u);
            const std::uint64_t expected = flush64 ? Flushed64[tid][op] : Kept64[tid][op];
            Require(returned == Rows64[tid][0], where + std::to_string(tid) + " " + Names[op] + "_x2 returned " + Hex(returned, 16) + ", expected " + Hex(Rows64[tid][0], 16));
            Require(stored == expected, where + std::to_string(tid) + " " + Names[op] + "_x2 stored " + Hex(stored, 16) + ", expected " + Hex(expected, 16));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics)) {
            std::puts("skipped, the device has no shaderBufferInt64Atomics");
            return VulkanTestSkipped;
        }
        GuestBlock guest;
        FillInput();
        struct Mode {
            std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
            const char* name;
            bool flush32;
            bool flush64;
            bool global;
        };
        const std::array<Mode, 6> modes{{
            {std::nullopt, "no float mode", true, false, false},
            {ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, "FLOAT_MODE 0xc0", true, false, false},
            {ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, "FLOAT_MODE 0xf0", false, false, false},
            {ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false}, "FLOAT_MODE 0x00", true, true, true},
            {ShaderRecompiler::ShaderFloatMode{0x50u, true, false, false}, "FLOAT_MODE 0x50", false, false, false},
            {ShaderRecompiler::ShaderFloatMode{0xa0u, true, false, false}, "FLOAT_MODE 0xa0", true, true, false},
        }};
        for (const auto kind : {Kind::Buffer, Kind::Image, Kind::Global}) {
            for (const auto& mode : modes) {
                if (kind == Kind::Global && !mode.global) continue;
                Run(*device, kind, guest, mode.floatMode);
                Check(kind, mode.name, mode.flush32, mode.flush64);
            }
        }
        std::puts("memory float atomic modes tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
