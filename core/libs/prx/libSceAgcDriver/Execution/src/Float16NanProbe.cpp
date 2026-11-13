#include "prx/libSceAgcDriver/Execution/include/Float16NanProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver {
namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Combinations = 4096;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> PackedCode{
    0x34020084u, 0x34060085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xcc0f400au, 0x18020b04u,
    0xcc10400bu, 0x18020b04u,
    0xcc0e400cu, 0x1c1a0b04u,
    0xcc0f4a0du, 0x30020b04u,
    0xcc10c00eu, 0x18020d04u,
    0xcc0e330fu, 0x8c1a0b04u,
    0x7e200306u, 0x78200b04u,
    0xcc0ec011u, 0x3c1a0f04u,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xbf810000u,
};
constexpr std::uint32_t PackedResults = 8;

alignas(256) constexpr std::array<std::uint32_t, 41> ScalarCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x64140b04u,
    0x66160b04u,
    0x68180b04u,
    0x6a1a0b04u,
    0xd74b000eu, 0x041a0b04u,
    0xd74b980fu, 0x041a0b04u,
    0xd74b3110u, 0xc4160d04u,
    0x7e220306u, 0x6c220b04u,
    0x6e240b04u, 0x00003d55u,
    0x70260b04u, 0x00000001u,
    0xd74ba214u, 0x241a0b04u,
    0x662a08f2u,
    0xd5328116u, 0x20020b04u,
    0xd5350217u, 0x40020d04u,
    0xd5338018u, 0x00020b04u,
    0xd5340019u, 0x20020b04u,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};
constexpr std::uint32_t ScalarFmaFirst = 4;
constexpr std::uint32_t ScalarFmaLast = 10;

constexpr std::array<std::uint32_t, 16> Halves{
    0x7e00u, 0xfe00u, 0xff2au, 0x7d55u, 0xfc01u, 0x7c01u, 0x7c00u, 0xfc00u,
    0x0000u, 0x8000u, 0x0001u, 0x0400u, 0x3c00u, 0xbc00u, 0x3555u, 0x7bffu,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

bool IsNan16(std::uint32_t half) {
    return (half & 0x7fffu) > 0x7c00u;
}

std::uint32_t Half(std::uint32_t combination, std::uint32_t operand) {
    return Halves[(combination >> (operand * 4u)) & 15u];
}

void FillRows(std::uint32_t first) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t low = (first + tid) % Combinations;
        const std::uint32_t high = Combinations - 1u - low;
        for (std::uint32_t operand = 0; operand < Inputs; ++operand) {
            const std::uint32_t lowOperand = operand < 3u ? operand : 1u;
            const std::uint32_t highOperand = operand < 3u ? operand : 2u;
            Input[tid * Inputs + operand] = Half(low, lowOperand) | (Half(high, highOperand) << 16u);
        }
    }
}

std::vector<std::uint32_t> Run(VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::span<const std::uint32_t> code, std::uint32_t first) {
    FillRows(first);
    Output.fill(0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return {Output.begin(), Output.end()};
}

ShaderRecompiler::RecompileResult Compile(ShaderRecompiler::SpirvTarget target, bool hostNans, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    target.float16HostNans = hostNans;
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

}

Float16NanProbeResult ProbeFloat16HostNans(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    Float16NanProbeResult result;
    const std::array<std::optional<ShaderRecompiler::ShaderFloatMode>, 3> modes{std::nullopt, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, true}, ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, true}};
    for (const auto& mode : modes) {
        const auto packedSelected = Compile(target, false, mode, PackedCode);
        const auto packedHost = Compile(target, true, mode, PackedCode);
        for (std::uint32_t first = 0; first < Combinations; first += Threads) {
            const auto selected = Run(device, packedSelected, PackedCode, first);
            const auto host = Run(device, packedHost, PackedCode, first);
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                for (std::uint32_t index = 0; index < PackedResults; ++index) {
                    ++result.compared;
                    if (selected[tid * PackedResults + index] != host[tid * PackedResults + index]) ++result.mismatches;
                }
            }
            if (result.mismatches != 0u) return result;
        }
        const auto scalarSelected = Compile(target, false, mode, ScalarCode);
        const auto scalarHost = Compile(target, true, mode, ScalarCode);
        for (std::uint32_t first = 0; first < Combinations; first += Threads) {
            const auto scalarExact = Run(device, scalarSelected, ScalarCode, first);
            const auto scalar = Run(device, scalarHost, ScalarCode, first);
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                for (std::uint32_t index = ScalarFmaFirst; index <= ScalarFmaLast; ++index) {
                    const std::uint32_t expected = scalarExact[tid * Results + index];
                    const std::uint32_t actual = scalar[tid * Results + index];
                    for (const std::uint32_t shift : {0u, 16u}) {
                        const std::uint32_t expectedHalf = (expected >> shift) & 0xffffu;
                        const std::uint32_t actualHalf = (actual >> shift) & 0xffffu;
                        ++result.compared;
                        if (expectedHalf != actualHalf && !(IsNan16(expectedHalf) && IsNan16(actualHalf) && expectedHalf != 0xfe00u)) ++result.mismatches;
                    }
                }
            }
            if (result.mismatches != 0u) return result;
        }
    }
    return result;
}

}
