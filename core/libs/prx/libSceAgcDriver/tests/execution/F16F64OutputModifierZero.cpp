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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t RowCount = 20;
constexpr std::uint32_t ModeCount = 3;
constexpr std::uint32_t OmodCount = 3;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e140304u, 0xd532000au, 0x00020b04u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

constexpr std::uint32_t F16Rows[RowCount][2] = {
    {0xabcd0000u, 0x00008000u},
    {0xabcd8000u, 0x00008000u},
    {0xabcd3c00u, 0x0000bc00u},
    {0xabcd0001u, 0x00000000u},
    {0xabcd8001u, 0x00008000u},
    {0xabcd0400u, 0x00000000u},
    {0xabcd8400u, 0x00008000u},
    {0xabcd07ffu, 0x00000000u},
    {0xabcd87ffu, 0x00008000u},
    {0xabcd0800u, 0x00000000u},
    {0xabcd8800u, 0x00008000u},
    {0xabcd77ffu, 0x00000000u},
    {0xabcdf7ffu, 0x00008000u},
    {0xabcd7c00u, 0x00000000u},
    {0xabcdfc00u, 0x00008000u},
    {0xabcd7e01u, 0x00000000u},
    {0xabcdfe01u, 0x00000000u},
    {0xabcd3c00u, 0x00000000u},
    {0xabcdbc00u, 0x00008000u},
    {0xabcd0600u, 0x00000000u},
};

constexpr std::uint32_t F64Rows[RowCount][4] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0xbff00000u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00100000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80100000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x001fffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x801fffffu, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00200000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80200000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7fdfffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffdfffffu, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0x80000000u},
    {0x00000001u, 0x7ff80000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0xfff80000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00180000u, 0x00000000u, 0x00000000u},
};

constexpr std::uint32_t ExpectedF16[ModeCount * OmodCount][RowCount][1] = {
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0800u},
    {0xabcd8800u},
    {0xabcd0bffu},
    {0xabcd8bffu},
    {0xabcd0c00u},
    {0xabcd8c00u},
    {0xabcd7bffu},
    {0xabcdfbffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd4000u},
    {0xabcdc000u},
    {0xabcd0a00u},
    },
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0c00u},
    {0xabcd8c00u},
    {0xabcd0fffu},
    {0xabcd8fffu},
    {0xabcd1000u},
    {0xabcd9000u},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd4400u},
    {0xabcdc400u},
    {0xabcd0e00u},
    },
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0400u},
    {0xabcd8400u},
    {0xabcd73ffu},
    {0xabcdf3ffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd3800u},
    {0xabcdb800u},
    {0xabcd0000u},
    },
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0800u},
    {0xabcd8800u},
    {0xabcd0bffu},
    {0xabcd8bffu},
    {0xabcd0c00u},
    {0xabcd8c00u},
    {0xabcd7bffu},
    {0xabcdfbffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd4000u},
    {0xabcdc000u},
    {0xabcd0a00u},
    },
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0c00u},
    {0xabcd8c00u},
    {0xabcd0fffu},
    {0xabcd8fffu},
    {0xabcd1000u},
    {0xabcd9000u},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd4400u},
    {0xabcdc400u},
    {0xabcd0e00u},
    },
    {
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0400u},
    {0xabcd8400u},
    {0xabcd73ffu},
    {0xabcdf3ffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd3800u},
    {0xabcdb800u},
    {0xabcd0000u},
    },
    {
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0000u},
    {0xabcd0001u},
    {0xabcd8001u},
    {0xabcd0400u},
    {0xabcd8400u},
    {0xabcd07ffu},
    {0xabcd87ffu},
    {0xabcd0800u},
    {0xabcd8800u},
    {0xabcd77ffu},
    {0xabcdf7ffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd3c00u},
    {0xabcdbc00u},
    {0xabcd0600u},
    },
    {
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0000u},
    {0xabcd0001u},
    {0xabcd8001u},
    {0xabcd0400u},
    {0xabcd8400u},
    {0xabcd07ffu},
    {0xabcd87ffu},
    {0xabcd0800u},
    {0xabcd8800u},
    {0xabcd77ffu},
    {0xabcdf7ffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd3c00u},
    {0xabcdbc00u},
    {0xabcd0600u},
    },
    {
    {0xabcd0000u},
    {0xabcd8000u},
    {0xabcd0000u},
    {0xabcd0001u},
    {0xabcd8001u},
    {0xabcd0400u},
    {0xabcd8400u},
    {0xabcd07ffu},
    {0xabcd87ffu},
    {0xabcd0800u},
    {0xabcd8800u},
    {0xabcd77ffu},
    {0xabcdf7ffu},
    {0xabcd7c00u},
    {0xabcdfc00u},
    {0xabcd7e01u},
    {0xabcdfe01u},
    {0xabcd3c00u},
    {0xabcdbc00u},
    {0xabcd0600u},
    },
};

constexpr std::uint32_t ExpectedF64[ModeCount * OmodCount][RowCount][2] = {
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00200000u},
    {0x00000000u, 0x80200000u},
    {0x00000000u, 0x002fffffu},
    {0x00000000u, 0x802fffffu},
    {0x00000000u, 0x00300000u},
    {0x00000000u, 0x80300000u},
    {0x00000000u, 0x7fefffffu},
    {0x00000000u, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x00280000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00300000u},
    {0x00000000u, 0x80300000u},
    {0x00000000u, 0x003fffffu},
    {0x00000000u, 0x803fffffu},
    {0x00000000u, 0x00400000u},
    {0x00000000u, 0x80400000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x00380000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00100000u},
    {0x00000000u, 0x80100000u},
    {0x00000000u, 0x7fcfffffu},
    {0x00000000u, 0xffcfffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x3fe00000u},
    {0x00000000u, 0xbfe00000u},
    {0x00000000u, 0x00000000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00200000u},
    {0x00000000u, 0x80200000u},
    {0x00000000u, 0x002fffffu},
    {0x00000000u, 0x802fffffu},
    {0x00000000u, 0x00300000u},
    {0x00000000u, 0x80300000u},
    {0x00000000u, 0x7fefffffu},
    {0x00000000u, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x00280000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00300000u},
    {0x00000000u, 0x80300000u},
    {0x00000000u, 0x003fffffu},
    {0x00000000u, 0x803fffffu},
    {0x00000000u, 0x00400000u},
    {0x00000000u, 0x80400000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x00380000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00100000u},
    {0x00000000u, 0x80100000u},
    {0x00000000u, 0x7fcfffffu},
    {0x00000000u, 0xffcfffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x3fe00000u},
    {0x00000000u, 0xbfe00000u},
    {0x00000000u, 0x00000000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u},
    {0x00000001u, 0x80000000u},
    {0x00000000u, 0x00100000u},
    {0x00000000u, 0x80100000u},
    {0x00000000u, 0x001fffffu},
    {0x00000000u, 0x801fffffu},
    {0x00000000u, 0x00200000u},
    {0x00000000u, 0x80200000u},
    {0x00000000u, 0x7fdfffffu},
    {0x00000000u, 0xffdfffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x00180000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u},
    {0x00000001u, 0x80000000u},
    {0x00000000u, 0x00100000u},
    {0x00000000u, 0x80100000u},
    {0x00000000u, 0x001fffffu},
    {0x00000000u, 0x801fffffu},
    {0x00000000u, 0x00200000u},
    {0x00000000u, 0x80200000u},
    {0x00000000u, 0x7fdfffffu},
    {0x00000000u, 0xffdfffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x00180000u},
    },
    {
    {0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u},
    {0x00000001u, 0x80000000u},
    {0x00000000u, 0x00100000u},
    {0x00000000u, 0x80100000u},
    {0x00000000u, 0x001fffffu},
    {0x00000000u, 0x801fffffu},
    {0x00000000u, 0x00200000u},
    {0x00000000u, 0x80200000u},
    {0x00000000u, 0x7fdfffffu},
    {0x00000000u, 0xffdfffffu},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x00180000u},
    },
};

constexpr std::uint32_t FloatModes[ModeCount] = {0x00u, 0x50u, 0xf0u};
constexpr std::uint32_t Omods[OmodCount] = {1u, 2u, 3u};
constexpr const char* OmodNames[OmodCount] = {"mul:2", "mul:4", "div:2"};
constexpr std::uint32_t F16AnyNan = (1u << 15u) | (1u << 16u);

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, bool f64, std::uint32_t floatMode, std::uint32_t omod) {
    Code[5] = f64 ? 0xbf800000u : 0x7e140304u;
    Code[6] = f64 ? 0xd564000au : 0xd532000au;
    Code[7] = (f64 ? 0x00020d04u : 0x00020b04u) | omod << 27u;
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        for (std::uint32_t word = 0; word < (f64 ? 4u : 2u); ++word) {
            Input[tid * Inputs + word] = f64 ? F64Rows[tid][word] : F16Rows[tid][word];
        }
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(bool f64, std::uint32_t mode, std::uint32_t omod) {
    const std::uint32_t variant = mode * OmodCount + omod;
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        for (std::uint32_t word = 0; word < (f64 ? 2u : 1u); ++word) {
            const std::uint32_t actual = Output[tid * Results + word];
            const std::uint32_t expected = f64 ? ExpectedF64[variant][tid][word] : ExpectedF16[variant][tid][word];
            const std::string where = std::string(f64 ? "v_add_f64 " : "v_add_f16 ") + OmodNames[omod] + " FLOAT_MODE " + Hex(FloatModes[mode]) + ": lane " + std::to_string(tid) +
                                      " word " + std::to_string(word) + " is " + Hex(actual);
            if (!f64 && ((F16AnyNan >> tid) & 1u) != 0u) {
                Require((actual & 0xffff0000u) == (expected & 0xffff0000u) && (actual & 0x7fffu) > 0x7c00u, where + ", expected a NaN in the low half and a high half of " + Hex(expected >> 16u));
                continue;
            }
            Require(actual == expected, where + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool f64 : {false, true}) {
            for (std::uint32_t mode = 0; mode < ModeCount; ++mode) {
                for (std::uint32_t omod = 0; omod < OmodCount; ++omod) {
                    Run(*device, f64, FloatModes[mode], Omods[omod]);
                    Check(f64, mode, omod);
                }
            }
        }
        std::puts("f16 and f64 output modifier zero tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
