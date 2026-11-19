#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_TEXTURELODFIDELITY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_TEXTURELODFIDELITY_HPP

#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace TextureLodFidelity {

struct Remap {
    float lod;
    float sampled;
};

inline constexpr std::uint32_t Threads = 32;
inline constexpr std::uint32_t Words = 32;
inline constexpr std::uint32_t ResultWord = 8;
inline constexpr std::uint32_t Size = 16;
inline constexpr std::uint32_t Levels = 5;
inline constexpr std::uint32_t Format8888UNorm = 56;
inline constexpr std::uint32_t Type2D = 9;
inline constexpr std::uint32_t MipFilterLinear = 2;
inline constexpr float Tolerance = 1.0f / 16.0f + 1.0f / 255.0f;
alignas(256) inline std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) inline std::array<std::uint8_t, 16384> Texels{};

enum class Path { Gradient, ExplicitLod };

alignas(256) inline constexpr std::array<std::uint32_t, 27> GradientCode{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xbf8c3f70,
    0xf08c0f08, 0x00610c05, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) inline constexpr std::array<std::uint32_t, 19> ExplicitLodCode{
    0x34020087, 0xe0301010, 0x80001001, 0xe0301014, 0x80001101, 0xe030101c, 0x80001201, 0xbf8c3f70,
    0xf0900f08, 0x00610c10, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xbf810000,
};

inline float Lod(std::uint32_t tid) {
    return static_cast<float>(tid % 3u) + static_cast<float>(tid % 7u + 1u) / 8.0f;
}

inline std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

inline void Fill(Path path) {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Size, Size, Levels);
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = static_cast<std::uint8_t>(level);
                texel[1] = 0u;
                texel[2] = 0u;
                texel[3] = 255u;
            }
        }
    }
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Buffer[tid * Words];
        words[0] = path == Path::Gradient ? Bits(std::exp2(Lod(tid)) / static_cast<float>(Size)) : 0u;
        words[1] = 0u;
        words[2] = 0u;
        words[3] = 0u;
        words[4] = Bits(0.5f / static_cast<float>(Size));
        words[5] = Bits(0.5f / static_cast<float>(Size));
        words[6] = Bits(0.0f);
        words[7] = Bits(path == Path::ExplicitLod ? Lod(tid) : 0.0f);
    }
}

inline std::vector<std::uint32_t> UserData() {
    const auto buffer = reinterpret_cast<std::uintptr_t>(Buffer.data());
    const auto texels = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(buffer), static_cast<std::uint32_t>((buffer >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu,
        static_cast<std::uint32_t>(texels >> 8u),
        static_cast<std::uint32_t>((texels >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u, (Levels - 1u) << 4u, 0u, 0u,
        0u, 0xfffu << 12u, MipFilterLinear << 26u, 0u,
    };
}

inline std::optional<Remap> Find(AgcDriver::VulkanDevice& device, Path path) {
    Fill(path);
    const auto userData = UserData();
    const std::span<const std::uint32_t> code = path == Path::Gradient ? std::span<const std::uint32_t>{GradientCode} : std::span<const std::uint32_t>{ExplicitLodCode};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    device.Dispatch(ShaderRecompiler::Recompile(request), 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::optional<Remap> worst;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sampled = std::bit_cast<float>(Buffer[tid * Words + ResultWord]) * 255.0f;
        const auto error = std::abs(sampled - Lod(tid));
        if (error > Tolerance && (!worst || error > std::abs(worst->sampled - worst->lod))) worst = Remap{Lod(tid), sampled};
    }
    return worst;
}

inline std::string Reason(Path path, const Remap& remap) {
    char text[400];
    std::snprintf(text, sizeof(text), "the driver samples linear mips at %s LOD %.3f as %.3f, more than %.3f away, so it does not apply fractional LODs as Vulkan specifies; on NVIDIA, Manage 3D settings > Texture filtering - Quality at High performance does this; set it to Quality (the default) or High quality", path == Path::Gradient ? "the gradient" : "the explicit", remap.lod, remap.sampled, Tolerance);
    return text;
}

}

#endif
