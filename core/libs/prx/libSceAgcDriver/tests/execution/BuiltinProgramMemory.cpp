#include "VulkanTestDevice.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::DriverDetail::ShaderSnapshot;
using ShaderRecompiler::MemoryRegion;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("builtin program memory: " + message);
}

struct GuestVertex {
    struct Header {
        Shader shader{};
        ShaderUserData users{};
    } header;
    alignas(256) std::array<std::uint32_t, 65> code{};
    std::shared_ptr<ShaderSnapshot> snapshot = std::make_shared<ShaderSnapshot>();

    GuestVertex() {
        header.shader.type = 2;
        header.shader.user_data = &header.users;
        code.fill(0xffffffffu);
        code.back() = 0xbf810000u;
        snapshot->type = 2;
        snapshot->codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        snapshot->headerAddress = reinterpret_cast<std::uintptr_t>(&header);
        snapshot->code.assign(code.begin(), code.end());
        snapshot->header.resize(sizeof(header));
        std::memcpy(snapshot->header.data(), &header, sizeof(header));
    }

    void Bind(AgcDriver::QueueState& queue) const {
        const auto address = snapshot->codeAddress + 256u;
        queue.shader[0xc8u] = static_cast<std::uint32_t>(address >> 8u);
        queue.shader[0xc9u] = static_cast<std::uint32_t>(address >> 40u);
        queue.shader[0x8bu] = 0u;
    }
};

bool Overlaps(const MemoryRegion& region, std::uint64_t begin, std::size_t bytes) {
    return !region.bytes.empty() && bytes != 0 && region.guestAddress < begin + bytes && begin < region.guestAddress + region.bytes.size();
}

bool Lists(const std::vector<MemoryRegion>& memory, std::uint64_t address, std::size_t bytes) {
    for (const auto& region : memory) {
        if (region.guestAddress == address && region.bytes.size() == bytes) return true;
    }
    return false;
}

void CheckDraw(const char* name, const AgcDriver::QueueState& queue, const AgcDriver::DriverDetail::ShaderRegistry& registry, const GuestVertex& vertex, std::uint64_t builtinAddress) {
    using namespace AgcDriver::DriverDetail;
    DrawDecode draw{};
    draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    Require(draw.programs.size() == 2 && draw.programs.front().snapshot == vertex.snapshot && draw.programs.back().snapshot->codeAddress == builtinAddress, std::string(name) + ": the draw did not decode the guest vertex program and the driver's pixel program");
    const auto& builtin = draw.programs.back().snapshot;
    std::vector<MemoryRegion> memory;
    AppendProgramMemory(memory, draw.programs);
    const auto codeBytes = builtin->code.size() * sizeof(std::uint32_t);
    std::size_t driverRegions = 0;
    for (const auto& region : memory) {
        if (Overlaps(region, builtin->codeAddress, codeBytes) || Overlaps(region, builtin->headerAddress, builtin->header.size())) ++driverRegions;
    }
    const bool guestListed = Lists(memory, vertex.snapshot->codeAddress, vertex.snapshot->code.size() * sizeof(std::uint32_t)) && Lists(memory, vertex.snapshot->headerAddress, vertex.snapshot->header.size());
    std::printf("%s: %zu captured program regions, %zu of them the driver's own code or header (0x%llx+0x%zx, 0x%llx+0x%zx), guest vertex code and header %s\n", name, memory.size(), driverRegions,
            static_cast<unsigned long long>(builtin->codeAddress), codeBytes, static_cast<unsigned long long>(builtin->headerAddress), builtin->header.size(), guestListed ? "listed" : "missing");
    Require(driverRegions == 0, std::string(name) + ": the draw captured the driver's own program code or header as guest memory, so every draw compares it again");
    Require(guestListed && memory.size() == 2, std::string(name) + ": the draw no longer captures the guest vertex program's code and header");
}

void DepthCopyProgramsAreNotCaptured() {
    using namespace AgcDriver::DriverDetail;
    for (const bool multisampled : {false, true}) {
        GuestVertex vertex;
        const auto copy = std::make_shared<const ShaderSnapshot>(PrepareDepthCopyPixelProgram(multisampled));
        ShaderRegistry registry;
        for (const auto& snapshot : {std::shared_ptr<const ShaderSnapshot>(vertex.snapshot), copy}) registry.emplace(snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{snapshot});
        AgcDriver::QueueState queue{};
        vertex.Bind(queue);
        const auto address = DepthCopyPixelProgramAddress(multisampled);
        const auto words = DepthCopyPixelUserWords(multisampled);
        queue.shader[0x008u] = static_cast<std::uint32_t>(address >> 8u);
        queue.shader[0x009u] = static_cast<std::uint32_t>(address >> 40u);
        queue.shader[0x00bu] = words << 1u;
        for (std::uint32_t i = 0; i < words; ++i) queue.shader[0x00cu + i] = 0u;
        queue.context[0x1b3u] = queue.context[0x1b4u] = 0x8002u;
        queue.context[0x1c5u] = 1u;
        queue.context[0x08eu] = 0xfu;
        queue.context[0x08fu] = 1u;
        queue.context[0x203u] = 0u;
        Require(ProgramSnapshot(registry, address, {1}) == copy, "the depth copy program is not the registered one");
        CheckDraw(multisampled ? "multisampled depth copy draw" : "single-sample depth copy draw", queue, registry, vertex, address);
    }
}

void StencilCopyProgramsAreNotCaptured() {
    using namespace AgcDriver::DriverDetail;
    for (const bool multisampled : {false, true}) {
        GuestVertex vertex;
        const auto copy = std::make_shared<const ShaderSnapshot>(PrepareStencilCopyPixelProgram(multisampled));
        ShaderRegistry registry;
        for (const auto& snapshot : {std::shared_ptr<const ShaderSnapshot>(vertex.snapshot), copy}) registry.emplace(snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{snapshot});
        AgcDriver::QueueState queue{};
        vertex.Bind(queue);
        const auto address = StencilCopyPixelProgramAddress(multisampled);
        const auto words = DepthCopyPixelUserWords(multisampled);
        queue.shader[0x008u] = static_cast<std::uint32_t>(address >> 8u);
        queue.shader[0x009u] = static_cast<std::uint32_t>(address >> 40u);
        queue.shader[0x00bu] = words << 1u;
        for (std::uint32_t i = 0; i < words; ++i) queue.shader[0x00cu + i] = 0u;
        queue.context[0x1b3u] = queue.context[0x1b4u] = 0x8002u;
        queue.context[0x1c5u] = 2u;
        queue.context[0x08eu] = 0xfu;
        queue.context[0x08fu] = 3u;
        queue.context[0x203u] = 0u;
        Require(ProgramSnapshot(registry, address, {1}) == copy, "the stencil copy program is not the registered one");
        CheckDraw(multisampled ? "multisampled stencil copy draw" : "single-sample stencil copy draw", queue, registry, vertex, address);
    }
}

void BindDepthOnly(AgcDriver::QueueState& queue) {
    queue.context[0x8eu] = queue.context[0x8fu] = queue.context[0x203u] = 0u;
    queue.context[0x2dcu] = 0xaa00u;
    queue.context[0x1c4u] = 1u;
}

void UnregisteredNullPixelProgramIsNotCaptured() {
    using namespace AgcDriver::DriverDetail;
    GuestVertex vertex;
    ShaderRegistry registry;
    registry.emplace(vertex.snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{vertex.snapshot});
    AgcDriver::QueueState queue{};
    vertex.Bind(queue);
    BindDepthOnly(queue);
    Require(ProgramSnapshot(registry, NullPixelProgramAddress(), {1})->header.empty(), "the unregistered null pixel program was not read raw");
    CheckDraw("depth-only draw before the null pixel program is registered", queue, registry, vertex, NullPixelProgramAddress());
}

void NullPixelProgramIsNotCaptured(const AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    GuestVertex vertex;
    const auto null = std::make_shared<const ShaderSnapshot>(PrepareNullPixelProgram(device));
    ShaderRegistry registry;
    for (const auto& snapshot : {std::shared_ptr<const ShaderSnapshot>(vertex.snapshot), null}) registry.emplace(snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{snapshot});
    AgcDriver::QueueState queue{};
    vertex.Bind(queue);
    BindDepthOnly(queue);
    Require(ProgramSnapshot(registry, NullPixelProgramAddress(), {1}) == null, "the null pixel program is not the registered one");
    CheckDraw("depth-only draw with the null pixel program", queue, registry, vertex, NullPixelProgramAddress());
}

}

int main() {
    try {
        DepthCopyProgramsAreNotCaptured();
        StencilCopyProgramsAreNotCaptured();
        UnregisteredNullPixelProgramIsNotCaptured();
        auto device = OpenVulkanTestDevice();
        if (device) {
            NullPixelProgramIsNotCaptured(*device);
        } else {
            std::puts("null pixel program check not run: no Vulkan device to prepare it on");
        }
        std::puts("builtin program memory: a draw captures the guest's program memory and none of the driver's own pixel programs");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
