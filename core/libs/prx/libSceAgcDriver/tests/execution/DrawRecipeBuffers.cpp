#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include "Cover_vert_spv.h"
#include "DrawRecipeBuffers_spv.h"
#include "DrawRecipeData_spv.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;
constexpr std::size_t Unit = 65536;

class Block {
public:
    Block() {
#ifdef _WIN32
        data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Bytes, Unit));
        Require(data != nullptr, "cannot reserve recipe test memory");
        GuestArena::GuestArenaCommit_nid_postfix(data, Bytes, PAGE_READWRITE, Bytes);
#else
        void* raw = mmap(nullptr, Bytes + Unit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot map recipe test memory");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + Unit - 1) & ~(static_cast<std::uintptr_t>(Unit) - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + Bytes != begin + Bytes + Unit) munmap(reinterpret_cast<void*>(aligned + Bytes), begin + Unit - aligned);
        data = reinterpret_cast<std::byte*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, Bytes);
#endif
        std::fill_n(data, Bytes, std::byte{0});
        GuestAllocations::Mutation().Add(data, Bytes, true, true, true);
    }
    ~Block() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(data, Bytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, Bytes);
#else
        munmap(data, Bytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, Bytes);
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address(std::size_t offset = 0) const { return reinterpret_cast<std::uintptr_t>(data + offset); }

private:
    static constexpr std::size_t Bytes = 3 * Unit;
    std::byte* data = nullptr;
};

void CheckPixels(const ColorTarget& target, const std::array<std::uint8_t, 4>& expected) {
    std::vector<std::byte> pixels(64 * 64 * 4);
    ReadColorTarget(target, pixels);
    for (std::size_t pixel = 0; pixel < pixels.size() / 4; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto actual = std::to_integer<std::uint8_t>(pixels[pixel * 4 + channel]);
            Require(actual == expected[channel], "recipe pixel " + std::to_string(pixel) + " channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[channel]));
        }
    }
}

bool CheckBindings(AgcDriver::VulkanDevice& device, const Block& block, bool dataWords) {
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!dataWords && context.hostImportAlignment == 0) {
        std::cout << "skipped, host imports are unavailable\n";
        return false;
    }
    Require(context.recorder != nullptr && context.descriptorCache != nullptr, "recipe tests need a recorder and descriptor cache");
    const auto program = [](const auto& spirv, std::uint64_t variant) {
        ShaderRecompiler::RecompileResult result;
        result.spirv = std::vector<std::uint32_t>(std::begin(spirv), std::end(spirv));
        result.variantId = variant;
        return result;
    };
    auto vertex = program(COVER_VERT_SPV, 1);
    auto fragment = dataWords ? program(DRAW_RECIPE_DATA_SPV, 3) : program(DRAW_RECIPE_BUFFERS_SPV, 2);
    if (dataWords) fragment.shaderDataDwords = 4;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = dataWords ? ShaderRecompiler::DescriptorRole::ShaderData : ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.bufferWritten = {false};
    fragment.bindings = {binding};
    const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}};
    State state{};
    state.stages.path = ShaderPath::Vertex;
    state.color = {block.Address(), {64, 64}, VK_FORMAT_R8G8B8A8_UNORM, Unit, 0xe4u, ColorTileMode::RenderTarget, 4};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {64, 64};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, 64, 64, -64, 0, 1};
    state.scissor = {{0, 0}, {64, 64}};
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters triangle{0, 3, 0, 1, 0, false};
    const std::array<float, 4> red{1, 0, 0, 1};
    const std::array<float, 4> green{0, 1, 0, 1};
    AgcDriver::GuestMemory::Write(block.Address(Unit), std::as_bytes(std::span(red)));
    AgcDriver::GuestMemory::Write(block.Address(2 * Unit), std::as_bytes(std::span(green)));
    const auto select = [&](bool second) {
        auto& words = fragment.bindings.front().guestDescriptor;
        if (dataWords) {
            const auto& color = second ? green : red;
            words.clear();
            for (const auto value : color) words.push_back(std::bit_cast<std::uint32_t>(value));
        } else {
            const auto address = block.Address(second ? 2 * Unit : Unit);
            words = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, sizeof(red), 0x31000000u};
        }
    };
    select(false);
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Draw(context, state, triangle, shaders, {}, &recipe);
    }
    WaitForOptimizedPipelines(context.device);
    Require(recipe != nullptr, "the initial draw did not produce a recipe");
    CheckPixels(state.color, {255, 0, 0, 255});
    if (!dataWords) {
        select(true);
        std::shared_ptr<const AgcDriver::DrawRecipe> relocated;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Draw(context, state, triangle, shaders, {}, &relocated);
        }
        WaitForOptimizedPipelines(context.device);
        Require(relocated != nullptr, "a draw over relocated buffers did not produce a recipe");
        CheckPixels(state.color, {0, 255, 0, 255});
        select(false);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto hit = DrawWithRecipe(context, state, triangle, shaders, {}, *relocated);
            Require(hit.recorded, "a recipe made over relocated buffers refused a safe buffer change");
        }
        CheckPixels(state.color, {255, 0, 0, 255});
    }
    for (const bool second : {true, true, false}) {
        select(second);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto hit = DrawWithRecipe(context, state, triangle, shaders, {}, *recipe);
            Require(hit.recorded, "a safe buffer change refused its recipe");
        }
        CheckPixels(state.color, second ? std::array<std::uint8_t, 4>{0, 255, 0, 255} : std::array<std::uint8_t, 4>{255, 0, 0, 255});
    }
    const auto refused = [&] {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto hit = DrawWithRecipe(context, state, triangle, shaders, {}, *recipe);
        Require(!hit.recorded && hit.miss == DrawRecipeMiss::Buffers, "an unsafe binding change did not fall back to the ordinary draw path");
    };
    if (dataWords) {
        fragment.bindings.front().guestDescriptor.pop_back();
        --fragment.shaderDataDwords;
        refused();
    } else {
        select(true);
        fragment.bindings.front().bufferWritten = {true};
        refused();
        fragment.bindings.front().bufferWritten = {false};
        fragment.bindings.front().guestDescriptor = {0x1000u, 0, sizeof(red), 0x31000000u};
        refused();
        fragment.bindings.front().guestDescriptor = {0, 0, 0, 0};
        refused();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Draw(context, state, triangle, shaders);
        }
        CheckPixels(state.color, {0, 0, 0, 0});
    }
    device.WaitIdle();
    return true;
}

}

int main(int argc, char** argv) {
    try {
        Block block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool dataWords = argc == 2 && std::string_view(argv[1]) == "--data";
        if (!CheckBindings(*device, block, dataWords)) return VulkanTestSkipped;
        std::cout << "draw recipes consume relocated buffers and changed shader data\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
