#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

namespace {
using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Words = 8;
alignas(256) std::array<std::uint32_t, Threads * Words> buffer{};
alignas(256) std::array<std::byte, 65536> texture{};
constexpr std::array<std::uint32_t, 4> Coordinates{0u, 1u, 3u, 4u};
constexpr std::array<std::uint32_t, 4> Samples{0u, 1u, 7u, 0xffffffffu};
constexpr std::array<std::uint32_t, 9> UnormExpected{0x0u, 0x3b808081u, 0x3c40c0c1u, 0x3e808081u, 0x3e828283u, 0x3e868687u, 0x3f40c0c1u, 0x3f41c1c2u, 0x3f43c3c4u};
constexpr std::array<std::uint32_t, 9> SrgbExpected{0x0u, 0x399f0000u, 0x3a6f0000u, 0x3d520000u, 0x3d590000u, 0x3d660000u, 0x3f070000u, 0x3f090000u, 0x3f0c0000u};
alignas(256) constexpr std::array<std::uint32_t, 14> MsaaCode{
    0x34020085u, 0xe0301000u, 0x80000201u, 0xe0301004u, 0x80000301u, 0xe0301008u, 0x80000401u,
    0xbf8c3f70u, 0xf0000130u, 0x00010802u, 0xbf8c3f70u, 0xe0701010u, 0x80000801u, 0xbf810000u
};
alignas(256) constexpr auto PlaneCode = [] {
    auto code = MsaaCode;
    code[8] = 0xf0000108u;
    return code;
}();

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(texture.data());
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | 0xc0000000u, 0xc000u, 0x90000facu, 0u, 0u, 0u, 0u};
}

void FillTexture(const std::array<std::uint32_t, 8>& descriptor) {
    const auto layout = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(layout.guestBytes <= texture.size(), "MSAA-address texture exceeds its storage");
    texture.fill(std::byte{0xee});
    const auto& mip = layout.mips.at(0);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const auto value = 0x12340000u + y * 64u + x;
            std::memcpy(texture.data() + mip.tiledOffset + y * mip.pitchBytes + x * 4u, &value, sizeof(value));
        }
    }
}

std::array<std::uint32_t, Threads> Load(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& image, std::span<const std::uint32_t> code, std::uint32_t wave) {
    buffer.fill(0xdeadbeefu);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        buffer[lane * Words] = Coordinates[lane % 4u];
        buffer[lane * Words + 1u] = Coordinates[(lane / 4u) % 4u];
        buffer[lane * Words + 2u] = Samples[lane / 16u];
    }
    const auto address = reinterpret_cast<std::uintptr_t>(buffer.data());
    std::vector<std::uint32_t> data{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(buffer.size() * 4u), 0x31016facu};
    data.insert(data.end(), image.begin(), image.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {wave, 0, data, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto compiled = ShaderRecompiler::Recompile(request);
    device.Dispatch(compiled, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::array<std::uint32_t, Threads> result{};
    for (std::uint32_t lane = 0; lane < Threads; ++lane) result[lane] = buffer[lane * Words + 4u];
    return result;
}

void Run(AgcDriver::VulkanDevice& device) {
    for (const auto format : {20u, 22u, 56u, 130u}) {
        const auto descriptor = TextureDescriptor(format);
        FillTexture(descriptor);
        for (const auto wave : {32u, 64u}) {
            const auto plain = Load(device, descriptor, PlaneCode, wave);
            const auto msaa = Load(device, descriptor, MsaaCode, wave);
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                Require(msaa[lane] == plain[lane], "MSAA-address load differs from the measured 2D control at lane " + std::to_string(lane));
                const auto x = Coordinates[lane % 4u];
                const auto y = Coordinates[(lane / 4u) % 4u];
                if (x == 4u || y == 4u) Require(msaa[lane] == 0u, "MSAA-address out-of-range load did not read zero");
                else if (format == 20u || format == 22u) Require(msaa[lane] == 0x12340000u + y * 64u + x, "MSAA-address load lost the oracle texel marker");
                else {
                    const auto index = ((lane / 4u) % 4u) * 3u + lane % 4u;
                    const auto expected = std::bit_cast<float>((format == 56u ? UnormExpected : SrgbExpected)[index]);
                    const auto actual = std::bit_cast<float>(msaa[lane]);
                    if (format == 130u) {
                        const auto encoded = [](float linear) { return std::lround((linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f) * 255.0f); };
                        Require(std::isfinite(actual) && encoded(actual) == encoded(expected), "MSAA-address sRGB load changed the oracle's encoded texel");
                    } else Require(std::abs(actual - expected) < 0.0001f, "MSAA-address normalized load differs from the Radeon oracle");
                }
            }
        }
    }
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
