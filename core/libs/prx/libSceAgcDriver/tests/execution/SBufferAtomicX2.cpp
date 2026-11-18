#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
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

constexpr std::uint32_t Ops = 6;
alignas(256) std::array<std::uint64_t, Ops> Target{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0xf59d0200u, 0xfa000000u,
    0xf5a10280u, 0xfa000008u,
    0xf5a50300u, 0xfa000010u,
    0xf5a90380u, 0xfa000018u,
    0xf5ad0400u, 0xfa000020u,
    0xf5b10480u, 0xfa000028u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 13> NoReturnCode{
    0xf59c0200u, 0xfa000000u,
    0xf5a00280u, 0xfa000008u,
    0xf5a40300u, 0xfa000010u,
    0xf5a80380u, 0xfa000018u,
    0xf5ac0400u, 0xfa000020u,
    0xf5b00480u, 0xfa000028u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Initialize() {
    Target = {0x0123456789abcdefull, 0xffffffff00000000ull, 0x0123456789abcdefull, 0xa5a5a5a5a5a5a5a5ull, 6ull, 6ull};
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> data(20u, 0u);
    const auto descriptor = BufferDescriptor(Target.data(), static_cast<std::uint32_t>(Target.size() * 8u));
    for (std::uint32_t word = 0u; word < 4u; ++word) data[word] = descriptor[word];
    const std::array<std::uint64_t, Ops> values{
        0xfedcba9876543210ull, 0x0000ffff0000ffffull, 0xf0f0f0f0f0f0f0f0ull,
        0x5a5a5a5a5a5a5a5aull, 10ull, 10ull
    };
    for (std::uint32_t op = 0u; op < Ops; ++op) {
        data[8u + op * 2u] = static_cast<std::uint32_t>(values[op]);
        data[9u + op * 2u] = static_cast<std::uint32_t>(values[op] >> 32u);
    }
    return data;
}

std::array<std::uint64_t, Ops> Expected(const std::array<std::uint64_t, Ops>& initial, const std::array<std::uint64_t, Ops>& data) {
    return {
        initial[0] > data[0] ? initial[0] : data[0],
        initial[1] & data[1],
        initial[2] | data[2],
        initial[3] ^ data[3],
        initial[4] >= data[4] ? 0ull : initial[4] + 1ull,
        initial[5] == 0ull || initial[5] > data[5] ? data[5] : initial[5] - 1ull,
    };
}

void Check(const std::array<std::uint64_t, Ops>& expected, const char* names[]) {
    for (std::uint32_t op = 0u; op < Ops; ++op) {
        Require(Target[op] == expected[op], std::string("s_buffer_atomic_x2: ") + names[op] + " is " + Hex(Target[op]) + ", expected " + Hex(expected[op]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics)) {
            std::puts("skipped, the device has no shaderBufferInt64Atomics");
            return VulkanTestSkipped;
        }
        constexpr const char* names[Ops]{"umax", "and", "or", "xor", "inc", "dec"};
        Initialize();
        const auto data = UserData();
        const auto initial = Target;
        const std::array<std::uint64_t, Ops> values{
            0xfedcba9876543210ull, 0x0000ffff0000ffffull, 0xf0f0f0f0f0f0f0f0ull,
            0x5a5a5a5a5a5a5a5aull, 10ull, 10ull
        };
        const auto expected = Expected(initial, values);
        const auto returning = Compile(*device, Code, data);
        device->Dispatch(returning, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device->WaitIdle();
        Check(expected, names);
        Initialize();
        const auto nonReturning = Compile(*device, NoReturnCode, data);
        device->Dispatch(nonReturning, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(NoReturnCode.data()));
        device->WaitIdle();
        Check(expected, names);
        std::puts("s_buffer_atomic_x2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
