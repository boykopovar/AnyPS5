#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "HeapSampleKernel.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 10> PixelCode{0xf4000100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0x06000002u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<float, 4> PixelConstant{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
constexpr std::uint32_t Rounds = 8;
alignas(256) std::array<std::uint8_t, 1024> TextureMemory{};
alignas(4096) std::array<std::array<float, 4>, Rounds> Outputs{};
alignas(256) std::array<std::uint64_t, 32 * Rounds> SampleMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> Setup() {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 3u << 1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    const auto table = reinterpret_cast<std::uintptr_t>(PixelConstant.data());
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(table)});
    Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(table >> 32u)});
    Append(commands, 0x76, {0x00e, std::bit_cast<std::uint32_t>(0.25f)});
    return commands;
}

void SetShaderRegister(std::vector<std::uint32_t>& commands, std::uint32_t offset, std::uint32_t value) {
    Append(commands, 0x76, {offset, value});
}

void Sample(std::vector<std::uint32_t>& commands, float* output) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    SetShaderRegister(commands, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(commands, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(commands, offset, 1);
    SetShaderRegister(commands, 0x212, 0);
    SetShaderRegister(commands, 0x213, 16u << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : SampleUserData(TextureMemory.data(), output)) SetShaderRegister(commands, offset++, value);
    Append(commands, 0x15, {1, 1, 1, 0x8041u});
}

void DumpSamples(std::vector<std::uint32_t>& commands, std::uint32_t round) {
    const auto address = reinterpret_cast<std::uintptr_t>(SampleMemory.data() + 32u * round);
    Append(commands, 0x46, {AgcDriver::Pm4::SampleDumpEvent | (1u << 8u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)});
}

}

int main() {
    try {
        bool libraries = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            libraries = device->GraphicsPipelineLibraries();
        }
        if (!libraries && std::getenv("APS5_NO_GPL") == nullptr) return VulkanTestSkipped;
        using AgcDriver::DriverDetail::DrawFinisher;
        const bool finisher = DrawFinisher::Enabled() && DrawFinisher::Overlap();
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        PixelConstant[0] = 0.25f;
        FillTexels(TextureMemory, SampleTexel);
        for (auto& output : Outputs) output.fill(-1.0f);
        std::string drains;
        for (std::uint32_t pass = 0; pass < 2; ++pass) {
            static_cast<void>(DrawFinisher::DrainText());
            auto commands = Setup();
            for (std::uint32_t round = 0; round < Rounds; ++round) {
                Append(commands, 0x2d, {3, 2});
                Sample(commands, Outputs[round].data());
                Append(commands, 0x2d, {3, 2});
                DumpSamples(commands, round);
            }
            Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
            Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
            AgcDriverWaitIdle_nid_postfix();
            drains = DrawFinisher::DrainText();
        }
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Outputs.data()), sizeof(Outputs));
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
        for (std::uint32_t round = 0; round < Rounds; ++round) RequireSampled(Outputs[round].data(), SampleTexel, "the dispatch after finisher draws in round " + std::to_string(round));
        unsigned brightest = 0;
        for (const auto value : ColorMemory) brightest = std::max(brightest, static_cast<unsigned>(std::to_integer<std::uint8_t>(value)));
        Require(brightest + 2 >= 128 && brightest <= 130, "the draws around the dispatches wrote " + std::to_string(brightest) + ", expected 128");
        if (finisher) Require(drains.find(" 0x15 x") == std::string::npos, "a pipelined dispatch after a draw handed to the finisher drained the finisher:" + drains);
        if (finisher) Require(drains.find(" 0x46 x") == std::string::npos, "a sample dump after a draw handed to the finisher drained the finisher:" + drains);
        AgcDriverShutdown_nid_postfix();
        std::cout << "dispatches and sample dumps after finisher draws " << (finisher ? "queued through the finisher" : "run in order") << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
