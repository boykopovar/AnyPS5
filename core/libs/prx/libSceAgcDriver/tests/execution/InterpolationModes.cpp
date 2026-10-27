#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <span>
#include <string>

namespace {

using namespace ShaderRecompiler;
using AgcDriver::Graphics::Require;

struct Row {
    std::array<std::uint32_t, 9> input;
    std::array<std::array<std::uint32_t, 3>, 8> expected;
};

constexpr std::array<const char*, 8> ModeNames{"IEEE=0 f32-flush f16-flush", "IEEE=0 f32-flush f16-keep", "IEEE=0 f32-keep f16-flush", "IEEE=0 f32-keep f16-keep",
    "IEEE=1 f32-flush f16-flush", "IEEE=1 f32-flush f16-keep", "IEEE=1 f32-keep f16-flush", "IEEE=1 f32-keep f16-keep"};
constexpr std::array<std::uint32_t, 8> ModeFlags{
    InterpolationFlush32 | InterpolationFlush16, InterpolationFlush32, InterpolationFlush16, 0u,
    InterpolationQuiet | InterpolationFlush32 | InterpolationFlush16, InterpolationQuiet | InterpolationFlush32, InterpolationQuiet | InterpolationFlush16, InterpolationQuiet};
constexpr std::array<const char*, 3> OpNames{"v_interp_p1ll_f16", "v_interp_p2_f16", "v_interp_p1lv_f16"};

constexpr std::array<Row, 32> Rows{{
    {{0x33800000u, 0x7fa00000u, 0x43000000u, 0x33800000u, 0x4f000000u, 0xabcd0001u, 0x33800000u, 0x7fa00000u, 0x33800000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00005800u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00005800u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00005800u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00005800u, 0x7fe00000u}}}},
    {{0xbf800000u, 0x00000001u, 0x7fa00000u, 0xbf800000u, 0x477ff000u, 0xabcd8200u, 0xbf800000u, 0x00000001u, 0xb8000000u}, {{{0x7fa00000u, 0x0000fc00u, 0x80000000u}, {0x7fa00000u, 0x0000fc00u, 0xb8000000u}, {0x7fa00000u, 0x0000fc00u, 0x80000001u}, {0x7fa00000u, 0x0000fc00u, 0xb8000000u}, {0x7fe00000u, 0x0000fc00u, 0x80000000u}, {0x7fe00000u, 0x0000fc00u, 0xb8000000u}, {0x7fe00000u, 0x0000fc00u, 0x80000001u}, {0x7fe00000u, 0x0000fc00u, 0xb8000000u}}}},
    {{0x33800000u, 0x7fa00000u, 0xff800000u, 0x33800000u, 0x7f7fffffu, 0xabcdbc00u, 0x33800000u, 0x7fa00000u, 0xbf800000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00007c00u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00007c00u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00007c00u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00007c00u, 0x7fe00000u}}}},
    {{0xb8000000u, 0x7fa00000u, 0x80000000u, 0xb8000000u, 0x7f7fffffu, 0xabcdbc00u, 0xb8000000u, 0x7fa00000u, 0xbf800000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0x33800000u, 0xff800000u, 0xff800000u, 0x33800000u, 0x7f800000u, 0xabcd0000u, 0x33800000u, 0xff800000u, 0x00000000u}, {{{0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xff800000u, 0x00007c00u, 0xff800000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xff800000u, 0x00007c00u, 0xff800000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xff800000u, 0x00007c00u, 0xff800000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xff800000u, 0x00007c00u, 0xff800000u}}}},
    {{0x33800000u, 0x7f800000u, 0xffc02000u, 0x33800000u, 0x47800000u, 0xabcd8000u, 0x33800000u, 0x7f800000u, 0x80000000u}, {{{0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc02000u, 0x00001c00u, 0x7f800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc02000u, 0x00001c00u, 0x7f800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc02000u, 0x00001c00u, 0x7f800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc02000u, 0x00001c00u, 0x7f800000u}}}},
    {{0xb8000000u, 0x477ff000u, 0x00000000u, 0xb8000000u, 0x4f000000u, 0xabcd5800u, 0xb8000000u, 0x477ff000u, 0x43000000u}, {{{0x00000000u, 0x00008000u, 0x43000000u}, {0xbffff000u, 0x0000fc00u, 0x42fc0040u}, {0x00000000u, 0x00008000u, 0x43000000u}, {0xbffff000u, 0x0000fc00u, 0x42fc0040u}, {0x00000000u, 0x00008000u, 0x43000000u}, {0xbffff000u, 0x0000fc00u, 0x42fc0040u}, {0x00000000u, 0x00008000u, 0x43000000u}, {0xbffff000u, 0x0000fc00u, 0x42fc0040u}}}},
    {{0x33800000u, 0x7fa00000u, 0x477fe000u, 0x33800000u, 0x47800000u, 0xabcd7d00u, 0x33800000u, 0x7fa00000u, 0x7fa00000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00001c00u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00001c00u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00001c00u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00001c00u, 0x7fe00000u}}}},
    {{0xb8000000u, 0x47800000u, 0x7fa00000u, 0xb8000000u, 0x7f7fffffu, 0xabcd7bffu, 0xb8000000u, 0x47800000u, 0x477fe000u}, {{{0x7fa00000u, 0x00008000u, 0x477fe000u}, {0x7fa00000u, 0x0000fc00u, 0x477fde00u}, {0x7fa00000u, 0x00008000u, 0x477fe000u}, {0x7fa00000u, 0x0000fc00u, 0x477fde00u}, {0x7fe00000u, 0x00008000u, 0x477fe000u}, {0x7fe00000u, 0x0000fc00u, 0x477fde00u}, {0x7fe00000u, 0x00008000u, 0x477fe000u}, {0x7fe00000u, 0x0000fc00u, 0x477fde00u}}}},
    {{0x33800000u, 0xff800000u, 0xbf800000u, 0x33800000u, 0x3f800000u, 0xabcd7bffu, 0x33800000u, 0xff800000u, 0x477fe000u}, {{{0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00000001u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00000001u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00000001u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00000001u, 0xff800000u}}}},
    {{0xb8000000u, 0x477ff000u, 0x33800000u, 0xb8000000u, 0x3f800000u, 0xabcd8200u, 0xb8000000u, 0x477ff000u, 0xb8000000u}, {{{0x00000000u, 0x00008000u, 0x80000000u}, {0xbffff000u, 0x00008200u, 0xbffff100u}, {0x00000000u, 0x00008000u, 0x80000000u}, {0xbffff000u, 0x00008200u, 0xbffff100u}, {0x00000000u, 0x00008000u, 0x80000000u}, {0xbffff000u, 0x00008200u, 0xbffff100u}, {0x00000000u, 0x00008000u, 0x80000000u}, {0xbffff000u, 0x00008200u, 0xbffff100u}}}},
    {{0x33800000u, 0x7fa00000u, 0xb8000000u, 0x33800000u, 0xff800000u, 0xabcd7bffu, 0x33800000u, 0x7fa00000u, 0x477fe000u}, {{{0x7fa00000u, 0x0000fe00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fe00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fe00000u, 0x0000fe00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fe00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0xff800000u, 0x00000001u, 0x43000000u, 0xff800000u, 0x47800000u, 0xabcd7d00u, 0xff800000u, 0x00000001u, 0x7fa00000u}, {{{0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xff800000u, 0x0000fc00u, 0x7fa00000u}, {0xff800000u, 0x0000fc00u, 0x7fa00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xff800000u, 0x0000fc00u, 0x7fe00000u}, {0xff800000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0x7f800000u, 0x7fa00000u, 0x33800000u, 0x7f800000u, 0x80400000u, 0xabcd3c00u, 0x7f800000u, 0x7fa00000u, 0x3f800000u}, {{{0x7fa00000u, 0x0000fe00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fe00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fe00000u, 0x0000fe00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fe00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0x3f800000u, 0x00000001u, 0x7fa00000u, 0x3f800000u, 0xff800000u, 0xabcd0001u, 0x3f800000u, 0x00000001u, 0x33800000u}, {{{0x7fa00000u, 0x0000fc00u, 0x00000000u}, {0x7fa00000u, 0x0000fc00u, 0x33800000u}, {0x7fa00000u, 0x0000fc00u, 0x00000001u}, {0x7fa00000u, 0x0000fc00u, 0x33800000u}, {0x7fe00000u, 0x0000fc00u, 0x00000000u}, {0x7fe00000u, 0x0000fc00u, 0x33800000u}, {0x7fe00000u, 0x0000fc00u, 0x00000001u}, {0x7fe00000u, 0x0000fc00u, 0x33800000u}}}},
    {{0xb8000000u, 0x7fa00000u, 0x33800000u, 0xb8000000u, 0x4f000000u, 0xabcd7e00u, 0xb8000000u, 0x7fa00000u, 0x7fc00000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x0000fc00u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0xff800000u, 0x80400000u, 0x43000000u, 0xff800000u, 0x477ff000u, 0xabcd7d00u, 0xff800000u, 0x80400000u, 0x7fa00000u}, {{{0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0x7f800000u, 0x0000fc00u, 0x7fa00000u}, {0x7f800000u, 0x0000fc00u, 0x7fa00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0xffc00000u, 0x0000fc00u, 0xffc00000u}, {0x7f800000u, 0x0000fc00u, 0x7fe00000u}, {0x7f800000u, 0x0000fc00u, 0x7fe00000u}}}},
    {{0x33800000u, 0x4f000000u, 0x3f800000u, 0x33800000u, 0x47800000u, 0xabcdbc00u, 0x33800000u, 0x4f000000u, 0xbf800000u}, {{{0x3f800000u, 0x00008000u, 0xbf800000u}, {0x43010000u, 0x00001c00u, 0x42fe0000u}, {0x3f800000u, 0x00008000u, 0xbf800000u}, {0x43010000u, 0x00001c00u, 0x42fe0000u}, {0x3f800000u, 0x00008000u, 0xbf800000u}, {0x43010000u, 0x00001c00u, 0x42fe0000u}, {0x3f800000u, 0x00008000u, 0xbf800000u}, {0x43010000u, 0x00001c00u, 0x42fe0000u}}}},
    {{0x7f800000u, 0x00000001u, 0x3f800000u, 0x7f800000u, 0x80400000u, 0xabcd5800u, 0x7f800000u, 0x00000001u, 0x43000000u}, {{{0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0x7f800000u, 0x0000fc00u, 0x7f800000u}, {0x7f800000u, 0x0000fc00u, 0x7f800000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0xffc00000u, 0x0000fe00u, 0xffc00000u}, {0x7f800000u, 0x0000fc00u, 0x7f800000u}, {0x7f800000u, 0x0000fc00u, 0x7f800000u}}}},
    {{0x33800000u, 0x7fa00000u, 0x7fc00000u, 0x33800000u, 0x3f800000u, 0xabcd7d00u, 0x33800000u, 0x7fa00000u, 0x7fa00000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00000001u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00000001u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00000001u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00000001u, 0x7fe00000u}}}},
    {{0xb8000000u, 0x7fa00000u, 0x477fe000u, 0xb8000000u, 0x3f800000u, 0xabcd0000u, 0xb8000000u, 0x7fa00000u, 0x00000000u}, {{{0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00008200u, 0x7fa00000u}, {0x7fa00000u, 0x00008000u, 0x7fa00000u}, {0x7fa00000u, 0x00008200u, 0x7fa00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00008200u, 0x7fe00000u}, {0x7fe00000u, 0x00008000u, 0x7fe00000u}, {0x7fe00000u, 0x00008200u, 0x7fe00000u}}}},
    {{0x33800000u, 0x4f000000u, 0x7fa00000u, 0x33800000u, 0x477ff000u, 0xabcd8000u, 0x33800000u, 0x4f000000u, 0x80000000u}, {{{0x7fa00000u, 0x00008000u, 0x00000000u}, {0x7fa00000u, 0x00001bffu, 0x43000000u}, {0x7fa00000u, 0x00008000u, 0x00000000u}, {0x7fa00000u, 0x00001bffu, 0x43000000u}, {0x7fe00000u, 0x00008000u, 0x00000000u}, {0x7fe00000u, 0x00001bffu, 0x43000000u}, {0x7fe00000u, 0x00008000u, 0x00000000u}, {0x7fe00000u, 0x00001bffu, 0x43000000u}}}},
    {{0xb8000000u, 0x7f800000u, 0x00000000u, 0xb8000000u, 0x3f800000u, 0xabcd7bffu, 0xb8000000u, 0x7f800000u, 0x477fe000u}, {{{0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00008200u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00008200u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00008200u, 0xff800000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xff800000u, 0x00008200u, 0xff800000u}}}},
    {{0xb8000000u, 0x477ff000u, 0x33800000u, 0xb8000000u, 0xff800000u, 0xabcd0001u, 0xb8000000u, 0x477ff000u, 0x33800000u}, {{{0x00000000u, 0x0000fe00u, 0x00000000u}, {0xbffff000u, 0x00007c00u, 0xbffff000u}, {0x00000000u, 0x0000fe00u, 0x00000000u}, {0xbffff000u, 0x00007c00u, 0xbffff000u}, {0x00000000u, 0x0000fe00u, 0x00000000u}, {0xbffff000u, 0x00007c00u, 0xbffff000u}, {0x00000000u, 0x0000fe00u, 0x00000000u}, {0xbffff000u, 0x00007c00u, 0xbffff000u}}}},
    {{0xb8000000u, 0x00000000u, 0x7fc00000u, 0xb8000000u, 0x7f7fffffu, 0xabcd8200u, 0xb8000000u, 0x00000000u, 0xb8000000u}, {{{0x7fc00000u, 0x00008000u, 0x80000000u}, {0x7fc00000u, 0x0000fc00u, 0xb8000000u}, {0x7fc00000u, 0x00008000u, 0x80000000u}, {0x7fc00000u, 0x0000fc00u, 0xb8000000u}, {0x7fc00000u, 0x00008000u, 0x80000000u}, {0x7fc00000u, 0x0000fc00u, 0xb8000000u}, {0x7fc00000u, 0x00008000u, 0x80000000u}, {0x7fc00000u, 0x0000fc00u, 0xb8000000u}}}},
    {{0xffc02000u, 0x00000001u, 0x7f800000u, 0xffc02000u, 0x7fa00000u, 0xabcd0000u, 0xffc02000u, 0x00000001u, 0x00000000u}, {{{0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}}}},
    {{0xffc02000u, 0x80400000u, 0x3f800000u, 0xffc02000u, 0x47800000u, 0xabcdfe01u, 0xffc02000u, 0x80400000u, 0xffc02000u}, {{{0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}}}},
    {{0x7fa00000u, 0x4f000000u, 0x3f800000u, 0x7fa00000u, 0x7fc00000u, 0xabcdfc00u, 0x7fa00000u, 0x4f000000u, 0xff800000u}, {{{0x7fa00000u, 0x00007f00u, 0x7fa00000u}, {0x7fa00000u, 0x00007f00u, 0x7fa00000u}, {0x7fa00000u, 0x00007f00u, 0x7fa00000u}, {0x7fa00000u, 0x00007f00u, 0x7fa00000u}, {0x7fe00000u, 0x00007f00u, 0x7fe00000u}, {0x7fe00000u, 0x00007f00u, 0x7fe00000u}, {0x7fe00000u, 0x00007f00u, 0x7fe00000u}, {0x7fe00000u, 0x00007f00u, 0x7fe00000u}}}},
    {{0x7f800000u, 0x00000001u, 0xff800000u, 0x7f800000u, 0x477ff000u, 0xabcd7e00u, 0x7f800000u, 0x00000001u, 0x7fc00000u}, {{{0xffc00000u, 0x00007c00u, 0xffc00000u}, {0xffc00000u, 0x00007c00u, 0xffc00000u}, {0xffc00000u, 0x00007c00u, 0x7fc00000u}, {0xffc00000u, 0x00007c00u, 0x7fc00000u}, {0xffc00000u, 0x00007c00u, 0xffc00000u}, {0xffc00000u, 0x00007c00u, 0xffc00000u}, {0xffc00000u, 0x00007c00u, 0x7fc00000u}, {0xffc00000u, 0x00007c00u, 0x7fc00000u}}}},
    {{0xffc02000u, 0x80400000u, 0xb8000000u, 0xffc02000u, 0x00000000u, 0xabcd7c00u, 0xffc02000u, 0x80400000u, 0x7f800000u}, {{{0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}, {0xffc02000u, 0x0000fe01u, 0xffc02000u}}}},
    {{0x7f800000u, 0xffc00001u, 0x7fa00000u, 0x7f800000u, 0xff800000u, 0xabcd0000u, 0x7f800000u, 0xffc00001u, 0x00000000u}, {{{0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}, {0xffc00001u, 0x0000fc00u, 0xffc00001u}}}},
    {{0x80000000u, 0x7f800000u, 0x43000000u, 0x80000000u, 0x3f800000u, 0xabcd0000u, 0x80000000u, 0x7f800000u, 0x00000000u}, {{{0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}, {0xffc00000u, 0x00008000u, 0xffc00000u}}}},
}};

constexpr std::uint32_t Words = 12;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t RandomRows = 2048;
constexpr std::uint32_t RandomModes = 32;
constexpr std::uint32_t RandomWords = 9 + RandomModes * 3;
alignas(256) std::array<std::uint32_t, RandomRows * RandomWords + 4> Data{};

RecompileResult Compile(std::span<const std::uint32_t> modes, bool integer, bool raw, std::uint32_t words) {
    IrProgram program;
    const ShaderStageInputInfo inputs{};
    SpirvEmitterState state(program, inputs);
    state.integerInterpolation = integer;
    auto& module = state.module;
    module.EmitCapability(spv::CapabilityShader);
    module.EmitCapability(integer ? spv::CapabilityInt64 : spv::CapabilityFloat64);
    module.EmitCapability(spv::CapabilitySignedZeroInfNanPreserve);
    module.EmitExtension("SPV_KHR_float_controls");
    module.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    const auto u32 = TypeU32(state);
    const auto array = module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, u32);
    const auto structure = module.Type(spv::OpTypeStruct, array);
    module.AddAnnotation(spv::OpDecorate, structure, spv::DecorationBlock);
    module.AddAnnotation(spv::OpMemberDecorate, structure, 0u, spv::DecorationOffset, 0u);
    const auto buffer = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, structure), spv::StorageClassStorageBuffer);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationDescriptorSet, 0u);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationBinding, 0u);
    const auto invocation = module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32Vector(state, 3u)), spv::StorageClassInput);
    module.AddAnnotation(spv::OpDecorate, invocation, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
    const auto element = module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, u32);
    const auto main = module.AllocateId();
    const auto voidType = module.Type(spv::OpTypeVoid);
    module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {invocation});
    module.AddExecutionMode(main, spv::ExecutionModeLocalSize, static_cast<std::uint32_t>(Rows.size()), 1u, 1u);
    module.AddExecutionMode(main, spv::ExecutionModeSignedZeroInfNanPreserve, 32u);
    if (!integer) module.AddExecutionMode(main, spv::ExecutionModeSignedZeroInfNanPreserve, 64u);
    module.AddFunction(spv::OpFunction, voidType, main, spv::FunctionControlMaskNone, module.Type(spv::OpTypeFunction, voidType));
    module.AddFunction(spv::OpLabel, module.AllocateId());
    const auto id = module.AllocateId();
    module.AddFunction(spv::OpLoad, TypeU32Vector(state, 3u), id, invocation);
    const auto lane = module.AllocateId();
    module.AddFunction(spv::OpCompositeExtract, u32, lane, id, 0u);
    const auto rowBase = Binary(state, spv::OpIMul, u32, lane, ConstantU32(state, words));
    const auto address = [&](std::uint32_t offset) {
        const auto pointer = module.AllocateId();
        module.AddFunction(spv::OpAccessChain, element, pointer, buffer, ConstantU32(state, 0u), Binary(state, spv::OpIAdd, u32, rowBase, ConstantU32(state, offset)));
        return pointer;
    };
    const auto load = [&](std::uint32_t offset) {
        const auto word = module.AllocateId();
        module.AddFunction(spv::OpLoad, u32, word, address(offset));
        return Unary(state, spv::OpBitcast, TypeF32(state), word);
    };
    IrProgram constants;
    IrBuilder builder(constants);
    builder.SetInsertionPoint(constants.CreateBlock());
    std::array<std::uint32_t, 9> operands{};
    for (std::uint32_t index = 0; index < operands.size(); ++index) operands[index] = load(index);
    for (std::uint32_t slot = 0; slot < modes.size(); ++slot) {
        const auto* flags = &builder.Constant(modes[slot]);
        for (std::uint32_t op = 0; op < 3u; ++op) {
            const auto delta = operands[op * 3u];
            const auto coordinate = operands[op * 3u + 1u];
            const auto base = operands[op * 3u + 2u];
            const auto value = op == 1u ? EmitFPInterpolateF16(state, delta, coordinate, base, flags) : EmitFPInterpolateF32(state, delta, coordinate, base, flags);
            const auto result = op == 1u && !raw ? EmitConvertF16F32(state, value) : Unary(state, spv::OpBitcast, u32, value);
            module.AddFunction(spv::OpStore, address(9u + slot * 3u + op), result);
        }
    }
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    RecompileResult result{};
    result.spirv = module.Finalize();
    const auto base = reinterpret_cast<std::uintptr_t>(Data.data());
    result.bindings.push_back({DescriptorKind::StorageBuffer, DescriptorRole::GuestBuffers, 0u, 0u, 1u,
        {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Data)), 0x31016facu}, false});
    return result;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t mode, bool integer) {
    Data.fill(Sentinel);
    for (std::uint32_t row = 0; row < Rows.size(); ++row) std::copy(Rows[row].input.begin(), Rows[row].input.end(), Data.begin() + row * Words);
    const std::array<std::uint32_t, 1> modes{ModeFlags[mode]};
    device.Dispatch(Compile(modes, integer, false, Words), 1u, 1u, 1u);
    device.WaitIdle();
    for (std::uint32_t row = 0; row < Rows.size(); ++row) {
        for (std::uint32_t input = 0; input < 9u; ++input) Require(Data[row * Words + input] == Rows[row].input[input], "interpolation modes changed an input");
        for (std::uint32_t op = 0; op < 3u; ++op) {
            const auto actual = Data[row * Words + 9u + op];
            const auto expected = Rows[row].expected[mode][op];
            char message[200];
            std::snprintf(message, sizeof(message), "%s (%s), %s, row %u: got 0x%08x, expected 0x%08x", OpNames[op], integer ? "integer" : "Float64", ModeNames[mode], row, actual, expected);
            Require(actual == expected, message);
        }
    }
    for (std::size_t index = Rows.size() * Words; index < Data.size(); ++index) Require(Data[index] == Sentinel, "interpolation modes wrote beyond the rows");
}

}

std::uint32_t Flush(std::uint32_t bits, bool flush, std::uint32_t smallestNormal) {
    const auto magnitude = bits & 0x7fffffffu;
    return flush && magnitude != 0u && magnitude < smallestNormal ? bits & 0x80000000u : bits;
}

bool NonFinite(std::uint32_t bits) { return (bits & 0x7f800000u) == 0x7f800000u; }
bool Nan(std::uint32_t bits) { return (bits & 0x7fffffffu) > 0x7f800000u; }
bool Infinite(std::uint32_t bits) { return (bits & 0x7fffffffu) == 0x7f800000u; }
bool Zero(std::uint32_t bits) { return (bits & 0x7fffffffu) == 0u; }

std::uint32_t Reference(std::uint32_t delta, std::uint32_t coordinate, std::uint32_t base, bool half, std::uint32_t mode) {
    const bool quiet = half || (mode & InterpolationQuiet) != 0u;
    const bool flush32 = (mode & InterpolationFlush32) != 0u;
    const bool flush16 = (mode & InterpolationFlush16) != 0u;
    const std::uint32_t rounding = half ? (mode >> InterpolationRound16Shift) & 3u : 0u;
    const auto d = Flush(delta, flush16, 0x38800000u);
    const auto c = Flush(coordinate, flush32, 0x00800000u);
    const auto b = half ? Flush(base, flush32, 0x00800000u) : Flush(base, flush16, 0x38800000u);
    const double fd = std::bit_cast<float>(d);
    const double fc = std::bit_cast<float>(c);
    const double fb = std::bit_cast<float>(b);
    std::uint32_t result = 0;
    if (NonFinite(d) || NonFinite(c) || NonFinite(b)) {
        const double unbounded = fd * fc + fb;
        result = std::isnan(unbounded) ? 0xffc00000u : (std::signbit(unbounded) ? 0xff800000u : 0x7f800000u);
        const auto quieted = [&](std::uint32_t bits) { return quiet ? bits | 0x00400000u : bits; };
        const bool invalid = (Infinite(d) && Zero(c)) || (Zero(d) && Infinite(c));
        if (Nan(b) && !invalid) result = quieted(b);
        if (Nan(c)) result = quieted(c);
        if (Nan(d)) result = quieted(d);
    } else {
        const double product = fd * fc;
        double sum = product + fb;
        const double back = sum - product;
        const double error = (product - (sum - back)) + (fb - back);
        if (error != 0.0 && (std::bit_cast<std::uint64_t>(sum) & 1u) == 0u) sum = std::nextafter(sum, error > 0.0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity());
        if (sum == 0.0 && rounding == 2u && !(product == 0.0 && !std::signbit(product) && fb == 0.0 && !std::signbit(fb))) sum = -0.0;
        double rounded = sum;
        if (sum != 0.0) {
            const int precision = half ? 11 : 24;
            const int smallest = half ? -24 : -149;
            const int step = std::max(std::ilogb(sum) - (precision - 1), smallest);
            const double scaled = std::ldexp(sum, -step);
            const double integral = rounding == 1u ? std::ceil(scaled) : rounding == 2u ? std::floor(scaled) : rounding == 3u ? std::trunc(scaled) : std::nearbyint(scaled);
            rounded = std::ldexp(integral, step);
        }
        if (half) {
            const double magnitude = std::fabs(rounded);
            const bool negative = std::signbit(rounded);
            if (rounding == 0u && magnitude > 65536.0) rounded = std::copysign(65536.0, rounded);
            if (rounding != 0u && magnitude > 65504.0) {
                const bool larger = (rounding == 1u && !negative) || (rounding == 2u && negative);
                rounded = std::copysign(larger ? 65536.0 : 65504.0, rounded);
            }
        }
        result = std::bit_cast<std::uint32_t>(static_cast<float>(rounded));
    }
    return half ? Flush(result, flush16, 0x38800000u) : Flush(result, flush32, 0x00800000u);
}

struct Generator {
    std::uint64_t state;
    std::uint64_t Next() {
        state += 0x9e3779b97f4a7c15ull;
        auto value = state;
        value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
        return value ^ (value >> 31u);
    }
    std::uint32_t Pick(std::uint32_t count) { return static_cast<std::uint32_t>(Next() % count); }
    std::uint32_t Half() {
        constexpr std::array<std::uint32_t, 12> special{0x0000u, 0x8000u, 0x0001u, 0x8001u, 0x03ffu, 0x0400u, 0x3c00u, 0xbc00u, 0x7bffu, 0xfbffu, 0x7c00u, 0xfc00u};
        const auto bits = Pick(4u) == 0u ? special[Pick(static_cast<std::uint32_t>(special.size()))] : static_cast<std::uint32_t>(Next() & 0xffffu);
        const auto sign = (bits & 0x8000u) << 16u;
        const auto exponent = (bits >> 10u) & 0x1fu;
        const auto fraction = bits & 0x3ffu;
        if (exponent == 31u) return sign | 0x7f800000u | (fraction << 13u);
        if (exponent != 0u) return sign | ((exponent + 112u) << 23u) | (fraction << 13u);
        return sign | std::bit_cast<std::uint32_t>(static_cast<float>(fraction) * 0x1p-24f);
    }
    std::uint32_t Single() {
        constexpr std::array<std::uint32_t, 14> special{0x00000000u, 0x80000000u, 0x00000001u, 0x807fffffu, 0x00800000u, 0x3f800000u, 0xbf800000u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffa00001u, 0x33800000u};
        switch (Pick(4u)) {
        case 0: return special[Pick(static_cast<std::uint32_t>(special.size()))];
        case 1: return static_cast<std::uint32_t>(Next());
        default: return (Pick(2u) << 31u) | ((Pick(254u) + 1u) << 23u) | static_cast<std::uint32_t>(Next() & 0x7fffffu);
        }
    }
    std::uint32_t Cancelling(std::uint32_t delta, std::uint32_t coordinate, bool half) {
        const double product = static_cast<double>(std::bit_cast<float>(delta)) * static_cast<double>(std::bit_cast<float>(coordinate));
        if (!std::isfinite(product)) return half ? Single() : Half();
        auto bits = std::bit_cast<std::uint32_t>(static_cast<float>(-product));
        if (Pick(2u) == 0u) bits += Pick(5u) - 2u;
        if (half) return bits;
        const auto value = std::bit_cast<float>(bits);
        if (!std::isfinite(value) || std::fabs(value) > 65504.0f) return Half();
        if (value == 0.0f) return bits;
        const auto exponent = std::max(std::ilogb(value) - 10, -24);
        return std::bit_cast<std::uint32_t>(static_cast<float>(std::ldexp(std::nearbyint(std::ldexp(value, -exponent)), exponent)));
    }
};

void RunRandom(AgcDriver::VulkanDevice& device, bool integer) {
    std::array<std::uint32_t, RandomModes> modes{};
    for (std::uint32_t slot = 0; slot < RandomModes; ++slot) modes[slot] = ModeFlags[slot % ModeFlags.size()] | ((slot / ModeFlags.size()) << InterpolationRound16Shift);
    Generator generator{0x6b5f00000000ull};
    Data.fill(Sentinel);
    for (std::uint32_t row = 0; row < RandomRows; ++row) {
        for (std::uint32_t op = 0; op < 3u; ++op) {
            const bool half = op == 1u;
            const auto delta = generator.Half();
            const auto coordinate = generator.Single();
            const auto base = generator.Pick(3u) == 0u ? generator.Cancelling(delta, coordinate, half) : half ? generator.Single() : generator.Half();
            Data[row * RandomWords + op * 3u] = delta;
            Data[row * RandomWords + op * 3u + 1u] = coordinate;
            Data[row * RandomWords + op * 3u + 2u] = base;
        }
    }
    device.Dispatch(Compile(modes, integer, true, RandomWords), RandomRows / static_cast<std::uint32_t>(Rows.size()), 1u, 1u);
    device.WaitIdle();
    for (std::uint32_t row = 0; row < RandomRows; ++row) {
        for (std::uint32_t slot = 0; slot < RandomModes; ++slot) {
            for (std::uint32_t op = 0; op < 3u; ++op) {
                const auto* input = &Data[row * RandomWords + op * 3u];
                const auto expected = Reference(input[0], input[1], input[2], op == 1u, modes[slot]);
                const auto actual = Data[row * RandomWords + 9u + slot * 3u + op];
                char message[240];
                std::snprintf(message, sizeof(message), "%s (%s), mode 0x%x, random row %u: 0x%08x * 0x%08x + 0x%08x gave 0x%08x, reference 0x%08x",
                    OpNames[op], integer ? "integer" : "Float64", modes[slot], row, input[0], input[1], input[2], actual, expected);
                Require(actual == expected, message);
            }
        }
    }
    for (std::size_t index = RandomRows * RandomWords; index < Data.size(); ++index) Require(Data[index] == Sentinel, "random interpolation rows wrote beyond the rows");
}

constexpr std::uint32_t ChainWords = 8;
constexpr std::uint32_t ChainGroup = 64;
alignas(256) std::array<std::uint32_t, (1u << 16u) * ChainWords> ChainData{};

RecompileResult CompileChain(std::span<const std::uint32_t> steps, bool half, bool integer, std::uint32_t iterations = 1u) {
    IrProgram program;
    const ShaderStageInputInfo inputs{};
    SpirvEmitterState state(program, inputs);
    state.integerInterpolation = integer;
    auto& module = state.module;
    module.EmitCapability(spv::CapabilityShader);
    module.EmitCapability(integer ? spv::CapabilityInt64 : spv::CapabilityFloat64);
    module.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    const auto u32 = TypeU32(state);
    const auto array = module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, u32);
    const auto structure = module.Type(spv::OpTypeStruct, array);
    module.AddAnnotation(spv::OpDecorate, structure, spv::DecorationBlock);
    module.AddAnnotation(spv::OpMemberDecorate, structure, 0u, spv::DecorationOffset, 0u);
    const auto buffer = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, structure), spv::StorageClassStorageBuffer);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationDescriptorSet, 0u);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationBinding, 0u);
    const auto invocation = module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32Vector(state, 3u)), spv::StorageClassInput);
    module.AddAnnotation(spv::OpDecorate, invocation, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
    const auto element = module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, u32);
    const auto main = module.AllocateId();
    const auto voidType = module.Type(spv::OpTypeVoid);
    module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {invocation});
    module.AddExecutionMode(main, spv::ExecutionModeLocalSize, ChainGroup, 1u, 1u);
    module.AddFunction(spv::OpFunction, voidType, main, spv::FunctionControlMaskNone, module.Type(spv::OpTypeFunction, voidType));
    module.AddFunction(spv::OpLabel, module.AllocateId());
    const auto id = module.AllocateId();
    module.AddFunction(spv::OpLoad, TypeU32Vector(state, 3u), id, invocation);
    const auto lane = module.AllocateId();
    module.AddFunction(spv::OpCompositeExtract, u32, lane, id, 0u);
    const auto rowBase = Binary(state, spv::OpIMul, u32, lane, ConstantU32(state, ChainWords));
    const auto address = [&](std::uint32_t offset) {
        const auto pointer = module.AllocateId();
        module.AddFunction(spv::OpAccessChain, element, pointer, buffer, ConstantU32(state, 0u), Binary(state, spv::OpIAdd, u32, rowBase, ConstantU32(state, offset)));
        return pointer;
    };
    const auto load = [&](std::uint32_t offset) {
        const auto word = module.AllocateId();
        module.AddFunction(spv::OpLoad, u32, word, address(offset));
        return Unary(state, spv::OpBitcast, TypeF32(state), word);
    };
    IrProgram constants;
    IrBuilder builder(constants);
    builder.SetInsertionPoint(constants.CreateBlock());
    const std::array deltas{load(0u), load(1u), load(2u), load(3u)};
    const auto coordinate = load(4u);
    const auto chain = [&](std::uint32_t value) {
        for (std::uint32_t step = 0; step < steps.size(); ++step) {
            const auto* flags = &builder.Constant(steps[step]);
            value = half ? EmitFPInterpolateF16(state, deltas[step % 4u], coordinate, value, flags) : EmitFPInterpolateF32(state, deltas[step % 4u], coordinate, value, flags);
        }
        return value;
    };
    auto value = load(5u);
    if (iterations == 1u) {
        value = chain(value);
    } else {
        const auto entry = module.AllocateId();
        const auto header = module.AllocateId();
        const auto body = module.AllocateId();
        const auto next = module.AllocateId();
        const auto merge = module.AllocateId();
        const auto counter = module.AllocateId();
        const auto carried = module.AllocateId();
        const auto counted = module.AllocateId();
        const auto chained = module.AllocateId();
        module.AddFunction(spv::OpBranch, entry);
        EmitLabel(state, entry);
        module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, header);
        module.AddFunction(spv::OpPhi, u32, counter, ConstantU32(state, 0u), entry, counted, next);
        module.AddFunction(spv::OpPhi, TypeF32(state), carried, value, entry, chained, next);
        module.AddFunction(spv::OpLoopMerge, merge, next, spv::LoopControlMaskNone);
        module.AddFunction(spv::OpBranchConditional, Binary(state, spv::OpULessThan, TypeBool(state), counter, ConstantU32(state, iterations)), body, merge);
        EmitLabel(state, body);
        module.AddFunction(spv::OpCopyObject, TypeF32(state), chained, chain(carried));
        module.AddFunction(spv::OpBranch, next);
        EmitLabel(state, next);
        module.AddFunction(spv::OpIAdd, u32, counted, counter, ConstantU32(state, 1u));
        module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, merge);
        value = carried;
    }
    module.AddFunction(spv::OpStore, address(6u), Unary(state, spv::OpBitcast, u32, value));
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    RecompileResult result{};
    result.spirv = module.Finalize();
    const auto base = reinterpret_cast<std::uintptr_t>(ChainData.data());
    result.bindings.push_back({DescriptorKind::StorageBuffer, DescriptorRole::GuestBuffers, 0u, 0u, 1u,
        {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(ChainData)), 0x31016facu}, false});
    return result;
}

void FillChains(std::uint32_t lanes, bool half, bool special) {
    Generator generator{half ? 0x6b5f00000001ull : 0x6b5f00000002ull};
    const auto mantissa = [&]() {
        const auto bits = generator.Pick(3u) == 0u ? 1u + generator.Pick(23u) : 23u;
        return static_cast<std::uint32_t>(generator.Next() & 0x7fffffu) & ~((1u << (23u - bits)) - 1u);
    };
    const auto value = [&](std::uint32_t lowest, std::uint32_t count) { return (generator.Pick(2u) << 31u) | ((lowest + generator.Pick(count)) << 23u) | mantissa(); };
    ChainData.fill(Sentinel);
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        auto* row = &ChainData[lane * ChainWords];
        for (std::uint32_t index = 0; index < 4u; ++index) row[index] = special && generator.Pick(8u) == 0u ? generator.Half() : special && generator.Pick(8u) == 0u ? generator.Single() : value(112u, 16u);
        row[4] = special && generator.Pick(8u) == 0u ? generator.Single() : value(117u, 10u) & 0x7fffffffu;
        row[5] = special && generator.Pick(8u) == 0u ? generator.Single() : value(110u, 24u);
    }
}

std::uint32_t ChainReference(const std::uint32_t* row, std::span<const std::uint32_t> steps, bool half) {
    auto value = row[5];
    for (std::uint32_t step = 0; step < steps.size(); ++step) value = Reference(row[step % 4u], row[4], value, half, steps[step]);
    return value;
}

void RunChains(AgcDriver::VulkanDevice& device, bool integer) {
    constexpr std::uint32_t lanes = 8192;
    std::array<std::uint32_t, 2 * RandomModes> steps{};
    for (std::uint32_t step = 0; step < steps.size(); ++step) steps[step] = ModeFlags[step % ModeFlags.size()] | (((step / ModeFlags.size()) % 4u) << InterpolationRound16Shift);
    for (const bool half : {false, true}) {
        FillChains(lanes, half, true);
        device.Dispatch(CompileChain(steps, half, integer), lanes / ChainGroup, 1u, 1u);
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            const auto* row = &ChainData[lane * ChainWords];
            const auto expected = ChainReference(row, steps, half);
            char message[200];
            std::snprintf(message, sizeof(message), "%s chain (%s), lane %u: 0x%08x 0x%08x 0x%08x 0x%08x * 0x%08x from 0x%08x gave 0x%08x, reference 0x%08x",
                half ? "f16" : "f32", integer ? "integer" : "Float64", lane, row[0], row[1], row[2], row[3], row[4], row[5], row[6], expected);
            Require(row[6] == expected, message);
        }
    }
}

void Benchmark(AgcDriver::VulkanDevice& device, bool integer) {
    constexpr std::uint32_t lanes = 1u << 16u;
    constexpr std::uint32_t iterations = 1024;
    const std::array<std::uint32_t, 4> steps{ModeFlags[0], ModeFlags[0], ModeFlags[0], ModeFlags[0]};
    for (const bool half : {false, true}) {
        FillChains(lanes, half, false);
        const auto program = CompileChain(steps, half, integer, iterations);
        device.Dispatch(program, lanes / ChainGroup, 1u, 1u);
        device.WaitIdle();
        std::array<double, 9> times{};
        for (auto& milliseconds : times) {
            const auto start = std::chrono::steady_clock::now();
            device.Dispatch(program, lanes / ChainGroup, 1u, 1u);
            device.WaitIdle();
            milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        std::sort(times.begin(), times.end());
        std::printf("%s path, %u lanes x %u chained %s interpolations: median %.3f ms, min %.3f, max %.3f\n", integer ? "integer" : "Float64", lanes, iterations * static_cast<std::uint32_t>(steps.size()), half ? "f16" : "f32", times[4], times.front(), times.back());
    }
}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const bool float64 = std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityFloat64) != target.supportedCapabilities.end();
        if (argc == 2 && std::string(argv[1]) == "--benchmark") {
            Benchmark(*device, true);
            if (float64) Benchmark(*device, false);
            return 0;
        }
        for (std::uint32_t mode = 0; mode < ModeFlags.size(); ++mode) {
            Run(*device, mode, true);
            if (float64) Run(*device, mode, false);
        }
        RunRandom(*device, true);
        if (float64) RunRandom(*device, false);
        RunChains(*device, true);
        if (float64) RunChains(*device, false);
        std::puts(float64 ? "interpolation modes passed: 32 hardware rows and 2048 random rows against the reference in 32 float modes, 3 ops, 8192 chains of 64 f32 and f16 interpolations, integer and Float64 paths"
                          : "interpolation modes passed: 32 hardware rows and 2048 random rows against the reference in 32 float modes, 3 ops, 8192 chains of 64 f32 and f16 interpolations, integer path (the device has no Float64)");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
