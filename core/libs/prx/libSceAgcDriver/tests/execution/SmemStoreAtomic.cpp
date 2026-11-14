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
constexpr std::uint32_t Words = 64;
alignas(256) std::array<std::uint32_t, Words> Output{};

constexpr std::uint32_t NullOffset = 0x7du;

constexpr std::array<std::uint32_t, 2> Smem(std::uint32_t opcode, std::uint32_t data, std::uint32_t base, std::uint32_t offset, bool glc = false) {
    return {0xf4000000u | (opcode << 18u) | (glc ? 1u << 16u : 0u) | (data << 6u) | (base >> 1u), offset | (NullOffset << 25u)};
}

constexpr std::array<std::uint32_t, 2> Move(std::uint32_t sgpr, std::uint32_t value) {
    return {0xbe8003ffu | (sgpr << 16u), value};
}

constexpr std::array<std::array<std::uint32_t, 2>, 23> CommonWords{{
    Move(8u, 0x11111111u), Move(9u, 0x22222222u), Move(10u, 0x33333333u), Move(11u, 0x44444444u),
    Smem(0x10u, 8u, 4u, 0x00u), Smem(0x11u, 8u, 4u, 0x08u), Smem(0x12u, 8u, 4u, 0x10u),
    Smem(0x18u, 9u, 0u, 0x20u), Smem(0x19u, 10u, 0u, 0x28u), Smem(0x1au, 8u, 0u, 0x30u),
    Move(14u, 5u), Smem(0x82u, 14u, 4u, 0x40u, true), Smem(0x10u, 14u, 4u, 0x44u),
    Move(15u, 7u), Smem(0x82u, 15u, 4u, 0x48u), Smem(0x10u, 15u, 4u, 0x4cu),
    Move(16u, 0xaaaau), Move(17u, 0x1234u), Smem(0x81u, 16u, 4u, 0x50u, true), Smem(0x10u, 16u, 4u, 0x54u),
    Move(20u, 3u), Smem(0x42u, 20u, 0u, 0x68u, true), Smem(0x10u, 20u, 4u, 0x6cu),
}};

constexpr std::array<std::array<std::uint32_t, 2>, 7> Int64Words{{
    Move(18u, 1u), Move(19u, 0u), Smem(0xa2u, 18u, 4u, 0x58u, true), Smem(0x11u, 18u, 4u, 0x60u),
    Move(22u, 9u), Move(23u, 0u), Smem(0x67u, 22u, 0u, 0x70u),
}};

constexpr std::array<std::array<std::uint32_t, 2>, 3> EndWords{{
    {0xf4840000u, 0x00000000u}, {0xbf810000u, 0xbf800000u}, {0xbf800000u, 0xbf800000u},
}};

template<bool WithInt64> constexpr auto BuildCode() {
    std::array<std::uint32_t, (CommonWords.size() + (WithInt64 ? Int64Words.size() : 0u) + EndWords.size()) * 2u> code{};
    std::size_t next = 0;
    const auto append = [&](const auto& words) {
        for (const auto& word : words) {
            code[next++] = word[0];
            code[next++] = word[1];
        }
    };
    append(CommonWords);
    if constexpr (WithInt64) append(Int64Words);
    append(EndWords);
    return code;
}

alignas(256) constexpr auto Code = BuildCode<true>();
alignas(256) constexpr auto CodeWithoutInt64 = BuildCode<false>();

alignas(256) constexpr std::array<std::uint32_t, 4> ScratchStoreCode{0xf4540200u, 0xfa000000u, 0xbf810000u, 0xbf800000u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    userData[4] = static_cast<std::uint32_t>(address);
    userData[5] = static_cast<std::uint32_t>(address >> 32u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    Output[0x40 / 4] = 100u;
    Output[0x48 / 4] = 1000u;
    Output[0x50 / 4] = 0x1234u;
    Output[0x58 / 4] = 0xffffffffu;
    Output[0x5c / 4] = 1u;
    Output[0x68 / 4] = 50u;
    Output[0x70 / 4] = 5u;
    Output[0x74 / 4] = 0u;
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

bool Int64Row(std::uint32_t offset) {
    return (offset >= 0x58u && offset < 0x68u) || offset >= 0x70u;
}

void Check(bool int64Atomics) {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 30> expected{{
        {0x00, 0x11111111u}, {0x04, 0xdeadbeefu}, {0x08, 0x11111111u}, {0x0c, 0x22222222u},
        {0x10, 0x11111111u}, {0x14, 0x22222222u}, {0x18, 0x33333333u}, {0x1c, 0x44444444u},
        {0x20, 0x22222222u}, {0x24, 0xdeadbeefu}, {0x28, 0x33333333u}, {0x2c, 0x44444444u},
        {0x30, 0x11111111u}, {0x34, 0x22222222u}, {0x38, 0x33333333u}, {0x3c, 0x44444444u},
        {0x40, 105u}, {0x44, 100u}, {0x48, 1007u}, {0x4c, 7u},
        {0x50, 0xaaaau}, {0x54, 0x1234u},
        {0x58, 0u}, {0x5c, 2u}, {0x60, 0xffffffffu}, {0x64, 1u},
        {0x68, 53u}, {0x6c, 50u}, {0x70, 9u}, {0x74, 0u},
    }};
    for (const auto& [offset, value] : expected) {
        if (!int64Atomics && Int64Row(offset)) continue;
        const std::uint32_t actual = Output[offset / 4u];
        Require(actual == value, "smem store/atomic: byte " + Hex(offset) + " is " + Hex(actual) + ", expected " + Hex(value));
    }
}

void CheckScratchStoreRejected(AgcDriver::VulkanDevice& device) {
    try {
        (void)Compile(device, ScratchStoreCode);
    } catch (const std::exception&) {
        return;
    }
    Require(false, "smem store/atomic: s_scratch_store_dword was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool int64Atomics = TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics);
        Run(*device, int64Atomics ? std::span<const std::uint32_t>(Code) : std::span<const std::uint32_t>(CodeWithoutInt64));
        Check(int64Atomics);
        if (!int64Atomics) std::puts("skipped the 64-bit atomic rows, the device has no shaderBufferInt64Atomics");
        CheckScratchStoreRejected(*device);
        std::puts("smem store/atomic tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
