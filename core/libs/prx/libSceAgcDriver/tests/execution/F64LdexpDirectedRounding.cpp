#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 4;
alignas(256) std::array<std::uint32_t, Threads * Words> Input{};
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e180280u, 0x7e1a0280u, 0xd568000au, 0x00020d04u,
    0xe0781000u, 0x80010a01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 12> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0x9b6a38c1u, 0x002ccac5u, 0x000007f9u, 0x00000000u,
    0x01ecff5eu, 0x00071996u, 0x000007ffu, 0x00000000u,
    0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u,
    0xdf9ed6ceu, 0x000f7cf1u, 0x00000800u, 0x00000000u,
    0xcc0d9ea1u, 0x65e63e72u, 0x0000064au, 0x00000000u,
    0x00000000u, 0x00000000u, 0x000003ffu, 0x00000000u,
    0x11a7b794u, 0x000a6dddu, 0xffffffffu, 0x00000000u,
    0xb39d1feeu, 0x0086a1c0u, 0xffffffe6u, 0x00000000u,
    0xa20cb894u, 0x00069d4bu, 0xffffffecu, 0x00000000u,
    0x6940975eu, 0x37bdf5f5u, 0xfffffc6bu, 0x00000000u,
    0xa2976a1fu, 0x0001d7c0u, 0xfffff5aeu, 0x00000000u,
    0x4a5b3150u, 0x9eb56a8eu, 0x00000610u, 0x00000000u,
    0x592088f5u, 0x8007afd9u, 0x00000110u, 0x00000000u,
    0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u,
    0xab2c0000u, 0x80045a6cu, 0x00000802u, 0x00000000u,
    0x00000000u, 0x80100000u, 0x000007feu, 0x00000000u,
    0x00000000u, 0x81b44000u, 0xffffffd4u, 0x00000000u,
    0x00000000u, 0x80072c00u, 0xffffffe6u, 0x00000000u,
    0xd0d2d52eu, 0x802ef895u, 0xffffffdbu, 0x00000000u,
    0x2c201ee3u, 0x800eff49u, 0xfffff56bu, 0x00000000u,
    0x211f4580u, 0xcf447c03u, 0xfffffaf9u, 0x00000000u,
    0x388e447cu, 0x800963e6u, 0xfffffff1u, 0x00000000u,
    0xae044c45u, 0x7e075630u, 0x0000001fu, 0x00000000u,
    0x6524a0cfu, 0xff8d419au, 0x00000008u, 0x00000000u,
    0x4c450806u, 0x80099daeu, 0xffffffcfu, 0x00000000u,
    0x81800000u, 0x015a82f0u, 0xffffffd0u, 0x00000000u,
    0x12dbae48u, 0x002a3eecu, 0x000007ffu, 0x00000000u,
    0x08000000u, 0x0003d2bcu, 0xffffffd0u, 0x00000000u,
    0xf416f4fbu, 0x43d4a4ccu, 0xfffffb90u, 0x00000000u,
    0x69c4eac3u, 0x38dd1167u, 0xfffffc59u, 0x00000000u,
    0x1b98fbe0u, 0x000d8b9bu, 0x00000a32u, 0x00000000u,
    0xf2650b71u, 0xd4123edcu, 0x00000379u, 0x00000000u
};

constexpr std::array<std::uint32_t, 2> Denormals{0xc0u, 0x00u};

constexpr std::uint64_t Expected[3][2][Threads] = {
    {
        {
            0x7fbccac59b6a38c1ull, 0x7fec665807b3fd78ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
            0x7ff0000000000000ull, 0x0000000000000000ull, 0x000536ee88d3dbcaull, 0x00000002d4381674ull,
            0x0000000069d4ba21ull, 0x0000000003bebeaeull, 0x0000000000000001ull, 0xffb56a8e4a5b3150ull,
            0x90febf65648223d4ull, 0xffefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
            0x8000000510000000ull, 0x8000000001cb0000ull, 0x800000000001ef89ull, 0x8000000000000000ull,
            0x8000000147c03211ull, 0x80000012c7cc711cull, 0x7ff0000000000000ull, 0xffefffffffffffffull,
            0x8000000000000004ull, 0x0000000001a82f09ull, 0x7ff0000000000000ull, 0x0000000000000004ull,
            0x0000000000000002ull, 0x0000000003a22ceeull, 0x7ff0000000000000ull, 0xffefffffffffffffull
        },
        {
            0x7fbccac59b6a38c1ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0x0000000000000000ull,
            0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xffb56a8e4a5b3150ull,
            0x8000000000000000ull, 0xffefffffffffffffull, 0x8000000000000000ull, 0xffefffffffffffffull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xffefffffffffffffull,
            0x8000000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xffefffffffffffffull
        }
    },
    {
        {
            0x7fbccac59b6a38c1ull, 0x7fec665807b3fd78ull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x000536ee88d3dbcaull, 0x00000002d4381673ull,
            0x0000000069d4ba20ull, 0x0000000003bebeadull, 0x0000000000000000ull, 0xffb56a8e4a5b3150ull,
            0x90febf65648223d4ull, 0xffefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
            0x8000000510000000ull, 0x8000000001cb0000ull, 0x800000000001ef8aull, 0x8000000000000001ull,
            0x8000000147c03212ull, 0x80000012c7cc711dull, 0x7fefffffffffffffull, 0xfff0000000000000ull,
            0x8000000000000005ull, 0x0000000001a82f08ull, 0x7fefffffffffffffull, 0x0000000000000003ull,
            0x0000000000000001ull, 0x0000000003a22cedull, 0x7fefffffffffffffull, 0xfff0000000000000ull
        },
        {
            0x7fbccac59b6a38c1ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0x0000000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xffb56a8e4a5b3150ull,
            0x8000000000000000ull, 0xffefffffffffffffull, 0x8000000000000000ull, 0xfff0000000000000ull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x7fefffffffffffffull, 0xfff0000000000000ull,
            0x8000000000000000ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xfff0000000000000ull
        }
    },
    {
        {
            0x7fbccac59b6a38c1ull, 0x7fec665807b3fd78ull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x000536ee88d3dbcaull, 0x00000002d4381673ull,
            0x0000000069d4ba20ull, 0x0000000003bebeadull, 0x0000000000000000ull, 0xffb56a8e4a5b3150ull,
            0x90febf65648223d4ull, 0xffefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
            0x8000000510000000ull, 0x8000000001cb0000ull, 0x800000000001ef89ull, 0x8000000000000000ull,
            0x8000000147c03211ull, 0x80000012c7cc711cull, 0x7fefffffffffffffull, 0xffefffffffffffffull,
            0x8000000000000004ull, 0x0000000001a82f08ull, 0x7fefffffffffffffull, 0x0000000000000003ull,
            0x0000000000000001ull, 0x0000000003a22cedull, 0x7fefffffffffffffull, 0xffefffffffffffffull
        },
        {
            0x7fbccac59b6a38c1ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0x0000000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xffb56a8e4a5b3150ull,
            0x8000000000000000ull, 0xffefffffffffffffull, 0x8000000000000000ull, 0xffefffffffffffffull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x8000000000000000ull, 0x8000000000000000ull, 0x7fefffffffffffffull, 0xffefffffffffffffull,
            0x8000000000000000ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0xffefffffffffffffull
        }
    }
};

constexpr std::array<std::uint32_t, Threads * Words> Boundary{
    0x00000001u, 0x00000000u, 0x00000034u, 0x00000000u,
    0x00000000u, 0x00100000u, 0xffffffffu, 0x00000000u,
    0x00000001u, 0x80000000u, 0x00000034u, 0x00000000u,
    0x00000000u, 0x80100000u, 0xffffffffu, 0x00000000u,
    0x00000000u, 0x00100000u, 0xffffffccu, 0x00000000u,
    0x00000000u, 0x00100000u, 0xffffffcbu, 0x00000000u,
    0xffffffffu, 0x000fffffu, 0x00000000u, 0x00000000u,
    0xffffffffu, 0x000fffffu, 0x00000001u, 0x00000000u,
    0xffffffffu, 0x001fffffu, 0xffffffffu, 0x00000000u,
    0xffffffffu, 0x801fffffu, 0xffffffffu, 0x00000000u,
    0x00000000u, 0x3ff00000u, 0xfffffbceu, 0x00000000u,
    0x00000000u, 0x3ff00000u, 0xfffffbcdu, 0x00000000u,
    0x00000000u, 0xbff00000u, 0xfffffbcdu, 0x00000000u,
    0x00000000u, 0x3ff00000u, 0xfffffc02u, 0x00000000u,
    0xffffffffu, 0x3fffffffu, 0xfffffc01u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x00000831u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x00000832u, 0x00000000u,
    0x00000001u, 0x80000000u, 0x00000832u, 0x00000000u,
    0x00000000u, 0x00080000u, 0xffffffcdu, 0x00000000u,
    0x00000000u, 0x00080000u, 0x00000033u, 0x00000000u,
    0x00000000u, 0x7fe00000u, 0x00000001u, 0x00000000u,
    0xffffffffu, 0x7fefffffu, 0xfffff7ceu, 0x00000000u,
    0xffffffffu, 0x7fefffffu, 0x80000000u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x7fffffffu, 0x00000000u,
    0x00000003u, 0x00000000u, 0xffffffffu, 0x00000000u,
    0x00000003u, 0x80000000u, 0xffffffffu, 0x00000000u,
    0x00000002u, 0x00000000u, 0xffffffffu, 0x00000000u,
    0x00000001u, 0x00000000u, 0xffffffffu, 0x00000000u,
    0xffffffffu, 0x000fffffu, 0xffffffccu, 0x00000000u,
    0x00001234u, 0x00000000u, 0x00000028u, 0x00000000u,
    0x00000000u, 0x3ff80000u, 0xfffffbceu, 0x00000000u,
    0xffffffffu, 0x800fffffu, 0x00000001u, 0x00000000u
};

constexpr std::uint64_t BoundaryExpected[3][2][Threads] = {
    {
        {
            0x0010000000000000ull, 0x0008000000000000ull, 0x8010000000000000ull, 0x8008000000000000ull,
            0x0000000000000001ull, 0x0000000000000001ull, 0x000fffffffffffffull, 0x001ffffffffffffeull,
            0x0010000000000000ull, 0x800fffffffffffffull, 0x0000000000000001ull, 0x0000000000000001ull,
            0x8000000000000000ull, 0x0010000000000000ull, 0x0010000000000000ull, 0x7fe0000000000000ull,
            0x7ff0000000000000ull, 0xffefffffffffffffull, 0x0000000000000001ull, 0x0330000000000000ull,
            0x7ff0000000000000ull, 0x0000000000000001ull, 0x0000000000000001ull, 0x7ff0000000000000ull,
            0x0000000000000002ull, 0x8000000000000001ull, 0x0000000000000001ull, 0x0000000000000001ull,
            0x0000000000000001ull, 0x0012340000000000ull, 0x0000000000000002ull, 0x801ffffffffffffeull
        },
        {
            0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x8000000000000000ull, 0x0010000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull
        }
    },
    {
        {
            0x0010000000000000ull, 0x0008000000000000ull, 0x8010000000000000ull, 0x8008000000000000ull,
            0x0000000000000001ull, 0x0000000000000000ull, 0x000fffffffffffffull, 0x001ffffffffffffeull,
            0x000fffffffffffffull, 0x8010000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull,
            0x8000000000000001ull, 0x0010000000000000ull, 0x000fffffffffffffull, 0x7fe0000000000000ull,
            0x7fefffffffffffffull, 0xfff0000000000000ull, 0x0000000000000001ull, 0x0330000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7fefffffffffffffull,
            0x0000000000000001ull, 0x8000000000000002ull, 0x0000000000000001ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0012340000000000ull, 0x0000000000000001ull, 0x801ffffffffffffeull
        },
        {
            0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x8000000000000000ull, 0x0010000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull
        }
    },
    {
        {
            0x0010000000000000ull, 0x0008000000000000ull, 0x8010000000000000ull, 0x8008000000000000ull,
            0x0000000000000001ull, 0x0000000000000000ull, 0x000fffffffffffffull, 0x001ffffffffffffeull,
            0x000fffffffffffffull, 0x800fffffffffffffull, 0x0000000000000001ull, 0x0000000000000000ull,
            0x8000000000000000ull, 0x0010000000000000ull, 0x000fffffffffffffull, 0x7fe0000000000000ull,
            0x7fefffffffffffffull, 0xffefffffffffffffull, 0x0000000000000001ull, 0x0330000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7fefffffffffffffull,
            0x0000000000000001ull, 0x8000000000000001ull, 0x0000000000000001ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0012340000000000ull, 0x0000000000000001ull, 0x801ffffffffffffeull
        },
        {
            0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x8000000000000000ull, 0x0010000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x7fefffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull,
            0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull
        }
    }
};

struct Table {
    const char* name;
    const std::array<std::uint32_t, Threads * Words>& values;
    const std::uint64_t (&expected)[3][2][Threads];
};

const std::array<Table, 2> Tables{{{"", Values, Expected}, {"boundary ", Boundary, BoundaryExpected}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::span<const std::uint32_t> SetRoundMode(std::uint32_t field) {
    RuntimeCode[0] = 0xbfa40000u | field;
    std::copy(Code.begin(), Code.end(), RuntimeCode.begin() + 1);
    return RuntimeCode;
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

ShaderRecompiler::RecompileRequest Request(const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const std::array<ShaderRecompiler::MemoryRegion, 1>& memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return request;
}

std::vector<std::uint32_t> Spirv(std::uint32_t floatMode, std::span<const std::uint32_t> code) {
    constexpr std::array<std::uint32_t, 9> capabilities{
        spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses,
        spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityDenormPreserve, spv::CapabilitySignedZeroInfNanPreserve,
    };
    constexpr std::array<std::string_view, 4> extensions{
        "SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls",
        "SPV_KHR_storage_buffer_storage_class",
    };
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00403000u;
    target.spirvVersion = 0x00010600u;
    target.subgroupSize = 32;
    target.bdaAbiVersion = 1;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = extensions;
    target.maxWorkgroupSize = {1024, 1024, 64};
    target.maxWorkgroupInvocations = 1024;
    target.maxWorkgroupSharedMemoryBytes = 49152;
    target.float64DenormPreserve = false;
    const auto userData = UserData();
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    auto request = Request(target, code, userData, memory);
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, true, false, false};
    return ShaderRecompiler::Recompile(request).spirv.Copy();
}

bool HasFloat64Values(std::span<const std::uint32_t> words) {
    std::vector<std::uint32_t> types;
    bool body = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        Require(count != 0 && cursor + count <= words.size(), "malformed SPIR-V while scanning for f64 values");
        if (opcode == spv::OpTypeFloat && count >= 3 && words[cursor + 2] == 64u) types.push_back(words[cursor + 1]);
        if (opcode == spv::OpTypeVector && count == 4 && std::find(types.begin(), types.end(), words[cursor + 2]) != types.end()) types.push_back(words[cursor + 1]);
        body = body || opcode == spv::OpFunction;
        if (body && count >= 3 && std::find(types.begin(), types.end(), words[cursor + 1]) != types.end()) return true;
        cursor += count;
    }
    return false;
}

void VerifyIntegerLowering() {
    Require(HasFloat64Values(Spirv(0xc0u, Code)), "v_ldexp_f64 FLOAT_MODE 0xc0 has no f64 value, so the scan cannot see one");
    for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
        for (const std::uint32_t denormal : Denormals) {
            const std::uint32_t field = rounding << 2u;
            Require(!HasFloat64Values(Spirv(denormal | field, Code)), "v_ldexp_f64 FLOAT_MODE " + Hex(denormal | field) + " computes an f64 value, which a device without DenormPreserve 64 may flush");
            Require(!HasFloat64Values(Spirv(denormal, SetRoundMode(field))), "v_ldexp_f64 after s_round_mode " + Hex(field) + " computes an f64 value, which a device without DenormPreserve 64 may flush");
        }
    }
}

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, bool preserve, ShaderRecompiler::ShaderFloatMode floatMode, std::span<const std::uint32_t> code, const Table& table) {
    Input = table.values;
    Output.fill(0xdeadbeefu);
    const auto userData = UserData();
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    auto request = Request(device.Target(), code, userData, memory);
    request.target.float64DenormPreserve = request.target.float64DenormPreserve && preserve;
    request.context.floatMode = floatMode;
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return std::nullopt;
}

void Check(const Table& table, const std::uint64_t (&expected)[Threads], const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &table.values[tid * Words];
        const std::uint32_t* out = &Output[tid * Words];
        const std::uint64_t source = in[0] | static_cast<std::uint64_t>(in[1]) << 32u;
        const std::uint64_t actual = out[0] | static_cast<std::uint64_t>(out[1]) << 32u;
        const auto power = static_cast<std::int32_t>(in[2]);
        Require(actual == expected[tid], "v_ldexp_f64 " + mode + " of " + Hex(source) + " by " + std::to_string(power) + " is " + Hex(actual) + ", expected " + Hex(expected[tid]));
    }
}

}

int main() {
    try {
        VerifyIntegerLowering();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        std::uint32_t checked = 0u;
        for (const bool preserve : {true, false}) {
            if (!preserve && !device->Target().float64DenormPreserve) continue;
            const std::string target = preserve ? "" : " without DenormPreserve 64";
            for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
                for (std::uint32_t denormal = 0u; denormal < Denormals.size(); ++denormal) {
                    for (const bool ieee : {false, true}) {
                        const std::uint32_t field = rounding << 2u;
                        const std::string flags = std::string(ieee ? ", IEEE=1" : ", IEEE=0") + target;
                        for (const Table& table : Tables) {
                            const std::string declared = table.name + std::string("FLOAT_MODE ") + Hex(Denormals[denormal] | field) + flags;
                            const auto refusal = Run(*device, preserve, ShaderRecompiler::ShaderFloatMode{Denormals[denormal] | field, true, ieee, false}, Code, table);
                            Require(!refusal, declared + ": " + refusal.value_or(""));
                            Check(table, table.expected[rounding - 1u][denormal], declared);
                            const std::string runtime = table.name + std::string("s_round_mode ") + Hex(field) + " after FLOAT_MODE " + Hex(Denormals[denormal]) + flags;
                            const auto runtimeRefusal = Run(*device, preserve, ShaderRecompiler::ShaderFloatMode{Denormals[denormal], true, ieee, false}, SetRoundMode(field), table);
                            Require(!runtimeRefusal, runtime + ": " + runtimeRefusal.value_or(""));
                            Check(table, table.expected[rounding - 1u][denormal], runtime);
                            checked += 2u;
                        }
                    }
                }
            }
        }
        std::printf("v_ldexp_f64 directed rounding tests passed (%u runs, declared and set by s_round_mode, %s)\n", checked, device->Target().float64DenormPreserve ? "with and without DenormPreserve 64" : "the device has no DenormPreserve 64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
