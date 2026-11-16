#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Cover_vert_spv.h"
#include "Cover_frag_spv.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr int VulkanTestSkipped = 77;
constexpr std::uint32_t Width = 61;
constexpr std::uint32_t Height = 13;
constexpr std::uint8_t RegisterClearKey = 0x20;
constexpr std::uint32_t Stale = 0x5a5a5a5au;

alignas(4096) std::array<std::byte, 2 * 65536> SurfaceStorage{};
const std::span<std::byte, 65536> Surface{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(SurfaceStorage.data()) + 65535u) & ~std::uintptr_t{65535u}), 65536};
alignas(4096) std::array<std::uint8_t, 4096> Keys{};

ColorTarget target() {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 16);
    ColorTarget color{};
    color.address = reinterpret_cast<std::uintptr_t>(Surface.data());
    color.extent = {Width, Height};
    color.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    color.bytes = layout.Bytes();
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 16;
    color.clearWords = {0x3f800000u, 0x3f000000u};
    color.surfaceAddress = color.address;
    color.surfaceExtent = color.extent;
    color.dccAddress = reinterpret_cast<std::uintptr_t>(Keys.data());
    return color;
}

void prepare(const ColorTarget& color) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(color.address, Surface.size(), nullptr, "test");
    std::vector<std::byte> stale(Surface.size());
    for (std::size_t offset = 0; offset < stale.size(); offset += 4) std::memcpy(stale.data() + offset, &Stale, 4);
    AgcDriver::GuestMemory::Write(color.address, stale);
    std::vector<std::byte> keys(color.bytes / 256u, std::byte{RegisterClearKey});
    AgcDriver::GuestMemory::Write(color.dccAddress, keys);
}

void draw(AgcDriver::VulkanDevice& device, const ColorTarget& color) {
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = std::vector<std::uint32_t>(std::begin(COVER_VERT_SPV), std::end(COVER_VERT_SPV));
    ShaderRecompiler::RecompileResult fragment;
    fragment.spirv = std::vector<std::uint32_t>(std::begin(COVER_FRAG_SPV), std::end(COVER_FRAG_SPV));
    const std::array<CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}
    }};
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = color;
    state.colors = {color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, 0, static_cast<float>(Width), static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {20, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    device.Draw(state, {0, 3, 0, 1, 0, false}, shaders);
    device.WaitIdle();
}

std::size_t staleTexels(AgcDriver::VulkanDevice& device, const ColorTarget& color) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
    device.WaitIdle();
    const ColorTargetLayout layout(Width, Height, color.tileMode, 16);
    std::size_t stale = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 20; x < Width; ++x) {
            std::array<std::uint32_t, 4> stored{};
            std::memcpy(stored.data(), Surface.data() + layout.Offset(x, y), sizeof(stored));
            if (stored == std::array<std::uint32_t, 4>{Stale, Stale, Stale, Stale}) ++stale;
        }
    }
    return stale;
}

void wideRegisterClear(AgcDriver::VulkanDevice& device) {
    const auto color = target();
    prepare(color);
    const std::string what = "a draw into a 128-bit DCC color target whose keys hold the register clear code";
    try {
        draw(device, color);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("the DCC register clear of a texel over 64 bits is not modeled") != std::string::npos, what + ": refused with " + error.what());
        return;
    }
    throw std::runtime_error(what + ": drew into the resident image whose register clear was not materialized; " + std::to_string(staleTexels(device, color)) + " texels outside the draw kept their stale value");
}

}

int main() {
    try {
        std::unique_ptr<AgcDriver::VulkanDevice> device;
        try {
            device = std::make_unique<AgcDriver::VulkanDevice>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(SurfaceStorage.data(), SurfaceStorage.size(), true, true, true);
            mutation.Add(Keys.data(), Keys.size(), true, true, true);
        }
        wideRegisterClear(*device);
        device->WaitIdle();
        device.reset();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(SurfaceStorage.data());
            mutation.Remove(Keys.data());
        }
        std::cout << "resident target register clear tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
