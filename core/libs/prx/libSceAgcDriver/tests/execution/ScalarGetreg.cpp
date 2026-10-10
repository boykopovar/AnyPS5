#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x34060086, 0xbe9403ff, 0x55555555, 0xbe9503ff, 0x55555555, 0xb9141801, 0xb9150881, 0x7e140214,
    0x7e160215, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileRequest Request(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return request;
}

template<typename TAction>
std::string RequireRefusal(TAction action, const std::string& message) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    Testing::Fail(message);
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint32_t word, const char* what) {
    alignas(256) const std::array<std::uint32_t, 2> code{word, 0xbf810000u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span<const std::uint32_t>(code))}}};
    const std::vector<std::uint32_t> userData(8, 0u);
    const auto refusal = RequireRefusal([&] { static_cast<void>(ShaderRecompiler::Recompile(Request(device, code, userData, memory))); }, std::string("s_getreg_b32 reading ") + what + " was not refused");
    Testing::Require(refusal.find("only the MODE round mode fields are modeled") != std::string::npos, std::string("s_getreg_b32 reading ") + what + " was refused for another reason: " + refusal);
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto result = ShaderRecompiler::Recompile(Request(device, code, userData, memory));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    constexpr std::array<const char*, 2> names{"hwreg(MODE, 0, 4)", "hwreg(MODE, 2, 2)"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < names.size(); ++index) {
            const std::uint32_t actual = Output[tid * Results + index];
            Testing::Require(actual == 0u, std::string("s_getreg_b32: lane ") + std::to_string(tid) + " " + names[index] + " is " + Hex(actual) + ", expected 0x00000000");
        }
    }
}

const Testing::Case readModeRoundFields{"GetregMode_RoundModeFields_ReadZeroInEveryLane", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

const Testing::Case refuseModeDenormalFields{"GetregMode_DenormalFields_AreRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0xb9141901u, "the MODE denormal fields");
}};

const Testing::Case refuseHardwareRegister4{"GetregHardwareRegister4_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0xb914f804u, "hardware register 4");
}};

} // namespace
