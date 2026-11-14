#include "prx/libSceAgcDriver/Execution/include/Float32NanProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver {
namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t Compared = 48;
constexpr std::uint32_t Rounds = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::array<std::uint32_t, 78> BuildCode() {
    std::array<std::uint32_t, 78> code{};
    std::size_t n = 0;
    for (const std::uint32_t word : {0x34020084u, 0x34060088u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u}) code[n++] = word;
    std::uint32_t destination = 10u;
    for (const std::uint32_t opcode : {3u, 8u, 4u, 5u}) {
        for (std::uint32_t first = 4u; first < 8u; ++first) {
            for (std::uint32_t second = 4u; second < 8u; ++second) {
                if (first != second) code[n++] = (opcode << 25u) | (destination++ << 17u) | (second << 9u) | 0x100u | first;
            }
        }
    }
    for (std::uint32_t store = 0; store < Compared / 4u; ++store) {
        code[n++] = 0xe0781000u | (store << 4u);
        code[n++] = 0x80010003u | ((10u + store * 4u) << 8u);
    }
    code[n++] = 0xbf810000u;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, 78> Code = BuildCode();

constexpr std::array<std::uint32_t, 16> Values{
    0x7fc12345u, 0x7fa54321u, 0xffc0beefu, 0xff812345u, 0x7fc00000u, 0xffc00000u, 0x7f800000u, 0xff800000u,
    0x00000000u, 0x80000000u, 0x3fc00000u, 0xc0100000u, 0x7f61b1e6u, 0x00800000u, 0x00000001u, 0x807fffffu,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Build(const ShaderRecompiler::SpirvTarget& target, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
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
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    return ShaderRecompiler::Recompile(request);
}

std::vector<std::uint32_t> Run(VulkanDevice& device, const ShaderRecompiler::RecompileResult& shader, std::uint32_t round) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t pair = round * Threads + tid;
        const std::uint32_t first = pair / 16u;
        const std::uint32_t second = pair % 16u;
        Input[tid * Inputs] = Values[first];
        Input[tid * Inputs + 1u] = Values[second];
        Input[tid * Inputs + 2u] = Values[(first + second + round) % 16u];
        Input[tid * Inputs + 3u] = Values[(first * 3u + second * 5u + 1u) % 16u];
    }
    Output.fill(0xdeadbeefu);
    device.Dispatch(shader, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    return {Output.begin(), Output.end()};
}

}

std::span<const std::uint32_t> Float32NanProbeCode() {
    return Code;
}

Float32NanProbeResult ProbeFloat32SingleNans(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    Float32NanProbeResult result;
    auto emulated = target;
    emulated.float32SingleNans = false;
    auto guarded = target;
    guarded.float32SingleNans = true;
    const std::array<std::optional<ShaderRecompiler::ShaderFloatMode>, 2> modes{std::nullopt, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}};
    for (const auto& mode : modes) {
        const auto emulatedShader = Build(emulated, mode);
        const auto guardedShader = Build(guarded, mode);
        for (std::uint32_t round = 0; round < Rounds; ++round) {
            const auto expected = Run(device, emulatedShader, round);
            const auto actual = Run(device, guardedShader, round);
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                for (std::uint32_t slot = 0; slot < Compared; ++slot) {
                    const std::uint32_t index = tid * Results + slot;
                    ++result.compared;
                    if (expected[index] == actual[index]) continue;
                    ++result.mismatches;
                    if (!result.first) result.first = Float32NanMismatch{slot, expected[index], actual[index]};
                }
            }
        }
    }
    return result;
}

}
