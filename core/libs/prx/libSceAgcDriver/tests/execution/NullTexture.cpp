#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 8;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t CompareAlways = 7u << 12u;
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x7e0402f0, 0x7e0602f0, 0xf09c0f08, 0x00610802, 0x7e080280, 0xf0380f08, 0x00010c04, 0xbf8c3f70,
    0x34020085, 0xe0781000, 0x80000801, 0xe0781010, 0x80000c01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> CompareCode{
    0x7e0202f2, 0x7e0402f0, 0x7e0602f0, 0xf0bc0f08, 0x00610801, 0xbf8c3f70, 0x34020085, 0xe0781000,
    0x80000801, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t samplerWord0) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(output.begin(), output.end(), userData.begin());
    userData[12] = ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u) | samplerWord0;
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

bool BindsComparisonImage(const ShaderRecompiler::RecompileResult& result) {
    return std::any_of(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) {
        return std::any_of(binding.imageDepthCompare.begin(), binding.imageDepthCompare.end(), [](bool compare) { return compare; });
    });
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool nullDescriptor = device->NullDescriptor();
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(Code.data());
        Output.fill(Sentinel);
        const auto sample = Compile(*device, std::span<const std::uint32_t>(Code), 0u);
        device->Dispatch(sample, 1, 1, 1, {}, codeAddress);
        device->WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto sampled = Output[tid * Words + component];
                Require(sampled == 0u, "an all-zero T#: image_sample_lz thread " + std::to_string(tid) + " component " + std::to_string(component) + " read " + std::to_string(std::bit_cast<float>(sampled)) + ", expected 0");
            }
            if (!nullDescriptor) continue;
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto size = Output[tid * Words + 4u + component];
                Require(size == 0u, "an all-zero T#: image_get_resinfo thread " + std::to_string(tid) + " component " + std::to_string(component) + " read " + std::to_string(size) + ", expected 0");
            }
        }

        Output.fill(Sentinel);
        const auto compareCodeAddress = reinterpret_cast<std::uintptr_t>(CompareCode.data());
        const auto compare = Compile(*device, std::span<const std::uint32_t>(CompareCode), CompareAlways);
        Require(BindsComparisonImage(compare), "the image_sample_c_lz test shader binds no depth-comparison image, so the comparison checks prove nothing");
        if (nullDescriptor) {
            device->Dispatch(compare, 1, 1, 1, {}, compareCodeAddress);
            device->WaitIdle();
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                for (std::uint32_t component = 0; component < 4u; ++component) {
                    const auto compared = Output[tid * Words + component];
                    Require(compared == 0u, "an all-zero T#: image_sample_c_lz thread " + std::to_string(tid) + " component " + std::to_string(component) + " read " + std::to_string(std::bit_cast<float>(compared)) + ", expected 0");
                }
            }
        } else {
            std::string thrown;
            try {
                device->Dispatch(compare, 1, 1, 1, {}, compareCodeAddress);
                device->WaitIdle();
            } catch (const std::exception& error) {
                thrown = error.what();
            }
            Require(!thrown.empty(), "an all-zero T# bound by a depth-comparison sampler without nullDescriptor did not throw; the 1x1 fallback read " + std::to_string(std::bit_cast<float>(Output[0])) + " for thread 0 component 0, the hardware reads 0");
            Require(thrown.find("nullDescriptor") != std::string::npos, "the comparison fallback threw without naming nullDescriptor: " + thrown);
        }
        std::puts(nullDescriptor ? "null texture tests passed" : "null texture tests passed (sizes not checked and comparisons throw: the device has no nullDescriptor)");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
