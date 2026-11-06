#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_HEAPSAMPLEKERNEL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_HEAPSAMPLEKERNEL_HPP

#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

alignas(256) inline constexpr std::array<std::uint32_t, 8> SampleCode{0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u,
                                                                      0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};
inline constexpr std::array<std::uint8_t, 4> SampleTexel{0x10u, 0x80u, 0xc0u, 0xffu};

inline std::array<std::uint32_t, 16> SampleUserData(const void* image, const float* output) {
    const auto imageAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output));
    return {static_cast<std::uint32_t>(imageAddress >> 8u),
            static_cast<std::uint32_t>((imageAddress >> 40u) & 0xffu) | (56u << 20u) | (3u << 30u),
            3u << 14u,
            0xfacu | (9u << 28u),
            0u,
            0u,
            0u,
            0u,
            0u,
            0u,
            0u,
            0u,
            static_cast<std::uint32_t>(outputAddress),
            static_cast<std::uint32_t>(outputAddress >> 32u) & 0xffffu,
            static_cast<std::uint32_t>(4u * sizeof(float)),
            0x31016facu};
}

inline void FillTexels(std::span<std::uint8_t> memory, const std::array<std::uint8_t, 4>& texel) {
    for (std::size_t offset = 0; offset < memory.size(); offset += texel.size()) std::memcpy(memory.data() + offset, texel.data(), texel.size());
}

inline void RequireSampled(const float* output, const std::array<std::uint8_t, 4>& expected, const std::string& what, bool heap = false) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const float wanted = static_cast<float>(expected[channel]) / 255.0f;
        if (!(std::fabs(output[channel] - wanted) < 1e-6f))
            throw std::runtime_error(what + ": channel " + std::to_string(channel) + " is " + std::to_string(output[channel]) + ", expected " +
                                     std::to_string(wanted) + (heap ? " through the heap" : ""));
    }
}

inline ShaderRecompiler::RecompileResult CompileSampleKernel(AgcDriver::VulkanDevice& device, const void* image, const float* output,
                                                             bool useCache = true, std::span<const std::uint32_t> code = SampleCode,
                                                             std::uint32_t ldsDwords = 0u) {
    const auto userData = SampleUserData(image, output);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1u, 1u, 1u}, ldsDwords, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32u),
        {0u, 0u, 0u, 128u}};
    request.useCache = useCache;
    return ShaderRecompiler::Recompile(request);
}

#endif
