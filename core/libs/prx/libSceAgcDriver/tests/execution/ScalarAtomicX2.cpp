#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 1;

struct Operation {
    std::uint32_t encoding;
    const char* name;
    bool compare;
};

constexpr auto Operations = std::to_array<Operation>({
    {0xa0u, "swap", false},
    {0xa1u, "cmpswap", true},
    {0xa2u, "add", false},
    {0xa3u, "sub", false},
    {0xa4u, "smin", false},
    {0xa5u, "umin", false},
    {0xa6u, "smax", false},
});

struct Row {
    std::uint64_t old;
    std::uint64_t data;
    std::uint64_t comparator;
};

constexpr auto Rows = std::to_array<Row>({
    {0x0123456789abcdefull, 0xfedcba9876543210ull, 0x0123456789abcdefull},
    {0x8000000000000000ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {0xffffffffffffffffull, 1ull, 0ull},
    {7ull, 0x123456789abcdef0ull, 7ull},
    {0x0000000100000000ull, 0x0000000100000000ull, 0x0000000100000000ull},
    {0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull},
    {0xdeadbeefdeadbeefull, 0x1234567887654321ull, 0xdeadbeefdeadbeefull},
});

alignas(8) std::uint64_t Target{};
alignas(256) std::array<std::uint64_t, Operations.size()> Output{};

constexpr std::uint32_t SmovB64(std::uint32_t dst, std::uint32_t src) {
    return 0x80000000u | (0x7du << 23u) | (dst << 16u) | (0x04u << 8u) | src;
}

constexpr std::uint32_t VmovB32(std::uint32_t dst, std::uint32_t src) {
    return (0x7eu << 24u) | (dst << 17u) | (0x01u << 9u) | src;
}

constexpr std::uint32_t StoreWord0() {
    return (0x38u << 26u) | (0x1du << 18u);
}

constexpr std::uint32_t StoreWord1() {
    return (2u << 16u) | 2u;
}

constexpr std::uint32_t EndPgm() {
    return 0x80000000u | (0x7fu << 23u) | (1u << 16u);
}

std::vector<std::uint32_t> Code(const Operation& op) {
    const std::uint32_t atomicWord0 = (0x3du << 26u) | (op.encoding << 18u) | (1u << 16u) | (12u << 6u);
    const std::uint32_t atomicWord1 = 2u << 25u;
    std::vector<std::uint32_t> words{SmovB64(12u, 4u)};
    if (op.compare) words.push_back(SmovB64(14u, 6u));
    words.insert(words.end(), {atomicWord0, atomicWord1, VmovB32(0u, 12u), VmovB32(1u, 13u), VmovB32(2u, 128u), StoreWord0(), StoreWord1(), EndPgm()});
    return words;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint64_t Reference(const Operation& op, const Row& row) {
    switch (op.encoding) {
    case 0xa0u: return row.data;
    case 0xa1u: return row.old == row.comparator ? row.data : row.old;
    case 0xa2u: return row.old + row.data;
    case 0xa3u: return row.old - row.data;
    case 0xa4u: return static_cast<std::uint64_t>(std::min(static_cast<std::int64_t>(row.old), static_cast<std::int64_t>(row.data)));
    case 0xa5u: return std::min(row.old, row.data);
    case 0xa6u: return static_cast<std::uint64_t>(std::max(static_cast<std::int64_t>(row.old), static_cast<std::int64_t>(row.data)));
    default: throw std::runtime_error("unknown scalar atomic");
    }
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::size_t index, std::span<const std::uint32_t> code) {
    const auto target = reinterpret_cast<std::uintptr_t>(&Target);
    const auto output = BufferDescriptor(&Output[index], 8u);
    std::vector<std::uint32_t> userData(12u, 0u);
    userData[0] = static_cast<std::uint32_t>(target);
    userData[1] = static_cast<std::uint32_t>(target >> 32u);
    userData[4] = static_cast<std::uint32_t>(Rows[index].data);
    userData[5] = static_cast<std::uint32_t>(Rows[index].data >> 32u);
    userData[6] = static_cast<std::uint32_t>(Rows[index].comparator);
    userData[7] = static_cast<std::uint32_t>(Rows[index].comparator >> 32u);
    std::copy(output.begin(), output.end(), userData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

int Run() {
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    if (!TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics)) {
        std::puts("skipped, no shaderBufferInt64Atomics");
        return VulkanTestSkipped;
    }
    for (std::size_t index = 0; index < Operations.size(); ++index) {
        const auto& op = Operations[index];
        const auto code = Code(op);
        const auto result = Compile(*device, index, code);
        Target = Rows[index].old;
        Output[index] = 0xdeadbeefdeadbeefull;
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        const std::uint64_t expectedNew = Reference(op, Rows[index]);
        Require(Output[index] == Rows[index].old, std::string("s_atomic_") + op.name + "_x2 returned " + Hex(Output[index]) + ", expected old value " + Hex(Rows[index].old));
        Require(Target == expectedNew, std::string("s_atomic_") + op.name + "_x2 stored " + Hex(Target) + ", expected " + Hex(expectedNew));
    }
    std::puts("scalar atomic x2 tests passed");
    return 0;
}

}

int main() {
    try {
        return Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
