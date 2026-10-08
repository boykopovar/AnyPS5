#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Range = std::pair<std::uint64_t, std::uint64_t>;
using Ranges = std::vector<Range>;

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("draw write ranges: " + what);
}

bool Contains(const Ranges& ranges, std::uint64_t begin, std::uint64_t end) {
    return std::any_of(ranges.begin(), ranges.end(), [&](const Range& range) { return range.first <= begin && end <= range.second; });
}

bool Touches(const Ranges& ranges, std::uint64_t begin, std::uint64_t end) {
    return std::any_of(ranges.begin(), ranges.end(), [&](const Range& range) { return range.first < end && begin < range.second; });
}

std::array<std::uint32_t, 8> ImageWords(std::uint64_t base, std::uint32_t width, std::uint32_t height) {
    std::array<std::uint32_t, 8> words{};
    const auto base40 = base >> 8u;
    words[0] = static_cast<std::uint32_t>(base40);
    words[1] = static_cast<std::uint32_t>((base40 >> 32u) & 0xffu) | (56u << 20u) | (((width - 1u) & 0x3u) << 30u);
    words[2] = ((width - 1u) >> 2u) | ((height - 1u) << 14u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | (9u << 28u);
    return words;
}

}

int main() {
    try {
        using AgcDriver::DriverDetail::DrawWriteRanges;
        constexpr std::uint64_t written = 0x100000000ull;
        constexpr std::uint64_t readOnly = 0x200000000ull;
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding images;
        images.kind = ShaderRecompiler::DescriptorKind::StorageImage;
        images.role = ShaderRecompiler::DescriptorRole::GuestImages;
        images.count = 2;
        for (const auto base : {written, readOnly}) {
            const auto words = ImageWords(base, 256, 128);
            images.guestDescriptor.insert(images.guestDescriptor.end(), words.begin(), words.end());
        }
        images.imageWritten = {true, false};
        program.bindings.push_back(images);
        const std::array<AgcDriver::Graphics::CompiledShader, 1> stages{{{ShaderRecompiler::ShaderStage::Fragment, &program, 0}}};
        AgcDriver::Graphics::State state{};
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(std::span<const std::uint32_t>(images.guestDescriptor).subspan(0, 8));
        const auto surfaceBytes = AgcDriver::Graphics::DescribeSurface(resource).guestBytes;
        Require(resource.baseAddress == written && surfaceBytes >= 256u * 128u * 4u, "the test T# does not decode as meant");
        for (const bool exact : {false, true}) {
            const auto ranges = DrawWriteRanges(state, stages, exact);
            Require(Contains(ranges, written, written + surfaceBytes), "a storage image the draw stores to is not a write range");
            Require(!Touches(ranges, readOnly, readOnly + surfaceBytes), "a storage image the draw only reads is a write range");
        }
        program.bindings[0].imageWritten.clear();
        Require(Contains(DrawWriteRanges(state, stages, true), readOnly, readOnly + surfaceBytes), "an image without written flags is not a write range");
        AgcDriver::Graphics::State targets{};
        targets.hasColorTarget = true;
        auto& color = targets.color;
        color.address = 0x300000000ull;
        color.surfaceAddress = color.address;
        color.extent = {1920, 1080};
        color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
        color.elementBytes = 4;
        const AgcDriver::Graphics::ColorTargetLayout layout(1920, 1080, color.tileMode, 4);
        color.bytes = layout.Bytes();
        color.dccAddress = 0x380000000ull;
        AgcDriver::Graphics::DepthTarget depth{};
        depth.address = 0x400000000ull;
        depth.stencilAddress = 0x480000000ull;
        depth.extent = {1920, 1080};
        depth.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        targets.depth = depth;
        const std::span<const AgcDriver::Graphics::CompiledShader> none;
        const auto exact = DrawWriteRanges(targets, none, true);
        const auto estimated = DrawWriteRanges(targets, none, false);
        std::size_t last = 0;
        for (std::uint32_t y = 0; y < 1080; y += 7) {
            for (std::uint32_t x = 0; x < 1920; x += 5) last = std::max(last, layout.Offset(x, y));
        }
        last = std::max(last, layout.Offset(1919, 1079));
        Require(last + 4 <= color.bytes, "the color layout addresses past its bytes");
        Require(Contains(exact, color.address, color.address + color.bytes), "the exact color range misses the layout");
        Require(!Touches(exact, color.address + color.bytes, color.address + color.bytes + 1), "the exact color range runs past the layout");
        Require(Contains(exact, color.dccAddress, color.dccAddress + color.bytes / 256) && !Touches(exact, color.dccAddress + (color.bytes + 255) / 256, color.dccAddress + (color.bytes + 255) / 256 + 1), "the exact DCC range is not one key byte per 256 surface bytes");
        const auto depthBytes = AgcDriver::Graphics::DepthSliceBytes(depth.extent, 4);
        const auto stencilBytes = AgcDriver::Graphics::DepthSliceBytes(depth.extent, 1);
        Require(Contains(exact, depth.address, depth.address + depthBytes) && !Touches(exact, depth.address + depthBytes, depth.address + depthBytes + 1), "the exact depth range is not DepthSliceBytes");
        Require(Contains(exact, depth.stencilAddress, depth.stencilAddress + stencilBytes) && !Touches(exact, depth.stencilAddress + stencilBytes, depth.stencilAddress + stencilBytes + 1), "the exact stencil range is not DepthSliceBytes");
        for (const auto& [begin, end] : exact) Require(Contains(estimated, begin, end), "an exact range is not inside the estimates");
        std::printf("draw write ranges: color %zu bytes (estimate %llu), depth %llu, stencil %llu\n", color.bytes, static_cast<unsigned long long>(2 * color.bytes + 65536), static_cast<unsigned long long>(depthBytes), static_cast<unsigned long long>(stencilBytes));
        std::puts("draw write ranges tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
