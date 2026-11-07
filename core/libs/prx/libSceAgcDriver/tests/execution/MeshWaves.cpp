#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t Waves = Threads / WaveSize;
constexpr std::uint32_t GroupVertices = 192;
constexpr std::uint32_t GroupPrimitives = 64;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;

alignas(256) std::array<std::uint32_t, Threads> Mismatches{};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Waves> Values{};
alignas(16) std::array<std::uint32_t, 8> Descriptor{};
constexpr std::uint32_t ValueTag = 0x5a000000u;

alignas(256) constexpr std::array<std::uint32_t, 13 + 10 + 12> ScalarCode{
    0xd7650001u, 0x000100c1u, 0xd7660001u, 0x000202c1u, 0x9382ff03u, 0x00040018u, 0xd7460002u, 0x04050c02u,
    0xbefe04c1u, 0xf40c0204u, 0xfa000000u, 0x34100482u, 0x7e0c0280u,
    0x8f158202u, 0xbf8cc07fu, 0xf4200506u, 0x2a000000u, 0x8816ff02u, ValueTag, 0xbf8cc07fu, 0xbf061614u, 0x85178180u, 0x7e0c0217u,
    0xbf8cc07fu, 0xe0701000u, 0x80020608u, 0xbf8c0000u,
    0xbefc0380u, 0xbf900009u, 0xbefe0480u, 0xf8000941u, 0x00000000u, 0xf80008cfu, 0x00000000u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData users{};
};

Header ScalarHeader{};
Header PixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t size, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = static_cast<std::uint32_t>(size);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.users;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Write(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    commands.insert(commands.end(), {0xc0010000u | (opcode << 8u), offset, value});
}

void Draw(const std::uint32_t* code) {
    Mismatches.fill(0xdeadbeefu);
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto geometry = reinterpret_cast<std::uintptr_t>(code);
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto table = reinterpret_cast<std::uintptr_t>(Descriptor.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x20}, {0x1ff, Threads}, {0x2ce, 3}, {0x29b, 2}, {0x2ab, 4}, {0x260, 0},
        {0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0},
        {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Height - 1u) << 14u) | (Width - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0}, {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(32.0f)}, {0x112, std::bit_cast<std::uint32_t>(32.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shaders{
        {0xc8, static_cast<std::uint32_t>(geometry >> 8u)}, {0xc9, static_cast<std::uint32_t>(geometry >> 40u)},
        {0x8a, 0x600c0000u}, {0x8b, (32u << 19u) | (3u << 16u) | (2u << 1u)},
        {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)},
        {0x8, static_cast<std::uint32_t>(pixel >> 8u)}, {0x9, static_cast<std::uint32_t>(pixel >> 40u)}, {0xb, 0}
    };
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, 4u);
    Write(commands, 0x79, 0x25b, (GroupVertices << 9u) | GroupPrimitives);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0012d00u, GroupVertices, 2u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "mesh wave submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Mismatches.data()), sizeof(Mismatches));
}

struct FunctionControls {
    std::uint32_t helpers = 0;
    std::uint32_t notInlined = 0;
};

FunctionControls HelperFunctions(const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> code) {
    const auto table = reinterpret_cast<std::uintptr_t>(Descriptor.data());
    std::vector<std::uint32_t> userData(12, 0u);
    userData[8] = static_cast<std::uint32_t>(table);
    userData[9] = static_cast<std::uint32_t>(table >> 32u);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        {table, std::as_bytes(std::span(Descriptor))}
    }};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Mesh, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        target,
        {0, 0, 0, ShaderRecompiler::MeshDrawPushOffsetBytes},
        ShaderRecompiler::GraphicsCompileContext{0, {}, ShaderRecompiler::MeshConfiguration{4u, GroupPrimitives, GroupVertices, GroupVertices, GroupPrimitives, Threads, 4096u, 0u, 4u}, std::nullopt, {0u, 0u, 0u, 1u}}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const std::span<const std::uint32_t> words(result.spirv.Words());
    std::vector<std::uint32_t> entries;
    FunctionControls controls;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpEntryPoint) entries.push_back(words[at + 2]);
        if (op != spv::OpFunction || std::ranges::find(entries, words[at + 2]) != entries.end()) continue;
        ++controls.helpers;
        if ((words[at + 3] & spv::FunctionControlInlineMask) == 0u) ++controls.notInlined;
    }
    return controls;
}

void Check(const std::string& name) {
    std::array<std::uint32_t, Waves> wrong{};
    std::string first;
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        if (Mismatches[thread] == 0u) continue;
        ++wrong[thread / WaveSize];
        if (first.empty()) first = "thread " + std::to_string(thread) + " read " + std::to_string(Mismatches[thread]) + " values wrong";
    }
    if (first.empty()) return;
    std::string summary = first + "; wrong threads per wave:";
    for (const auto count : wrong) summary += " " + std::to_string(count);
    Require(false, name + ": " + summary);
}

}

int main() {
    try {
        const auto address = reinterpret_cast<std::uintptr_t>(Mismatches.data());
        const auto values = reinterpret_cast<std::uintptr_t>(Values.data());
        for (std::uint32_t wave = 0; wave < Waves; ++wave) Values[wave] = ValueTag | wave;
        Descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Mismatches)), 0x31016facu,
                      static_cast<std::uint32_t>(values), static_cast<std::uint32_t>((values >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Values)), 0x31016facu};
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (!TargetHasCapability(device->Target(), spv::CapabilityMeshShadingEXT)) {
                std::puts("skipped, the device has no VK_EXT_mesh_shader");
                return VulkanTestSkipped;
            }
            if (device->DeviceName().starts_with("llvmpipe")) {
                std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
                return VulkanTestSkipped;
            }
            const bool inlineExpected = device->DriverId() == VK_DRIVER_ID_NVIDIA_PROPRIETARY && std::getenv("APS5_NO_MESH_INLINE_FUNCTIONS") == nullptr;
            Require(device->Target().meshInlineFunctions == inlineExpected, std::string("the device target ") + (inlineExpected ? "does not ask" : "asks") + " for inlined mesh helper functions on Vulkan driver " + std::to_string(static_cast<int>(device->DriverId())) + "; NVIDIA's proprietary driver needs them unless APS5_NO_MESH_INLINE_FUNCTIONS is set, and other drivers keep the calls");
            if (device->Target().meshInlineFunctions) {
                const auto controls = HelperFunctions(device->Target(), ScalarCode);
                Require(controls.helpers != 0u, "a wave64 mesh program's scalar buffer load through a descriptor read from memory calls no helper function, so the scalar load draw does not cover helper calls");
                Require(controls.notInlined == 0u, "a mesh program on this driver keeps " + std::to_string(controls.notInlined) + " of " + std::to_string(controls.helpers) + " helper functions without the Inline control");
            }
        }
        Register(ScalarHeader, ScalarCode.data(), sizeof(ScalarCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        for (std::uint32_t pass = 0; pass < 4; ++pass) {
            Draw(ScalarCode.data());
            Check("mesh scalar buffer load in each wave");
        }
        AgcDriverShutdown_nid_postfix();
        std::cout << "mesh wave tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
