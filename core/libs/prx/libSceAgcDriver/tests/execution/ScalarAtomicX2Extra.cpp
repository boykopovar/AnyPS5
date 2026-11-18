#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;

struct Operation {
    std::uint32_t opcode;
    const char* name;
    std::uint64_t (*reference)(std::uint64_t, std::uint64_t);
};

std::uint64_t Umax(std::uint64_t old, std::uint64_t data) { return old > data ? old : data; }
std::uint64_t And(std::uint64_t old, std::uint64_t data) { return old & data; }
std::uint64_t Or(std::uint64_t old, std::uint64_t data) { return old | data; }
std::uint64_t Xor(std::uint64_t old, std::uint64_t data) { return old ^ data; }
std::uint64_t Inc(std::uint64_t old, std::uint64_t data) { return old >= data ? 0u : old + 1u; }
std::uint64_t Dec(std::uint64_t old, std::uint64_t data) { return old == 0u || old > data ? data : old - 1u; }

constexpr auto Operations = std::to_array<Operation>({
    {0xa7u, "s_atomic_umax_x2", Umax},
    {0xa8u, "s_atomic_and_x2", And},
    {0xa9u, "s_atomic_or_x2", Or},
    {0xaau, "s_atomic_xor_x2", Xor},
    {0xabu, "s_atomic_inc_x2", Inc},
    {0xacu, "s_atomic_dec_x2", Dec},
});

constexpr std::uint32_t SmemAtomicWord0(std::uint32_t opcode, bool returnsValue) {
    return (0x3du << 26u) | (opcode << 18u) | (returnsValue ? (1u << 16u) : 0u) | (2u << 6u);
}

void Compile(AgcDriver::VulkanDevice& device, std::uint32_t opcode, bool returnsValue) {
    const std::array<std::uint32_t, 3> code{SmemAtomicWord0(opcode, returnsValue), 4u << 25u, 0xbf810000u};
    std::vector<std::uint32_t> userData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(!result.spirv.Words().empty(), "scalar atomic x2 produced no SPIR-V");
}

void CheckReferences() {
    constexpr std::array<std::uint64_t, 8> values{0ull, 1ull, 7ull, 8ull, 0xffffffffffffffffull, 0x8000000000000000ull, 0xaaaaaaaa55555555ull, 0x123456789abcdef0ull};
    for (std::uint64_t old : values) {
        for (std::uint64_t data : values) {
            Require(Umax(old, data) == (old > data ? old : data), "umax reference");
            Require(And(old, data) == (old & data), "and reference");
            Require(Or(old, data) == (old | data), "or reference");
            Require(Xor(old, data) == (old ^ data), "xor reference");
            Require(Inc(old, data) == (old >= data ? 0u : old + 1u), "inc reference");
            Require(Dec(old, data) == (old == 0u || old > data ? data : old - 1u), "dec reference");
        }
    }
}

void Run(AgcDriver::VulkanDevice& device) {
    for (const Operation& operation : Operations) {
        for (bool returnsValue : {false, true}) {
            try {
                Compile(device, operation.opcode, returnsValue);
            } catch (const std::exception& error) {
                Require(false, std::string("scalar atomic x2 ") + operation.name + (returnsValue ? " rtn" : "") + ": " + error.what());
            }
        }
    }
    CheckReferences();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        std::puts("scalar atomic x2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
