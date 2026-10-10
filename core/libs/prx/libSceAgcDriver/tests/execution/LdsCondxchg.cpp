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
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Words = 8;
constexpr std::uint32_t LdsDwords = 4096;
constexpr std::uint32_t Sign = 0x80000000u;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, MaxThreads * Words> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Words> Output{};

auto Code(std::uint32_t waveSize, std::uint32_t offset, std::uint32_t destination, bool inactive, bool contended) {
    const std::uint32_t saveExec = waveSize == 32u ? 0xbe943c6au : 0xbe94246au;
    const std::uint32_t restoreExec = waveSize == 32u ? 0xbefe03c1u : 0xbefe04c1u;
    return std::to_array<std::uint32_t>({
        0x34020085u, 0xe0381000u, 0x80000201u, 0xe0381010u, 0x80000601u, 0xbf8c3f70u,
        0x7e160304u, 0x7e180305u, 0x7e1a02ffu, Sentinel, 0x7e140280u,
        contended ? 0x7d840080u : 0x7d841480u, saveExec,
        0xd9340000u, 0x00000206u, 0xd9340008u, 0x00000806u, 0xd9340010u, 0x00000806u,
        0xbf8cc07fu, restoreExec, 0xbf8a0000u,
        inactive ? 0x36140081u : 0x36140080u, 0x7d841480u, saveExec,
        0xd9f80000u | offset, (destination << 24u) | 0x00000b07u,
        0xbf8cc07fu, restoreExec, 0xbf8a0000u,
        0x7e280300u | destination, 0x7e2a0300u | (destination + 1u),
        0xd9d80000u, 0x16000006u, 0xd9d80008u, 0x18000006u, 0xd9d80010u, 0x1a000006u,
        0xbf8cc07fu, 0xe0781000u, 0x80011401u, 0xe0781010u, 0x80011801u, 0xbf810000u,
    });
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

template<typename TUse>
void Translate(std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, TUse&& use) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    use(ShaderRecompiler::Recompile(request));
}

void Expect(std::uint32_t tid, std::uint32_t index, std::uint32_t expected) {
    const auto actual = Output[tid * Words + index];
    Require(actual == expected, "lds condxchg: lane " + std::to_string(tid) + " word " + std::to_string(index) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
}

template<typename TCheck>
void ExpectError(TCheck&& check, const std::string& reason) {
    std::string error;
    try {
        check();
    } catch (const std::exception& exception) {
        error = exception.what();
    }
    Require(error.find(reason) != std::string::npos, "lds condxchg: expected \"" + reason + "\", got \"" + error + "\"");
}

void CheckRejections(AgcDriver::VulkanDevice& device) {
    const auto translate = [&](std::span<const std::uint32_t> code) { Translate(code, 32u, device.Target(), [](const auto&) {}); };
    constexpr std::array<std::uint32_t, 3> boundary{0xd9f80000u, 0xfe00fe00u, 0xbf810000u};
    translate(boundary);
    ExpectError([&] { translate(std::span(boundary).first(1u)); }, "truncated DS instruction");
    constexpr std::array<std::uint32_t, 3> sourceOverflow{0xd9f80000u, 0x0200ff00u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 3> destinationOverflow{0xd9f80000u, 0xff000400u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 3> gds{0xd9fa0000u, 0x02000400u, 0xbf810000u};
    ExpectError([&] { translate(sourceOverflow); }, "DS conditional exchange source register range overflow");
    ExpectError([&] { translate(destinationOverflow); }, "DS conditional exchange destination register range overflow");
    ExpectError([&] { translate(gds); }, "DS conditional exchange GDS mode is not supported");
}

void Fill(std::uint32_t threads, std::uint32_t offset, bool contended) {
    for (std::uint32_t tid = 0u; tid < threads; ++tid) {
        auto* in = &Input[tid * Words];
        const auto seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
        in[0] = contended ? 0xffffffffu : static_cast<std::uint32_t>(seed);
        in[1] = contended ? 0xfffffffeu : static_cast<std::uint32_t>(seed >> 32u);
        in[2] = contended ? Sign | (tid + 1u) : ((tid & 1u) != 0u ? Sign : 0u) | ((tid / 4u) % 2u != 0u ? 0x7fffffffu : 0u);
        in[3] = contended ? Sign | (tid + 1u) : ((tid & 2u) != 0u ? Sign : 0u) | ((tid / 8u) % 2u != 0u ? 0x13579bdfu : 0u);
        const bool outOfRange = !contended && ((tid & 8u) == 0u || offset >= LdsDwords * 4u);
        in[4] = contended ? 0u : outOfRange ? tid * 32u : LdsDwords * 4u - 24u - tid * 32u;
        in[5] = in[4] + (outOfRange ? 0x10000u : 0u) - offset;
        in[6] = 0xa5a5a5a5u;
        in[7] = 0x5a5a5a5au;
    }
}

void CheckContention(std::uint32_t threads) {
    for (std::uint32_t word = 0u; word < 2u; ++word) {
        const auto finalValue = Output[2u + word];
        Require(finalValue >= 1u && finalValue <= threads, "lds condxchg: invalid final contended dword");
        std::array<bool, MaxThreads + 1u> seen{};
        for (std::uint32_t tid = 0u; tid < threads; ++tid) {
            Expect(tid, 2u + word, finalValue);
            const auto old = Output[tid * Words + word];
            const auto index = old == Input[word] ? 0u : old;
            Require(index <= threads && index != tid + 1u && !seen[index], "lds condxchg: invalid or duplicate returned contended dword");
            seen[index] = true;
        }
        Require(seen[0] && !seen[finalValue], "lds condxchg: contended dword returns lost an exchange");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t offset, std::uint32_t destination, bool inactive, bool contended) {
    Fill(waveSize, offset, contended);
    Output.fill(Sentinel);
    alignas(256) const auto code = Code(waveSize, offset, destination, inactive, contended);
    Translate(code, waveSize, target, [&](const auto& result) {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
    });
    for (std::uint32_t tid = 0u; tid < waveSize; ++tid) {
        const auto* in = &Input[tid * Words];
        if (!contended) {
            const bool active = !inactive || tid % 2u == 0u;
            const bool inBounds = in[5] + offset < LdsDwords * 4u;
            Expect(tid, 0u, active ? inBounds ? in[0] : 0u : destination == 12u ? in[3] : in[5]);
            Expect(tid, 1u, active ? inBounds ? in[1] : 0u : destination == 12u ? Sentinel : in[6]);
            Expect(tid, 2u, active && inBounds && (in[2] & Sign) != 0u ? in[2] & ~Sign : in[0]);
            Expect(tid, 3u, active && inBounds && (in[3] & Sign) != 0u ? in[3] & ~Sign : in[1]);
        }
        for (std::uint32_t index = 4u; index < Words; ++index) Expect(tid, index, in[6u + index % 2u]);
    }
    if (contended) CheckContention(waveSize);
}

void CheckExecution(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    for (const auto offset : {0u, 0x120u, 0xfff8u}) Run(device, waveSize, target, offset, 12u, false, false);
    Run(device, waveSize, target, 0x120u, 12u, true, false);
    Run(device, waveSize, target, 0xfff8u, 7u, true, false);
    Run(device, waveSize, target, 0x120u, 12u, false, true);
}


const Testing::Case rejections{"LdsCondxchg_MalformedInstructions_AreRejected", [] {
    auto& device = SharedVulkanTestDevice();
    CheckRejections(device);
}};

const Testing::Case wave32{"LdsCondxchg_Wave32_ExchangesConditionally", [] {
    auto& device = SharedVulkanTestDevice();
    CheckExecution(device, 32u, device.Target());
}};

const Testing::Case wave64{"LdsCondxchg_Wave64_ExchangesConditionally", [] {
    auto& device = SharedVulkanTestDevice();
    CheckExecution(device, 64u, device.Target());
}};

const Testing::Case wave64Split{"LdsCondxchg_Wave64OnThirtyTwoLaneSubgroups_ExchangesConditionally", [] {
    auto& device = SharedVulkanTestDevice();
    CheckExecution(device, 64u, device.ComputeTarget(32u));
}};

} // namespace
