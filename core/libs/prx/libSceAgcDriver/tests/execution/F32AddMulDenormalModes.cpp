#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 96;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Instructions = 7;
constexpr std::uint32_t FirstMultiply = 3;
constexpr std::uint32_t FirstConstantFactor = 5;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 20> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x06140b04u, 0x08160b04u, 0x0a180b04u,
    0x101a0b04u, 0x0e1c0b04u, 0x101e08ffu, 0x3fc00000u, 0xd5070010u, 0x0001ff04u, 0x3fc00000u, 0xe0781000u,
    0x80010a02u, 0xe07c1010u, 0x80010e02u, 0xbf810000u,
};

constexpr const char* Names[Instructions] = {"v_add_f32", "v_sub_f32", "v_subrev_f32", "v_mul_f32", "v_mul_legacy_f32", "v_mul_f32 1.5", "v_mul_legacy_f32 1.5"};

template <std::uint32_t Count>
struct Vector {
    std::uint32_t operands[2];
    std::uint32_t outputsFlushed[Count];
    std::uint32_t inputsFlushed[Count];
};

constexpr std::array<Vector<FirstMultiply + 2>, 40> ArithmeticVectors{{
    {{0x00000000u, 0x00000001u}, {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x00000001u, 0x00800002u}, {0x00800003u, 0x80800001u, 0x00800001u, 0x00000000u, 0x00000000u}, {0x00800002u, 0x80800002u, 0x00800002u, 0x00000000u, 0x00000000u}},
    {{0x00000001u, 0x81000001u}, {0x81000000u, 0x01000002u, 0x81000002u, 0x80000000u, 0x80000000u}, {0x81000001u, 0x01000001u, 0x81000001u, 0x80000000u, 0x00000000u}},
    {{0x00000002u, 0x3f800000u}, {0x3f800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u}, {0x3f800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u}},
    {{0x00000002u, 0xff7fffffu}, {0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0xb57fffffu, 0xb57fffffu}, {0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x80000000u, 0x00000000u}},
    {{0x00000003u, 0x80000001u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u}},
    {{0x00400000u, 0x00000002u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x00400000u, 0x807fffffu}, {0x80000000u, 0x00bfffffu, 0x80bfffffu, 0x80000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u}},
    {{0x007fffffu, 0x00800000u}, {0x00ffffffu, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00800000u, 0x80800000u, 0x00800000u, 0x00000000u, 0x00000000u}},
    {{0x007fffffu, 0x80800002u}, {0x80000000u, 0x01000000u, 0x81000000u, 0x80000000u, 0x80000000u}, {0x80800002u, 0x00800002u, 0x80800002u, 0x80000000u, 0x00000000u}},
    {{0x00800000u, 0x80000001u}, {0x00000000u, 0x00800001u, 0x80800001u, 0x80000000u, 0x80000000u}, {0x00800000u, 0x00800000u, 0x80800000u, 0x80000000u, 0x00000000u}},
    {{0x00800001u, 0x33800000u}, {0x33800000u, 0xb3800000u, 0x33800000u, 0x00000000u, 0x00000000u}, {0x33800000u, 0xb3800000u, 0x33800000u, 0x00000001u, 0x00000001u}},
    {{0x00800002u, 0x00800000u}, {0x01000001u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}, {0x01000001u, 0x00000002u, 0x80000002u, 0x00000000u, 0x00000000u}},
    {{0x00ffffffu, 0x00000002u}, {0x01000000u, 0x00fffffdu, 0x80fffffdu, 0x00000000u, 0x00000000u}, {0x00ffffffu, 0x00ffffffu, 0x80ffffffu, 0x00000000u, 0x00000000u}},
    {{0x00ffffffu, 0x80800002u}, {0x00000000u, 0x01400000u, 0x81400000u, 0x80000000u, 0x80000000u}, {0x007ffffdu, 0x01400000u, 0x81400000u, 0x80000000u, 0x80000000u}},
    {{0x01000000u, 0x80000003u}, {0x00fffffdu, 0x01000002u, 0x81000002u, 0x80000000u, 0x80000000u}, {0x01000000u, 0x01000000u, 0x81000000u, 0x80000000u, 0x00000000u}},
    {{0x01000001u, 0x80000001u}, {0x01000000u, 0x01000002u, 0x81000002u, 0x80000000u, 0x80000000u}, {0x01000001u, 0x01000001u, 0x81000001u, 0x80000000u, 0x00000000u}},
    {{0x3f800000u, 0x807fffffu}, {0x3f800000u, 0x3f800000u, 0xbf800000u, 0x80000000u, 0x80000000u}, {0x3f800000u, 0x3f800000u, 0xbf800000u, 0x80000000u, 0x00000000u}},
    {{0x34000000u, 0x00800001u}, {0x34000000u, 0x34000000u, 0xb4000000u, 0x00000000u, 0x00000000u}, {0x34000000u, 0x34000000u, 0xb4000000u, 0x00000001u, 0x00000001u}},
    {{0x7f7fffffu, 0x00000003u}, {0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x35bfffffu, 0x35bfffffu}, {0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {{0x80000000u, 0x00000001u}, {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u}, {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u}},
    {{0x80000001u, 0x00800002u}, {0x00800001u, 0x80800003u, 0x00800003u, 0x80000000u, 0x80000000u}, {0x00800002u, 0x80800002u, 0x00800002u, 0x80000000u, 0x00000000u}},
    {{0x80000001u, 0x80ffffffu}, {0x81000000u, 0x00fffffeu, 0x80fffffeu, 0x00000000u, 0x00000000u}, {0x80ffffffu, 0x00ffffffu, 0x80ffffffu, 0x00000000u, 0x00000000u}},
    {{0x80000002u, 0x3f800000u}, {0x3f800000u, 0xbf800000u, 0x3f800000u, 0x80000000u, 0x80000000u}, {0x3f800000u, 0xbf800000u, 0x3f800000u, 0x80000000u, 0x00000000u}},
    {{0x80000002u, 0xff7fffffu}, {0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x357fffffu, 0x357fffffu}, {0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {{0x80000003u, 0x7f800000u}, {0x7f800000u, 0xff800000u, 0x7f800000u, 0xff800000u, 0xff800000u}, {0x7f800000u, 0xff800000u, 0x7f800000u, 0xffc00000u, 0x00000000u}},
    {{0x80400000u, 0x00000002u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u}, {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u}},
    {{0x80400000u, 0x80000003u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {{0x807fffffu, 0x00800000u}, {0x00000000u, 0x80ffffffu, 0x00ffffffu, 0x80000000u, 0x80000000u}, {0x00800000u, 0x80800000u, 0x00800000u, 0x80000000u, 0x00000000u}},
    {{0x807fffffu, 0x80800001u}, {0x81000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}, {0x80800001u, 0x00800001u, 0x80800001u, 0x00000000u, 0x00000000u}},
    {{0x80800000u, 0x80000001u}, {0x80800001u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80800000u, 0x80800000u, 0x00800000u, 0x00000000u, 0x00000000u}},
    {{0x80800001u, 0x33800000u}, {0x33800000u, 0xb3800000u, 0x33800000u, 0x80000000u, 0x80000000u}, {0x33800000u, 0xb3800000u, 0x33800000u, 0x80000001u, 0x80000001u}},
    {{0x80800002u, 0x00800000u}, {0x80000000u, 0x81000001u, 0x01000001u, 0x80000000u, 0x80000000u}, {0x80000002u, 0x81000001u, 0x01000001u, 0x80000000u, 0x80000000u}},
    {{0x80ffffffu, 0x00000002u}, {0x80fffffdu, 0x81000000u, 0x01000000u, 0x80000000u, 0x80000000u}, {0x80ffffffu, 0x80ffffffu, 0x00ffffffu, 0x80000000u, 0x00000000u}},
    {{0x80ffffffu, 0x80800002u}, {0x81400000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x81400000u, 0x807ffffdu, 0x007ffffdu, 0x00000000u, 0x00000000u}},
    {{0x81000000u, 0x80000003u}, {0x81000002u, 0x80fffffdu, 0x00fffffdu, 0x00000000u, 0x00000000u}, {0x81000000u, 0x81000000u, 0x01000000u, 0x00000000u, 0x00000000u}},
    {{0x81000001u, 0x80000001u}, {0x81000002u, 0x81000000u, 0x01000000u, 0x00000000u, 0x00000000u}, {0x81000001u, 0x81000001u, 0x01000001u, 0x00000000u, 0x00000000u}},
    {{0xbf800000u, 0x807fffffu}, {0xbf800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u}, {0xbf800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u}},
    {{0xb4000000u, 0x00000003u}, {0xb4000000u, 0xb4000000u, 0x34000000u, 0x80000000u, 0x80000000u}, {0xb4000000u, 0xb4000000u, 0x34000000u, 0x80000000u, 0x00000000u}},
    {{0xff7fffffu, 0x00000003u}, {0xff7fffffu, 0xff7fffffu, 0x7f7fffffu, 0xb5bfffffu, 0xb5bfffffu}, {0xff7fffffu, 0xff7fffffu, 0x7f7fffffu, 0x80000000u, 0x00000000u}},
}};

constexpr std::array<Vector<2>, 24> TinyProductVectors{{
    {{0x3f7ffffeu, 0x00800001u}, {0x00800000u, 0x00800000u}, {0x00800000u, 0x00800000u}},
    {{0x3f7ffffeu, 0x80800001u}, {0x80800000u, 0x80800000u}, {0x80800000u, 0x80800000u}},
    {{0xbf7ffffeu, 0x00800001u}, {0x80800000u, 0x80800000u}, {0x80800000u, 0x80800000u}},
    {{0xbf7ffffeu, 0x80800001u}, {0x00800000u, 0x00800000u}, {0x00800000u, 0x00800000u}},
    {{0x3f800001u, 0x007fffffu}, {0x00800000u, 0x00800000u}, {0x00000000u, 0x00000000u}},
    {{0x3f800001u, 0x807fffffu}, {0x80800000u, 0x80800000u}, {0x80000000u, 0x00000000u}},
    {{0xbf800001u, 0x007fffffu}, {0x80800000u, 0x80800000u}, {0x80000000u, 0x00000000u}},
    {{0xbf800001u, 0x807fffffu}, {0x00800000u, 0x00800000u}, {0x00000000u, 0x00000000u}},
    {{0x3f800002u, 0x007ffffeu}, {0x00800000u, 0x00800000u}, {0x00000000u, 0x00000000u}},
    {{0x3f800002u, 0x807ffffeu}, {0x80800000u, 0x80800000u}, {0x80000000u, 0x00000000u}},
    {{0xbf800002u, 0x007ffffeu}, {0x80800000u, 0x80800000u}, {0x80000000u, 0x00000000u}},
    {{0xbf800002u, 0x807ffffeu}, {0x00800000u, 0x00800000u}, {0x00000000u, 0x00000000u}},
    {{0x3f7ffffeu, 0x00800000u}, {0x00000000u, 0x00000000u}, {0x007fffffu, 0x007fffffu}},
    {{0xbf7fffffu, 0x00400000u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x3f800001u, 0x80000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0xbfffffffu, 0x00400000u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x3f400000u, 0x00800000u}, {0x00000000u, 0x00000000u}, {0x00600000u, 0x00600000u}},
    {{0x40000000u, 0x807fffffu}, {0x80fffffeu, 0x80fffffeu}, {0x80000000u, 0x00000000u}},
    {{0xcb000000u, 0x007ffffeu}, {0x8bfffffcu, 0x8bfffffcu}, {0x80000000u, 0x00000000u}},
    {{0xcb7fffffu, 0x807ffffeu}, {0x0c7ffffbu, 0x0c7ffffbu}, {0x00000000u, 0x00000000u}},
    {{0x5f800000u, 0x00400000u}, {0x20000000u, 0x20000000u}, {0x00000000u, 0x00000000u}},
    {{0x7f000000u, 0x80400000u}, {0xbf800000u, 0xbf800000u}, {0x80000000u, 0x00000000u}},
    {{0xb3800000u, 0x00ffffffu}, {0x80000000u, 0x80000000u}, {0x80000001u, 0x80000001u}},
    {{0xb4000000u, 0x80800000u}, {0x00000000u, 0x00000000u}, {0x00000001u, 0x00000001u}},
}};

constexpr std::array<Vector<2>, 32> ConstantFactorVectors{{
    {{0x00000001u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00000002u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00000003u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00155555u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x003fffffu, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00400000u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00400001u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x00555555u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x007ffffeu, 0x00000001u}, {0x00bffffdu, 0x00bffffdu}, {0x00000000u, 0x00000000u}},
    {{0x007fffffu, 0x00000001u}, {0x00bffffeu, 0x00bffffeu}, {0x00000000u, 0x00000000u}},
    {{0x00800000u, 0x00000001u}, {0x00c00000u, 0x00c00000u}, {0x00c00000u, 0x00c00000u}},
    {{0x00800001u, 0x00000001u}, {0x00c00002u, 0x00c00002u}, {0x00c00002u, 0x00c00002u}},
    {{0x00aaaaabu, 0x00000001u}, {0x01000000u, 0x01000000u}, {0x01000000u, 0x01000000u}},
    {{0x00ffffffu, 0x00000001u}, {0x013fffffu, 0x013fffffu}, {0x013fffffu, 0x013fffffu}},
    {{0x01000000u, 0x00000001u}, {0x01400000u, 0x01400000u}, {0x01400000u, 0x01400000u}},
    {{0x3f800000u, 0x00000001u}, {0x3fc00000u, 0x3fc00000u}, {0x3fc00000u, 0x3fc00000u}},
    {{0x00000000u, 0x00000001u}, {0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u}},
    {{0x80000001u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80000002u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80000003u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80155555u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x803fffffu, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80400000u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80400001u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x80555555u, 0x00000001u}, {0x80000000u, 0x80000000u}, {0x80000000u, 0x00000000u}},
    {{0x807ffffeu, 0x00000001u}, {0x80bffffdu, 0x80bffffdu}, {0x80000000u, 0x00000000u}},
    {{0x807fffffu, 0x00000001u}, {0x80bffffeu, 0x80bffffeu}, {0x80000000u, 0x00000000u}},
    {{0x80800000u, 0x00000001u}, {0x80c00000u, 0x80c00000u}, {0x80c00000u, 0x80c00000u}},
    {{0x80800001u, 0x00000001u}, {0x80c00002u, 0x80c00002u}, {0x80c00002u, 0x80c00002u}},
    {{0x80aaaaabu, 0x00000001u}, {0x81000000u, 0x81000000u}, {0x81000000u, 0x81000000u}},
    {{0x80ffffffu, 0x00000001u}, {0x813fffffu, 0x813fffffu}, {0x813fffffu, 0x813fffffu}},
    {{0x81000000u, 0x00000001u}, {0x81400000u, 0x81400000u}, {0x81400000u, 0x81400000u}},
}};

constexpr std::uint32_t FirstTinyProductLane = static_cast<std::uint32_t>(ArithmeticVectors.size());
constexpr std::uint32_t FirstConstantFactorLane = FirstTinyProductLane + static_cast<std::uint32_t>(TinyProductVectors.size());
static_assert(FirstConstantFactorLane + ConstantFactorVectors.size() <= Threads);

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

template <std::uint32_t Count, std::size_t Size>
void Fill(const std::array<Vector<Count>, Size>& vectors, std::uint32_t firstLane) {
    for (std::uint32_t row = 0; row < Size; ++row) {
        Input[(firstLane + row) * Inputs] = vectors[row].operands[0];
        Input[(firstLane + row) * Inputs + 1u] = vectors[row].operands[1];
    }
}

bool Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    Fill(ArithmeticVectors, 0u);
    Fill(TinyProductVectors, FirstTinyProductLane);
    Fill(ConstantFactorVectors, FirstConstantFactorLane);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
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
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        const std::string message = error.what();
        const bool keepsDenormals = request.target.float32DenormPreserve && !request.target.float32FlushToZero;
        Require(!keepsDenormals && message.find("needs DenormPreserve 32") != std::string::npos, "f32 add and multiply FLOAT_MODE " + Hex(floatMode.floatMode) + ": " + message);
        return false;
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return true;
}

template <std::uint32_t Count, std::size_t Size>
void Check(const std::array<Vector<Count>, Size>& vectors, std::uint32_t firstLane, std::uint32_t firstInstruction, std::uint32_t denormals, bool ieee) {
    for (std::uint32_t row = 0; row < Size; ++row) {
        const auto& vector = vectors[row];
        for (std::uint32_t index = 0; index < Count; ++index) {
            const std::uint32_t instruction = firstInstruction + index;
            const auto actual = Output[(firstLane + row) * Results + instruction];
            const auto expected = denormals == 1u ? vector.outputsFlushed[index] : vector.inputsFlushed[index];
            Require(actual == expected, std::string(Names[instruction]) + " f32 denormal mode " + std::to_string(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0") + " of " +
                                            Hex(vector.operands[0]) + ", " + Hex(vector.operands[1]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t checked = 0u;
        for (const std::uint32_t denormals : {1u, 2u}) {
            for (const bool ieee : {false, true}) {
                if (!Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u, true, ieee, false})) continue;
                ++checked;
                Check(ArithmeticVectors, 0u, 0u, denormals, ieee);
                Check(TinyProductVectors, FirstTinyProductLane, FirstMultiply, denormals, ieee);
                Check(ConstantFactorVectors, FirstConstantFactorLane, FirstConstantFactor, denormals, ieee);
            }
        }
        std::printf("f32 add and multiply denormal mode 1 and 2 tests passed (%u of 4 modes run; the others need DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
