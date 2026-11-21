#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64u;
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) std::array<std::byte, 256> vertices{};
alignas(256) std::array<std::uint16_t, 4> indices{0, 1, 2, 0};
alignas(256) std::array<std::uint32_t, 8> indirectRecords{};
constexpr std::array<std::array<float, 4>, 3> positions{{{-1.0f, -1.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}}};
constexpr std::array<std::uint32_t, 11> vertexCode{0xf4080100u, 0xfa000000u, 0x4a0a0a02u, 0x4a0a0b08u, 0x4a0a0a03u, 0xe00c2000u, 0x80010005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> pixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

struct Scene {
    AgcDriver::VulkanDevice& device;
    std::array<std::uint32_t, 4> descriptor{};
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
    AgcDriver::Graphics::State state{};
    AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};

    explicit Scene(AgcDriver::VulkanDevice& device) : device(device) {
        constexpr std::uint32_t stride = 16u;
        constexpr std::uint32_t offset = 1u;
        const auto tableAddress = reinterpret_cast<std::uintptr_t>(descriptor.data());
        const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), offset, 1u};
        const auto firstRecord = offset + userData[3] + 2u;
        for (std::size_t index = 0; index < positions.size(); ++index) std::memcpy(vertices.data() + (firstRecord + index) * stride, positions[index].data(), 16u);
        const auto address = reinterpret_cast<std::uintptr_t>(vertices.data());
        descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), 8u, 0x01000facu | (77u << 12u)};
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{tableAddress, std::as_bytes(std::span(descriptor))}}};
        ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
        vertexInfo.fetchEmbedded = true;
        vertexInfo.resourcesNum = 1u;
        vertexInfo.resourcesDst[0] = {0, 4, 0, 0};
        ShaderRecompiler::RecompileRequest request{{ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}}, {32u, 0u, userData, std::nullopt, std::nullopt, vertexInfo, memory}, device.Target(), {0u, 0u, 0u, 96u}};
        request.context.vertex->resources[0].fields = descriptor;
        vertex = ShaderRecompiler::Recompile(request);
        const auto pushBytes = static_cast<std::uint32_t>(vertex.pushConstants.size());
        ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
        pixelInfo.wave32 = true;
        pixelInfo.targetOutputMode[0] = 9u;
        pixelInfo.targetExportMapping.fill(0xe4u);
        ShaderRecompiler::RecompileRequest pixelRequest{{ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}}, {32u, 0u, {}, std::nullopt, pixelInfo, std::nullopt, {}}, device.Target(), PixelPushLayout(pushBytes, device.Target())};
        pixel = ShaderRecompiler::Recompile(pixelRequest);
        shaders = {{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(pushBytes, device.Target())}}};
        state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
        state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
        state.colors = {state.color};
        state.hasColorTarget = true;
        state.renderExtent = {Extent, Extent};
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
        state.scissor = {{0, 0}, {Extent, Extent}};
        state.cullMode = VK_CULL_MODE_NONE;
        state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        state.blend.colorWriteMask = 15u;
        state.blends = {state.blend};
        draw.firstVertex = 1u;
        draw.firstInstance = 1u;
    }

    void Draw(const AgcDriver::Graphics::PreparedDraw* prepared, const AgcDriver::Pm4::DrawParameters& parameters, bool drawn, const char* what) {
        pixels.fill(std::byte{0});
        device.Draw(state, parameters, shaders, {}, nullptr, nullptr, 0, prepared);
        device.WaitIdle();
        const auto red = pixels[0] == std::byte{255} && pixels[1] == std::byte{0} && pixels[2] == std::byte{0} && pixels[3] == std::byte{255};
        const auto untouched = pixels[0] == std::byte{0} && pixels[3] == std::byte{0};
        Require(drawn ? red : untouched, std::string("prepared draw: ") + what + (drawn ? " did not draw the triangle" : " drew although it draws nothing"));
    }
};

std::string Counts(const AgcDriver::Graphics::PreparedDrawCounts& counts) {
    return "planned " + std::to_string(counts.planned) + ", failed " + std::to_string(counts.failed) + ", used " + std::to_string(counts.used) + ", other device " + std::to_string(counts.otherDevice) + ", inline " + std::to_string(counts.inlinePlans);
}

std::string VerifyCounts(const AgcDriver::Graphics::DrawPlanVerifyCounts& counts) {
    return "selected " + std::to_string(counts.selectedPrepared) + " prepared / " + std::to_string(counts.selectedInline) + " inline, taken " + std::to_string(counts.takenPrepared) + " / " + std::to_string(counts.takenInline) + ", after record-time work " + std::to_string(counts.afterRecordWorkPrepared) + " / " + std::to_string(counts.afterRecordWorkInline);
}

AgcDriver::Graphics::DrawPlanVerifyCounts VerifyDelta(const AgcDriver::Graphics::DrawPlanVerifyCounts& before) {
    auto now = AgcDriver::Graphics::ReadDrawPlanVerifyCounts();
    now.selectedPrepared -= before.selectedPrepared;
    now.selectedInline -= before.selectedInline;
    now.takenPrepared -= before.takenPrepared;
    now.takenInline -= before.takenInline;
    now.afterRecordWorkPrepared -= before.afterRecordWorkPrepared;
    now.afterRecordWorkInline -= before.afterRecordWorkInline;
    return now;
}

AgcDriver::Pm4::DrawParameters::IndirectDraw IndirectRecords(std::uint32_t count) {
    return {reinterpret_cast<std::uintptr_t>(indirectRecords.data()), 0x24u, 16u, 16u, count, false, 0u, 0x280u, 0x280u, 0x280u, false, 0u};
}

void BadPlansOnEarlyReturnsAreCaught(AgcDriver::VulkanDevice& device, Scene& scene) {
    auto indexed = scene.draw;
    indexed.indexed = true;
    indexed.indexSize = 2u;
    indexed.indexAddress = reinterpret_cast<std::uintptr_t>(indices.data());
    indexed.firstVertex = 0u;
    struct Case {
        const char* what;
        std::uint32_t count;
    };
    for (const Case& early : {Case{"an indirect draw without records", 0u}, Case{"an indirect draw whose records draw nothing", 1u}}) {
        auto indirect = scene.draw;
        indirect.indirect = IndirectRecords(early.count);
        indirectRecords.fill(0u);
        static_cast<void>(AgcDriver::Graphics::TakePreparedDrawCounts());
        scene.Draw(nullptr, indirect, false, early.what);
        auto prepared = device.PrepareDraw(scene.state, indexed, scene.shaders, true);
        Require(prepared != nullptr, std::string("prepared draw: the indexed draw whose plan is given to ") + early.what + " was not prepared");
        const auto before = AgcDriver::Graphics::ReadDrawPlanVerifyCounts();
        std::string verifyError;
        try {
            scene.Draw(prepared.get(), indirect, false, early.what);
        } catch (const std::exception& error) {
            verifyError = error.what();
        }
        Require(verifyError.find("APS5_VERIFY_DRAW_PLAN") != std::string::npos && verifyError.find("when it took it") != std::string::npos, std::string("prepared draw: ") + early.what + " given the plan of an indexed draw was not caught when it took the plan (got '" + verifyError + "')");
        const auto delta = VerifyDelta(before);
        Require(delta.selectedPrepared == 1 && delta.takenPrepared == 0 && delta.selectedInline == 0 && delta.afterRecordWorkPrepared == 0, std::string("prepared draw: the verifier did not count the wrong plan given to ") + early.what + " as selected and unconfirmed (" + VerifyCounts(delta) + ")");
    }
    static_cast<void>(AgcDriver::Graphics::TakePreparedDrawCounts());
}

void Run(AgcDriver::VulkanDevice& device) {
    Scene scene(device);
    static_cast<void>(AgcDriver::Graphics::TakePreparedDrawCounts());
    const bool verify = std::getenv("APS5_VERIFY_DRAW_PLAN") != nullptr;

    auto verifyBefore = AgcDriver::Graphics::ReadDrawPlanVerifyCounts();
    auto prepared = device.PrepareDraw(scene.state, scene.draw, scene.shaders, true);
    Require(prepared != nullptr, "prepared draw: a valid draw was not prepared");
    scene.Draw(prepared.get(), scene.draw, true, "a draw with its plan");
    auto counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.planned == 1 && counts.used == 1 && counts.inlinePlans == 0 && counts.otherDevice == 0, "prepared draw: the committer did not take the plan (" + Counts(counts) + ")");

    scene.Draw(nullptr, scene.draw, true, "a draw that was not prepared");
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.inlinePlans == 1 && counts.used == 0 && counts.planned == 0, "prepared draw: a draw that was not prepared was not planned by the committer (" + Counts(counts) + ")");
    const auto verified = VerifyDelta(verifyBefore);
    const std::uint64_t expected = verify ? 1u : 0u;
    Require(verified.selectedPrepared == expected && verified.takenPrepared == expected && verified.afterRecordWorkPrepared == expected && verified.selectedInline == expected && verified.takenInline == expected && verified.afterRecordWorkInline == expected, "prepared draw: the plan verifier did not count one prepared and one inline plan through both checks (" + VerifyCounts(verified) + ")");

    prepared = device.PrepareDraw(scene.state, scene.draw, scene.shaders, false);
    Require(prepared != nullptr, "prepared draw: a draw prepared without a plan was not prepared");
    scene.Draw(prepared.get(), scene.draw, true, "a draw prepared without a plan");
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.planned == 0 && counts.failed == 0 && counts.inlinePlans == 1 && counts.used == 0 && counts.otherDevice == 0, "prepared draw: a draw prepared without a plan was not planned by the committer (" + Counts(counts) + ")");

    prepared = device.PrepareDraw(scene.state, scene.draw, scene.shaders, true);
    AgcDriver::Graphics::SetPreparedDrawDeviceForTest(*prepared, VK_NULL_HANDLE);
    scene.Draw(prepared.get(), scene.draw, true, "a draw prepared for another device");
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.otherDevice == 1 && counts.inlinePlans == 1 && counts.used == 0, "prepared draw: a plan for another device was taken (" + Counts(counts) + ")");

    auto empty = scene.draw;
    empty.indexCount = 0u;
    prepared = device.PrepareDraw(scene.state, empty, scene.shaders, true);
    Require(prepared != nullptr, "prepared draw: an empty draw was not prepared");
    scene.Draw(prepared.get(), empty, false, "an empty draw with its plan");
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.planned == 1 && counts.used == 1 && counts.inlinePlans == 0, "prepared draw: the plan of an empty draw was not taken (" + Counts(counts) + ")");

    if (std::getenv("APS5_VERIFY_DRAW_PLAN") != nullptr) {
        prepared = device.PrepareDraw(scene.state, empty, scene.shaders, true);
        std::string verifyError;
        try {
            scene.Draw(prepared.get(), scene.draw, true, "a draw given the plan of an empty draw");
        } catch (const std::exception& error) {
            verifyError = error.what();
        }
        Require(verifyError.find("APS5_VERIFY_DRAW_PLAN") != std::string::npos, "prepared draw: a draw given the empty plan of another draw was not caught by the plan verifier (got '" + verifyError + "')");
        counts = AgcDriver::Graphics::TakePreparedDrawCounts();
        Require(counts.planned == 1 && counts.used == 1 && counts.inlinePlans == 0, "prepared draw: the wrongly empty plan was not the one the committer took (" + Counts(counts) + ")");
        BadPlansOnEarlyReturnsAreCaught(device, scene);
    }

    auto modified = scene.draw;
    modified.flags = 1u;
    prepared = device.PrepareDraw(scene.state, modified, scene.shaders, true);
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(prepared != nullptr && counts.failed == 1 && counts.planned == 0, "prepared draw: an invalid draw was planned instead of refused (" + Counts(counts) + ")");
    std::string inlineError;
    try {
        scene.Draw(prepared.get(), modified, false, "an invalid draw");
    } catch (const std::exception& error) {
        inlineError = error.what();
    }
    Require(inlineError.find("draw modifiers are unsupported") != std::string::npos, "prepared draw: a refused plan did not leave the committer to report the draw's error (got '" + inlineError + "')");
    counts = AgcDriver::Graphics::TakePreparedDrawCounts();
    Require(counts.inlinePlans == 1 && counts.used == 0, "prepared draw: the committer did not plan the refused draw itself (" + Counts(counts) + ")");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        std::cout << "prepared draw tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
