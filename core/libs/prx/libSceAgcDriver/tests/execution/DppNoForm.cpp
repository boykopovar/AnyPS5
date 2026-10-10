#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

struct NoFormOpcode {
    bool vop2;
    std::uint32_t opcode;
    const char* name;
};

constexpr std::array<NoFormOpcode, 23> NoFormOpcodes{{
    {false, 0x02u, "v_readfirstlane_b32"}, {false, 0x03u, "v_cvt_i32_f64"}, {false, 0x04u, "v_cvt_f64_i32"},
    {false, 0x0fu, "v_cvt_f32_f64"}, {false, 0x10u, "v_cvt_f64_f32"}, {false, 0x15u, "v_cvt_u32_f64"},
    {false, 0x16u, "v_cvt_f64_u32"}, {false, 0x17u, "v_trunc_f64"}, {false, 0x18u, "v_ceil_f64"},
    {false, 0x19u, "v_rndne_f64"}, {false, 0x1au, "v_floor_f64"}, {false, 0x3cu, "v_frexp_exp_i32_f64"},
    {false, 0x3du, "v_frexp_mant_f64"}, {false, 0x3eu, "v_fract_f64"}, {false, 0x65u, "v_swap_b32"},
    {false, 0x68u, "v_swaprel_b32"}, {true, 0x06u, "v_mac_legacy_f32"}, {true, 0x20u, "v_madmk_f32"},
    {true, 0x21u, "v_madak_f32"}, {true, 0x2cu, "v_fmamk_f32"}, {true, 0x2du, "v_fmaak_f32"},
    {true, 0x37u, "v_fmamk_f16"}, {true, 0x38u, "v_fmaak_f16"},
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void CheckRefused(AgcDriver::VulkanDevice& device, std::uint32_t word0, std::uint32_t word1, const std::string& reason, const std::string& what) {
    alignas(256) const std::array<std::uint32_t, 4> code{word0, word1, 0x3c003c00u, 0xbf810000u};
    const auto error = Testing::RequireThrows<std::runtime_error>([&] { static_cast<void>(Compile(device, code)); }, what + " " + Hex(word0) + " was not refused");
    Require(std::string(error.what()).find(reason) != std::string::npos, what + " " + Hex(word0) + " was not refused");
}

void CheckAccepted(AgcDriver::VulkanDevice& device, std::uint32_t word0, std::uint32_t word1) {
    alignas(256) const std::array<std::uint32_t, 3> code{word0, word1, 0xbf810000u};
    static_cast<void>(Compile(device, code));
}

constexpr std::uint32_t Dpp16 = 0xff00b104u;
constexpr std::uint32_t Dpp8 = 0x00fac604u;

std::uint32_t Encode(const NoFormOpcode& op, std::uint32_t form) {
    return op.vop2 ? (op.opcode << 25u) | (10u << 17u) | (5u << 9u) | form : 0x7e000000u | (10u << 17u) | (op.opcode << 9u) | form;
}

const Testing::Case dpp16Refused{"DppNoForm_Dpp16OnOpcodeWithoutDppForm_IsRefused", [] {
    auto& device = SharedVulkanTestDevice();
    for (const auto& op : NoFormOpcodes) {
        CheckRefused(device, Encode(op, 0xfau), Dpp16, "DPP modifier is not supported for opcode", std::string(op.name) + " with DPP16");
    }
}};

const Testing::Case dpp8Refused{"DppNoForm_Dpp8OnOpcodeWithoutDppForm_IsRefused", [] {
    auto& device = SharedVulkanTestDevice();
    for (const auto& op : NoFormOpcodes) {
        const std::string name = op.name;
        CheckRefused(device, Encode(op, 0xe9u), Dpp8, "DPP8 modifier is not supported for opcode", name + " with DPP8");
        CheckRefused(device, Encode(op, 0xeau), Dpp8, "DPP8 modifier is not supported for opcode", name + " with DPP8 fi:1");
    }
}};

const Testing::Case dpp16Accepted{"DppNoForm_Dpp16OnMovAndAdd_IsAccepted", [] {
    auto& device = SharedVulkanTestDevice();
    CheckAccepted(device, 0x7e1402fau, Dpp16);
    CheckAccepted(device, 0x4a140afau, Dpp16);
}};

} // namespace
