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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 4;
constexpr std::uint32_t Rows = 28;
constexpr std::uint32_t FirstF32Row = 9;
constexpr std::uint32_t FirstF64Row = 18;
constexpr std::uint32_t ModeCount = 18;
constexpr std::uint32_t RuntimeRoundTowardZeroMode = 14;
alignas(256) std::array<std::uint32_t, Threads * Words> Input{};
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

alignas(256) constexpr std::array<std::uint32_t, 20> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd75f000au, 0x041a0b04u, 0xd55f000bu,
    0x041a0b04u, 0x7e3c0307u, 0x7e3e0304u, 0x7e400280u, 0x7e420305u, 0x7e440280u, 0x7e460306u, 0xd560000cu,
    0x048a411eu, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 21> RuntimeCode{
    0xbfa4000fu, 0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd75f000au, 0x041a0b04u,
    0xd55f000bu, 0x041a0b04u, 0x7e3c0307u, 0x7e3e0304u, 0x7e400280u, 0x7e420305u, 0x7e440280u, 0x7e460306u,
    0xd560000cu, 0x048a411eu, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr std::uint32_t Sources[Rows][Words] = {
    {0x3c007c00u, 0x3c003c00u, 0x3c003c00u, 0x00000000u},
    {0x3c00fc00u, 0x3c003c00u, 0x3c00bc00u, 0x00000000u},
    {0x3c007e00u, 0x3c003c00u, 0x3c003c00u, 0x00000000u},
    {0x3c007e00u, 0x3c00bc00u, 0x3c003c00u, 0x00000000u},
    {0x3c007c00u, 0x3c000000u, 0x3c003c00u, 0x00000000u},
    {0x3c007c00u, 0x3c003c00u, 0x3c007c00u, 0x00000000u},
    {0x3c007bffu, 0x3c003c00u, 0x3c003c00u, 0x00000000u},
    {0x3c007c00u, 0x3c007e00u, 0x3c003c00u, 0x00000000u},
    {0x3c007c00u, 0x3c000000u, 0x3c000000u, 0x00000000u},
    {0x7f800000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0xff800000u, 0x3f800000u, 0xbf800000u, 0x00000000u},
    {0x7fc00000u, 0xbf800000u, 0x3f800000u, 0x00000000u},
    {0x7f800000u, 0x00000000u, 0x3f800000u, 0x00000000u},
    {0x3f800000u, 0x71800000u, 0x0d800000u, 0x00000000u},
    {0x3f800000u, 0xf1800000u, 0x0d800000u, 0x00000000u},
    {0x7f800000u, 0x59000000u, 0x0d800000u, 0x00000000u},
    {0x7f800000u, 0x58800000u, 0x0d800000u, 0x00000000u},
    {0x7f7fffffu, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x7ff00000u, 0x3ff00000u, 0x3ff00000u, 0x00000000u},
    {0xfff00000u, 0x3ff00000u, 0xbff00000u, 0x00000000u},
    {0x7ff80000u, 0xbff00000u, 0x3ff00000u, 0x00000000u},
    {0x7ff00000u, 0x00000000u, 0x3ff00000u, 0x00000000u},
    {0x3ff00000u, 0x46300000u, 0x01700000u, 0x00000000u},
    {0x3ff00000u, 0xc6300000u, 0x01700000u, 0x00000000u},
    {0x7ff00000u, 0x44b00000u, 0x01700000u, 0x00000000u},
    {0x7ff00000u, 0x44a00000u, 0x01700000u, 0x00000000u},
    {0x7fefffffu, 0x3ff00000u, 0x3ff00000u, 0xffffffffu},
    {0x7ff40000u, 0x3ff00000u, 0x3ff00000u, 0x00000000u},
};

constexpr ShaderRecompiler::ShaderFloatMode Modes[ModeCount] = {
    {0x00u, true, false, false},
    {0x00u, true, false, true},
    {0xf0u, true, false, false},
    {0xf0u, true, false, true},
    {0x05u, true, false, false},
    {0x05u, true, false, true},
    {0xf5u, true, false, false},
    {0xf5u, true, false, true},
    {0x0au, true, false, false},
    {0x0au, true, false, true},
    {0xfau, true, false, false},
    {0xfau, true, false, true},
    {0x0fu, true, false, false},
    {0x0fu, true, false, true},
    {0xffu, true, false, false},
    {0xffu, true, false, true},
    {0xd5u, true, false, false},
    {0xe5u, true, false, false},
};

constexpr std::uint64_t Expected[ModeCount][Rows] = {
    {
        0x7c00u, 0xfc00u, 0x7c00u, 0xfc00u, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7c00u, 0xfc00u, 0x7c00u, 0xfc00u, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7c00u, 0xfbffu, 0x7c00u, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7c00u, 0xfbffu, 0x7c00u, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x1u,
        0x80000000u, 0x1u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x1u, 0x8000000000000000ull, 0x1u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x1u,
        0x80000000u, 0x1u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x1u, 0x8000000000000000ull, 0x1u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7bffu, 0xfc00u, 0x7bffu, 0xfc00u, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfc00u, 0x7bffu, 0xfc00u, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000001u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000001ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff800000u, 0xff800000u, 0x7f800000u, 0x0u,
        0x80000001u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000001ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7bffu, 0xfbffu, 0x7bffu, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f7fffffu, 0x7f7fffffu, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x0u, 0x8000000000000000ull, 0x0u, 0x7fefffffffffffffull, 0x7fefffffffffffffull, 0x7fefffffffffffffull,
    },
    {
        0x7c00u, 0xfbffu, 0x7c00u, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x0u,
        0x80000000u, 0x0u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x1u, 0x8000000000000000ull, 0x1u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
    {
        0x7c00u, 0xfbffu, 0x7c00u, 0xfbffu, 0x7c00u, 0x7c00u, 0x7bffu,
        0x7e00u, 0xfe00u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x1u,
        0x80000000u, 0x1u, 0x7f800000u, 0x7f7fffffu, 0x7ff0000000000000ull, 0xffefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0x1u, 0x8000000000000000ull, 0x1u, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull,
    },
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const ShaderRecompiler::ShaderFloatMode& mode) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::string& name, std::uint32_t mode) {
    for (std::uint32_t row = 0; row < Rows; ++row) {
        const std::uint32_t* out = &Output[row * Words];
        const std::uint64_t actual = row < FirstF32Row ? out[0] : row < FirstF64Row ? out[1] : (static_cast<std::uint64_t>(out[3]) << 32u) | out[2];
        const char* format = row < FirstF32Row ? "v_div_fixup_f16" : row < FirstF64Row ? "v_div_fixup_f32" : "v_div_fixup_f64";
        Require(actual == Expected[mode][row], std::string(format) + " " + name + " lane " + std::to_string(row) + " is " + Hex(actual) + ", expected " + Hex(Expected[mode][row]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t row = 0; row < Rows; ++row) std::copy(std::begin(Sources[row]), std::end(Sources[row]), &Input[row * Words]);
        for (std::uint32_t mode = 0; mode < ModeCount; ++mode) {
            Run(*device, Code, Modes[mode]);
            Check("FLOAT_MODE " + Hex(Modes[mode].floatMode) + " FP16_OVFL " + std::to_string(Modes[mode].fp16Overflow), mode);
        }
        Run(*device, RuntimeCode, ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false});
        Check("after s_round_mode 0xf", RuntimeRoundTowardZeroMode);
        std::puts("v_div_fixup directed rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
