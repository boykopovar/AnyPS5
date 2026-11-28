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
constexpr std::uint32_t RowCount = 14;
constexpr std::uint32_t OpCount = 2;
constexpr std::uint32_t ModeCount = 5;
constexpr std::uint32_t OmodCount = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xbf800000u, 0xd564000au, 0x00020d04u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

constexpr std::uint32_t Rows[OpCount][RowCount][4] = {
    {
    {0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7fe00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffe00000u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0x7fdfffffu, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffdfffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7fd00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffd00000u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0x7fcfffffu, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffcfffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0x00000000u},
    },
    {
    {0xffffffffu, 0x7fefffffu, 0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu, 0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u, 0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u, 0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu, 0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu, 0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x7fd00000u, 0x00000000u, 0x7fd00000u},
    {0x00000000u, 0xffd00000u, 0x00000000u, 0xffd00000u},
    {0xffffffffu, 0x7fcfffffu, 0xffffffffu, 0x7fcfffffu},
    {0xffffffffu, 0xffcfffffu, 0xffffffffu, 0xffcfffffu},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0xbff00000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u},
    },
};

constexpr std::uint32_t Expected[OpCount][ModeCount][OmodCount][RowCount][2] = {
    {
    {
    {
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x7fd00000u},
    {0x00000000u, 0xffd00000u},
    {0xffffffffu, 0x7fcfffffu},
    {0xffffffffu, 0xffcfffffu},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x7fd00000u},
    {0x00000000u, 0xffd00000u},
    {0xffffffffu, 0x7fcfffffu},
    {0xffffffffu, 0xffcfffffu},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    },
    {
    {
    {
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7ff00000u},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x40000000u},
    {0x00000000u, 0xc0000000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x40100000u},
    {0x00000000u, 0xc0100000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    {
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x7fd00000u},
    {0x00000000u, 0xffd00000u},
    {0xffffffffu, 0x7fcfffffu},
    {0xffffffffu, 0xffcfffffu},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    {
    {0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x7fe00000u},
    {0x00000000u, 0xffe00000u},
    {0xffffffffu, 0x7fdfffffu},
    {0xffffffffu, 0xffdfffffu},
    {0x00000000u, 0x7fd00000u},
    {0x00000000u, 0xffd00000u},
    {0xffffffffu, 0x7fcfffffu},
    {0xffffffffu, 0xffcfffffu},
    {0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u},
    {0x00000000u, 0x7ff00000u},
    {0x00000000u, 0xfff00000u},
    },
    },
    },
};

constexpr std::uint32_t Opcodes[OpCount] = {0xd564000au, 0xd567000au};
constexpr const char* OpNames[OpCount] = {"v_add_f64", "v_max_f64"};
constexpr std::uint32_t FloatModes[ModeCount] = {0x30u, 0x34u, 0x38u, 0x3cu, 0xf8u};
constexpr std::uint32_t Omods[OmodCount] = {1u, 2u};
constexpr const char* OmodNames[OmodCount] = {"mul:2", "mul:4"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t op, std::uint32_t floatMode, std::uint32_t omod, std::optional<std::uint32_t> runtimeRounding) {
    Code[5] = runtimeRounding.has_value() ? 0xbfa40000u | *runtimeRounding << 2u : 0xbf800000u;
    Code[6] = Opcodes[op];
    Code[7] = 0x00020d04u | omod << 27u;
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        for (std::uint32_t word = 0; word < 4u; ++word) Input[tid * Inputs + word] = Rows[op][tid][word];
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

void Check(std::uint32_t op, std::uint32_t mode, std::uint32_t omod, const char* how) {
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        for (std::uint32_t word = 0; word < 2u; ++word) {
            const std::uint32_t actual = Output[tid * Results + word];
            const std::uint32_t expected = Expected[op][mode][omod][tid][word];
            Require(actual == expected, std::string(OpNames[op]) + " " + OmodNames[omod] + " FLOAT_MODE " + Hex(FloatModes[mode]) + " " + how + ": lane " + std::to_string(tid) + " word " +
                                            std::to_string(word) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t op = 0; op < OpCount; ++op) {
            for (std::uint32_t mode = 0; mode < ModeCount; ++mode) {
                for (std::uint32_t omod = 0; omod < OmodCount; ++omod) {
                    Run(*device, op, FloatModes[mode], Omods[omod], std::nullopt);
                    Check(op, mode, omod, "declared");
                }
            }
        }
        for (std::uint32_t mode = 1; mode < 4u; ++mode) {
            for (std::uint32_t omod = 0; omod < OmodCount; ++omod) {
                Run(*device, 1u, FloatModes[0], Omods[omod], mode);
                Check(1u, mode, omod, "set by s_round_mode");
            }
        }
        std::puts("f64 output modifier rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
