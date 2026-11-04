#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Columns = 12;
constexpr std::uint32_t Float64Low = 5;
constexpr std::uint32_t Float64High = 6;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e141104u, 0x7e160f04u, 0x7e181904u,
    0x7e1a1b04u, 0x7e1c7f04u, 0xd7680011u, 0x00020b04u, 0xd7690012u, 0x00020b04u, 0xd55e0013u, 0x02010104u,
    0x5e280b04u, 0x7e2a1504u, 0xe0781000u, 0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0781020u, 0x80011202u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 13> Float64Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1e2104u, 0xe0781000u, 0x80010a02u,
    0xe0781010u, 0x80010e02u, 0xe0781020u, 0x80011202u, 0xbf810000u,
};

constexpr const char* Names[Columns] = {
    "v_cvt_i32_f32 v10, v4",
    "v_cvt_u32_f32 v11, v4",
    "v_cvt_rpi_i32_f32 v12, v4",
    "v_cvt_flr_i32_f32 v13, v4",
    "v_frexp_exp_i32_f32 v14, v4",
    "v_cvt_f64_f32 v[15:16], v4 (low)",
    "v_cvt_f64_f32 v[15:16], v4 (high)",
    "v_cvt_pknorm_i16_f32 v17, v4, v5",
    "v_cvt_pknorm_u16_f32 v18, v4, v5",
    "v_cvt_pk_u8_f32 v19, v4, 0, 0",
    "v_cvt_pkrtz_f16_f32 v20, v4, v5",
    "v_cvt_f16_f32 v21, v4",
};

struct ConvertVector {
    std::uint32_t operands[2];
    std::uint32_t denormalsFlushed[Columns];
    std::uint32_t denormalsKept[Columns];
};

constexpr std::array<ConvertVector, 32> Vectors{{
    {{0x00000001u, 0x00400000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff6cu, 0x00000000u, 0x36a00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x007fffffu, 0x80000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff82u, 0xc0000000u, 0x380fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}},
    {{0x00400000u, 0x3f800000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff82u, 0x00000000u, 0x38000000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c000000u, 0x00000000u}},
    {{0x80000001u, 0x007fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff6cu, 0x00000000u, 0xb6a00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}},
    {{0x807fffffu, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff82u, 0xc0000000u, 0xb80fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}},
    {{0x80400000u, 0xbf000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0xc0000000u, 0x00000000u, 0x00000000u, 0xb8008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff82u, 0x00000000u, 0xb8000000u, 0xc0000000u, 0x00000000u, 0x00000000u, 0xb8008000u, 0x00008000u}},
    {{0x00800000u, 0x80400000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x00000000u, 0x38100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x00000000u, 0x38100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}},
    {{0x80800000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff83u, 0x00000000u, 0xb8100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff83u, 0x00000000u, 0xb8100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}},
    {{0x3f000000u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00004000u, 0x00008000u, 0x00000000u, 0x00003800u, 0x00003800u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00004000u, 0x00008000u, 0x00000000u, 0x00003800u, 0x00003800u}},
    {{0xbfc00000u, 0x807fffffu}, {0xffffffffu, 0x00000000u, 0xffffffffu, 0xfffffffeu, 0x00000001u, 0x00000000u, 0xbff80000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x8000be00u, 0x0000be00u}, {0xffffffffu, 0x00000000u, 0xffffffffu, 0xfffffffeu, 0x00000001u, 0x00000000u, 0xbff80000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x8000be00u, 0x0000be00u}},
    {{0x3f800000u, 0x80000002u}, {0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3ff00000u, 0x00007fffu, 0x0000ffffu, 0x00000001u, 0x80003c00u, 0x00003c00u}, {0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3ff00000u, 0x00007fffu, 0x0000ffffu, 0x00000001u, 0x80003c00u, 0x00003c00u}},
    {{0x00000000u, 0x00000002u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x80000000u, 0x3f800000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c008000u, 0x00008000u}},
    {{0x33800000u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffe9u, 0x00000000u, 0x3e700000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffe9u, 0x00000000u, 0x3e700000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u}},
    {{0x477fe000u, 0x80000001u}, {0x0000ffe0u, 0x0000ffe0u, 0x0000ffe0u, 0x0000ffe0u, 0x00000010u, 0x00000000u, 0x40effc00u, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x80007bffu, 0x00007bffu}, {0x0000ffe0u, 0x0000ffe0u, 0x0000ffe0u, 0x0000ffe0u, 0x00000010u, 0x00000000u, 0x40effc00u, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x80007bffu, 0x00007bffu}},
    {{0x501502f9u, 0x00800000u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000022u, 0x20000000u, 0x4202a05fu, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x00007bffu, 0x00007c00u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000022u, 0x20000000u, 0x4202a05fu, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x00007bffu, 0x00007c00u}},
    {{0xd01502f9u, 0x007fffffu}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000022u, 0x20000000u, 0xc202a05fu, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fbffu, 0x0000fc00u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000022u, 0x20000000u, 0xc202a05fu, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fbffu, 0x0000fc00u}},
    {{0x7f800000u, 0xbfc00000u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x7ff00000u, 0x80017fffu, 0x0000ffffu, 0x000000ffu, 0xbe007c00u, 0x00007c00u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x7ff00000u, 0x80017fffu, 0x0000ffffu, 0x000000ffu, 0xbe007c00u, 0x00007c00u}},
    {{0xff800000u, 0x00000001u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xfff00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fc00u, 0x0000fc00u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xfff00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fc00u, 0x0000fc00u}},
    {{0x7fc00000u, 0x80400000u}, {0x00000000u, 0x00000000u, 0x7fffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80007e00u, 0x00007e00u}, {0x00000000u, 0x00000000u, 0x7fffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80007e00u, 0x00007e00u}},
    {{0xffc00000u, 0x00400000u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000fe00u, 0x0000fe00u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000fe00u, 0x0000fe00u}},
    {{0x4f800000u, 0x80000000u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000021u, 0x00000000u, 0x41f00000u, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x80007bffu, 0x00007c00u}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000021u, 0x00000000u, 0x41f00000u, 0x00007fffu, 0x0000ffffu, 0x000000ffu, 0x80007bffu, 0x00007c00u}},
    {{0xcf000000u, 0x007fffffu}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000020u, 0x00000000u, 0xc1e00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fbffu, 0x0000fc00u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000020u, 0x00000000u, 0xc1e00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000fbffu, 0x0000fc00u}},
    {{0x3f7fffffu, 0x80000001u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0xe0000000u, 0x3fefffffu, 0x00007fffu, 0x0000ffffu, 0x00000001u, 0x80003bffu, 0x00003c00u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0xe0000000u, 0x3fefffffu, 0x00007fffu, 0x0000ffffu, 0x00000001u, 0x80003bffu, 0x00003c00u}},
    {{0x00000002u, 0x3f800000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff6du, 0x00000000u, 0x36b00000u, 0x7fff0000u, 0xffff0000u, 0x00000000u, 0x3c000000u, 0x00000000u}},
    {{0x80000002u, 0x00000003u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff6du, 0x00000000u, 0xb6b00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}},
    {{0x00000001u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff6cu, 0x00000000u, 0x36a00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x007fffffu, 0x807fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffff82u, 0xc0000000u, 0x380fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u}},
    {{0x3f000000u, 0x80000001u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00004000u, 0x00008000u, 0x00000000u, 0x80003800u, 0x00003800u}, {0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00004000u, 0x00008000u, 0x00000000u, 0x80003800u, 0x00003800u}},
    {{0x80000001u, 0x00400000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffff6cu, 0x00000000u, 0xb6a00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u}},
    {{0x3e800000u, 0x80800000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000000u, 0x3fd00000u, 0x00002000u, 0x00004000u, 0x00000000u, 0x80003400u, 0x00003400u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000000u, 0x3fd00000u, 0x00002000u, 0x00004000u, 0x00000000u, 0x80003400u, 0x00003400u}},
    {{0xbf800000u, 0x00000001u}, {0xffffffffu, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u, 0x00000000u, 0xbff00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000bc00u, 0x0000bc00u}, {0xffffffffu, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u, 0x00000000u, 0xbff00000u, 0x00008001u, 0x00000000u, 0x00000000u, 0x0000bc00u, 0x0000bc00u}},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool InPass(std::uint32_t column, bool float64) {
    return (column == Float64Low || column == Float64High) == float64;
}

std::vector<std::uint32_t> Run(AgcDriver::VulkanDevice& device, bool float64, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        Input[lane * Inputs] = Vectors[lane].operands[0];
        Input[lane * Inputs + 1u] = Vectors[lane].operands[1];
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto code = float64 ? std::span<const std::uint32_t>(Float64Code) : std::span<const std::uint32_t>(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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
    return {Output.begin(), Output.end()};
}

std::string Where(std::uint32_t lane, std::uint32_t column, std::uint32_t denormals, bool ieee) {
    return std::string(Names[column]) + " of " + Hex(Vectors[lane].operands[0]) + ", " + Hex(Vectors[lane].operands[1]) + " in f32 denormal mode " +
           std::to_string(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0");
}

void CheckFlushed(const std::vector<std::uint32_t>& output, bool float64, std::uint32_t denormals, bool ieee) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            if (!InPass(column, float64)) continue;
            const auto value = output[lane * Results + column];
            const auto expected = Vectors[lane].denormalsFlushed[column];
            Require(value == expected, Where(lane, column, denormals, ieee) + " is " + Hex(value) + ", expected " + Hex(expected));
        }
    }
}

void CheckKept(const std::vector<std::uint32_t>& output, bool float64, bool ieee) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            if (!InPass(column, float64)) continue;
            const auto value = output[lane * Results + column];
            const auto& vector = Vectors[lane];
            Require(value == vector.denormalsKept[column] || value == vector.denormalsFlushed[column],
                    Where(lane, column, 3u, ieee) + " is " + Hex(value) + ", expected " + Hex(vector.denormalsKept[column]) + " or, on a driver that flushes f32 denormals, " +
                        Hex(vector.denormalsFlushed[column]));
        }
    }
}

void CheckSame(const std::vector<std::uint32_t>& output, const std::vector<std::uint32_t>& reference, bool float64, std::uint32_t denormals, std::uint32_t like, bool ieee) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            if (!InPass(column, float64)) continue;
            const auto value = output[lane * Results + column];
            const auto expected = reference[lane * Results + column];
            Require(value == expected, Where(lane, column, denormals, ieee) + " is " + Hex(value) + ", but mode " + std::to_string(like) + " gave " + Hex(expected));
        }
    }
}

ShaderRecompiler::ShaderFloatMode Mode(std::uint32_t denormals, bool ieee) {
    return ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u, true, ieee, false};
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool float64 : {false, true}) {
            for (const bool ieee : {false, true}) {
                const auto flushed = Run(*device, float64, Mode(0u, ieee));
                CheckFlushed(flushed, float64, 0u, ieee);
                const auto inputsFlushed = Run(*device, float64, Mode(2u, ieee));
                CheckFlushed(inputsFlushed, float64, 2u, ieee);
                CheckSame(inputsFlushed, flushed, float64, 2u, 0u, ieee);
                const auto kept = Run(*device, float64, Mode(3u, ieee));
                CheckKept(kept, float64, ieee);
                const auto outputsFlushed = Run(*device, float64, Mode(1u, ieee));
                CheckSame(outputsFlushed, kept, float64, 1u, 3u, ieee);
            }
        }
        std::puts("f32 conversion denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
