#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
namespace LeanFront = AgcDriver::DriverDetail::LeanFront;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 10> PixelCode{0xf4000100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0x06000002u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<float, 4> PixelConstant{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
constexpr std::uint32_t Segments = 8;
alignas(256) std::array<std::uint32_t, Segments> LabelMemory{};
alignas(16) std::array<std::uint32_t, 4> TargetMaskPairs{0x8e, 0xf, 0x8f, 0xf};
alignas(16) std::array<std::uint32_t, 2> TargetMaskValues{0xf, 0xf};
alignas(16) std::array<std::uint32_t, 2> PixelUserPairs{0x00e, std::bit_cast<std::uint32_t>(0.25f)};
alignas(16) std::array<std::uint32_t, 2> PrimitivePairs{0x242, 4};

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

template <std::size_t TWords>
void List(std::vector<std::uint32_t>& commands, std::uint32_t opcode, const std::array<std::uint32_t, TWords>& words, std::uint32_t mode, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(words.data());
    Append(commands, opcode, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), mode, count});
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

void WriteLabel(std::vector<std::uint32_t>& commands, std::uint32_t index, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(&LabelMemory[index]);
    commands.insert(commands.end(), {0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value});
}

void WaitLabel(std::vector<std::uint32_t>& commands, std::uint32_t index, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(&LabelMemory[index]);
    commands.insert(commands.end(), {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0xffffffffu, 0x19u});
}

std::vector<std::uint32_t> Chain() {
    auto commands = Setup();
    List(commands, 0x9f, TargetMaskPairs, 0x80000000u, 2);
    Append(commands, 0x59, {0x80000000u});
    for (std::uint32_t index = 0; index < Segments; ++index) {
        List(commands, 0x9f, TargetMaskPairs, 0x80000000u, 2);
        List(commands, 0x9f, TargetMaskValues, 0x8eu, 2);
        List(commands, 0x63, PixelUserPairs, 0x80000000u, 1);
        List(commands, 0x64, PrimitivePairs, 0x80000000u, 1);
        Append(commands, 0x2d, {3, 2});
        WriteLabel(commands, index, 0x100u + index);
        WaitLabel(commands, index, 0x100u + index);
        if ((index & 1u) != 0) Append(commands, 0x46, {0x407});
        if (index + 1 < Segments) Append(commands, 0x59, {0x80000000u});
    }
    return commands;
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
        using AgcDriver::DriverDetail::DrawPipeline;
        using AgcDriver::Graphics::RuntimeSwitch;
        namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;
        const bool finisher = DrawFinisher::Enabled() && DrawFinisher::Overlap();
        const bool latched = DrawPipeline::Depth() != 0;
        const bool environment = RuntimeSwitches::Set(RuntimeSwitch::LeanFront);
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        PixelConstant[0] = 0.25f;
        std::array<std::byte, 1024> todayColors{};
        for (const bool lean : {false, true}) {
            RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, lean);
            const std::string arm = lean ? " (lean front)" : " (today's front)";
            LabelMemory.fill(0);
            ColorMemory.fill(std::byte{0});
            static_cast<void>(DrawFinisher::DrainText());
            const auto leanBase = LeanFront::Total(LeanFront::Counter::LeanSegments);
            const auto todayBase = LeanFront::Total(LeanFront::Counter::TodaySegments);
            const auto differencesBase = LeanFront::Differences();
            const auto leanDrawsBase = LeanFront::Total(LeanFront::Counter::LeanDraws);
            const auto checkedDrawsBase = LeanFront::Checked(LeanFront::Item::Draw);
            const auto recordsBase = LeanFront::Total(LeanFront::Counter::ShapeRecordsBuilt);
            static std::vector<std::uint32_t> commands;
            commands = Chain();
            Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
            Require(sceAgcDriverSubmitDcb(&packet) == 0, "the REWIND chain submission failed" + arm);
            AgcDriverWaitIdle_nid_postfix();
            const auto drains = DrawFinisher::DrainText();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(LabelMemory.data()), sizeof(LabelMemory));
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
            for (std::uint32_t index = 0; index < Segments; ++index) {
                Require(LabelMemory[index] == 0x100u + index, "the label of REWIND segment " + std::to_string(index) + " is " + std::to_string(LabelMemory[index]) + ", expected " + std::to_string(0x100u + index) + arm);
            }
            unsigned brightest = 0;
            for (const auto value : ColorMemory) brightest = std::max(brightest, static_cast<unsigned>(std::to_integer<std::uint8_t>(value)));
            Require(brightest + 2 >= 128 && brightest <= 130, "the draws in the REWIND segments wrote " + std::to_string(brightest) + ", expected 128" + arm);
            if (!lean) todayColors = ColorMemory;
            else Require(ColorMemory == todayColors, "the lean front rendered the REWIND chain differently from today's front");
            const auto leanSegments = LeanFront::Total(LeanFront::Counter::LeanSegments) - leanBase;
            const auto todaySegments = LeanFront::Total(LeanFront::Counter::TodaySegments) - todayBase;
            const std::uint64_t expected = latched ? Segments + 1 : 0;
            Require(leanSegments == (lean ? expected : 0) && todaySegments == (lean ? 0 : expected), "the REWIND chain ran " + std::to_string(leanSegments) + " lean and " + std::to_string(todaySegments) + " of today's segments, expected " + std::to_string(expected) + " of one kind" + arm);
            Require(LeanFront::Differences() == differencesBase, "the lean front verify found differences in the REWIND chain" + arm);
            const auto leanDraws = LeanFront::Total(LeanFront::Counter::LeanDraws) - leanDrawsBase;
            const auto checkedDraws = LeanFront::Checked(LeanFront::Item::Draw) - checkedDrawsBase;
            const auto records = LeanFront::Total(LeanFront::Counter::ShapeRecordsBuilt) - recordsBase;
            if (!lean || !latched || !finisher) {
                Require(leanDraws == 0 && checkedDraws == 0, "draws took the lean draw path outside lean segments" + arm);
            } else {
                Require(leanDraws + checkedDraws == Segments, "the lean front sent " + std::to_string(leanDraws) + " draws to the finisher ring and checked " + std::to_string(checkedDraws) + ", expected all " + std::to_string(Segments) + " draws of the REWIND segments" + arm);
                Require(LeanFront::VerifyEvery() == 0 || checkedDraws != 0, "the lean front verify checked no draw of the REWIND chain" + arm);
                Require(records >= 1 && records <= 2, "the REWIND segments' draws of one shape built " + std::to_string(records) + " shape records" + arm);
            }
            if (finisher) {
                for (const char* opcode : {" 0x37 x", " 0x3c x", " 0x59 x", " 0x9f x", " 0x63 x", " 0x64 x"}) Require(drains.find(opcode) == std::string::npos, std::string("a packet inside a released REWIND segment drained the finisher:") + opcode + arm + drains);
            }
        }
        RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, environment);
        AgcDriverShutdown_nid_postfix();
        std::cout << "draws, labels, register lists and seals in " << Segments << " released REWIND segments ran alike with today's and the lean front\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
