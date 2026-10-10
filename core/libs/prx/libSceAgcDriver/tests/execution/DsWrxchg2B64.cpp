#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t LdsDwords = 3072;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr auto Code = std::to_array<std::uint32_t>({
    0x34020085, 0xe0381000, 0x80000201, 0xe0381010, 0x80000601,
    0x345c0085, 0x4a5c5cff, 0x00000480, 0x345e0083, 0x4a5e5eff, 0x00000880,
    0xbf8c3f70,
    0xd9380301, 0x0004022e, 0xd9b80301, 0x1208062e, 0xd9dc0301, 0x1600002e,
    0xd93c0100, 0x0004022f, 0xd9bc0100, 0x1a08062f, 0xd9e00100, 0x1e00002f,
    0x34020086,
    0xbf8cc07f,
    0xe0781000, 0x80011201, 0xe0781010, 0x80011601, 0xe0781020, 0x80011a01, 0xe0781030, 0x80011e01,
    0xbf810000,
});

alignas(256) constexpr auto CoincidingOffsets = std::to_array<std::uint32_t>({0xd9b80101, 0x1208062e, 0xbf810000});
alignas(256) constexpr auto CoincidingOffsetsSt64 = std::to_array<std::uint32_t>({0xd9bc0000, 0x1a08062f, 0xbf810000});
alignas(256) constexpr auto DestinationOverflow = std::to_array<std::uint32_t>({0xd9b80301, 0xfd08062e, 0xbf810000});

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    for (std::uint32_t i = 0; i < Inputs; ++i) {
        seed = seed * 0xbf58476d1ce4e5b9ull + 0x94d049bb133111ebull;
        words[i] = static_cast<std::uint32_t>(seed >> 32u);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

template<typename TUse>
void Translate(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, TUse&& use) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    use(ShaderRecompiler::Recompile(request));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    Translate(device, Code, [&](const auto& result) {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device.WaitIdle();
    });
}

void Reject(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    const auto rejection = Testing::RequireThrows<std::runtime_error>([&] { Translate(device, code, [](const auto&) {}); }, "ds wrxchg2 b64: expected a rejection for \"" + reason + "\"");
    const std::string error = rejection.what();
    Require(error.find(reason) != std::string::npos, "ds wrxchg2 b64: expected a rejection for \"" + reason + "\", got \"" + error + "\"");
}

void Check() {
    constexpr std::array<const char*, 2> names{"ds_wrxchg2_rtn_b64", "ds_wrxchg2st64_rtn_b64"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t op = 0; op < 2u; ++op) {
            for (std::uint32_t i = 0; i < 4u; ++i) {
                const auto returned = out[op * 8u + i];
                const auto stored = out[op * 8u + 4u + i];
                Require(returned == in[i], std::string("ds wrxchg2 b64: lane ") + std::to_string(tid) + " " + names[op] + " returned dword " + std::to_string(i) + " is " + Hex(returned) + ", expected " + Hex(in[i]));
                Require(stored == in[4u + i], std::string("ds wrxchg2 b64: lane ") + std::to_string(tid) + " " + names[op] + " stored dword " + std::to_string(i) + " is " + Hex(stored) + ", expected " + Hex(in[4u + i]));
            }
        }
    }
}

const Testing::Case exchange{"DsWrxchg2B64_ExchangeForms_ReturnOldValuesAndStoreNewOnes", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

const Testing::Case coincidingOffsets{"DsWrxchg2B64_CoincidingOffsets_AreRejected", [] {
    auto& device = SharedVulkanTestDevice();
    Reject(device, CoincidingOffsets, "DS write exchange of one location through both offsets is not supported");
    Reject(device, CoincidingOffsetsSt64, "DS write exchange of one location through both offsets is not supported");
}};

const Testing::Case destinationOverflow{"DsWrxchg2B64_DestinationRegisterOverflow_IsRejected", [] {
    auto& device = SharedVulkanTestDevice();
    Reject(device, DestinationOverflow, "DS write exchange destination register range overflow");
}};

} // namespace
