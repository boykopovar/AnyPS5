#include "BdaTests.hpp"
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "SceShaders.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "CacheKey.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "BdaAbi.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) std::array<std::byte, 1024> colorMemory{};
alignas(256) std::array<std::byte, 2048> sliceMemory{};

AgcDriver::QueueState makeState() {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
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
        {0x94, 0x80000000}, {0x95, 0x40040}
    };
    const auto address = reinterpret_cast<std::uintptr_t>(colorMemory.data());
    queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x110] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x111] = std::bit_cast<std::uint32_t>(-2.0f);
    queue.context[0x112] = std::bit_cast<std::uint32_t>(2.0f);
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    queue.context[0xb4] = 0;
    queue.context[0xb5] = std::bit_cast<std::uint32_t>(1.0f);
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    return queue;
}

template<typename TAction>
void expectFailure(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected failure: ") + error.what());
        return;
    }
    throw std::runtime_error("expected graphics rejection: " + std::string(reason));
}

void stateTests() {
    AgcDriver::QueueState initial;
    Require(initial.context.at(0x200) == 0 && initial.context.at(0x83) == 0xffff, "initial context state is missing");
    initial.context[0x200] = 7;
    initial.context[0xdead] = 1;
    initial.ClearContext();
    Require(initial.context.at(0x200) == 0 && !initial.context.contains(0xdead), "context reset did not restore defaults");
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        const auto info = 0x31cu + 0xfu * slot;
        Require(initial.context.at(info) == 0, "reset color target was not disabled");
        initial.context[info] = 10u << 2u;
    }
    initial.ClearContext();
    initial.context[0x8e] = initial.context[0x8f] = 0xffffffffu;
    Require(AgcDriver::Graphics::ColorWriteMask(initial.context) == 0, "context clear kept old color targets enabled");
    Require(initial.userConfig.at(0x24b) == 0, "primitive restart must be disabled in initial queue state");
    initial.userConfig[0x24b] = 1;
    initial.ClearContext();
    Require(initial.userConfig.at(0x24b) == 1, "context clear must preserve user configuration");
    initial = AgcDriver::QueueState{};
    Require(initial.userConfig.at(0x24b) == 0, "queue reset must disable primitive restart");
    auto queue = makeState();
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()) && state.color.bytes == colorMemory.size(), "render-target address or size changed");
    Require(state.viewport.y == 4 && state.viewport.height == -4, "negative viewport height was lost");
    Require(state.color.format == VK_FORMAT_R8G8B8A8_UNORM, "RGBA format changed");
    queue.userConfig[0x24b] = 1;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "primitive restart rejected a non-indexed draw");
    queue.userConfig[0x242] = 9;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("point, line and triangle") != std::string::npos, "primitive restart was accepted for patches");
    queue.userConfig[0x242] = 6;
    queue.context[0x103] = 0xffffffffu;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "primitive restart was rejected for an indexed strip");
    Require(AgcDriver::Graphics::DecodeState(queue).primitiveRestart, "primitive restart was not decoded for a strip");
    queue.context[0x103] = 5;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("all ones") != std::string::npos, "a restart index other than all ones was accepted");
    queue = makeState();
    queue.context[0x293] = 0x06020000u;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("sample iteration") == std::string::npos, "per-engine primitive discard was rejected");
    queue.context[0x293] = 0x760201bcu;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("sample iteration") == std::string::npos, "an out-of-order watermark without out-of-order rasterization was rejected");
    queue.context[0x293] = 0x7e0201bcu;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("sample iteration") != std::string::npos, "out-of-order rasterization was accepted");
    queue.context[0x293] = 0x06030000u;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("sample iteration") != std::string::npos, "per-sample shading was accepted");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "sample iteration");
    queue.context[0x293] = 0;
    for (const auto disabled : {0x6000u, 0x00100000u, 0u, 0x000e6000u}) {
        queue.context[0x313] = disabled;
        Require(AgcDriver::Graphics::DecodeState(queue).conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT, "disabled conservative rasterization decoded as enabled");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("PA_SC_CONSERVATIVE") == std::string::npos, "disabled conservative rasterization was rejected");
    }
    for (const auto enabled : {0x00e00001u, 0x01e00022u, 0x000e6001u, 0x000e6020u}) {
        queue.context[0x313] = enabled;
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("PA_SC_CONSERVATIVE") != std::string::npos, "conservative rasterization was accepted");
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "PA_SC_CONSERVATIVE");
    }
    queue.context[0x313] = 0x6000u;
    queue = makeState();
    queue.userConfig.erase(0x24b);
    queue.context[0x2a5] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "user-config bank at DWORD 0x24b");
    queue = makeState();
    queue.context[0x90] = 0x80010003;
    queue.context[0x91] = 0x30020;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.scissor.offset.x == 3 && state.scissor.offset.y == 1 && state.scissor.extent.width == 29 && state.scissor.extent.height == 2, "scissor intersection changed");
    queue.context[0x90] = 0x10003;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.scissor.offset.x == 3 && state.scissor.offset.y == 1 && state.scissor.extent.width == 29 && state.scissor.extent.height == 2, "a scissor that applies the zero window offset changed");
    queue.context[0x90] = 0x80008000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "scissor reserved bits");
    queue.context[0x90] = 0x80010003;
    queue.context[0x31c] |= 0x10000000;
    queue.context[0x325] = 0x1234;
    Require(AgcDriver::Graphics::DecodeState(queue).color.dccAddress == 0x123400, "DCC key address decode changed");
    queue = makeState();
    queue.context.erase(0x3b8);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
    queue = makeState();
    queue.context[0x3b8] |= 1u << 14u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color tile mode");
    queue = makeState();
    queue.context[0x3b0] = (62u << 14u) | 3u;
    Require(AgcDriver::Graphics::DecodeState(queue).color.bytes == 64u * 4u * 4u, "padded linear pitch changed");
    queue = makeState();
    queue.context[0x8e] = 0xff;
    queue.context[0x8f] = 0xff;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "color export format 0");
    queue = makeState();
    queue.context[0x200] = 2;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "depth");
    queue = makeState();
    queue.context[0x200] = 0x007007b4;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a depth write without the depth test was rejected");
    queue = makeState();
    queue.context[0x200] = 0x007007b6;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "depth");
    queue = makeState();
    queue.context[0x010] = 0x80000180;
    queue.context[0x011] = 0x20000180;
    queue.context[0x200] = 0x007007b3;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depthTest && !state.stencilTest, "tests on absent depth and stencil planes were kept");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "tests on absent depth and stencil planes were rejected");
    queue.context[0x200] = 0x007007bbu;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depth.has_value() && !state.depthTest && !state.depthBoundsTest && !state.stencilTest, "a depth bounds test on absent depth and stencil planes was kept");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a depth bounds test on absent depth and stencil planes was rejected");
    queue.context[0x200] = 8u;
    Require(!AgcDriver::Graphics::DecodeState(queue).depthBoundsTest && AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a depth bounds test alone on absent depth and stencil planes was kept or rejected");
    queue.context[0x200] = 0x007007b3;
    queue.context[0x011] = 0x20000181;
    queue.context[0x012] = 0x00001000;
    queue.context[0x013] = 0x00002000;
    queue.context[0x015] = 0x00002000;
    queue.context[0x007] = 0x003f003f;
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x00a] = 0;
    queue.context[0x00b] = 0x3f800000;
    queue.context[0x10b] = 0;
    queue.context[0x10c] = 0x01ffff00;
    queue.context[0x10d] = 0x01ffff00;
    state =AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depthTest && state.stencilTest, "a depth test on an absent depth plane was kept beside a stencil plane");
    queue.context[0x200] = 0x007007bbu;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "depth bounds without a depth plane");
    queue = makeState();
    queue.context[0x10f] = 0x7fc00000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "non-finite");
    // The register facade: every register the decoders read is in DrawKeyRegisters (a wrong hit of
    // the draw key otherwise), and nothing is recorded without a log pointer.
    queue = makeState();
    std::vector<AgcDriver::Graphics::RegisterRead> log;
    AgcDriver::Graphics::RegisterReadLog() = &log;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "precheck rejected the reference state");
    static_cast<void>(AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state)));
    AgcDriver::Graphics::RegisterReadLog() = nullptr;
    Require(!log.empty(), "the register facade recorded nothing");
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a register the decoders read: " + std::string(AgcDriver::Graphics::RegisterBankName(read.bank)) + " " + std::to_string(read.offset));
    Require(!AgcDriver::Graphics::DrawKeyCovers({AgcDriver::Graphics::RegisterBank::Context, 0x100}) && AgcDriver::Graphics::DrawKeyCovers({AgcDriver::Graphics::RegisterBank::Shader, 0xab}) && !AgcDriver::Graphics::DrawKeyCovers({AgcDriver::Graphics::RegisterBank::Shader, 0xac}), "DrawKeyRegisters coverage changed");
    log.clear();
    static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
    Require(log.empty(), "the register facade recorded without a log");
    queue = makeState();
    queue.context.erase(0x1b3);
    queue.context.erase(0x1b4);
    queue.context.erase(0x1c5);
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "a pixel program address was read as unset");
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("missing register at DWORD 0x1b3") != std::string::npos, "a real pixel program without SPI_PS_INPUT_ENA was accepted");
    expectFailure([&] { AgcDriver::Graphics::DecodePixelStageInfo(queue.context, std::array<std::uint8_t, 8>{0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u}); }, "missing register");
    AgcDriver::QueueState cleared;
    cleared.context[0x1b3] = 2;
    cleared.context[0x1b4] = 2;
    cleared.context[0x1b6] = 2;
    cleared.context[0x192] = 7;
    const auto clearedPixel = AgcDriver::Graphics::DecodePixelStageInfo(cleared.context, std::array<std::uint8_t, 8>{0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
    Require(clearedPixel.interpolatorSettings[0] == 0 && clearedPixel.interpolatorSettings[1] == 7, "an unwritten SPI_PS_INPUT_CNTL_0 did not read as its clear-state value");
    cleared.ClearContext();
    for (std::uint32_t i = 0; i < 32; ++i) Require(cleared.context.at(0x191 + i) == 0, "CLEAR_STATE did not reset SPI_PS_INPUT_CNTL");
    queue.shader[0x008] = 0;
    Require(AgcDriver::Graphics::PixelProgramSkipped(queue), "a zero pixel program address was not read as unset");
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("writes color") != std::string::npos, "a draw without a pixel program that writes color was accepted");
    queue.shader.erase(0x008);
    queue.shader.erase(0x009);
    Require(AgcDriver::Graphics::PixelProgramSkipped(queue), "an unwritten pixel program address was not read as unset");
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("writes color") != std::string::npos, "a draw with an unwritten pixel program that writes color was accepted");
    queue.context[0x8e] = 0;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "a depth-only draw with an unwritten pixel program was rejected");
    queue.shader[0x008] = 0;
    queue.shader[0x009] = 0;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "a depth-only draw without a pixel program was rejected");
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.hasColorTarget, "a depth-only draw without a pixel program decoded a color target");
    const auto unset = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state), true);
    Require(unset.inputAddr == ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter), "unset pixel inputs of the null program did not read as PERSP_CENTER_ENA");
    for (const auto mode : unset.targetOutputMode) Require(mode == 0, "an unset SPI_SHADER_COL_FORMAT exported a color");
    queue.shader[0x008] = 0x100;
    queue.context[0x1c4] = 0;
    queue.context[0x203] = 0;
    Require(AgcDriver::Graphics::PixelProgramSkipped(queue), "a pixel program that writes nothing and cannot run was not skipped");
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "a depth-only draw whose pixel program cannot run was rejected");
    for (const auto [control, what] : {std::pair{0x400u, "EXEC_ON_NOOP"}, std::pair{0x40u, "KILL_ENABLE"}}) {
        queue.context[0x203] = control;
        Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), std::string("a pixel program with ") + what + " was skipped");
        Require(AgcDriver::Graphics::DrawRejection(queue, true).find("missing register at DWORD 0x1b3") != std::string::npos, std::string("a pixel program with ") + what + " ran without SPI_PS_INPUT_ENA");
    }
    queue.context[0x203] = 1;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "a pixel program with Z_EXPORT_ENABLE was skipped");
    queue.context[0x203] = 0;
    queue.context[0x1c4] = 1;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "a pixel program exporting depth was skipped");
    queue.context[0x1c4] = 0;
    queue.context[0x8e] = 0xf;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "a pixel program writing color was skipped");
    queue.context[0x1b3] = queue.context[0x1b4] = queue.context[0x1b6] = 0xffffffffu;
    queue.context[0x1c5] = queue.context[0x203] = 0xffffffffu;
    const auto disabled = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state), true);
    Require(disabled.interpolatorCount == 0 && disabled.inputAddr == ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter), "the null pixel program inherited stale input state");
    Require(!disabled.pixelKillEnable && !disabled.depthExportEnable && !disabled.sampleMaskExportEnable, "the null pixel program inherited stale exports");
    for (const auto mode : disabled.targetOutputMode) Require(mode == 0, "the null pixel program inherited stale color exports");
    expectFailure([&] { AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state), false); }, "input count exceeds 32");
}

VkFormatFeatureFlags srgb8Features = 0;

void srgb8FormatProperties(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format == VK_FORMAT_R8_SRGB) properties->optimalTilingFeatures = srgb8Features;
}

void setProxyVariable(const char* value) {
#ifdef _WIN32
    _putenv_s("APS5_SRGB_ATTACHMENT_PROXY", value);
#else
    if (*value == 0) unsetenv("APS5_SRGB_ATTACHMENT_PROXY");
    else setenv("APS5_SRGB_ATTACHMENT_PROXY", value, 1);
#endif
}

void srgb8TargetTests() {
    setProxyVariable("");
    auto queue = makeState();
    queue.context[0x31c] = 0x8604;
    const auto color = AgcDriver::Graphics::DecodeState(queue).color;
    Require(color.format == VK_FORMAT_R8_SRGB && color.elementBytes == 1 && color.componentMapping == 0xe4u, "the 8_SRGB color target did not decode as R8_SRGB");
    for (std::uint32_t swap = 1; swap < 4; ++swap) {
        queue.context[0x31c] = 0x8604 | (swap << 11u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color format 1 number type 6 component swap " + std::to_string(swap));
    }
    queue.context[0x31c] = 0x860c;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color format 3 number type 6 component swap 0");
    AgcDriver::Graphics::Context context{};
    context.formatProperties = srgb8FormatProperties;
    const auto proxyOn = [&](std::uintptr_t device, VkFormatFeatureFlags features, VkFormat format = VK_FORMAT_R8_SRGB) {
        srgb8Features = features;
        context.physical = reinterpret_cast<VkPhysicalDevice>(device);
        return AgcDriver::Graphics::AttachmentProxyFormat(context, format);
    };
    constexpr VkFormatFeatureFlags attachment = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr VkFormatFeatureFlags blend = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    Require(proxyOn(0x1000, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == VK_FORMAT_R8G8B8A8_SRGB, "a device without R8_SRGB attachments must render 8_SRGB through RGBA8_SRGB");
    Require(proxyOn(0x2000, attachment) == VK_FORMAT_R8G8B8A8_SRGB, "a device without R8_SRGB blending must render 8_SRGB through RGBA8_SRGB");
    Require(proxyOn(0x3000, attachment | blend) == VK_FORMAT_UNDEFINED, "a device with R8_SRGB attachments must render 8_SRGB directly");
    Require(proxyOn(0x3000, 0) == VK_FORMAT_UNDEFINED, "the attachment proxy decision is not kept per device");
    Require(proxyOn(0x4000, 0, VK_FORMAT_R8_UNORM) == VK_FORMAT_UNDEFINED && proxyOn(0x4000, 0, VK_FORMAT_R8G8_SRGB) == VK_FORMAT_UNDEFINED, "only 8_SRGB targets may render through a proxy");
    setProxyVariable("1");
    Require(proxyOn(0x5000, attachment | blend) == VK_FORMAT_R8G8B8A8_SRGB, "APS5_SRGB_ATTACHMENT_PROXY=1 did not force the proxy");
    setProxyVariable("");
}

void hardwareScreenOffsetTests() {
    auto queue = makeState();
    queue.context[0x90] = 0x80010003;
    queue.context[0x91] = 0x30020;
    const auto reference = AgcDriver::Graphics::DecodeState(queue);
    for (const auto offset : {0u, 1u, 0x10000u, 0x0020003cu, 0x01ff0000u, 0x000001ffu, 0x01ff01ffu}) {
        queue.context[0x8d] = offset;
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.viewport.x == reference.viewport.x && state.viewport.y == reference.viewport.y && state.viewport.width == reference.viewport.width && state.viewport.height == reference.viewport.height && state.viewport.minDepth == reference.viewport.minDepth && state.viewport.maxDepth == reference.viewport.maxDepth, "hardware guard-band offset changed the viewport");
        Require(state.scissor.offset.x == reference.scissor.offset.x && state.scissor.offset.y == reference.scissor.offset.y && state.scissor.extent.width == reference.scissor.extent.width && state.scissor.extent.height == reference.scissor.extent.height, "hardware guard-band offset changed the scissor");
        Require(state.renderExtent.width == reference.renderExtent.width && state.renderExtent.height == reference.renderExtent.height, "hardware guard-band offset changed the framebuffer extent");
    }
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        if (bit < 9 || (bit >= 16 && bit < 25)) continue;
        queue.context[0x8d] = 1u << bit;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits");
    }
    queue.context.erase(0x8d);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
}

void ShaderStageTests() {
    auto queue = makeState();
    for (const auto routing : {0x2000u, 0x2010u, 0x02002000u, 0x02002010u}) {
        for (const auto vertexWave32 : {false, true}) {
            for (const auto fragmentWave32 : {false, true}) {
                queue.context[0x2d5] = routing | (vertexWave32 ? 0x00400000u : 0u);
                queue.context[0x1b6] = fragmentWave32 ? 0x8000u : 0u;
                const auto state = AgcDriver::Graphics::DecodeState(queue);
                Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Vertex, "vertex routing changed");
                Require(state.stages.vertexWaveSize == (vertexWave32 ? 32u : 64u), "incorrect vertex wave size");
                Require(state.stages.fragmentWaveSize == (fragmentWave32 ? 32u : 64u), "incorrect fragment wave size");
            }
        }
    }
    queue = makeState();
    queue.context[0x2d5] = 0x2020;
    queue.userConfig[0x25b] = (64u << 9u) | 21u;
    queue.context[0x1ff] = 64;
    queue.context[0x2ce] = 3;
    queue.context[0x29b] = 2;
    queue.context[0x2ab] = 4;
    queue.shader[0x8a] = 3u << 29u;
    queue.shader[0x8b] = 3u << 16u;
    auto stages = AgcDriver::Graphics::DecodeState(queue).stages;
    Require(stages.path == AgcDriver::Graphics::ShaderPath::Geometry && stages.mesh && stages.mesh->primitivesPerGroup == 21 && stages.mesh->verticesPerGroup == 63, "geometry assembly changed");
    Require(stages.mesh->maxVertices == 64 && stages.mesh->maxPrimitives == 21 && stages.mesh->threadsPerGroup == 64 && stages.mesh->esgsItemSize == 4, "geometry subgroup outputs changed");
    {
        auto fan = makeState();
        fan.userConfig[0x242] = 5;
        fan.userConfig[0x24b] = 1;
        fan.context[0x103] = 0xffffffffu;
        fan.context[0x2d5] = 0x2030;
        fan.userConfig[0x25b] = 0x4020;
        fan.context[0x1ff] = 256;
        fan.context[0x2ce] = 8;
        fan.context[0x29b] = 2;
        fan.context[0x2ab] = 4;
        fan.shader[0x8a] = 3u << 29u;
        fan.shader[0x8b] = 3u << 16u;
        fan.context[0x1b3] = 2;
        fan.context[0x1b4] = 2;
        const auto state = AgcDriver::Graphics::DecodeState(fan);
        Require(AgcDriver::Graphics::DrawRejection(fan, true).empty(), "the precheck rejected an indexed triangle fan with restart into a geometry shader");
        Require(state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN && state.primitiveRestart && state.stages.path == AgcDriver::Graphics::ShaderPath::Geometry && state.stages.mesh, "a triangle fan did not decode as geometry input");
        const auto& mesh = *state.stages.mesh;
        Require(mesh.inputPrimitive == 5 && mesh.primitivesPerGroup == 30 && mesh.verticesPerGroup == 32 && mesh.maxVertices == 256 && mesh.maxPrimitives == 192 && mesh.threadsPerGroup == 256 && mesh.esgsItemSize == 4, "triangle fan subgroup assembly changed");
        fan.userConfig[0x25b] = (3u << 9u) | 3u;
        Require(AgcDriver::Graphics::DecodeState(fan).stages.mesh->primitivesPerGroup == 1, "a three-vertex subgroup did not take one fan triangle");
        fan.userConfig[0x25b] = (2u << 9u) | 3u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fan); }, "invalid geometry subgroup");
        fan.userConfig[0x25b] = 0x4020;
        fan.userConfig[0x242] = 3;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fan); }, "unsupported geometry input or output assembly");
    }
    queue.context[0x2ab] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "invalid VGT_ESGS_RING_ITEMSIZE");
    queue.context[0x2ab] = 4;
    queue.userConfig[0x25b] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "invalid geometry subgroup");
    queue = makeState();
    queue.context[0x2d5] = 0x200d;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "Patch topology and HS_EN disagree");
    queue.userConfig[0x242] = 9;
    queue.context[0x2d6] = (3u << 8u) | (3u << 14u);
    queue.context[0x2db] = 1u | (2u << 2u) | (2u << 5u);
    stages = AgcDriver::Graphics::DecodeState(queue).stages;
    Require(stages.path == AgcDriver::Graphics::ShaderPath::Tessellation && stages.tessellation && stages.tessellation->inputControlPoints == 3, "tessellation routing changed");
    queue.context[0x2d5] = 0x202d;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "combined tessellation and geometry");
    queue.context[0x2d5] = 0x200d;
    queue.context[0x2d6] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "control-point counts");
    queue = makeState();
    for (const auto value : {0x2003u, 0x2018u, 0x20c0u, 0x80002000u}) {
        queue.context[0x2d5] = value;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "reserved");
    }
    for (const auto bit : {1u, 8u, 0x40u, 0x100u, 0x200u, 0x400u, 0x1000u, 0x4000u, 0x80000u, 0x200000u, 0x800000u, 0x1000000u}) {
        queue.context[0x2d5] = 0x2000u | bit;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported vertex");
    }
    queue.context[0x2d5] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "without PRIMGEN_EN");
    queue.context.erase(0x2d5);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
    queue.context[0x2d5] = 0x2000;
    queue.context.erase(0x1b6);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
}

constexpr std::array<std::uint8_t, 8> IdentityExports{0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u};

std::vector<spv::BuiltIn> pixelBuiltinsRead(std::uint32_t ena, std::uint32_t addr, std::uint32_t source) {
    auto queue = makeState();
    queue.context[0x1b3] = ena;
    queue.context[0x1b4] = addr;
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    const std::array<std::uint32_t, 3> code{0xf800180fu, source * 0x01010101u, 0xbf810000u};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, spv::BuiltIn> builtins;
    std::vector<spv::BuiltIn> read;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn) builtins[words[at + 1]] = static_cast<spv::BuiltIn>(words[at + 3]);
        if (op != spv::OpLoad && op != spv::OpAccessChain && op != spv::OpInBoundsAccessChain) continue;
        const auto found = builtins.find(words[at + 3]);
        if (found != builtins.end() && std::find(read.begin(), read.end(), found->second) == read.end()) read.push_back(found->second);
    }
    return read;
}

bool readsBuiltin(const std::vector<spv::BuiltIn>& read, spv::BuiltIn builtin) {
    return std::find(read.begin(), read.end(), builtin) != read.end();
}

std::vector<std::uint32_t> pixelNoPerspectiveLocations(bool barycentricEnabled) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x22u;
    queue.context[0x1b4] = 0x22u;
    queue.context[0x1b6] = 2u;
    queue.context[0x191] = 0u;
    queue.context[0x192] = 1u;
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    const std::array<std::uint32_t, 8> code{0xc8100000u, 0xc8110001u, 0xc8140402u, 0xc8150403u, 0xf800180fu, 0x05040504u, 0xbf810000u, 0xbf810000u};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = barycentricEnabled;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> locations;
    std::vector<std::uint32_t> noPerspective;
    std::uint32_t perVertex = 0;
    bool perspectiveBarycentrics = false;
    bool linearBarycentrics = false;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpDecorate) continue;
        if (words[at + 2] == spv::DecorationLocation) locations[words[at + 1]] = words[at + 3];
        if (words[at + 2] == spv::DecorationNoPerspective) noPerspective.push_back(words[at + 1]);
        if (words[at + 2] == spv::DecorationPerVertexKHR) perVertex++;
        if (words[at + 2] == spv::DecorationBuiltIn) {
            perspectiveBarycentrics |= words[at + 3] == spv::BuiltInBaryCoordKHR;
            linearBarycentrics |= words[at + 3] == spv::BuiltInBaryCoordNoPerspKHR;
        }
    }
    if (barycentricEnabled) Require(perVertex == 2u && perspectiveBarycentrics && linearBarycentrics, "explicit interpolation must preserve both parameter vertices and both barycentric inputs");
    std::vector<std::uint32_t> result2;
    for (const auto id : noPerspective) result2.push_back(locations.count(id) != 0 ? locations.at(id) : 0xffffffffu);
    return result2;
}

void ComputeScratchTests() {
    auto queue = makeState();
    auto& shader = queue.shader;
    shader[0x207] = 64u;
    shader[0x208] = 1u;
    shader[0x209] = 1u;
    shader[0x213] = 0x1u;
    std::vector<std::byte> header(sizeof(Shader));
    Shader agc{};
    agc.scratch_size_dw_per_thread = 24;
    std::memcpy(header.data(), &agc, sizeof(Shader));
    const auto compute = AgcDriver::Graphics::DecodeComputeStageInfo(shader, header);
    Require(compute.scratchDwords == 24u, "SCRATCH_EN did not take the AGC header's per-thread scratch size");
    agc.scratch_size_dw_per_thread = 0;
    std::memcpy(header.data(), &agc, sizeof(Shader));
    expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeComputeStageInfo(shader, header)); }, "zero scratch size");
    shader[0x213] = 0u;
    Require(AgcDriver::Graphics::DecodeComputeStageInfo(shader, {}).scratchDwords == 0u, "a dispatch without SCRATCH_EN got scratch");
    ShaderRecompiler::RecompileRequest request{};
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    request.shader = {ShaderRecompiler::ShaderStage::Compute, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.compute = compute;
    const ShaderRecompiler::RequestSerializer serializer;
    const auto back = serializer.Deserialize(serializer.Serialize(request));
    Require(back.request.context.compute->scratchDwords == 24u, "the compute scratch size did not survive serialization");
}

void shaderUserDataTailPaddingTests() {
    constexpr std::size_t userDataOffset = 288;
    constexpr auto userDataBytes = offsetof(ShaderUserData, sharp_resource_count) + sizeof(ShaderUserData{}.sharp_resource_count);
    static_assert(userDataBytes == 54);
    std::vector<std::byte> header(userDataOffset + userDataBytes);
    Shader shader{};
    shader.user_data = reinterpret_cast<ShaderUserData*>(header.data() + userDataOffset);
    std::memcpy(header.data(), &shader, sizeof(shader));
    const auto headerAddress = reinterpret_cast<std::uintptr_t>(header.data());
    const auto info = AgcDriver::Graphics::DecodeVertexStageInfo(header, headerAddress, {}, nullptr, true);
    Require(!info.fetchEmbedded, "a ShaderUserData block without trailing struct padding was rejected");
}

void ColorPipeBankXorTests() {
    constexpr std::uint32_t side = 128;
    constexpr std::size_t blockBytes = 65536;
    static std::vector<std::byte> storage(2 * blockBytes + side * side * 4);
    const auto block = (reinterpret_cast<std::uintptr_t>(storage.data()) + blockBytes - 1) / blockBytes * blockBytes;
    auto queue = makeState();
    queue.context[0x3b8] |= static_cast<std::uint32_t>(AgcDriver::Graphics::ColorTileMode::RenderTarget) << 14u;
    queue.context[0x3b0] = ((side - 1u) << 14u) | (side - 1u);
    for (const auto offset : {0xdu, 0x82u, 0x91u, 0x95u}) queue.context[offset] = (side << 16u) | side;
    for (const std::uint32_t pipeBankXor : {0u, 0x5600u, 0xff00u}) {
        const auto address = block + pipeBankXor;
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        const auto color = AgcDriver::Graphics::DecodeState(queue).color;
        Require(color.address == block && color.surfaceAddress == block && color.pipeBankXor == pipeBankXor, "a SW_64KB_R_X color base did not split into its block base and pipe/bank XOR " + std::to_string(pipeBankXor));
    }
    queue.context[0x3b8] = 0x9000000u | (0x16u << 14u) | (0x14u << 19u);
    for (const std::uint32_t pipeBankXor : {0u, 0xa00u, 0xf00u}) {
        const auto address = block + 0x3000u + pipeBankXor;
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        const auto color = AgcDriver::Graphics::DecodeState(queue).color;
        Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::D4KBX && color.address == block + 0x3000u && color.pipeBankXor == pipeBankXor, "a SW_4KB_D_X color base did not split into its 4 KiB block base and pipe/bank XOR " + std::to_string(pipeBankXor));
    }
}

void PixelInputLayoutTests() {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputVgpr;
    auto queue = makeState();
    const auto decode = [&](std::uint32_t ena, std::uint32_t addr) {
        queue.context[0x1b3] = ena;
        queue.context[0x1b4] = addr;
        return AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    };
    auto pixel = decode(0x326u, 0x326u);
    Require(pixel.inputAddr == 0x326u && pixel.hasPerspectiveCenterVgpr && pixel.perspectiveCentroid && pixel.noPerspective && !pixel.linearCentroid && pixel.posX && pixel.posY && !pixel.posZ, "the centroid input flags were not decoded");
    Require(PixelInputVgpr(pixel.inputAddr, PixelInput::PerspectiveCenter) == 0u && PixelInputVgpr(pixel.inputAddr, PixelInput::PerspectiveCentroid) == 2u && PixelInputVgpr(pixel.inputAddr, PixelInput::LinearCenter) == 4u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionX) == 6u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionY) == 7u, "the centroid layout moved the inputs");
    pixel = decode(0x1146u, 0x1146u);
    Require(pixel.linearCentroid && !pixel.noPerspective && PixelInputVgpr(pixel.inputAddr, PixelInput::LinearCentroid) == 4u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionX) == 6u && PixelInputVgpr(pixel.inputAddr, PixelInput::FrontFace) == 7u, "the linear centroid layout moved the inputs");
    pixel = decode(0x506u, 0x7afu);
    Require(pixel.inputAddr == 0x7afu && !pixel.posY && pixel.posZ && PixelInputVgpr(pixel.inputAddr, PixelInput::PerspectiveCentroid) == 4u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionX) == 12u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionZ) == 14u, "ADDR-only inputs did not reserve their VGPRs");
    for (const auto bit : {0x8u, 0x4000u}) {
        expectFailure([&] { static_cast<void>(decode(0x2u | bit, 0x2u | bit)); }, "unsupported SPI_PS_INPUT_ENA/ADDR");
    }
    pixel = decode(0x80u, 0x80u);
    Require(pixel.inputAddr == 0x80u && PixelInputVgpr(pixel.inputAddr, PixelInput::LineStipple) == 0u, "a pixel shader with only LINE_STIPPLE_TEX enabled was rejected");
    pixel = decode(0x8082u, 0x8082u);
    Require(PixelInputVgpr(pixel.inputAddr, PixelInput::LineStipple) == 2u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionFixedPoint) == 3u, "LINE_STIPPLE_TEX and POS_FIXED_PT did not take their VGPRs after the perspective center");
    Require(readsBuiltin(pixelBuiltinsRead(0x8082u, 0x8082u, 3u), spv::BuiltInFragCoord), "POS_FIXED_PT is not read from the fragment coordinate");
    Require(!readsBuiltin(pixelBuiltinsRead(0x82u, 0x82u, 2u), spv::BuiltInFragCoord), "LINE_STIPPLE_TEX read the fragment coordinate");
    pixel = decode(0x546u, 0x7c7u);
    {
        ShaderRecompiler::RecompileRequest request{};
        const std::array<std::uint32_t, 1> code{0xbf810000u};
        request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = pixel;
        const ShaderRecompiler::RequestSerializer serializer;
        const auto back = serializer.Deserialize(serializer.Serialize(request));
        const auto& p = *back.request.context.pixel;
        Require(p.inputAddr == 0x7c7u && p.hasPerspectiveCenterVgpr && p.perspectiveCentroid && p.linearCentroid && !p.noPerspective && p.posX && !p.posY && p.posZ, "the pixel input layout did not survive serialization");
    }
    for (const auto source : {0u, 2u, 3u}) {
        const auto read = pixelBuiltinsRead(0x106u, 0x106u, source);
        Require(readsBuiltin(read, spv::BuiltInBaryCoordKHR) && !readsBuiltin(read, spv::BuiltInFragCoord), "a centroid-layout I/J VGPR does not hold the barycentrics: v" + std::to_string(source));
    }
    const auto noPerspective = pixelNoPerspectiveLocations(false);
    Require(noPerspective.size() == 1 && noPerspective[0] == 1u, "only the parameter interpolated through the linear pair must be NoPerspective");
    Require(pixelNoPerspectiveLocations(true).empty(), "explicit interpolation must not interpolate parameter arrays a second time");
    auto read = pixelBuiltinsRead(0x106u, 0x106u, 4u);
    Require(readsBuiltin(read, spv::BuiltInFragCoord) && !readsBuiltin(read, spv::BuiltInBaryCoordKHR), "POS_X is not in v4 after the center and centroid pairs");
    read = pixelBuiltinsRead(0x326u, 0x326u, 5u);
    Require(readsBuiltin(read, spv::BuiltInBaryCoordNoPerspKHR) && !readsBuiltin(read, spv::BuiltInFragCoord), "LINEAR_CENTER's J is not v5");
    read = pixelBuiltinsRead(0x326u, 0x326u, 6u);
    Require(readsBuiltin(read, spv::BuiltInFragCoord) && !readsBuiltin(read, spv::BuiltInBaryCoordKHR) && !readsBuiltin(read, spv::BuiltInBaryCoordNoPerspKHR), "POS_X is not in v6 after three I/J pairs");
    read = pixelBuiltinsRead(0x102u, 0x106u, 4u);
    Require(readsBuiltin(read, spv::BuiltInFragCoord), "an ADDR-only centroid pair did not reserve v2/v3");
    read = pixelBuiltinsRead(0x102u, 0x106u, 2u);
    Require(!readsBuiltin(read, spv::BuiltInFragCoord) && !readsBuiltin(read, spv::BuiltInBaryCoordKHR), "an ADDR-only centroid pair was loaded");
}

void OpaqueDestinationAlphaTests() {
    auto queue = makeState();
    for (std::uint32_t constant = 0x105; constant < 0x109; ++constant) queue.context[constant] = 0;
    queue.context[0x1e0] = (1u << 30u) | (1u << 29u) | 6u | (7u << 8u) | (8u << 16u) | (10u << 24u);
    auto blend = AgcDriver::Graphics::DecodeState(queue).blend;
    Require(blend.srcColorBlendFactor == VK_BLEND_FACTOR_DST_ALPHA && blend.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, "color blend factors changed");
    Require(blend.srcAlphaBlendFactor == VK_BLEND_FACTOR_DST_COLOR && blend.dstAlphaBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, "alpha blend factors changed");
    queue.context[0x31d] = 0x20000u;
    blend = AgcDriver::Graphics::DecodeState(queue).blend;
    Require(blend.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && blend.dstColorBlendFactor == VK_BLEND_FACTOR_ZERO, "an opaque destination left a destination-alpha color factor");
    Require(blend.srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE && blend.dstAlphaBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, "an opaque destination left a destination alpha factor");
    queue.context[0x1e0] = (1u << 30u) | 10u | (9u << 8u);
    blend = AgcDriver::Graphics::DecodeState(queue).blend;
    Require(blend.srcColorBlendFactor == VK_BLEND_FACTOR_ZERO && blend.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, "alpha saturation with an opaque destination");
    Require(blend.dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO, "an opaque destination left a destination-color alpha factor");
    queue.context[0x31d] = 0x40000u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "destination alpha override");
}

void DisabledColorTests() {
    auto queue = makeState();
    queue.context[0x8e] = 0;
    for (const auto offset : {0x31cu, 0x31bu, 0x31du, 0x3b0u, 0x3b8u, 0x390u, 0x318u, 0x1e0u}) queue.context.erase(offset);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.hasColorTarget && state.color.address == 0 && state.color.bytes == 0, "disabled color writes accessed a color surface");
    Require(state.renderExtent.width == 64 && state.renderExtent.height == 4, "attachment-free framebuffer lost screen scissor extent");
    queue.context[0xd] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "empty framebuffer extent");
    queue = makeState();
    queue.context[0x8e] = 3;
    const auto partial = AgcDriver::Graphics::DecodeState(queue);
    Require(partial.hasColorTarget && partial.blend.colorWriteMask == 3, "partial color write mask changed");
    queue.context.erase(0x31c);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
    queue = makeState();
    queue.context[0x31c] = 0;
    for (const auto offset : {0x31bu, 0x31du, 0x3b0u, 0x3b8u, 0x390u, 0x318u, 0x1e0u}) queue.context.erase(offset);
    const auto disabled = AgcDriver::Graphics::DecodeState(queue);
    Require(!disabled.hasColorTarget && disabled.colors.empty() && disabled.blends.empty(), "COLOR_INVALID retained an attachment despite the disabled buffer");
    Require(disabled.renderExtent.width == 64 && disabled.renderExtent.height == 4, "COLOR_INVALID lost the attachment-free render extent");
    queue.shader[0x008] = 0;
    queue.shader[0x009] = 0;
    Require(AgcDriver::Graphics::NullPixelProgramRejection(queue).empty(), "COLOR_INVALID rejected a draw without a pixel shader");
    queue.context[0x200] = 0x36;
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x010] = 0x80000181;
    queue.context[0x011] = 0x20000180;
    queue.context[0x012] = 0x100;
    queue.context[0x014] = 0x100;
    queue.context[0x007] = 0x003f003f;
    queue.context[0x00a] = 0;
    queue.context[0x00b] = std::bit_cast<std::uint32_t>(1.0f);
    const auto depthOnly = AgcDriver::Graphics::DecodeState(queue);
    Require(!depthOnly.hasColorTarget && depthOnly.depth && depthOnly.depthTest && depthOnly.depthWrite && depthOnly.renderExtent.height == 64, "COLOR_INVALID discarded a depth-only draw");
    for (const auto colorControl : {0x0u, 0xcc0000u}) {
        queue = makeState();
        queue.context[0x202] = colorControl;
        const auto unwritten = AgcDriver::Graphics::DecodeState(queue);
        Require(!unwritten.hasColorTarget && unwritten.colors.empty() && unwritten.color.address == 0, "CB_COLOR_CONTROL mode disable kept a color attachment");
        Require(unwritten.renderExtent.width == 64 && unwritten.renderExtent.height == 4, "CB_COLOR_CONTROL mode disable lost the screen scissor extent");
    }
}

void TuningFieldTests() {
    auto queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto baseline = AgcDriver::Graphics::DecodeState(queue);
    queue.context[0x292] = 0x22;
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "ALTERNATE_RBS_PER_TILE was rejected");
    Require(state.scissor.offset.x == baseline.scissor.offset.x && state.scissor.extent.width == baseline.scissor.extent.width && state.scissor.extent.height == baseline.scissor.extent.height, "ALTERNATE_RBS_PER_TILE changed the scissor");
    queue.context[0x292] = 0x26;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "scan conversion mode");
    queue.context[0x292] = 2;
    queue.context[0x202] = 0xcc0011;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "DISABLE_DUAL_QUAD was rejected");
    Require(state.hasColorTarget && state.blend.colorWriteMask == baseline.blend.colorWriteMask, "DISABLE_DUAL_QUAD changed color output");
    queue.context[0x202] = 0xcc0013;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "copy ROP");
    queue.context[0x202] = 0xcc0010;
    for (const auto groups : {1u, 2u, 15u}) {
        queue.context[0x2d5] = 0x2000u | (groups << 15u);
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "MAX_PRIMGRP_IN_WAVE was rejected");
        Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Vertex && state.stages.vertexWaveSize == 64u, "MAX_PRIMGRP_IN_WAVE changed vertex routing");
    }
}

void ReversedComponentOrderTests() {
    for (const auto& [swap, mapping] : {std::pair{2u, 0x1bu}, std::pair{3u, 0x93u}}) {
        auto queue = makeState();
        queue.context[0x31c] = (queue.context[0x31c] & ~(3u << 11u)) | (swap << 11u);
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.colors.size() == 1 && state.colors[0].format == VK_FORMAT_R8G8B8A8_UNORM && state.colors[0].componentMapping == mapping, "an 8_8_8_8 target with a reversed component order did not map exports onto RGBA8");
        Require(AgcDriver::Graphics::ExportMappings(state)[0] == mapping && state.blends[0].colorWriteMask == 0xfu, "a reversed 8_8_8_8 target did not write all four channels through its export mapping");
        queue.context[0x1e0] = 0x40010001u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "reversed component order");
        queue.context[0x1e0] = 0;
        queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u))) | (12u << 2u) | (7u << 8u);
        const auto wide = AgcDriver::Graphics::DecodeState(queue);
        Require(wide.colors.size() == 1 && wide.colors[0].format == VK_FORMAT_R16G16B16A16_SFLOAT && wide.colors[0].componentMapping == mapping && wide.blends[0].colorWriteMask == 0xfu, "a 16_16_16_16 float target with a reversed component order did not map exports onto RGBA16");
    }
    auto queue = makeState();
    queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u) | (3u << 11u))) | (12u << 2u) | (7u << 8u) | (1u << 11u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "component swap 1");
}

void CompactedExportTests() {
    alignas(256) static std::array<std::byte, 1024> slotFourMemory{};
    const auto slotFour = reinterpret_cast<std::uintptr_t>(slotFourMemory.data());
    auto queue = makeState();
    for (const auto offset : {0x31bu, 0x31cu, 0x31du}) queue.context[offset + 4u * 0xfu] = queue.context.at(offset);
    for (const auto offset : {0x3b0u, 0x3b8u}) queue.context[offset + 4u] = queue.context.at(offset);
    queue.context[0x318 + 4u * 0xfu] = static_cast<std::uint32_t>(slotFour >> 8u);
    queue.context[0x390 + 4u] = static_cast<std::uint32_t>(slotFour >> 40u);
    queue.context[0x1e4] = 0x40010001u;
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = 0;
    queue.context[0x8e] = 0x3000fu;
    queue.context[0x8f] = 0xf000fu;
    queue.context[0x1c5] = 0x44u;
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.colors.size() == 2 && state.blends.size() == 2, "two compacted exports did not give two attachments");
    Require(state.colors[0].slot == 0 && state.colors[0].exportIndex == 0 && state.colors[0].address == reinterpret_cast<std::uintptr_t>(colorMemory.data()), "export 0 did not reach MRT slot 0");
    Require(state.colors[1].slot == 4 && state.colors[1].exportIndex == 1 && state.colors[1].address == slotFour, "export 1 did not reach MRT slot 4, the second slot CB_SHADER_MASK enables");
    Require(!state.blends[0].blendEnable && state.blends[1].blendEnable && state.blends[1].colorWriteMask == (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT), "export 1 did not take MRT slot 4's blend control and target mask");
    Require(AgcDriver::Graphics::ExportMappings(state)[1] == state.colors[1].componentMapping, "export 1 did not take MRT slot 4's component mapping");
    auto disabledFirst = queue;
    disabledFirst.context[0x31c] = 0;
    const auto withHole = AgcDriver::Graphics::DecodeState(disabledFirst);
    Require(withHole.colors.size() == 1 && withHole.colors[0].slot == 4 && withHole.colors[0].exportIndex == 1 && withHole.blends.size() == 2 && withHole.blends[0].colorWriteMask == 0, "COLOR_INVALID shifted a later MRT export");
    queue.context[0x1c5] = 0x90009u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "color export format 0");
    queue.context[0x8e] = 0xf000fu;
    queue.context[0x8f] = 0xf0f0fu;
    queue.context[0x1c5] = 0x999u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.colors.size() == 2 && state.blends.size() == 3, "a write-masked export between written ones changed the attachment count");
    Require(state.colors[0].exportIndex == 0 && state.colors[1].slot == 4 && state.colors[1].exportIndex == 2, "export 2 did not reach MRT slot 4, the third slot CB_SHADER_MASK enables");
    Require(state.blends[1].colorWriteMask == 0 && !state.blends[1].blendEnable && state.blends[2].blendEnable, "the write-masked export 1 was not left unused");
}

void DepthStencilTests() {
    auto queue = makeState();
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x007] = (1u << 16u) | 3u;
    queue.context[0x00a] = 7;
    queue.context[0x00b] = 0;
    queue.context[0x010] = 0x22900983;
    queue.context[0x011] = 0x20000181;
    for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
    for (const auto offset : {0x013u, 0x015u}) queue.context[offset] = 0x200;
    queue.context[0x10b] = 0x00050050;
    queue.context[0x10c] = 0x01ffff01;
    queue.context[0x10d] = 0x01000001;
    queue.context[0x200] = 0x00700711;
    std::vector<AgcDriver::Graphics::RegisterRead> log;
    AgcDriver::Graphics::RegisterReadLog() = &log;
    auto state = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::RegisterReadLog() = nullptr;
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a depth register the decoder reads: " + std::to_string(read.offset));
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
    Require(rejection.empty(), "precheck rejected a stencil draw with a surface: " + rejection);
    Require(state.depth && state.depth->address == 0x10000 && state.depth->stencilAddress == 0x20000 && state.depth->format == VK_FORMAT_D32_SFLOAT_S8_UINT && state.depth->clearStencil == 7, "depth surface decode changed");
    Require(state.renderExtent.width == 4 && state.renderExtent.height == 2, "render extent ignores the depth surface");
    Require(!state.depthTest && !state.depthWrite && state.stencilTest, "depth/stencil enables changed");
    Require(state.depth->htileAddress == 0, "an HTILE surface without DB_HTILE_DATA_BASE decoded an HTILE address");
    queue.context[0x005] = 0x300;
    queue.context[0x01e] = 0x01;
    log.clear();
    AgcDriver::Graphics::RegisterReadLog() = &log;
    state = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::RegisterReadLog() = nullptr;
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks an HTILE register the decoder reads: " + std::to_string(read.offset));
    Require(state.depth->htileAddress == (0x30000ull | (1ull << 40u)), "the HTILE address was not decoded from DB_HTILE_DATA_BASE and its high byte");
    queue.context[0x010] &= ~0x20000000u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depth->htileAddress == 0, "a surface without TILE_SURFACE_ENABLE decoded an HTILE address");
    queue.context[0x010] |= 0x20000000u;
    const auto& front = state.stencilFront;
    Require(front.compareOp == VK_COMPARE_OP_ALWAYS && front.passOp == VK_STENCIL_OP_INCREMENT_AND_CLAMP && front.failOp == VK_STENCIL_OP_KEEP && front.reference == 1 && front.writeMask == 0xff, "stencil mask pass changed");
    Require(std::memcmp(&state.stencilBack, &front, sizeof(front)) == 0, "back faces without BACKFACE_ENABLE must use the front state");
    queue.context[0x10b] = 0;
    queue.context[0x10c] = 0x01ffff02;
    queue.context[0x200] = 0x00200211;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.compareOp == VK_COMPARE_OP_EQUAL && state.stencilFront.reference == 2 && state.stencilFront.passOp == VK_STENCIL_OP_KEEP, "stencil content pass changed");
    queue.context[0x10b] = 0x00030030;
    queue.context[0x10c] = 0x01ffff00;
    queue.context[0x200] = 0x00700771;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.passOp == VK_STENCIL_OP_REPLACE && state.stencilFront.reference == 0 && std::memcmp(&state.stencilBack, &state.stencilFront, sizeof(state.stencilFront)) == 0, "stencil clear pass changed");
    queue.context[0x200] = 0x007007f1;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilBack.compareOp == VK_COMPARE_OP_ALWAYS && state.stencilBack.passOp == VK_STENCIL_OP_KEEP && state.stencilBack.writeMask == 0 && state.stencilBack.reference == 1, "back-face stencil state changed");
    queue.context[0x10b] = 0x40;
    queue.context[0x10c] = 0x05ffff02;
    queue.context[0x200] = 0x00200211;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil replacement");
    queue.context[0x10b] = 0x50;
    queue.context[0x200] = 0x00700711;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil add/subtract");
    queue.context[0x10b] = 0;
    queue.context[0x000] = 1;
    queue.context[0x00b] = std::bit_cast<std::uint32_t>(0.25f);
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depth && state.depthTest && state.depthWrite && state.depthCompare == VK_COMPARE_OP_ALWAYS && !state.depthBoundsTest && !state.depthBias && state.viewport.minDepth == 0.25f && state.viewport.maxDepth == 0.25f, "a DEPTH_CLEAR_ENABLE draw does not store DB_DEPTH_CLEAR everywhere it covers");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "precheck rejected a depth clear");
    queue.context[0x002] = 0x01000000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "writable depth plane");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("writable depth plane") != std::string::npos, "precheck accepted read-only depth clear");
    queue.context[0x002] = 0;
    queue.context[0x00b] = 0;
    queue.context[0x000] = 0x22;
    state = AgcDriver::Graphics::DecodeState(queue);
    const auto clears = [](const VkStencilOpState& face) {
        return face.compareOp == VK_COMPARE_OP_ALWAYS && face.passOp == VK_STENCIL_OP_REPLACE && face.failOp == VK_STENCIL_OP_REPLACE && face.depthFailOp == VK_STENCIL_OP_REPLACE && face.writeMask == 0xff && face.reference == 7;
    };
    Require(state.stencilTest && clears(state.stencilFront) && clears(state.stencilBack), "a STENCIL_CLEAR_ENABLE draw does not store DB_STENCIL_CLEAR");
    queue.context[0x200] = 0;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilTest && clears(state.stencilFront), "a STENCIL_CLEAR_ENABLE draw without a stencil test does not store DB_STENCIL_CLEAR");
    queue.context[0x002] = 0x02000000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "writable stencil plane");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("writable stencil plane") != std::string::npos, "precheck accepted read-only stencil clear");
    queue.context[0x002] = 0;
    for (const auto offset : {0x31bu, 0x31cu, 0x31du}) queue.context[offset + 0xfu] = queue.context.at(offset);
    for (const auto offset : {0x3b0u, 0x3b8u}) queue.context[offset + 1u] = queue.context.at(offset);
    const auto secondColor = reinterpret_cast<std::uintptr_t>(sliceMemory.data());
    queue.context[0x327] = static_cast<std::uint32_t>(secondColor >> 8u);
    queue.context[0x391] = static_cast<std::uint32_t>(secondColor >> 40u);
    queue.context[0x1e1] = 0;
    queue.context[0x8e] = 0xf3;
    queue.context[0x8f] = 0xff;
    queue.context[0x1c5] = 0x99;
    queue.context[0x90] = 0x80000001;
    queue.context[0x91] = 0x00020003;
    queue.context[0x10b] = 0x00050050;
    queue.context[0x10c] = 0x05ffff02;
    queue.context[0x10d] = 0x090000ff;
    for (const auto control : {0u, 1u, 0x81u}) {
        queue.context[0x200] = control;
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.stencilTest && clears(state.stencilFront) && clears(state.stencilBack), "stencil clear used the overridden stencil operations");
        Require(state.depth && !state.depthTest && !state.depthWrite, "stencil clear changed depth state");
        Require(state.colors.size() == 2 && state.blends.size() == 2 && state.colors[1].address == secondColor && state.blends[0].colorWriteMask == 3 && state.blends[1].colorWriteMask == 0xf, "stencil clear lost MRT color writes");
        Require(state.scissor.offset.x == 1 && state.scissor.offset.y == 0 && state.scissor.extent.width == 2 && state.scissor.extent.height == 2, "stencil clear changed scissor coverage");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "precheck rejected combined color/stencil clear");
    }
    queue.context[0x002] = 0x01000000;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depthWrite && clears(state.stencilFront), "read-only depth prevented stencil clear");
    queue.context[0x002] = 0;
    queue.context[0x000] = 0x23;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthWrite && state.depthCompare == VK_COMPARE_OP_ALWAYS && clears(state.stencilFront), "a combined depth and stencil clear draw does not clear both planes");
    for (const auto control : {4u, 8u}) {
        queue.context[0x000] = 0x22u | control;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "DB_RENDER_CONTROL");
    }
    queue.context[0x000] = 0x22;
    queue.context[0x011] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "writable stencil plane");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("writable stencil plane") != std::string::npos, "precheck accepted clear without a stencil plane");
    queue.context[0x010] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "writable stencil plane");
    queue = makeState();
    queue.context[0x31b] = 1u << 26u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "mip exceeds");
    queue.context[0x31b] = 1u << 13u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "several array slices");
    const auto sliced = reinterpret_cast<std::uintptr_t>(sliceMemory.data());
    queue.context[0x318] = static_cast<std::uint32_t>(sliced >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>(sliced >> 40u);
    queue.context[0x31b] = 1u | (1u << 13u);
    const auto slice = AgcDriver::Graphics::DecodeState(queue);
    Require(slice.color.address == sliced + 1024u && slice.color.bytes == 1024u, "a color view of one slice did not move the target by one slice");
    queue.context[0x3b8] = 0x09000001;
    const auto arraySlice = AgcDriver::Graphics::DecodeState(queue);
    Require(arraySlice.color.address == sliced + 1024u && arraySlice.color.bytes == 1024u && arraySlice.color.depth == 1u, "a color view of one slice of a 2D array target did not move the target by one slice");
    queue.context[0x31b] = 2u | (2u << 13u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "beyond the array surface");
    queue.context[0x3b8] = 0x0a000003;
    queue.context[0x31b] = 2u | (2u << 13u);
    const auto volume = AgcDriver::Graphics::DecodeState(queue);
    Require(volume.color.address == sliced && volume.color.depth == 4u && volume.color.depthSlice == 2u, "a color view of one 3D depth slice did not keep the surface address and select the slice");
    queue.context[0x31b] = 4u | (4u << 13u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "beyond the 3D surface");
    queue.context[0x31b] = 0;
    queue.context[0x31c] |= 0x10000000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "DCC 3D color targets");
}

void OneDimensionalColorTests() {
    auto queue = makeState();
    queue.context[0x3b8] = 0x08000000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "taller than one row");
    queue.context[0x3b0] = 63u << 14u;
    const auto line = AgcDriver::Graphics::DecodeState(queue);
    Require(line.color.extent.width == 64u && line.color.extent.height == 1u && line.color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()) && line.color.depth == 1u, "a 1D color target did not decode as one row");
}

void htileDepthClearTests() {
    static constexpr std::array<std::uint32_t, 17> kernel{0xd7460000u, 0x04010c06u, 0x34000084u, 0xdc388000u, 0x04020000u, 0xbf8c3f70u, 0xd7710008u, 0x00120805u, 0xd7710009u, 0x00120a05u, 0xd771000au, 0x00120c05u, 0xd771000bu, 0x00120e05u, 0xdc788000u, 0x00020800u, 0xbf810000u};
    const std::array<std::uint32_t, 3> wave{64, 1, 1};
    const auto address = [&](std::span<const std::uint32_t> code, std::array<std::uint32_t, 6> userData, std::array<std::uint32_t, 3> threads) {
        return AgcDriver::Graphics::HtileDepthClearAddress(code, userData, threads);
    };
    Require(address(kernel, {0, 0, 0x0dc50000u, 0x40u, 0u, 0x00000ff0u}, wave) == 0x400dc50000ull, "RADV's HTILE depth fast clear (ZMASK to 0) was not recognized");
    Require(address(kernel, {0, 0, 0x0dc50000u, 0x40u, 0xfffff00fu, 0x00000ff0u}, wave) == 0, "an HTILE initialization to expanded (ZMASK 0xf) was taken for a clear");
    Require(address(kernel, {0, 0, 0x0dc50000u, 0x40u, 0x000003f0u, 0xfffffc0fu}, wave) == 0, "a stencil-only HTILE update that keeps ZMASK was taken for a depth clear");
    Require(address(kernel, {0, 0, 0x0dc50000u, 0x40u, 0u, 0x00000ff0u}, {32, 1, 1}) == 0, "the HTILE kernel matched with another group size");
    auto other = kernel;
    other[6] ^= 1u;
    Require(address(other, {0, 0, 0x0dc50000u, 0x40u, 0u, 0x00000ff0u}, wave) == 0, "a different kernel was taken for RADV's HTILE update");
    Require(address(std::span(kernel).first(16), {0, 0, 0x0dc50000u, 0x40u, 0u, 0x00000ff0u}, wave) == 0, "a truncated kernel was taken for RADV's HTILE update");
}

void depthMaintenanceTests() {
    for (const auto mode : {0x4u, 0x8u, 0x80u, 0x100u, 0x1000u, 0x4000u}) {
        for (const auto clear : {0u, 1u, 2u, 3u}) {
            auto queue = makeState();
            queue.context[0x000] = mode | clear;
            queue.context[0x200] = 0;
            queue.context[0x8e] = 0;
            queue.context[0x8f] = 0;
            queue.shader.erase(0x8);
            const auto reason = AgcDriver::Graphics::DepthMaintenanceRejection(queue);
            Require(reason.find("DB_RENDER_CONTROL") != std::string::npos, "depth maintenance passed without depth tests or color writes");
            Require(AgcDriver::Graphics::DrawRejection(queue, false) == reason, "draw precheck did not reject depth maintenance first");
            expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, reason);
        }
    }
    for (const auto control : {0u, 1u, 2u, 3u, 0x20u, 0x40u, 0x2000u, 0x2063u}) {
        auto queue = makeState();
        queue.context[0x000] = control;
        Require(AgcDriver::Graphics::DepthMaintenanceRejection(queue).empty(), "ordinary depth controls were mistaken for maintenance");
    }
    for (const auto clear : {0u, 1u, 2u, 3u}) {
        auto queue = makeState();
        queue.context[0x000] = 0x10u | clear;
        queue.context[0x200] = 0;
        queue.context[0x8e] = 0;
        queue.context[0x8f] = 0;
        Require(AgcDriver::Graphics::DepthMaintenanceRejection(queue).empty(), "a resummarize draw without depth, stencil or color work was rejected");
        queue.context[0x200] = 2;
        Require(AgcDriver::Graphics::DepthMaintenanceRejection(queue).find("DB_RENDER_CONTROL") != std::string::npos, "a resummarize draw with a depth test passed");
        queue.context[0x200] = 0;
        queue.context[0x8e] = 0xf;
        queue.context[0x8f] = 0xf;
        Require(AgcDriver::Graphics::DepthMaintenanceRejection(queue).find("DB_RENDER_CONTROL") != std::string::npos, "a resummarize draw with color writes passed");
    }
    auto absent = makeState();
    absent.context.erase(0x000);
    Require(AgcDriver::Graphics::DepthMaintenanceRejection(absent).empty(), "an absent depth control produced a maintenance verdict");
}

// SPI_SHADER_Z_FORMAT (0x1c4) and the export enables of DB_SHADER_CONTROL (0x203): Z export needs a
// format with a depth channel (1, 2, 3 or 32_ABGR 9), the sample mask needs 32_ABGR, and the
// formats the export path does not lay out are refused. A missing 0x1c4 gives no verdict here.
void ZExportTests() {
    constexpr std::uint32_t zExportEnable = 0x1u;
    constexpr std::uint32_t maskExportEnable = 0x100u;
    const auto withExports = [](std::uint32_t format, std::uint32_t exports) {
        auto queue = makeState();
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        queue.context[0x1c4] = format;
        queue.context[0x203] = 0x800u | exports;
        return queue;
    };
    for (const std::uint32_t format : {1u, 2u, 3u, 9u}) {
        const auto queue = withExports(format, zExportEnable);
        Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "a Z export with format " + std::to_string(format) + " was rejected");
    }
    Require(!AgcDriver::Graphics::DrawRejection(withExports(0, zExportEnable), true).empty(), "a Z export without a Z format was accepted");
    Require(AgcDriver::Graphics::DrawRejection(withExports(9, zExportEnable | maskExportEnable), true).empty(), "a sample-mask export with 32_ABGR was rejected");
    Require(!AgcDriver::Graphics::DrawRejection(withExports(1, zExportEnable | maskExportEnable), true).empty(), "a sample-mask export with 32_R was accepted");
    for (std::uint32_t format = 4; format <= 8; ++format) {
        Require(!AgcDriver::Graphics::DrawRejection(withExports(format, 0), true).empty(), "Z format " + std::to_string(format) + " was accepted");
    }
    auto absent = withExports(0, 0);
    absent.context.erase(0x1c4);
    Require(AgcDriver::Graphics::DrawRejection(absent, true).empty(), "a missing SPI_SHADER_Z_FORMAT was rejected (or threw) in the precheck");
}

void DepthBoundsBiasTests() {
    const auto bits = [](float value) {
        std::uint32_t word = 0;
        std::memcpy(&word, &value, sizeof(word));
        return word;
    };
    auto queue = makeState();
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x007] = (1u << 16u) | 3u;
    queue.context[0x00a] = 0;
    queue.context[0x00b] = 0;
    queue.context[0x010] = 0x22900983;
    queue.context[0x011] = 0x20000180;
    for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
    queue.context[0x200] = 0x0000006e;
    queue.context[0x008] = bits(0.25f);
    queue.context[0x009] = bits(0.75f);
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthTest && state.depthBoundsTest && state.minDepthBounds == 0.25f && state.maxDepthBounds == 0.75f, "depth bounds decode changed");
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
    Require(rejection.empty(), "precheck rejected depth bounds with a depth surface: " + rejection);
    queue.context[0x205] = 0x00001a48u;
    queue.context[0x2df] = bits(0.5f);
    for (const auto offset : {0x2e0u, 0x2e2u}) queue.context[offset] = bits(32.0f);
    for (const auto offset : {0x2e1u, 0x2e3u}) queue.context[offset] = bits(4.0f);
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasSlope == 2.0f && state.depthBiasConstant == 4.0f && state.depthBiasClamp == 0.5f, "depth bias decode changed");
    Require(!state.depthBiasPerFace, "equal front and back depth bias must draw both faces in one pass");
    queue.context[0x2e2] = bits(-64.0f);
    queue.context[0x2e3] = bits(8.0f);
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasPerFace && state.cullMode == VK_CULL_MODE_NONE, "differing front and back depth bias must draw each face with its own bias");
    Require(state.depthBiasSlope == 2.0f && state.depthBiasConstant == 4.0f && state.backDepthBiasSlope == -4.0f && state.backDepthBiasConstant == 8.0f && state.depthBiasClamp == 0.5f, "per-face depth bias decode changed");
    queue.context[0x205] = 0x00000a48u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasPerFace && state.depthBiasConstant == 4.0f && state.backDepthBiasSlope == 0.0f && state.backDepthBiasConstant == 0.0f, "back faces without depth bias must draw unbiased");
    queue.context[0x205] = 0x00001248u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasPerFace && state.depthBiasSlope == 0.0f && state.depthBiasConstant == 0.0f && state.backDepthBiasConstant == 8.0f, "front faces without depth bias must draw unbiased");
    queue.context[0x205] = 0x00001a4au;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && !state.depthBiasPerFace && state.depthBiasConstant == 4.0f, "culled back faces must not constrain the front depth bias");
    queue.context[0x205] = 0x00001a49u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && !state.depthBiasPerFace && state.depthBiasSlope == -4.0f && state.depthBiasConstant == 8.0f, "culled front faces must draw with the back depth bias");
    queue.context[0x205] = 0x00003a46u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasConstant == 4.0f && state.cullMode == VK_CULL_MODE_BACK_BIT, "a triangle draw with the point and line offset enable lost its depth bias");
    queue.userConfig[0x242] = 2;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "polygon mode, depth bias or nonstandard rasterization");
    queue.userConfig[0x242] = 4;
    queue.context[0x205] = 0x00001a4au;
    queue.context[0x2de] = 0x1f0u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "units other than the depth format");
    queue.context[0x2de] = 0x1e9u;
    auto cleared = queue;
    cleared.ClearContext();
    for (const auto& [offset, value] : queue.context) {
        if (offset != 0x2dfu) cleared.context[offset] = value;
    }
    state = AgcDriver::Graphics::DecodeState(cleared);
    Require(state.depthBias && state.depthBiasConstant == 4.0f && state.depthBiasClamp == 0.0f, "a depth bias clamp the title never writes must decode as its reset value 0");
}

alignas(256) std::array<std::uint8_t, 4> dccKeys{};

void metadataPassTests() {
    using AgcDriver::Graphics::ColorMetadataPass;
    using AgcDriver::Graphics::DecodeColorMetadataPass;
    auto queue = makeState();
    queue.context[0x0] = 0;
    Require(!DecodeColorMetadataPass(queue).has_value(), "normal color rendering decoded as a metadata pass");
    queue.context[0x202] = 0xcc0020;
    queue.context[0x323] = 0x11223344;
    queue.context[0x324] = 0x55667788;
    auto pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->mode == ColorMetadataPass::Mode::EliminateFastClear && pass->targets.size() == 1, "fast-clear eliminate did not decode");
    Require(pass->targets[0].address == reinterpret_cast<std::uintptr_t>(colorMemory.data()) && pass->targets[0].extent.width == 64 && pass->targets[0].extent.height == 4, "fast-clear eliminate target changed");
    Require(pass->targets[0].clearWords[0] == 0x11223344 && pass->targets[0].clearWords[1] == 0x55667788, "fast-clear eliminate lost CB_COLOR_CLEAR_WORD");
    queue.context[0x1c5] = 2;
    queue.context[0x8f] = 1;
    Require(DecodeColorMetadataPass(queue).has_value(), "the blit's export format refused a metadata pass");
    queue.context[0x202] = 0xcc0060;
    pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->mode == ColorMetadataPass::Mode::DccDecompress, "DCC decompress did not decode");
    queue.context[0x8e] = 0;
    Require(DecodeColorMetadataPass(queue)->targets.empty(), "a disabled target joined the metadata pass");
    queue.context[0x8e] = 0xf;
    queue.context[0x202] = 0xcc0061;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "nonstandard ROP");
    queue.context[0x202] = 0x330060;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "nonstandard ROP");
    queue.context[0x202] = 0xcc0060;
    queue.context[0x200] = 2;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "depth or stencil work");
    queue.context[0x200] = 0x70;
    Require(DecodeColorMetadataPass(queue).has_value(), "an always-pass depth function without a test refused the pass");
    queue.context[0x91] = 0x40020;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "over part of a color target");
    queue.context[0x91] = 0x40040;
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(16.0f);
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "over part of a color target");
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x202] = 0xcc0030;
    queue.context[0x1c5] = 9;
    queue.context[0x8f] = 0xf;
    Require(!DecodeColorMetadataPass(queue).has_value(), "resolve decoded as a metadata pass");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("mode resolve") != std::string::npos, "the resolve rejection does not name the mode");

    queue = makeState();
    queue.context[0x0] = 0;
    queue.context[0x202] = 0xcc0020;
    queue.context[0x31c] |= 0x10000000;
    const auto keysAddress = reinterpret_cast<std::uintptr_t>(dccKeys.data());
    queue.context[0x325] = static_cast<std::uint32_t>(keysAddress >> 8u);
    queue.context[0x3a8] = static_cast<std::uint32_t>(keysAddress >> 40u);
    queue.context[0x323] = 0x11223344;
    queue.context[0x324] = 0;
    pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->targets.size() == 1 && pass->targets[0].dccAddress == keysAddress, "the metadata pass lost the target's DCC keys");
    auto mipmapped = queue;
    mipmapped.context[0x3b0] |= 1u << 28u;
    expectFailure([&] { DecodeColorMetadataPass(mipmapped); }, "mipmapped DCC");
    const AgcDriver::Graphics::Context context{};
    const auto texels = [&](std::uint32_t value) {
        for (std::size_t offset = 0; offset < colorMemory.size(); offset += 4) {
            std::uint32_t texel = 0;
            std::memcpy(&texel, colorMemory.data() + offset, 4);
            if (texel != value) return false;
        }
        return true;
    };
    const auto keysAre = [&](std::uint8_t key) { return std::all_of(dccKeys.begin(), dccKeys.end(), [&](std::uint8_t value) { return value == key; }); };
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    dccKeys.fill(0x20);
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x11223344) && keysAre(0xff), "a register fast clear was not eliminated into the texels");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    dccKeys.fill(0xc0);
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0xffffffffu) && keysAre(0xff), "a 1111 fast clear was not eliminated into the texels");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x5a5a5a5au) && keysAre(0xff), "a pass over uncompressed keys changed the texels");
    dccKeys = {0x20, 0xff, 0x20, 0x20};
    expectFailure([&] { AgcDriver::Graphics::RunColorMetadataPass(context, *pass); }, "per-block metadata");
    Require(texels(0x5a5a5a5au), "a refused pass changed the texels");
    pass->targets[0].dccAddress = 0;
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x5a5a5a5au), "a pass over a target without DCC changed its texels");

    using AgcDriver::Graphics::DccKeys;
    struct TenBitClear {
        std::uint8_t key;
        DccKeys keys;
        std::uint32_t texel;
        const char* code;
    };
    for (const std::uint32_t swap : {0u, 1u}) {
        auto tenBit = queue;
        tenBit.context[0x31c] = (tenBit.context[0x31c] & ~0x187cu) | (9u << 2u) | (swap << 11u);
        const auto tenBitPass = DecodeColorMetadataPass(tenBit);
        const auto format = swap == 0 ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        const std::string name = swap == 0 ? "A2B10G10R10" : "A2R10G10B10";
        Require(tenBitPass.has_value() && tenBitPass->targets.size() == 1 && tenBitPass->targets[0].format == format && tenBitPass->targets[0].dccAlphaOnMsb, "the " + name + " metadata pass target changed");
        for (const auto& clear : {TenBitClear{0x00, DccKeys::Clear0000, 0u, "0000"}, TenBitClear{0x40, DccKeys::Clear0001, 0xc0000000u, "0001"}, TenBitClear{0x80, DccKeys::Clear1110, 0x3fffffffu, "1110"}, TenBitClear{0xc0, DccKeys::Clear1111, 0xffffffffu, "1111"}}) {
            std::memset(colorMemory.data(), 0x5a, colorMemory.size());
            dccKeys.fill(clear.key);
            AgcDriver::Graphics::RunColorMetadataPass(context, *tenBitPass);
            Require(texels(clear.texel) && keysAre(0xff), "a " + std::string(clear.code) + " fast clear of an " + name + " target was not eliminated into its 10/10/10/2 texel");
            std::array<std::uint32_t, 4> copied{};
            copied.fill(0x5a5a5a5au);
            Require(AgcDriver::Graphics::FillDccClear(tenBitPass->targets[0].format, clear.keys, tenBitPass->targets[0].dccAlphaOnMsb, std::as_writable_bytes(std::span(copied))) && std::ranges::all_of(copied, [&](std::uint32_t word) { return word == clear.texel; }), "a copied " + name + " target under " + clear.code + " keys was not filled with its 10/10/10/2 texel");
        }
    }

    constexpr std::uint32_t tiledSide = 128;
    constexpr std::size_t tiledBytes = tiledSide * tiledSide * 4;
    constexpr std::size_t blockAlignment = 65536;
    const auto keyCount = AgcDriver::Graphics::DccKeyCount(AgcDriver::Graphics::TextureTileMode::kR64KBX, 4, tiledSide, tiledSide, tiledBytes);
    Require(keyCount == 4096 && AgcDriver::Graphics::DccKeyBytes(tiledBytes) == 256, "the 128x128 SW_64KB_R_X DCC extent changed");
    std::vector<std::byte> tiledBlock(blockAlignment + tiledBytes + keyCount);
    const auto tiledAddress = (reinterpret_cast<std::uintptr_t>(tiledBlock.data()) + blockAlignment - 1) / blockAlignment * blockAlignment;
    auto* tiledTexels = reinterpret_cast<std::uint8_t*>(tiledAddress);
    auto* tiledKeys = tiledTexels + tiledBytes;
    for (const bool pipeAligned : {true, false}) {
        auto tiled = queue;
        tiled.context[0x3b8] |= (static_cast<std::uint32_t>(AgcDriver::Graphics::ColorTileMode::RenderTarget) << 14u) | (pipeAligned ? 1u << 30u : 0u);
        tiled.context[0x3b0] = ((tiledSide - 1u) << 14u) | (tiledSide - 1u);
        tiled.context[0x318] = static_cast<std::uint32_t>(tiledAddress >> 8u);
        tiled.context[0x390] = static_cast<std::uint32_t>(tiledAddress >> 40u);
        tiled.context[0x325] = static_cast<std::uint32_t>((tiledAddress + tiledBytes) >> 8u);
        tiled.context[0x3a8] = static_cast<std::uint32_t>((tiledAddress + tiledBytes) >> 40u);
        for (const auto offset : {0xdu, 0x82u, 0x91u, 0x95u}) tiled.context[offset] = (tiledSide << 16u) | tiledSide;
        tiled.context[0x10f] = std::bit_cast<std::uint32_t>(64.0f);
        tiled.context[0x110] = std::bit_cast<std::uint32_t>(64.0f);
        tiled.context[0x111] = std::bit_cast<std::uint32_t>(-64.0f);
        tiled.context[0x112] = std::bit_cast<std::uint32_t>(64.0f);
        const auto tiledPass = DecodeColorMetadataPass(tiled);
        const std::string alignment = pipeAligned ? "pipe-aligned" : "unaligned";
        Require(tiledPass.has_value() && tiledPass->targets.size() == 1 && tiledPass->targets[0].tileMode == AgcDriver::Graphics::ColorTileMode::RenderTarget && tiledPass->targets[0].bytes == tiledBytes && tiledPass->targets[0].dccAddress == tiledAddress + tiledBytes && tiledPass->targets[0].dccPipeAligned == pipeAligned, "the 128x128 SW_64KB_R_X metadata pass target with " + alignment + " DCC changed");
        const auto expected = pipeAligned ? keyCount : AgcDriver::Graphics::DccKeyBytes(tiledBytes);
        for (const auto& [key, texel, code] : {std::tuple{std::uint8_t{0x20}, 0x11223344u, "register"}, std::tuple{std::uint8_t{0xc0}, 0xffffffffu, "1111"}}) {
            std::memset(tiledTexels, 0x5a, tiledBytes);
            std::memset(tiledKeys, key, keyCount);
            AgcDriver::Graphics::RunColorMetadataPass(context, *tiledPass);
            const auto stored = static_cast<std::size_t>(std::count(tiledKeys, tiledKeys + keyCount, std::uint8_t{0xff}));
            bool filled = true;
            for (std::size_t offset = 0; offset < tiledBytes; offset += 4) filled = filled && std::memcmp(tiledTexels + offset, &texel, 4) == 0;
            Require(filled && stored == expected && std::all_of(tiledKeys, tiledKeys + expected, [](std::uint8_t value) { return value == 0xff; }), std::string("a ") + code + " fast clear eliminate of a 128x128 SW_64KB_R_X target with " + alignment + " DCC stored uncompressed keys over " + std::to_string(stored) + " of its " + std::to_string(keyCount) + " DCC key bytes, expected " + std::to_string(expected));
        }
    }
}

alignas(4096) std::array<std::uint8_t, 4096> cmaskMemory{};

void cmaskTests() {
    using AgcDriver::Graphics::DecodeColorBuffer;
    auto queue = makeState();
    queue.context[0x31c] |= 0x2000;
    const auto cmaskAddress = reinterpret_cast<std::uintptr_t>(cmaskMemory.data());
    queue.context[0x31f] = static_cast<std::uint32_t>(cmaskAddress >> 8u);
    queue.context[0x398] = static_cast<std::uint32_t>(cmaskAddress >> 40u);
    queue.context[0x323] = 0x11223344;
    queue.context[0x324] = 0;
    queue.context[0x3b8] = 0x0dc00000u;
    const auto target = DecodeColorBuffer(queue.context, 0);
    Require(target.cmaskAddress == cmaskAddress && target.cmaskBytes == cmaskMemory.size(), "the fast-clear target lost its CMASK");
    std::vector<AgcDriver::Graphics::RegisterRead> log;
    AgcDriver::Graphics::RegisterReadLog() = &log;
    static_cast<void>(DecodeColorBuffer(queue.context, 0));
    AgcDriver::Graphics::RegisterReadLog() = nullptr;
    Require(!log.empty(), "the register facade recorded nothing for a fast-clear target");
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a register the fast-clear target decode reads: " + std::to_string(read.offset));
    Require(DecodeColorBuffer(makeState().context, 0).cmaskAddress == 0, "a target without FAST_CLEAR got a CMASK");
    using AgcDriver::Graphics::CmaskBytes;
    Require(CmaskBytes(1920, 1080) == 0x6000 && CmaskBytes(960, 544) == 0x2000 && CmaskBytes(800, 450) == 0x1000 && CmaskBytes(1024, 512) == 0x1000 && CmaskBytes(3840, 2160) == 0x14000, "a CMASK does not span whole 1024x512 metablocks");
    auto mipmapped = queue;
    mipmapped.context[0x3b0] |= 1u << 28u;
    expectFailure([&] { DecodeColorBuffer(mipmapped.context, 0); }, "mipmapped, 3D or array");
    auto unaddressed = queue;
    unaddressed.context[0x31f] = 0;
    unaddressed.context[0x398] = 0;
    expectFailure([&] { DecodeColorBuffer(unaddressed.context, 0); }, "without a CMASK address");
    auto unaligned = queue;
    unaligned.context[0x3b8] = 0x09c00000u;
    expectFailure([&] { DecodeColorBuffer(unaligned.context, 0); }, "pipe-aligned SW_64KB_Z_X");
    unaligned.context[0x3b8] = 0x0d000000u;
    expectFailure([&] { DecodeColorBuffer(unaligned.context, 0); }, "pipe-aligned SW_64KB_Z_X");
    unaligned = queue;
    unaligned.context[0x31f] += 1u;
    expectFailure([&] { DecodeColorBuffer(unaligned.context, 0); }, "misaligned");
    using AgcDriver::Graphics::CmaskLayout;
    Require(CmaskBytes(61, 13) == CmaskLayout(61, 13).Bytes() && CmaskBytes(3840, 2160) == CmaskLayout(3840, 2160).Bytes(), "the CMASK size and layout disagree");

    queue.context[0x0] = 0;
    queue.context[0x202] = 0xcc0020;
    auto pass = AgcDriver::Graphics::DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->targets.size() == 1 && pass->targets[0].cmaskAddress == cmaskAddress, "the metadata pass lost the target's CMASK");
    const AgcDriver::Graphics::Context context{};
    const auto texels = [&](std::uint32_t value) {
        for (std::size_t offset = 0; offset < colorMemory.size(); offset += 4) {
            std::uint32_t texel = 0;
            std::memcpy(&texel, colorMemory.data() + offset, 4);
            if (texel != value) return false;
        }
        return true;
    };
    const auto cmaskIs = [&](std::uint8_t value) { return std::all_of(cmaskMemory.begin(), cmaskMemory.end(), [&](std::uint8_t byte) { return byte == value; }); };
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0);
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x11223344) && cmaskIs(0xff), "a CMASK fast clear was not eliminated into the texels");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x5a5a5a5au) && cmaskIs(0xff), "a pass over an expanded CMASK changed the texels");
    const CmaskLayout layout(64, 4);
    const auto setTile = [&](std::uint32_t tileX, std::uint8_t code) {
        const auto nibble = layout.Nibble(tileX, 0);
        auto& entry = cmaskMemory[nibble / 2u];
        const auto shift = (nibble % 2u) * 4u;
        entry = static_cast<std::uint8_t>((entry & ~(0xfu << shift)) | (code << shift));
    };
    const auto tiles = [&](const auto& cleared) {
        for (std::size_t offset = 0; offset < colorMemory.size(); offset += 4) {
            std::uint32_t texel = 0;
            std::memcpy(&texel, colorMemory.data() + offset, 4);
            if (texel != (cleared((offset / 4u) % 64u / 8u) ? 0x11223344u : 0x5a5a5a5au)) return false;
        }
        return true;
    };
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0);
    cmaskMemory[cmaskMemory.size() - 1] = 0xff;
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x11223344) && cmaskIs(0xff), "CMASK nibbles past the target's tiles changed what its tiles hold");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0xff);
    setTile(1, 0);
    setTile(7, 0);
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(tiles([](std::uint32_t tile) { return tile == 1 || tile == 7; }) && cmaskIs(0xff), "a partly fast-cleared CMASK did not clear exactly its cleared tiles");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0xff);
    setTile(4, 0);
    AgcDriver::Graphics::RunColorMetadataPass(context, {AgcDriver::Graphics::ColorMetadataPass::Mode::DccDecompress, pass->targets});
    Require(tiles([](std::uint32_t tile) { return tile == 4; }) && cmaskIs(0xff), "a DCC decompress pass did not eliminate the CMASK fast clear");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0xff);
    setTile(2, 0);
    setTile(3, 3);
    expectFailure([&] { AgcDriver::Graphics::RunColorMetadataPass(context, *pass); }, "not a single-sample fast-clear code");
    Require(texels(0x5a5a5a5au), "a refused pass changed the texels");

    queue.context[0x31c] |= 0x10000000;
    const auto keysAddress = reinterpret_cast<std::uintptr_t>(dccKeys.data());
    queue.context[0x325] = static_cast<std::uint32_t>(keysAddress >> 8u);
    queue.context[0x3a8] = static_cast<std::uint32_t>(keysAddress >> 40u);
    pass = AgcDriver::Graphics::DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->targets.size() == 1 && pass->targets[0].cmaskAddress == cmaskAddress && pass->targets[0].dccAddress == keysAddress, "a DCC fast-clear target lost its CMASK or DCC keys");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0xff);
    dccKeys.fill(0x20);
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(texels(0x11223344) && cmaskIs(0xff) && std::ranges::all_of(dccKeys, [](std::uint8_t key) { return key == 0xff; }), "a DCC fast clear under an expanded CMASK was not eliminated through its keys");
    std::memset(colorMemory.data(), 0x5a, colorMemory.size());
    cmaskMemory.fill(0);
    expectFailure([&] { AgcDriver::Graphics::RunColorMetadataPass(context, *pass); }, "DCC color target");
    Require(texels(0x5a5a5a5au), "a refused pass changed the texels of a DCC target");
}

void uint8x4TargetTests() {
    auto queue = makeState();
    queue.context[0x1c5] = 7;
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1ffcu) | (10u << 2u) | (4u << 8u);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.format == VK_FORMAT_R8G8B8A8_UINT && state.color.elementBytes == 4 && state.color.uintExport, "an 8_8_8_8 UINT color target was rejected");
    for (const std::uint32_t swap : {2u, 3u}) {
        queue.context[0x31c] = (queue.context[0x31c] & ~0x1800u) | (swap << 11u);
        Require(AgcDriver::Graphics::DecodeState(queue).color.format == VK_FORMAT_R8G8B8A8_UINT, "a reversed 8_8_8_8 UINT color target was rejected");
    }
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1800u) | (1u << 11u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color format 10 number type 4 component swap 1");
}

void uint16ExportTests() {
    auto queue = makeState();
    queue.context[0x1c5] = 7;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "color export format 7");
    queue.context[0x31c] = (queue.context[0x31c] & ~0x77cu) | 0x404u;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.hasColorTarget && state.color.format == VK_FORMAT_R8_UINT && state.color.uintExport, "a UINT16_ABGR export into an unsigned integer target did not decode");
    Require(!AgcDriver::Graphics::DecodeState(makeState()).color.uintExport, "a float export was marked unsigned integer");
    auto blended = queue;
    blended.context[0x1e0] = 1u << 30u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(blended); }, "blending into an unsigned integer target");
    queue.context[0x1c5] = 8;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "color export format 8");
}

void DepthClipTests() {
    auto queue = makeState();
    const auto direct = AgcDriver::Graphics::DecodeState(queue);
    Require(!direct.negativeOneToOne && direct.viewport.minDepth == 0 && direct.viewport.maxDepth == 1, "zero-to-one depth transform changed");
    queue.context[0x204] = 0;
    queue.context[0x113] = std::bit_cast<std::uint32_t>(0.5f);
    queue.context[0x114] = std::bit_cast<std::uint32_t>(0.5f);
    const auto symmetric = AgcDriver::Graphics::DecodeState(queue);
    Require(symmetric.negativeOneToOne && symmetric.viewport.minDepth == 0 && symmetric.viewport.maxDepth == 1, "negative-one-to-one depth transform is incorrect");
    queue.context[0x113] = std::bit_cast<std::uint32_t>(-0.5f);
    const auto reversed = AgcDriver::Graphics::DecodeState(queue);
    Require(reversed.viewport.minDepth == 1 && reversed.viewport.maxDepth == 0, "reversed depth transform is incorrect");
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    const auto unrestricted = AgcDriver::Graphics::DecodeState(queue);
    Require(unrestricted.viewport.minDepth == -1 && unrestricted.viewport.maxDepth == 1, "unrestricted viewport depth was normalized");
    queue.context[0x113] = std::bit_cast<std::uint32_t>(-1.0f);
    const auto unrestrictedReversed = AgcDriver::Graphics::DecodeState(queue);
    Require(unrestrictedReversed.viewport.minDepth == 1 && unrestrictedReversed.viewport.maxDepth == -1, "reversed unrestricted viewport depth changed");
    queue.context[0x113] = 0x7f7fffff;
    queue.context[0x114] = 0x7f7fffff;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported viewport transform");
    queue.context[0x114] = std::bit_cast<std::uint32_t>(0.5f);
    queue.context[0x113] = std::bit_cast<std::uint32_t>(0.5f);
    queue.context[0xb4] = std::bit_cast<std::uint32_t>(2.0f);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "inverted viewport depth clamp");
    queue.context[0xb4] = 0;
    auto collapsedQueue = makeState();
    collapsedQueue.context[0x10f] = 0;
    collapsedQueue.context[0x110] = 0;
    collapsedQueue.context[0x111] = std::bit_cast<std::uint32_t>(-0.0f);
    collapsedQueue.context[0x112] = 0;
    const auto collapsed = AgcDriver::Graphics::DecodeState(collapsedQueue);
    Require(collapsed.scissor.extent.width == 0 && collapsed.scissor.extent.height == 0, "a triangle draw through a zero-scale viewport rasterizes");
    Require(collapsed.viewport.width > 0 && collapsed.viewport.height > 0 && collapsed.viewport.minDepth == 0 && collapsed.viewport.maxDepth == 1, "a zero-scale viewport did not become a valid Vulkan viewport");
    collapsedQueue.userConfig[0x242] = 2;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(collapsedQueue); }, "unsupported viewport transform");
    collapsedQueue.userConfig[0x242] = 4;
    collapsedQueue.context[0x10f] = std::bit_cast<std::uint32_t>(-0.0f);
    Require(AgcDriver::Graphics::DecodeState(collapsedQueue).scissor.extent.width == 0, "a negative-zero viewport scale was not collapsed");
    collapsedQueue.context[0x10f] = std::bit_cast<std::uint32_t>(-1.0f);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(collapsedQueue); }, "unsupported viewport transform");
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        if (bit == 19 || bit == 24 || bit == 26 || bit == 27) continue;
        for (const auto linearBit : {0u, 0x01000000u}) {
            queue.context[0x204] = (1u << bit) | linearBit;
            expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "PA_CL_CLIP_CNTL");
            Require(AgcDriver::Graphics::DrawRejection(queue, false).find("PA_CL_CLIP_CNTL") != std::string::npos, "the precheck accepted an unsupported PA_CL_CLIP_CNTL bit");
        }
    }
    queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    for (const auto clip : {0x00080000u, 0x00000000u, 0x0c080000u, 0x04000000u}) {
        queue.context[0x204] = clip;
        const auto plain = AgcDriver::Graphics::DecodeState(queue);
        queue.context[0x204] = clip | 0x01000000u;
        const auto linearClip = AgcDriver::Graphics::DecodeState(queue);
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected DX_LINEAR_ATTR_CLIP_ENA");
        Require(linearClip.negativeOneToOne == plain.negativeOneToOne && linearClip.depthClamp == plain.depthClamp && linearClip.viewport.minDepth == plain.viewport.minDepth && linearClip.viewport.maxDepth == plain.viewport.maxDepth, "DX_LINEAR_ATTR_CLIP_ENA changed the clip space, depth clamping or depth range");
    }
}

void InitialContextTests() {
    const auto configured = makeState();
    AgcDriver::QueueState queue;
    Require(queue.context.at(0x3) == 0 && queue.context.at(0x8) == 0 && queue.context.at(0x9) == 0x3f800000, "depth bounds overlap render override");
    Require(queue.context.at(0x2dc) == 0xaa00 && queue.context.at(0x313) == 0x6000 && queue.context.at(0x2f9) == 0x2d, "initial raster controls are incomplete");
    Require(queue.context.at(0x30e) == 0xffffffff && queue.context.at(0x30f) == 0xffffffff, "initial sample mask excludes samples");
    queue.userConfig[0x242] = 4;
    for (const auto offset : {0x2d5u, 0x204u, 0x8eu, 0x8fu, 0x1c3u, 0x1c5u, 0x31cu, 0x3b0u, 0x3b8u, 0x318u, 0x390u, 0x10fu, 0x110u, 0x111u, 0x112u, 0x113u, 0x114u, 0xb4u, 0xb5u}) queue.context.at(offset) = configured.context.at(offset);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()) && state.color.bytes == colorMemory.size(), "sparse guest setup lost its render target");
    Require(queue.context.at(0x206) == 0x43f, "initial homogeneous viewport mode changed");
    for (const auto control : {0x3fu, 0x43eu, 0x53fu, 0x63fu, 0x8000043fu}) {
        queue.context[0x206] = control;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "PA_CL_VTE_CNTL=0x");
    }
    queue.context[0x206] = 0x43f;
    queue.context[0x2dc] |= 1;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "alpha-to-coverage");
    queue.context.erase(0x2dc);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "missing register");
    queue.ClearContext();
    Require(queue.context.at(0x2dc) == 0xaa00 && queue.context.at(0x318) == 0 && queue.context.at(0x8e) == 0, "clear did not restore controls and discard target state");
    Require(!queue.context.contains(0xdead), "unknown context register acquired a default");
}

struct MockDescriptorWrite {
    std::uint32_t binding;
    std::uint32_t arrayElement;
    std::uint32_t count;
    VkDescriptorType type;
    std::vector<VkDescriptorBufferInfo> buffers;
};

struct MockVulkan {
    std::uint64_t next = 1;
    std::int64_t live = 0;
    std::map<VkBuffer, VkDeviceSize> bufferSizes;
    std::map<VkBuffer, VkBufferUsageFlags> bufferUsage;
    std::map<VkDeviceMemory, VkMemoryAllocateFlags> allocationFlags;
    std::map<VkBuffer, VkDeviceMemory> bufferMemory;
    std::map<VkBuffer, VkDeviceSize> bufferOffset;
    std::map<VkDeviceMemory, std::vector<std::byte>> memories;
    std::map<VkDeviceMemory, VkDeviceSize> allocationSizes;
    VkDeviceSize allocatedBytes = 0;
    std::optional<VkDeviceSize> memoryLimit;
    std::uint64_t allocationAttempts = 0;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    VkDescriptorSetLayoutCreateFlags layoutFlags = 0;
    std::vector<VkDescriptorBindingFlags> layoutBindingFlags;
    std::vector<VkDescriptorPoolSize> poolSizes;
    std::uint32_t poolMaxSets = 0;
    VkDescriptorPoolCreateFlags poolFlags = 0;
    std::uint32_t freedSets = 0;
    std::vector<MockDescriptorWrite> writes;
    std::vector<VkCopyDescriptorSet> copies;
    bool allowCopies = false;
    std::uint32_t boundSets = 0;
    std::uint32_t boundFirst = 0;
    VkPipelineBindPoint boundPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    std::map<VkPipelineLayout, VkDeviceSize> pipelineLayoutPushConstantSize;
    std::uint32_t pipelineCreateCount = 0;
    std::vector<std::array<std::uint32_t, 3>> pipelineSpecializations;
    VkPipeline boundPipeline = VK_NULL_HANDLE;
    std::vector<std::byte> lastPushConstants;
    struct { std::uint32_t x = 0, y = 0, z = 0; } lastDispatchGroups;
};

MockVulkan mock;

template<typename THandle>
THandle makeHandle() {
    const auto value = mock.next++;
    if constexpr (std::is_pointer_v<THandle>) return reinterpret_cast<THandle>(static_cast<std::uintptr_t>(value));
    else return static_cast<THandle>(value);
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = makeHandle<VkBuffer>();
    mock.bufferSizes[*buffer] = info->size;
    mock.bufferUsage[*buffer] = info->usage;
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockGetBufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    *requirements = {mock.bufferSizes.at(buffer), 1, 1};
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    ++mock.allocationAttempts;
    if (mock.memoryLimit.has_value() && mock.allocatedBytes + info->allocationSize > *mock.memoryLimit) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *memory = makeHandle<VkDeviceMemory>();
    mock.memories[*memory] = std::vector<std::byte>(info->allocationSize);
    mock.allocationSizes[*memory] = info->allocationSize;
    mock.allocatedBytes += info->allocationSize;
    if (info->pNext != nullptr) {
        const auto* flags = static_cast<const VkMemoryAllocateFlagsInfo*>(info->pNext);
        mock.allocationFlags[*memory] = flags->flags;
        Require(flags->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO && flags->flags == VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, "invalid BDA allocation flags");
    }
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBindBufferMemory(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    Require(offset + mock.bufferSizes.at(buffer) <= mock.memories.at(memory).size(), "mock buffer memory is bound past its allocation");
    mock.bufferMemory[buffer] = memory;
    mock.bufferOffset[buffer] = offset;
    return VK_SUCCESS;
}

std::span<std::byte> mockBufferMemory(VkBuffer buffer) {
    auto& memory = mock.memories.at(mock.bufferMemory.at(buffer));
    return std::span<std::byte>(memory).subspan(static_cast<std::size_t>(mock.bufferOffset.at(buffer)), static_cast<std::size_t>(mock.bufferSizes.at(buffer)));
}

VKAPI_ATTR VkResult VKAPI_CALL mockMapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize, VkMemoryMapFlags, void** data) {
    Require(offset == 0, "mock memory must be mapped from offset zero");
    *data = mock.memories.at(memory).data();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockUnmapMemory(VkDevice, VkDeviceMemory) {}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    if (const auto found = mock.allocationSizes.find(memory); found != mock.allocationSizes.end()) {
        mock.allocatedBytes -= found->second;
        mock.allocationSizes.erase(found);
    }
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo* info, const VkAllocationCallbacks*, VkDescriptorSetLayout* layout) {
    *layout = makeHandle<VkDescriptorSetLayout>();
    mock.layoutBindings.assign(info->pBindings, info->pBindings + info->bindingCount);
    mock.layoutFlags = info->flags;
    const auto* bindingFlags = static_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(info->pNext);
    if (bindingFlags != nullptr) mock.layoutBindingFlags.assign(bindingFlags->pBindingFlags, bindingFlags->pBindingFlags + bindingFlags->bindingCount);
    else mock.layoutBindingFlags.clear();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo* info, const VkAllocationCallbacks*, VkDescriptorPool* pool) {
    *pool = makeHandle<VkDescriptorPool>();
    mock.poolSizes.assign(info->pPoolSizes, info->pPoolSizes + info->poolSizeCount);
    mock.poolMaxSets = info->maxSets;
    mock.poolFlags = info->flags;
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo* info, VkDescriptorSet* sets) {
    Require(info->descriptorSetCount == 1, "exactly one descriptor set must be allocated");
    sets[0] = makeHandle<VkDescriptorSet>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockFreeDescriptorSets(VkDevice, VkDescriptorPool, std::uint32_t count, const VkDescriptorSet*) {
    mock.freedSets += count;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockUpdateDescriptorSets(VkDevice, std::uint32_t count, const VkWriteDescriptorSet* writes, std::uint32_t copyCount, const VkCopyDescriptorSet* copies) {
    Require(copyCount == 0 || mock.allowCopies, "descriptor copies are not expected");
    if (copyCount != 0) mock.copies.insert(mock.copies.end(), copies, copies + copyCount);
    for (std::uint32_t i = 0; i < count; ++i) {
        MockDescriptorWrite write{writes[i].dstBinding, writes[i].dstArrayElement, writes[i].descriptorCount, writes[i].descriptorType, {}};
        write.buffers.assign(writes[i].pBufferInfo, writes[i].pBufferInfo + writes[i].descriptorCount);
        mock.writes.push_back(write);
    }
}

VKAPI_ATTR void VKAPI_CALL mockCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint point, VkPipelineLayout, std::uint32_t first, std::uint32_t count, const VkDescriptorSet*, std::uint32_t, const std::uint32_t*) {
    mock.boundPoint = point;
    mock.boundFirst = first;
    mock.boundSets = count;
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetMemoryHostPointerProperties(VkDevice, VkExternalMemoryHandleTypeFlagBits type, const void* pointer, VkMemoryHostPointerPropertiesEXT* properties) {
    Require(type == VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT && pointer != nullptr, "invalid host pointer import query");
    properties->memoryTypeBits = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL mockGetBufferDeviceAddress(VkDevice, const VkBufferDeviceAddressInfo* info) {
    Require(mock.bufferMemory.contains(info->buffer), "BDA buffer was not bound");
    Require((mock.bufferUsage.at(info->buffer) & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0, "BDA buffer usage is missing");
    Require((mock.allocationFlags.at(mock.bufferMemory.at(info->buffer)) & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT) != 0, "BDA allocation flags are missing");
    return 0x100000000000ULL + reinterpret_cast<std::uintptr_t>(info->buffer) * 0x10000;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo* info, const VkAllocationCallbacks*, VkPipelineLayout* layout) {
    *layout = makeHandle<VkPipelineLayout>();
    mock.pipelineLayoutPushConstantSize[*layout] = info->pushConstantRangeCount > 0 ? info->pPushConstantRanges[0].size : 0;
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule* module) {
    *module = makeHandle<VkShaderModule>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyShaderModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateComputePipelines(VkDevice, VkPipelineCache, std::uint32_t count, const VkComputePipelineCreateInfo* infos, const VkAllocationCallbacks*, VkPipeline* pipelines) {
    Require(count == 1, "mock expects exactly one compute pipeline per call");
    Require(infos[0].stage.pSpecializationInfo != nullptr, "compute pipeline must provide specialization data");
    // The detiler's constants 0-2 (element size, block size, addressing family) are what the tests
    // check; the swizzle equation and block extent follow them.
    std::array<std::uint32_t, 3> values{};
    Require(infos[0].stage.pSpecializationInfo->dataSize >= sizeof(values), "compute pipeline specialization data has an unexpected size");
    std::memcpy(values.data(), infos[0].stage.pSpecializationInfo->pData, sizeof(values));
    *pipelines = makeHandle<VkPipeline>();
    mock.pipelineSpecializations.push_back(values);
    ++mock.pipelineCreateCount;
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {
    --mock.live;
}

VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline pipeline) {
    mock.boundPipeline = pipeline;
}

VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t size, const void* values) {
    const auto* bytes = static_cast<const std::byte*>(values);
    mock.lastPushConstants.assign(bytes, bytes + size);
}

VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    mock.lastDispatchGroups = {x, y, z};
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    auto memory = mockBufferMemory(buffer);
    Require(offset + size <= memory.size(), "a buffer update exceeds its buffer");
    std::memcpy(memory.data() + offset, data, static_cast<std::size_t>(size));
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* commands) {
    for (std::uint32_t index = 0; index < info->commandBufferCount; ++index) commands[index] = makeHandle<VkCommandBuffer>();
    mock.live += info->commandBufferCount;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t count, const VkCommandBuffer*) { mock.live -= count; }
VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) { *fence = makeHandle<VkFence>(); ++mock.live; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) { --mock.live; }
VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t, const VkFence*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},

        {"vkGetBufferDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferDeviceAddress)},
        {"vkGetMemoryHostPointerPropertiesEXT", reinterpret_cast<PFN_vkVoidFunction>(mockGetMemoryHostPointerProperties)},
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCreateBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindBufferMemory)},
        {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockMapMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockUnmapMemory)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyBuffer)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(mockFreeMemory)},
        {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(mockCreateDescriptorSetLayout)},
        {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyDescriptorSetLayout)},
        {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(mockCreateDescriptorPool)},
        {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyDescriptorPool)},
        {"vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateDescriptorSets)},
        {"vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockFreeDescriptorSets)},
        {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockUpdateDescriptorSets)},
        {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindDescriptorSets)},
        {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(mockCreatePipelineLayout)},
        {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyPipelineLayout)},
        {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(mockCreateShaderModule)},
        {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyShaderModule)},
        {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(mockCreateComputePipelines)},
        {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyPipeline)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)}
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

AgcDriver::Graphics::Context mockContext() {
    AgcDriver::Graphics::Context context{};
    context.deviceProc = mockProc;
    context.memory.memoryTypeCount = 1;
    context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    context.limits.minStorageBufferOffsetAlignment = 1;
    context.limits.maxBoundDescriptorSets = 1;
    context.limits.maxStorageBufferRange = 16384;
    context.nullDescriptors = true;
    context.limits.maxPerStageDescriptorStorageBuffers = 16;
    context.limits.maxPerStageResources = 128;
    context.limits.maxDescriptorSetStorageBuffers = 32;
    return context;
}

using Role = ShaderRecompiler::DescriptorRole;
using Kind = ShaderRecompiler::DescriptorKind;

alignas(16) std::array<std::uint32_t, 4> guestFirst{0x11111111, 0x22222222, 0x33333333, 0x44444444};
alignas(16) std::array<std::uint32_t, 8> guestSecond{1, 2, 3, 4, 5, 6, 7, 8};
alignas(16) std::array<std::uint32_t, 2> guestThird{0xaaaaaaaa, 0xbbbbbbbb};

std::vector<std::uint32_t> vsharp(const void* pointer, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, bytes, 0x31000000u};
}

std::vector<std::uint32_t> join(std::vector<std::uint32_t> first, const std::vector<std::uint32_t>& second) {
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

ShaderRecompiler::DescriptorBinding makeBinding(Role role, std::uint32_t binding, std::uint32_t count, std::vector<std::uint32_t> words) {
    ShaderRecompiler::DescriptorBinding result;
    result.kind = Kind::StorageBuffer;
    result.role = role;
    result.descriptorSet = 0;
    result.binding = binding;
    result.count = count;
    result.guestDescriptor = std::move(words);
    return result;
}

std::vector<std::uint32_t> ShaderDataWords(std::initializer_list<std::uint32_t> userData) {
    std::vector<std::uint32_t> words(ShaderRecompiler::RuntimeAbi::ShaderDataDwords);
    words[0] = ShaderRecompiler::RuntimeAbi::Version;
    std::copy(userData.begin(), userData.end(), words.begin() + ShaderRecompiler::RuntimeAbi::UserDataDword);
    return words;
}

bool sameBytes(std::span<const std::byte> memory, const void* expected, std::size_t bytes) {
    return memory.size() >= bytes && std::memcmp(memory.data(), expected, bytes) == 0;
}

const MockDescriptorWrite& findWrite(std::uint32_t binding) {
    for (const auto& write : mock.writes) {
        if (write.binding == binding) return write;
    }
    throw std::runtime_error("expected descriptor write is missing for binding " + std::to_string(binding));
}

const VkDescriptorSetLayoutBinding& findLayoutBinding(std::uint32_t binding) {
    for (const auto& item : mock.layoutBindings) {
        if (item.binding == binding) return item;
    }
    throw std::runtime_error("expected descriptor set layout binding is missing for binding " + std::to_string(binding));
}

std::span<const std::byte> bufferBytes(VkBuffer buffer) {
    return mockBufferMemory(buffer);
}

void expectResourceFailure(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment, std::string_view reason) {
    mock = MockVulkan{};
    const auto context = mockContext();
    const auto color = AgcDriver::Graphics::DecodeState(makeState()).color;
    expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, color, 0, 0); }, reason);
    Require(mock.live == 0, "failed shader resources leaked Vulkan objects");
}

void expectSingleFailure(const ShaderRecompiler::DescriptorBinding& binding, std::string_view reason) {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    vertex.bindings.push_back(binding);
    expectResourceFailure(vertex, fragment, reason);
}

void expectSingleAccepted(const ShaderRecompiler::DescriptorBinding& binding, std::string_view what) {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    vertex.bindings.push_back(binding);
    mock = MockVulkan{};
    const auto context = mockContext();
    const auto color = AgcDriver::Graphics::DecodeState(makeState()).color;
    { AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, color, 0, 0); }
    Require(mock.live == 0, std::string(what) + " leaked Vulkan objects");
}

void pushConstantTests() {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    vertex.pushConstants.assign(8, std::byte{1});
    fragment.pushConstants.assign(12, std::byte{2});
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 8}}};
    Require(AgcDriver::Graphics::PushConstantStages(shaders) == (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), "push constant stage union changed");
    const auto bytes = AgcDriver::Graphics::AssemblePushConstants(shaders);
    Require(bytes.size() == AgcDriver::Graphics::PipelinePushConstantBytes && bytes[0] == std::byte{1} && bytes[7] == std::byte{1} && bytes[8] == std::byte{2} && bytes[19] == std::byte{2} && bytes[20] == std::byte{0} && bytes[127] == std::byte{0} && bytes.back() == std::byte{0}, "assembled push constants are misplaced");
    shaders[1].pushConstantOffset = 4;
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "overlap");
    shaders[1].pushConstantOffset = 120;
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "crosses a push constant slot");
    shaders[1].pushConstantOffset = AgcDriver::Graphics::PipelinePushConstantBytes - 4;
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "outside the pipeline push constant block");
    shaders[1].pushConstantOffset = AgcDriver::Graphics::PipelinePushSlotBytes;
    const auto slotted = AgcDriver::Graphics::AssemblePushConstants(shaders);
    Require(slotted[128] == std::byte{2} && slotted[139] == std::byte{2} && slotted[8] == std::byte{0}, "a pixel shader in its push slot was misplaced");
    shaders[1].pushConstantOffset = 2;
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "DWORD aligned");
    shaders[1].pushConstantOffset = 8;
    fragment.pushConstants.assign(6, std::byte{2});
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "DWORD aligned");
    fragment.pushConstants.clear();
    shaders[1].pushConstantOffset = 999;
    Require(AgcDriver::Graphics::PushConstantStages(shaders) == VK_SHADER_STAGE_VERTEX_BIT, "empty push constants contributed a stage");
    Require(AgcDriver::Graphics::AssemblePushConstants(shaders)[8] == std::byte{0}, "empty push constants were copied");
    shaders[1].program = nullptr;
    expectFailure([&] { AgcDriver::Graphics::AssemblePushConstants(shaders); }, "missing compiled shader");
}

void descriptorSnapshotTests() {
    using AgcDriver::Graphics::DescriptorCache;
    using AgcDriver::Graphics::Recorder;
    using AgcDriver::Graphics::ShaderResources;
    for (const unsigned selected : {1u, 2u, 3u}) {
        mock = MockVulkan{};
        mock.allowCopies = true;
        auto context = mockContext();
        {
            DescriptorCache cache(context);
            context.descriptorCache = &cache;
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Recorder recorder(context);
            ShaderRecompiler::RecompileResult vertex, fragment;
            auto binding = makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32)));
            binding.bufferWritten = {false, false};
            vertex.bindings.push_back(binding);
            fragment.bindings.push_back(makeBinding(Role::GuestBuffers, 4, 1, vsharp(guestThird.data(), 8)));
            const auto state = AgcDriver::Graphics::DecodeState(makeState());
            ShaderResources resources(context, vertex, fragment, state.color, 0, 0);
            const auto original = findWrite(0).buffers;
            std::vector<ShaderResources::MovedBuffer> moved;
            for (std::size_t index = 0; index < 2; ++index) {
                if ((selected & (1u << index)) == 0) continue;
                const std::size_t bytes = index == 0 ? 24 : 12;
                moved.push_back({index, 0, bytes, std::vector<std::uint32_t>(bytes / 4, static_cast<std::uint32_t>(0x12340000u + index))});
            }
            mock.writes.clear();
            const auto snapshots = resources.PrepareDrawBindings(recorder, moved);
            Require(snapshots != nullptr && snapshots->snapshots.size() == moved.size(), "draw snapshots are missing");
            Require(mock.copies.size() == 2 && mock.copies[0].descriptorCount == 2 && mock.copies[1].descriptorCount == 1, "snapshot descriptor set did not copy every original binding");
            Require(mock.writes.size() == 1, "snapshot updates must rewrite one complete affected array");
            const auto& write = mock.writes.front();
            Require(write.binding == 0 && write.arrayElement == 0 && write.count == 2, "snapshot update did not start from array element zero");
            std::size_t snapshot = 0;
            for (std::size_t index = 0; index < 2; ++index) {
                const auto& descriptor = write.buffers[index];
                if ((selected & (1u << index)) == 0) {
                    Require(descriptor.buffer == original[index].buffer && descriptor.offset == original[index].offset && descriptor.range == original[index].range, "snapshot update changed an untouched buffer descriptor");
                } else {
                    const auto& item = snapshots->snapshots[snapshot];
                    Require(descriptor.buffer == item.buffer->Handle() && descriptor.offset == 0 && descriptor.range == moved[snapshot].size, "snapshot descriptor used another element's buffer or range");
                    Require(sameBytes(bufferBytes(descriptor.buffer), moved[snapshot].words.data(), moved[snapshot].size), "snapshot descriptor lost its buffer data");
                    ++snapshot;
                }
            }
            recorder.Sync();
        }
        Require(mock.live == 0, "snapshot descriptor resources leaked Vulkan objects");
    }
}

void resourceTests() {
    mock = MockVulkan{};
    const auto context = mockContext();
    const auto state = AgcDriver::Graphics::DecodeState(makeState());
    const auto commands = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{1});
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32))));
        vertex.bindings.push_back(makeBinding(Role::ShaderData, 5, 1, ShaderDataWords({7, 8, 9})));
        vertex.shaderDataDwords = ShaderRecompiler::RuntimeAbi::ShaderDataDwords;
        fragment.bindings.push_back(makeBinding(Role::FlattenedSrt, 43, 1, {1, 2}));
        fragment.bindings.push_back(makeBinding(Role::GuestBuffers, 44, 1, vsharp(guestThird.data(), 8)));
        AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, state.color, 0, 0);
        Require(resources.Layout() != VK_NULL_HANDLE, "descriptor set layout was not created");
        Require(mock.layoutBindings.size() == 4 && mock.writes.size() == 4, "one layout binding and one write per shader binding are expected");
        Require(findLayoutBinding(0).descriptorCount == 2 && findLayoutBinding(0).stageFlags == VK_SHADER_STAGE_VERTEX_BIT && findLayoutBinding(0).descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "guest buffer array layout binding is incorrect");
        Require(findLayoutBinding(5).descriptorCount == 1 && findLayoutBinding(5).stageFlags == VK_SHADER_STAGE_VERTEX_BIT, "shader data layout binding is incorrect");
        Require(findLayoutBinding(43).descriptorCount == 1 && findLayoutBinding(43).stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT, "flattened SRT layout binding is incorrect");
        Require(findLayoutBinding(44).descriptorCount == 1 && findLayoutBinding(44).stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT, "fragment guest buffer layout binding is incorrect");
        Require(mock.poolMaxSets == 1 && mock.poolSizes.size() == 1 && mock.poolSizes[0].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && mock.poolSizes[0].descriptorCount == 5, "descriptor pool must hold one set with every storage descriptor");
        const auto& array = findWrite(0);
        Require(array.count == 2 && array.buffers.size() == 2 && array.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "guest buffer array write is incorrect");
        Require(array.buffers[0].offset == 0 && array.buffers[0].range == 16 && array.buffers[1].offset == 0 && array.buffers[1].range == 32, "guest buffers must be bound at zero offset with their descriptor size");
        Require(sameBytes(bufferBytes(array.buffers[0].buffer), guestFirst.data(), 16) && sameBytes(bufferBytes(array.buffers[1].buffer), guestSecond.data(), 32), "guest buffer contents were not uploaded");
        const auto data = ShaderDataWords({7, 8, 9});
        Require(findWrite(5).buffers.size() == 1 && findWrite(5).buffers[0].range == data.size() * 4u && sameBytes(bufferBytes(findWrite(5).buffers[0].buffer), data.data(), data.size() * 4u), "shader data buffer is incorrect");
        const std::array<std::uint32_t, 2> srt{1, 2};
        Require(findWrite(43).buffers.size() == 1 && findWrite(43).buffers[0].range == 8 && sameBytes(bufferBytes(findWrite(43).buffers[0].buffer), srt.data(), 8), "flattened SRT buffer is incorrect");
        Require(findWrite(44).buffers.size() == 1 && findWrite(44).buffers[0].range == 8 && sameBytes(bufferBytes(findWrite(44).buffers[0].buffer), guestThird.data(), 8), "fragment guest buffer is incorrect");
        resources.Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, VK_NULL_HANDLE);
        Require(mock.boundPoint == VK_PIPELINE_BIND_POINT_GRAPHICS && mock.boundFirst == 0 && mock.boundSets == 1, "exactly one descriptor set must be bound at set zero");
        auto first = mockBufferMemory(array.buffers[0].buffer);
        std::memset(first.data(), 0xab, 16);
        auto shaderData = mockBufferMemory(findWrite(5).buffers[0].buffer);
        std::memset(shaderData.data(), 0xcd, 12);
        resources.WriteBack();
        Require(guestFirst[0] == 0xabababab && guestFirst[3] == 0xabababab, "guest buffer was not written back");
        Require(guestSecond[0] == 1 && guestSecond[7] == 8 && guestThird[0] == 0xaaaaaaaa, "unmodified guest buffers changed on write back");
    }
    Require(mock.live == 0, "shader resources leaked Vulkan objects");
    mock = MockVulkan{};
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, state.color, 0, 0);
        Require(resources.Layout() != VK_NULL_HANDLE && mock.layoutBindings.empty() && mock.poolSizes.empty() && mock.writes.empty(), "a shader without bindings must produce only an empty set layout");
        resources.Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, VK_NULL_HANDLE);
        Require(mock.boundSets == 0, "an empty descriptor set was bound");
        resources.WriteBack();
    }
    Require(mock.live == 0, "empty shader resources leaked Vulkan objects");
    mock = MockVulkan{};
    {
        ShaderRecompiler::RecompileResult compute;
        compute.bindings.push_back(makeBinding(Role::GuestBuffers, 3, 1, vsharp(guestThird.data(), 8)));
        const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &compute, 0};
        AgcDriver::Graphics::ShaderResources resources(context, shader);
        Require(mock.layoutBindings.size() == 1 && findLayoutBinding(3).stageFlags == VK_SHADER_STAGE_COMPUTE_BIT && findLayoutBinding(3).descriptorCount == 1, "compute layout binding is incorrect");
        resources.Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, VK_NULL_HANDLE);
        Require(mock.boundPoint == VK_PIPELINE_BIND_POINT_COMPUTE && mock.boundSets == 1, "compute descriptors were bound to the wrong bind point");
        guestThird = {0xaaaaaaaa, 0xbbbbbbbb};
        std::memset(mockBufferMemory(findWrite(3).buffers[0].buffer).data(), 0x5a, 8);
        resources.WriteBack();
        Require(guestThird[0] == 0x5a5a5a5a && guestThird[1] == 0x5a5a5a5a, "compute buffer was not written back");
        guestThird = {0xaaaaaaaa, 0xbbbbbbbb};
    }
    Require(mock.live == 0, "compute resources leaked Vulkan objects");
    mock = MockVulkan{};
    {
        auto descriptor = vsharp(guestSecond.data(), 2);
        descriptor[1] |= 16u << 16u;
        descriptor[3] = 0x0004dfacu;
        ShaderRecompiler::RecompileResult compute;
        compute.bindings.push_back(makeBinding(Role::GuestBuffers, 3, 1, descriptor));
        const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &compute, 0};
        AgcDriver::Graphics::ShaderResources resources(context, shader);
        const auto& buffer = findWrite(3).buffers.at(0);
        Require(buffer.range == sizeof(guestSecond), "strided buffer range does not cover every record");
        Require(sameBytes(bufferBytes(buffer.buffer), guestSecond.data(), sizeof(guestSecond)), "strided buffer contents were not uploaded");
        const std::uint32_t changed = 0x12345678u;
        auto bytes = mockBufferMemory(buffer.buffer);
        std::memcpy(bytes.data() + 16, &changed, sizeof(changed));
        resources.WriteBack();
        Require(guestSecond[4] == changed && guestSecond[0] == 1 && guestSecond[7] == 8, "strided buffer write back changed the wrong record");
        guestSecond[4] = 5;
    }
    Require(mock.live == 0, "strided buffer resources leaked Vulkan objects");
    {
        ShaderRecompiler::RecompileResult vertex;
        const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0};
        expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(context, shader); }, "compute resources require a compute shader");
    }
    const auto base = makeBinding(Role::GuestBuffers, 0, 1, vsharp(guestThird.data(), 8));
    const auto changed = [&](auto mutate) {
        auto binding = base;
        mutate(binding);
        return binding;
    };
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::GuestImages; binding.kind = Kind::SampledImage; binding.binding = 1u; }), "guest texture descriptor must contain 8 dwords");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::GuestImages; binding.kind = Kind::StorageImage; binding.binding = 29u; }), "guest storage image descriptors must contain 8 dwords");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::GuestSamplers; binding.kind = Kind::Sampler; binding.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers); }), "shader sampler descriptors exceed per-stage limits");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::GuestImages; binding.kind = Kind::SampledImage; binding.binding = 29u; }), "resource class disagrees");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::Gds; }), "invalid GDS descriptor contract");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::BdaPagetable; binding.guestDescriptor.clear(); }), "BDA table and fault descriptors");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::FaultBuffer; binding.guestDescriptor.clear(); }), "BDA table and fault descriptors");
    expectSingleFailure(changed([](auto& binding) { binding.kind = Kind::UniformBuffer; }), "unsupported descriptor kind UniformBuffer");
    expectSingleFailure(changed([](auto& binding) { binding.kind = Kind::UniformTexelBuffer; }), "unsupported descriptor kind UniformTexelBuffer");
    expectSingleFailure(changed([](auto& binding) { binding.kind = Kind::StorageTexelBuffer; }), "unsupported descriptor kind StorageTexelBuffer");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::ShaderData; binding.kind = Kind::SampledImage; }), "unsupported descriptor kind SampledImage");
    expectSingleFailure(changed([](auto& binding) { binding.descriptorSet = 1; }), "unexpected descriptor set");
    expectSingleFailure(changed([](auto& binding) { binding.readOnly = true; }), "read-only descriptors are unsupported");
    expectSingleFailure(changed([](auto& binding) { binding.count = 0; }), "empty descriptor binding");
    expectSingleFailure(changed([](auto& binding) { binding.count = 2; }), "four DWORDs per array element");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::ShaderData; binding.count = 2; binding.guestDescriptor = {1, 2}; }), "must not be arrays");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::ShaderData; binding.guestDescriptor.clear(); }), "empty shader data descriptor");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::ShaderData; }), "invalid compact shader data size");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::ShaderData; binding.guestDescriptor = ShaderDataWords({}); binding.guestDescriptor.pop_back(); }), "invalid compact shader data size");
    expectSingleFailure(changed([](auto& binding) { binding.role = Role::FlattenedSrt; binding.guestDescriptor.clear(); }), "empty shader data descriptor");
    expectSingleFailure(changed([](auto& binding) { binding.guestDescriptor[1] |= 0x40000000u; }), "reserved bits");
    expectSingleFailure(changed([](auto& binding) { binding.guestDescriptor[3] |= 0x40000000u; }), "unsupported type");
    expectSingleFailure(changed([](auto& binding) { binding.guestDescriptor[1] |= 0x3fffu << 16u; binding.guestDescriptor[2] = 0xffffffffu; }), "descriptor range limit");
    expectSingleFailure(changed([](auto& binding) { binding.guestDescriptor[2] = 32768; }), "descriptor range limit");
    expectSingleAccepted(changed([](auto& binding) { binding.guestDescriptor = vsharp(reinterpret_cast<const void*>(0x1000), 8); }), "an unmapped V#");
    expectSingleFailure(changed([&](auto& binding) { binding.guestDescriptor = vsharp(reinterpret_cast<const void*>(state.color.address), 64); }), "aliases the render target");
    expectSingleAccepted(changed([](auto& binding) { binding.count = 3; binding.guestDescriptor = join(join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32)), vsharp(reinterpret_cast<const void*>(0x1000), 8)); }), "an unmapped V# element");
    expectSingleFailure(changed([&](auto& binding) { binding.count = 2; binding.guestDescriptor = join(vsharp(guestFirst.data(), 16), vsharp(reinterpret_cast<const void*>(state.color.address), 64)); }), "aliases the render target");
    expectSingleFailure(changed([](auto& binding) { binding.count = 17; binding.guestDescriptor.assign(68, 0); }), "per-stage limits");
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        std::vector<std::uint32_t> words;
        for (int index = 0; index < 17; ++index) words = join(words, vsharp(reinterpret_cast<const void*>(0x1000), 8));
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 17, words));
        auto updateAfterBind = mockContext();
        updateAfterBind.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 17;
        updateAfterBind.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 128;
        updateAfterBind.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 32;
        mock = MockVulkan{};
        {
            AgcDriver::Graphics::ShaderResources resources(updateAfterBind, vertex, fragment, state.color, 0, 0);
            Require(mock.layoutFlags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, "buffers above the per-stage limit did not get an update-after-bind layout");
            Require(mock.layoutBindingFlags.size() == 1 && mock.layoutBindingFlags[0] == VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, "the guest buffers binding is not update-after-bind");
            Require(mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, "the update-after-bind set did not come from an update-after-bind pool");
        }
        Require(mock.live == 0, "update-after-bind shader resources leaked Vulkan objects");
        updateAfterBind.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 16;
        expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(updateAfterBind, vertex, fragment, state.color, 0, 0); }, "shader descriptors exceed per-stage limits");
        Require(mock.live == 0, "failed update-after-bind shader resources leaked Vulkan objects");
        mock = MockVulkan{};
        vertex.bindings.front() = makeBinding(Role::GuestBuffers, 0, 1, vsharp(reinterpret_cast<const void*>(0x1000), 8));
        { AgcDriver::Graphics::ShaderResources resources(updateAfterBind, vertex, fragment, state.color, 0, 0); }
        Require(mock.layoutFlags == 0 && mock.layoutBindingFlags.empty() && mock.poolFlags == 0, "buffers within the per-stage limit took the update-after-bind path");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32))));
        vertex.bindings.push_back(makeBinding(Role::GuestImages, 1, 1, std::vector<std::uint32_t>(8, 0)));
        const auto expectStageResources = [&](Role role, Kind kind, std::uint32_t words, std::uint32_t limit, std::string_view reason) {
            vertex.bindings.back().role = role;
            vertex.bindings.back().kind = kind;
            vertex.bindings.back().binding = kind == Kind::StorageImage ? ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding : kind == Kind::Sampler ? static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers) : ShaderRecompiler::RuntimeAbi::FirstImageBinding;
            vertex.bindings.back().guestDescriptor.assign(words, 0);
            mock = MockVulkan{};
            auto limited = mockContext();
            limited.limits.maxPerStageResources = limit;
            expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(limited, vertex, fragment, state.color, 0, 0); }, reason);
            Require(mock.live == 0, "failed shader resources leaked Vulkan objects");
        };
        expectStageResources(Role::GuestImages, Kind::SampledImage, 8, 2, "shader descriptors exceed per-stage limits");
        expectStageResources(Role::GuestImages, Kind::StorageImage, 8, 2, "shader descriptors exceed per-stage limits");
        expectStageResources(Role::GuestImages, Kind::SampledImage, 8, 3, "missing an image shape");
        expectStageResources(Role::GuestImages, Kind::StorageImage, 8, 3, "detiler is unavailable");
        expectStageResources(Role::GuestSamplers, Kind::Sampler, 4, 2, "shader sampler descriptors exceed per-stage limits");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        fragment.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32))));
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        auto limited = mockContext();
        limited.limits.maxPerStageResources = 2;
        const auto expectAttachments = [&](std::uint32_t attachments, bool accepted, std::string_view what) {
            mock = MockVulkan{};
            if (accepted) {
                AgcDriver::Graphics::ShaderResources resources(limited, shaders, state.color, attachments, 0, 0);
            } else {
                expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(limited, shaders, state.color, attachments, 0, 0); }, "shader descriptors exceed per-stage limits");
            }
            Require(mock.live == 0, std::string(what) + " leaked Vulkan objects");
        };
        expectAttachments(0, true, "a pixel stage at the per-stage limit");
        expectAttachments(1, false, "a pixel stage over the per-stage limit with its colour attachment");
        std::swap(vertex.bindings, fragment.bindings);
        expectAttachments(1, true, "a vertex stage at the per-stage limit");
        expectAttachments(3, false, "colour attachments over the per-stage limit");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, vsharp(guestFirst.data(), 16)));
        fragment.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, vsharp(guestSecond.data(), 32)));
        expectResourceFailure(vertex, fragment, "duplicate shader binding");
        fragment.bindings.front().binding = 1;
        fragment.bindings.front().guestDescriptor = vsharp(guestFirst.data(), 16);
    }
}

void descriptorCacheTests() {
    mock = MockVulkan{};
    auto context = mockContext();
    context.limits.maxDescriptorSetStorageBuffers = 8192;
    context.limits.maxPerStageDescriptorStorageBuffers = 8192;
    context.limits.maxPerStageResources = 8192;
    context.limits.maxDescriptorSetSampledImages = 2048;
    {
        AgcDriver::Graphics::DescriptorCache cache(context);
        const auto layout = [&](std::uint32_t count) {
            const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
            const std::array<std::uint32_t, 4> key{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count, VK_SHADER_STAGE_FRAGMENT_BIT};
            return cache.Layout(key, std::span(&binding, 1));
        };
        const auto large = layout(4097);
        const auto live = mock.live;
        const std::array<VkDescriptorPoolSize, 2> oversized{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4097}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3}}};
        const auto dedicated = cache.Allocate(large, oversized);
        Require(dedicated.set != VK_NULL_HANDLE && dedicated.pool != VK_NULL_HANDLE, "a set above the chain pool's capacity got no set");
        Require(mock.live == live + 1 && mock.poolMaxSets == 1 && mock.poolFlags == 0, "an oversized set does not get a pool of its own");
        Require(mock.poolSizes.size() == 2 && mock.poolSizes[0].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && mock.poolSizes[0].descriptorCount == 4097 && mock.poolSizes[1].type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && mock.poolSizes[1].descriptorCount == 3, "the dedicated pool is not sized to its set");
        Require(cache.Counters().pools == 0 && cache.Counters().sets == 1, "an oversized set was counted in the chain pools");
        cache.Free(dedicated);
        Require(mock.live == live && mock.freedSets == 0, "freeing an oversized set did not destroy its pool");
        const std::array<VkDescriptorPoolSize, 1> fitting{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096}}};
        const auto chained = cache.Allocate(layout(4096), fitting);
        Require(chained.set != VK_NULL_HANDLE && mock.poolMaxSets == 1024 && mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT && cache.Counters().pools == 1, "a set the chain pool holds left the chain");
        cache.Free(chained);
        Require(mock.freedSets == 1 && mock.live == live + 2, "a chain set was not freed back to its pool");
        const std::array<VkDescriptorPoolSize, 1> beyondDevice{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8193}}};
        expectFailure([&] { cache.Allocate(large, beyondDevice); }, "descriptor set exceeds the device's per-set descriptor limit");
        const std::array<VkDescriptorPoolSize, 1> storageImages{{{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}}};
        expectFailure([&] { cache.Allocate(large, storageImages); }, "descriptor set exceeds the device's per-set descriptor limit");
        const std::array<VkDescriptorPoolSize, 1> uniformBuffers{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}}};
        expectFailure([&] { cache.Allocate(large, uniformBuffers); }, "descriptor set uses an unsupported descriptor type 6");
    }
    Require(mock.live == 0, "the descriptor cache leaked a pool or layout");
}

void textureCacheBudgetTests() {
    using AgcDriver::Graphics::TextureCacheBudget;
    constexpr std::uint64_t GiB = 1ull << 30u;
    VkPhysicalDeviceMemoryProperties memory{};
    Require(TextureCacheBudget(memory) == 2 * GiB, "a device without memory heaps does not keep 2 GiB of cached textures");
    memory.memoryHeapCount = 3;
    memory.memoryHeaps[0] = {256ull << 20u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory.memoryHeaps[1] = {32 * GiB, 0};
    memory.memoryHeaps[2] = {16 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT | VK_MEMORY_HEAP_MULTI_INSTANCE_BIT};
    Require(TextureCacheBudget(memory) == 4 * GiB, "a 16 GiB device-local heap does not give 4 GiB of cached textures");
    memory.memoryHeaps[2].size = 6 * GiB;
    Require(TextureCacheBudget(memory) == 2 * GiB, "a 6 GiB device-local heap does not keep the 2 GiB floor");
    memory.memoryHeaps[2].size = 24 * GiB;
    memory.memoryHeapCount = 2;
    Require(TextureCacheBudget(memory) == 2 * GiB, "a heap past memoryHeapCount or a host heap counted toward the texture caches");
    memory.memoryHeapCount = 3;
    Require(TextureCacheBudget(memory) == 6 * GiB, "a 24 GiB device-local heap does not give 6 GiB of cached textures");
}

void sampledTextureBudgetTests() {
    using AgcDriver::Graphics::SampledTextureBudget;
    constexpr std::uint64_t GiB = 1ull << 30u;
    constexpr std::uint64_t MiB = 1ull << 20u;
    VkPhysicalDeviceMemoryProperties memory{};
    memory.memoryHeapCount = 3;
    memory.memoryHeaps[0] = {256 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory.memoryHeaps[1] = {32 * GiB, 0};
    memory.memoryHeaps[2] = {16 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    Require(SampledTextureBudget(memory, nullptr, 0) == 4 * GiB, "without VK_EXT_memory_budget the sampled texture cache does not keep a quarter of the 16 GiB heap");
    reported.heapBudget[0] = 100 * GiB;
    reported.heapBudget[1] = 100 * GiB;
    Require(SampledTextureBudget(memory, &reported, 0) == 4 * GiB, "a zero budget for the largest device-local heap did not fall back to a quarter of the heap");
    reported.heapBudget[2] = 15 * GiB;
    reported.heapUsage[2] = 9 * GiB;
    Require(SampledTextureBudget(memory, &reported, 5 * GiB) == 5 * GiB + 128 * MiB, "a 15 GiB budget with 4 GiB used outside the texture caches does not leave 5 GiB 128 MiB after the 4 GiB storage cache and 1 GiB 896 MiB of headroom");
    reported.heapUsage[2] = 3 * GiB;
    Require(SampledTextureBudget(memory, &reported, 5 * GiB) == 9 * GiB + 128 * MiB, "usage below the cached bytes is not read as no other device memory in use");
    reported.heapUsage[2] = 12 * GiB;
    Require(SampledTextureBudget(memory, &reported, 1 * GiB) == 2 * GiB, "a heap whose other users leave no room does not keep the 2 GiB floor");
    memory.memoryHeaps[0].flags = 0;
    memory.memoryHeaps[2].flags = 0;
    Require(SampledTextureBudget(memory, &reported, 0) == 2 * GiB, "a device without a device-local heap does not keep 2 GiB of sampled textures");
}

void sampledBudgetReportTests() {
    using AgcDriver::Graphics::SampledBudgetReportDue;
    using std::chrono::seconds;
    constexpr std::uint64_t MiB = 1ull << 20u;
    Require(SampledBudgetReportDue(0, 2048 * MiB, seconds(10)), "a budget never reported is not due");
    Require(!SampledBudgetReportDue(4000 * MiB, 4400 * MiB, seconds(60)), "a budget 10% above the reported one is due, not only one more than 10% away");
    Require(!SampledBudgetReportDue(4000 * MiB, 3600 * MiB, seconds(60)), "a budget 10% below the reported one is due, not only one more than 10% away");
    Require(SampledBudgetReportDue(4000 * MiB, 4401 * MiB, seconds(10)), "a budget more than 10% above the reported one is not due after 10 s");
    Require(SampledBudgetReportDue(4000 * MiB, 3599 * MiB, seconds(10)), "a budget more than 10% below the reported one is not due after 10 s");
    Require(!SampledBudgetReportDue(4000 * MiB, 2048 * MiB, seconds(9)), "a budget change is reported again within 10 s of the last report");
}

void misalignedShaderDataTests() {
    mock = MockVulkan{};
    auto context = mockContext();
    context.limits.minStorageBufferOffsetAlignment = 16;
    const auto commands = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{1});
    alignas(64) std::array<std::uint32_t, 8> guest{1, 2, 3, 4, 5, 6, 7, 8};
    {
        ShaderRecompiler::RecompileResult compute;
        compute.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guest.data(), 32), vsharp(guest.data() + 1, 8))));
        compute.bindings.push_back(makeBinding(Role::ShaderData, 1, 1, ShaderDataWords({0x11, 0x22})));
        compute.shaderDataDwords = ShaderRecompiler::RuntimeAbi::ShaderDataDwords;
        compute.memoryOffsetDword = ShaderRecompiler::RuntimeAbi::BufferOffsetsDword;
        auto live = compute;
        live.bindings[1].guestDescriptor = ShaderDataWords({0x33, 0x44});
        const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &compute, 0};
        const AgcDriver::Graphics::CompiledShader liveShader{ShaderRecompiler::ShaderStage::Compute, &live, 0};
        AgcDriver::Graphics::ShaderResources resources(context, shader);
        const auto& views = findWrite(0);
        Require(views.buffers.size() == 2 && views.buffers[0].range == 32 && views.buffers[1].range == 12 && views.buffers[1].offset == views.buffers[0].offset, "the misaligned view is not bound from the aligned offset below it");
        const auto data = findWrite(1).buffers.at(0).buffer;
        auto patched = ShaderDataWords({0x11, 0x22});
        patched[ShaderRecompiler::RuntimeAbi::BufferOffsetsDword] = 0x400u;
        Require(sameBytes(bufferBytes(data), patched.data(), patched.size() * 4u), "the shader data buffer does not hold the misaligned view's offset");
        Require(resources.RefreshData(commands, liveShader), "a refresh with different words recorded nothing");
        auto refreshed = ShaderDataWords({0x33, 0x44});
        refreshed[ShaderRecompiler::RuntimeAbi::BufferOffsetsDword] = 0x400u;
        Require(sameBytes(bufferBytes(data), refreshed.data(), refreshed.size() * 4u), "a data refresh dropped the misaligned view's offset");
        Require(!resources.DataWordsDiffer(liveShader) && resources.DataWordsHash() == AgcDriver::Graphics::ShaderResources::DataWordsHash(liveShader), "the refreshed template's words are not the dispatch's");
    }
    Require(mock.live == 0, "misaligned shader data resources leaked Vulkan objects");
}

struct ModuleShape {
    bool fragment = false;
    std::optional<std::uint32_t> fragmentMode;
    bool push = false;
    std::uint32_t pushLength = 32;
    std::uint32_t pushStride = 4;
    std::uint32_t bufferArray = 0;
    bool plainBuffer = false;
    bool shaderData = false;
    bool vertexInput = false;
    bool barycentric = false;
    bool barycentricNoPerspective = false;
    std::uint32_t barycentricComponents = 3;
    bool perVertex = false;
    std::uint32_t perVertexLength = 3;
    bool parameterOutput = false;
    std::uint32_t parameterLocation = 0;
    bool rectParameters = false;
    bool secondTarget = false;
    bool sampleId = false;
    bool layer = false;
    bool fragDepth = false;
    std::uint32_t sampleMaskLength = 0;
    std::uint32_t floatControlsWidth = 0;
};

void emit(std::vector<std::uint32_t>& out, spv::Op op, std::initializer_list<std::uint32_t> operands) {
    out.push_back((static_cast<std::uint32_t>(operands.size() + 1) << 16u) | static_cast<std::uint32_t>(op));
    out.insert(out.end(), operands.begin(), operands.end());
}

std::vector<std::uint32_t> makeModule(const ModuleShape& shape) {
    std::vector<std::uint32_t> annotations;
    std::vector<std::uint32_t> declarations;
    std::vector<std::uint32_t> function;
    std::uint32_t next = 1;
    const auto id = [&] { return next++; };
    const auto voidType = id();
    const auto functionType = id();
    const auto floatType = id();
    const auto vectorType = id();
    const auto uintType = id();
    const auto outputPointer = id();
    const auto output = id();
    const auto main = id();
    const auto label = id();
    const auto inputPointer = id();
    const auto input = id();
    std::vector<std::uint32_t> extraInterface;
    emit(declarations, spv::OpTypeVoid, {voidType});
    emit(declarations, spv::OpTypeFunction, {functionType, voidType});
    emit(declarations, spv::OpTypeFloat, {floatType, 32});
    emit(declarations, spv::OpTypeVector, {vectorType, floatType, 4});
    emit(declarations, spv::OpTypeInt, {uintType, 32, 0});
    emit(declarations, spv::OpTypePointer, {outputPointer, spv::StorageClassOutput, vectorType});
    emit(declarations, spv::OpVariable, {outputPointer, output, spv::StorageClassOutput});
    if (shape.parameterOutput) {
        const auto parameter = id();
        emit(declarations, spv::OpVariable, {outputPointer, parameter, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {parameter, spv::DecorationLocation, shape.parameterLocation});
        extraInterface.push_back(parameter);
    }
    if (shape.secondTarget) {
        const auto target = id();
        emit(declarations, spv::OpVariable, {outputPointer, target, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {target, spv::DecorationLocation, 1});
        extraInterface.push_back(target);
    }
    if (shape.rectParameters) {
        const auto pointer = id();
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassInput, vectorType});
        for (std::uint32_t location = 0; location < 2; ++location) {
            const auto parameter = id();
            emit(declarations, spv::OpVariable, {pointer, parameter, spv::StorageClassInput});
            emit(annotations, spv::OpDecorate, {parameter, spv::DecorationLocation, location});
            if (location == 1) emit(annotations, spv::OpDecorate, {parameter, spv::DecorationFlat});
            extraInterface.push_back(parameter);
        }
    }
    if (shape.barycentric) {
        const auto vector = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpTypeVector, {vector, floatType, shape.barycentricComponents});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassInput, vector});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassInput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, shape.barycentricNoPerspective ? spv::BuiltInBaryCoordNoPerspKHR : spv::BuiltInBaryCoordKHR});
        extraInterface.push_back(variable);
    }
    if (shape.sampleId || shape.layer) {
        const auto intType = id();
        const auto pointer = id();
        emit(declarations, spv::OpTypeInt, {intType, 32, 1});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassInput, intType});
        for (const auto [wanted, builtin] : {std::pair{shape.sampleId, spv::BuiltInSampleId}, std::pair{shape.layer, spv::BuiltInLayer}}) {
            if (!wanted) continue;
            const auto variable = id();
            emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassInput});
            emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, static_cast<std::uint32_t>(builtin)});
            emit(annotations, spv::OpDecorate, {variable, spv::DecorationFlat});
            extraInterface.push_back(variable);
        }
    }
    if (shape.fragDepth) {
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassOutput, floatType});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, spv::BuiltInFragDepth});
        extraInterface.push_back(variable);
    }
    if (shape.sampleMaskLength != 0) {
        const auto length = id();
        const auto array = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpConstant, {uintType, length, shape.sampleMaskLength});
        emit(declarations, spv::OpTypeArray, {array, uintType, length});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassOutput, array});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, spv::BuiltInSampleMask});
        extraInterface.push_back(variable);
    }
    if (shape.perVertex) {
        const auto length = id();
        const auto array = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpConstant, {uintType, length, shape.perVertexLength});
        emit(declarations, spv::OpTypeArray, {array, vectorType, length});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassInput, array});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassInput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationLocation, 0});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationPerVertexKHR});
        extraInterface.push_back(variable);
    }
    if (shape.vertexInput) {
        emit(declarations, spv::OpTypePointer, {inputPointer, spv::StorageClassInput, vectorType});
        emit(declarations, spv::OpVariable, {inputPointer, input, spv::StorageClassInput});
        emit(annotations, spv::OpDecorate, {input, spv::DecorationLocation, 0});
    }
    if (shape.fragment) emit(annotations, spv::OpDecorate, {output, spv::DecorationLocation, 0});
    else emit(annotations, spv::OpDecorate, {output, spv::DecorationBuiltIn, spv::BuiltInPosition});
    if (shape.push) {
        const auto length = id();
        const auto array = id();
        const auto block = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpConstant, {uintType, length, shape.pushLength});
        emit(declarations, spv::OpTypeArray, {array, uintType, length});
        emit(declarations, spv::OpTypeStruct, {block, array});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassPushConstant, block});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassPushConstant});
        emit(annotations, spv::OpDecorate, {array, spv::DecorationArrayStride, shape.pushStride});
        emit(annotations, spv::OpDecorate, {block, spv::DecorationBlock});
        emit(annotations, spv::OpMemberDecorate, {block, 0, spv::DecorationOffset, 0});
    }
    if (shape.bufferArray != 0 || shape.plainBuffer || shape.shaderData) {
        const auto runtime = id();
        const auto block = id();
        emit(declarations, spv::OpTypeRuntimeArray, {runtime, uintType});
        emit(declarations, spv::OpTypeStruct, {block, runtime});
        emit(annotations, spv::OpDecorate, {runtime, spv::DecorationArrayStride, 4});
        emit(annotations, spv::OpDecorate, {block, spv::DecorationBlock});
        emit(annotations, spv::OpMemberDecorate, {block, 0, spv::DecorationOffset, 0});
        const auto declare = [&](std::uint32_t type, std::uint32_t binding) {
            const auto pointer = id();
            const auto variable = id();
            emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassStorageBuffer, type});
            emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassStorageBuffer});
            emit(annotations, spv::OpDecorate, {variable, spv::DecorationDescriptorSet, 0});
            emit(annotations, spv::OpDecorate, {variable, spv::DecorationBinding, binding});
        };
        if (shape.bufferArray != 0) {
            const auto length = id();
            const auto array = id();
            emit(declarations, spv::OpConstant, {uintType, length, shape.bufferArray});
            emit(declarations, spv::OpTypeArray, {array, block, length});
            declare(array, 0);
        }
        if (shape.plainBuffer) declare(block, 0);
        if (shape.shaderData) declare(block, 5);
    }
    emit(function, spv::OpFunction, {voidType, main, 0, functionType});
    emit(function, spv::OpLabel, {label});
    emit(function, spv::OpReturn, {});
    emit(function, spv::OpFunctionEnd, {});
    std::vector<std::uint32_t> words{spv::MagicNumber, 0x10300, 0, next, 0};
    emit(words, spv::OpCapability, {spv::CapabilityShader});
    if (shape.sampleId) emit(words, spv::OpCapability, {spv::CapabilitySampleRateShading});
    if (shape.layer) emit(words, spv::OpCapability, {spv::CapabilityGeometry});
    if (shape.barycentric) {
        emit(words, spv::OpCapability, {spv::CapabilityFragmentBarycentricKHR});
        const std::string extension = "SPV_KHR_fragment_shader_barycentric";
        const auto count = (extension.size() + 4) / 4;
        words.push_back((static_cast<std::uint32_t>(count + 1) << 16u) | spv::OpExtension);
        const auto start = words.size();
        words.resize(start + count, 0);
        for (std::size_t i = 0; i < extension.size(); ++i) words[start + i / 4] |= static_cast<std::uint32_t>(static_cast<unsigned char>(extension[i])) << ((i % 4) * 8);
    }
    emit(words, spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
    const auto entryPointOffset = words.size();
    if (shape.vertexInput) emit(words, spv::OpEntryPoint, {spv::ExecutionModelVertex, main, 0x6e69616du, 0, output, input});
    else emit(words, spv::OpEntryPoint, {shape.fragment ? spv::ExecutionModelFragment : spv::ExecutionModelVertex, main, 0x6e69616du, 0, output});
    words[entryPointOffset] += static_cast<std::uint32_t>(extraInterface.size()) << 16u;
    words.insert(words.end(), extraInterface.begin(), extraInterface.end());
    if (shape.fragmentMode) emit(words, spv::OpExecutionMode, {main, *shape.fragmentMode});
    if (shape.fragment) emit(words, spv::OpExecutionMode, {main, spv::ExecutionModeOriginUpperLeft});
    if (shape.fragDepth) emit(words, spv::OpExecutionMode, {main, spv::ExecutionModeDepthReplacing});
    if (shape.floatControlsWidth != 0) {
        for (const auto mode : {spv::ExecutionModeRoundingModeRTE, spv::ExecutionModeDenormPreserve, spv::ExecutionModeSignedZeroInfNanPreserve}) emit(words, spv::OpExecutionMode, {main, mode, shape.floatControlsWidth});
    }
    words.insert(words.end(), annotations.begin(), annotations.end());
    words.insert(words.end(), declarations.begin(), declarations.end());
    words.insert(words.end(), function.begin(), function.end());
    return words;
}

void barycentricEmulationTests() {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::Graphics;
    const std::array<FragmentParameter, 3> parameters{{{0u, 0u, false, false}, {1u, 1u, false, true}, {2u, 2u, true, false}}};
    const auto layout = LayoutBarycentricEmulation(parameters, {true, true, true});
    Require(layout.smoothLocation == 1u && layout.linearLocation == 3u && layout.perVertexLocations == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1u, 4u}}, "emulated barycentrics did not take the free locations in order");
    Require(LayoutBarycentricEmulation(parameters, {}).perVertexLocations.empty() && LayoutBarycentricEmulation(parameters, {}).smoothLocation == BarycentricEmulationLayout::NoLocation, "an inactive emulation was given locations");
    std::vector<FragmentParameter> crowded;
    for (std::uint32_t location = 0; location < 11u; ++location) crowded.push_back({location, location, false, true});
    expectFailure([&] { static_cast<void>(LayoutBarycentricEmulation(crowded, {true, false, false})); }, "exceed 32 locations");

    const auto compile = [&](std::span<const std::uint32_t> code, bool barycentric, std::uint32_t secondInput = 0xffffffffu) {
        auto queue = makeState();
        queue.context[0x1b3] = 0x2u;
        queue.context[0x1b4] = 0x2u;
        queue.context[0x1b6] = secondInput == 0xffffffffu ? 1u : 2u;
        queue.context[0x191] = 0u;
        if (secondInput != 0xffffffffu) queue.context[0x192] = secondInput;
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = DecodePixelStageInfo(queue.context, IdentityExports);
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.target.fragmentShaderBarycentricEnabled = barycentric;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request);
    };
    const auto decorations = [](const RecompileResult& result, spv::Decoration decoration) {
        const auto& words = result.spirv.Words();
        std::set<std::uint32_t> inputs;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpVariable && words[at + 3] == spv::StorageClassInput) inputs.insert(words[at + 2]);
        }
        std::vector<std::uint32_t> values;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpDecorate && words[at + 2] == decoration && inputs.contains(words[at + 1])) values.push_back((words[at] >> 16u) > 3u ? words[at + 3] : words[at + 1]);
        }
        return values;
    };
    const auto barycentricCapability = [](const RecompileResult& result) {
        const auto& words = result.spirv.Words();
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpCapability && words[at + 1] == spv::CapabilityFragmentBarycentricKHR) return true;
        }
        return false;
    };
    static constexpr std::array<std::uint32_t, 3> readsIj{0xf800180fu, 0x01000100u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> interpolates{0xc8080000u, 0xc8090001u, 0xf800180fu, 0x02020202u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 4> readsVertex{0xc80a0000u, 0xf800180fu, 0x02020202u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> shiftsIj{0x060000f0u, 0xc8080000u, 0xc8090001u, 0xf800180fu, 0x02020202u, 0xbf810000u};
    auto fragment = compile(readsIj, false);
    Require(fragment.barycentricEmulation.active && fragment.barycentricEmulation.smooth && !fragment.barycentricEmulation.linear, "reading the I/J VGPRs without the extension did not emulate barycentrics");
    const auto builtins = decorations(fragment, spv::DecorationBuiltIn);
    Require(std::find(builtins.begin(), builtins.end(), static_cast<std::uint32_t>(spv::BuiltInBaryCoordKHR)) == builtins.end() && !barycentricCapability(fragment), "emulated barycentrics still need VK_KHR_fragment_shader_barycentric");
    Require(decorations(fragment, spv::DecorationLocation) == std::vector<std::uint32_t>{0u}, "the emulated barycentrics were not read from location 0");
    Require(!compile(readsIj, true).barycentricEmulation.active, "a device with the extension emulated barycentrics");
    Require(!compile(interpolates, false).barycentricEmulation.active, "plain v_interp_p1/p2 lost host interpolation");
    Require(compile(shiftsIj, false).barycentricEmulation.active, "v_interp_p1/p2 through a changed I lost exact barycentrics");
    static constexpr std::array<std::uint32_t, 6> overwritesI{0xc8080000u, 0xc8090001u, 0x7e0002f2u, 0xf800180fu, 0x02020002u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> accumulatesInI{0xc8000000u, 0xc8010001u, 0xf800180fu, 0x00000000u, 0xbf810000u};
    for (const auto& code : {std::span<const std::uint32_t>(overwritesI), std::span<const std::uint32_t>(accumulatesInI)}) {
        const auto reused = compile(code, false);
        const auto reusedBuiltins = decorations(reused, spv::DecorationBuiltIn);
        Require(!reused.barycentricEmulation.active && !barycentricCapability(reused) && std::find(reusedBuiltins.begin(), reusedBuiltins.end(), static_cast<std::uint32_t>(spv::BuiltInBaryCoordKHR)) == reusedBuiltins.end(), "reusing the I/J VGPRs after v_interp_p1/p2 lost host interpolation");
    }
    static constexpr std::array<std::uint32_t, 7> widensExec{0xbefe0381u, 0xc8080000u, 0xbefe03c1u, 0xc8090001u, 0xf800180fu, 0x02020202u, 0xbf810000u};
    Require(compile(widensExec, false).barycentricEmulation.active, "v_interp_p2_f32 under a wider EXEC than its v_interp_p1_f32 lost exact barycentrics");
    static constexpr std::array<std::uint32_t, 6> sharedSlot{0xc8080000u, 0xc8090001u, 0xc80e0402u, 0xf800180fu, 0x03030202u, 0xbf810000u};
    const auto shared = compile(sharedSlot, false, 0x400u);
    Require(shared.barycentricEmulation.active && !barycentricCapability(shared), "inputs sharing a slot with different interpolation did not emulate per-vertex parameters");
    fragment = compile(readsVertex, false);
    Require(fragment.barycentricEmulation.active && !fragment.barycentricEmulation.smooth && fragment.fragmentParameters.size() == 1u && fragment.fragmentParameters[0].perVertex, "v_interp_mov of P10 did not emulate per-vertex parameters");
    Require(decorations(fragment, spv::DecorationLocation) == std::vector<std::uint32_t>{0u} && decorations(fragment, spv::DecorationPerVertexKHR).empty() && !decorations(fragment, spv::DecorationFlat).empty() && !barycentricCapability(fragment), "the emulated per-vertex parameter was not a flat array at the first free location");

    RecompileResult vertex;
    vertex.spirv = makeModule({.parameterOutput = true});
    vertex.parameterExports = {0u};
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGeometry};
    SpirvTarget target{};
    target.vulkanVersion = VK_API_VERSION_1_1;
    target.spirvVersion = 0x00010300u;
    target.supportedCapabilities = capabilities;
    const GeometryStageLimits limits{64u, 128u, 256u, 1024u, 128u};
    auto geometry = BuildBarycentricGeometryShader(vertex, fragment, target, limits);
    const std::array<CompiledShader, 3> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Geometry, &geometry, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    auto state = DecodeState(makeState());
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "the reference state is not a triangle list");
    ValidateShaders(shaders, state, subgroup, false, false, false, true);
    state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    expectFailure([&] { ValidateShaders(shaders, state, subgroup, false, false, false, true); }, "triangle primitives");
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    ValidateShaders(shaders, state, subgroup, false, false, false, true);
    const auto plain = compile(interpolates, false);
    const std::array<CompiledShader, 3> plainShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Geometry, &geometry, 0}, {ShaderStage::Fragment, &plain, 0}}};
    expectFailure([&] { ValidateShaders(plainShaders, state, subgroup, false, false, false, true); }, "only for emulated barycentrics");
    expectFailure([&] { static_cast<void>(BuildBarycentricGeometryShader(vertex, plain, target, limits)); }, "does not emulate barycentrics");
    expectFailure([&] { static_cast<void>(BuildBarycentricGeometryShader(vertex, fragment, target, std::nullopt)); }, "geometry shaders are unavailable");
    expectFailure([&] { static_cast<void>(BuildBarycentricGeometryShader(vertex, fragment, target, GeometryStageLimits{64u, 12u, 256u, 1024u, 128u})); }, "exceeds device limits");
    vertex.parameterExports.clear();
    expectFailure([&] { static_cast<void>(BuildBarycentricGeometryShader(vertex, fragment, target, limits)); }, "not exported by the vertex shader");
}

void rectListTests() {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::Graphics;
    RecompileResult vertex;
    vertex.spirv = makeModule({});
    RecompileResult fragment;
    fragment.spirv = makeModule({.fragment = true});
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityTessellation};
    SpirvTarget target{};
    target.vulkanVersion = VK_API_VERSION_1_1;
    target.spirvVersion = 0x00010300u;
    target.supportedCapabilities = capabilities;
    target.tessellation = TessellationTargetLimits{32, 128, 128, 120, 4096, 128, 128};
    vertex.bindings = {{DescriptorKind::StorageBuffer, DescriptorRole::GuestBuffers, 0, 0, 1, {}}};
    const auto faultAuxiliary = BuildRectListShaders(vertex, fragment, target);
    Require(faultAuxiliary.control.bindings.size() == 1 && faultAuxiliary.control.bindings[0].role == DescriptorRole::FaultBuffer && faultAuxiliary.control.bindings[0].binding == RuntimeAbi::BindingNumber(RuntimeAbi::Stage::TessellationControl, RuntimeAbi::Binding::FaultBuffer), "the rect-list control shader's fault buffer is not in the tessellation control binding group");
    vertex.bindings.clear();
    for (const auto version : {0x00010300u, 0x00010400u}) {
        target.spirvVersion = version;
        auto auxiliary = BuildRectListShaders(vertex, fragment, target);
        const std::array<CompiledShader, 4> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, &auxiliary.control, 0}, {ShaderStage::TessellationEvaluation, &auxiliary.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
        for (const auto primitive : {7u, 17u}) {
            auto queue = makeState();
            queue.userConfig[0x242] = primitive;
            queue.context[0x205] = 3;
            auto state = DecodeState(queue);
            Require(state.rectList && state.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && state.cullMode == VK_CULL_MODE_NONE, "rect-list state was not decoded");
            Require(!state.stages.tessellation && state.stages.path == ShaderPath::Vertex, "rect-list changed guest shader routing");
            ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
            state.rectList = false;
            expectFailure([&] { ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false); }, "stage count");
        }
    }
    vertex.spirv = makeModule({.parameterOutput = true});
    fragment.spirv = makeModule({.fragment = true, .rectParameters = true});
    vertex.parameterExports = {0};
    fragment.fragmentParameters = {{0, 0, false, false}, {1, 0, true, false}};
    auto auxiliary = BuildRectListShaders(vertex, fragment, target);
    Require(!auxiliary.control.spirv.empty() && !auxiliary.evaluation.spirv.empty(), "rect-list parameter shaders are empty");
    auto parameterQueue = makeState();
    parameterQueue.userConfig[0x242] = 17;
    const auto parameterState = DecodeState(parameterQueue);
    const std::array<CompiledShader, 4> parameterShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, &auxiliary.control, 0}, {ShaderStage::TessellationEvaluation, &auxiliary.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    ValidateShaders(parameterShaders, parameterState, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
    fragment.fragmentParameters[0].perVertex = true;
    auto explicitInterpolation = BuildRectListShaders(vertex, fragment, target);
    Require(!explicitInterpolation.control.spirv.empty() && !explicitInterpolation.evaluation.spirv.empty(), "rect-list shaders for an explicitly interpolated parameter are empty");
    fragment.fragmentParameters[0].custom = true;
    expectFailure([&] { static_cast<void>(BuildRectListShaders(vertex, fragment, target)); }, "per-vertex interpolation");
    fragment.fragmentParameters[0].perVertex = false;
    fragment.fragmentParameters[0].custom = false;
    vertex.parameterExports.clear();
    auto unexported = BuildRectListShaders(vertex, fragment, target);
    Require(!unexported.control.spirv.empty() && !unexported.evaluation.spirv.empty(), "rect-list shaders with an unexported parameter are empty");
    fragment.fragmentParameters.clear();
    target.tessellation->maxPatchSize = 3;
    expectFailure([&] { static_cast<void>(BuildRectListShaders(vertex, fragment, target)); }, "device limits");
    target.tessellation.reset();
    expectFailure([&] { static_cast<void>(BuildRectListShaders(vertex, fragment, target)); }, "unavailable");
    auto queue = makeState();
    queue.userConfig[0x242] = 17;
    const auto state = DecodeState(queue);
    const Context context{};
    const AgcDriver::Pm4::DrawParameters draw{0, 4, 0, 1, 0, false};
    expectFailure([&] { Draw(context, state, draw, {}); }, "incomplete rect-list");
}

ShaderRecompiler::RecompileResult recompilePixel(std::initializer_list<std::uint32_t> controls, std::span<const std::uint32_t> code) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x2u;
    queue.context[0x1b4] = 0x2u;
    queue.context[0x1b6] = static_cast<std::uint32_t>(controls.size());
    std::uint32_t index = 0;
    for (const auto control : controls) queue.context[0x191 + index++] = control;
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct LocatedInput {
    std::uint32_t location;
    bool flat;
    bool perVertex;
};

std::vector<LocatedInput> locatedInputs(std::span<const std::uint32_t> words) {
    std::map<std::uint32_t, LocatedInput> decorated;
    std::set<std::uint32_t> inputs;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpVariable && words[at + 3] == spv::StorageClassInput) inputs.insert(words[at + 2]);
        if (op != spv::OpDecorate) continue;
        auto& input = decorated[words[at + 1]];
        if (words[at + 2] == spv::DecorationLocation) input.location = words[at + 3] + 1u;
        if (words[at + 2] == spv::DecorationFlat) input.flat = true;
        if (words[at + 2] == spv::DecorationPerVertexKHR) input.perVertex = true;
    }
    std::vector<LocatedInput> result;
    for (const auto& [id, input] : decorated) {
        if (input.location != 0 && inputs.contains(id)) result.push_back({input.location - 1u, input.flat, input.perVertex});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.location < b.location; });
    return result;
}

constexpr std::array<std::uint32_t, 3> DepthExportCode{0xf8001881u, 2u, 0xbf810000u};

AgcDriver::QueueState depthExportQueue(std::uint32_t shaderControl) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x402u;
    queue.context[0x1b4] = 0x402u;
    queue.context[0x203] = shaderControl;
    return queue;
}

ShaderRecompiler::RecompileRequest depthExportRequest(std::uint32_t shaderControl) {
    const auto queue = depthExportQueue(shaderControl);
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, DepthExportCode, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

std::set<std::uint32_t> depthModes(const ShaderRecompiler::RecompileRequest& request) {
    const auto result = ShaderRecompiler::Recompile(request);
    const auto& words = result.spirv.Words();
    std::set<std::uint32_t> modes;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpExecutionMode) continue;
        const auto mode = words[at + 2];
        if (mode == spv::ExecutionModeDepthReplacing || mode == spv::ExecutionModeDepthGreater || mode == spv::ExecutionModeDepthLess || mode == spv::ExecutionModeDepthUnchanged) modes.insert(mode);
    }
    return modes;
}

void conservativeZExportTests() {
    const std::array<std::pair<std::uint32_t, std::set<std::uint32_t>>, 4> cases{{
        {0x0021u, {spv::ExecutionModeDepthReplacing}},
        {0x2021u, {spv::ExecutionModeDepthReplacing, spv::ExecutionModeDepthLess}},
        {0x4021u, {spv::ExecutionModeDepthReplacing, spv::ExecutionModeDepthGreater}},
        {0x2020u, {}}
    }};
    const ShaderRecompiler::RequestSerializer serializer;
    std::set<std::vector<std::uint64_t>> keys;
    for (const auto& [shaderControl, expected] : cases) {
        const auto request = depthExportRequest(shaderControl);
        const auto replayed = serializer.Deserialize(serializer.Serialize(request));
        Require(depthModes(request) == expected && depthModes(replayed.request) == expected, "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " declared the wrong depth execution modes");
        std::vector<std::uint64_t> key;
        ShaderRecompiler::RecompileCacheKey::Build(request, key);
        keys.insert(key);
    }
    Require(keys.size() == cases.size(), "the recompile cache key ignores CONSERVATIVE_Z_EXPORT");
    for (const auto shaderControl : {0x2020u, 0x4020u}) {
        const auto queue = depthExportQueue(shaderControl);
        static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected DB_SHADER_CONTROL " + std::to_string(shaderControl));
    }
    expectFailure([] { static_cast<void>(depthExportRequest(0x6021u)); }, "CONSERVATIVE_Z_EXPORT");
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    ShaderRecompiler::RecompileResult pixel;
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    for (const auto mode : {spv::ExecutionModeDepthLess, spv::ExecutionModeDepthGreater}) {
        pixel.spirv = makeModule({.fragment = true, .fragmentMode = mode});
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false);
    }
    pixel.spirv = makeModule({.fragment = true, .fragmentMode = spv::ExecutionModeDepthUnchanged});
    expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported fragment execution mode");
}

constexpr std::array<std::uint32_t, 5> OrderedPixelCode{0x7e0e02f2u, 0xbf900007u, 0xf800180fu, 0x07070707u, 0xbf810000u};
constexpr std::array<std::uint32_t, 12> OrderedBdaPixelCode{0x7e040f00u, 0x7e060280u, 0xdc208001u, 0x047d0002u, 0xdc308008u, 0x057d0002u, 0xbf8c3f70u, 0x4a080b04u, 0x7e0e0d04u, 0xf800180fu, 0x07070707u, 0xbf810000u};

AgcDriver::QueueState orderedPixelQueue(std::uint32_t shaderControl) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x2u;
    queue.context[0x1b4] = 0x2u;
    queue.context[0x203] = shaderControl;
    return queue;
}

ShaderRecompiler::RecompileRequest orderedPixelRequest(std::uint32_t shaderControl, std::span<const std::uint32_t> capabilities, std::span<const std::uint32_t> code = OrderedPixelCode) {
    const auto queue = orderedPixelQueue(shaderControl);
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.supportedCapabilities = capabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

std::string interlockShape(std::span<const std::uint32_t> words) {
    std::string shape;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpCapability && words[at + 1] == spv::CapabilityFragmentShaderPixelInterlockEXT) shape += "capability ";
        if (op == spv::OpExtension && std::string_view(reinterpret_cast<const char*>(&words[at + 1])) == "SPV_EXT_fragment_shader_interlock") shape += "extension ";
        if (op == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModePixelInterlockOrderedEXT) shape += "ordered ";
        if (op == spv::OpBeginInvocationInterlockEXT) shape += "begin ";
        if (op == spv::OpEndInvocationInterlockEXT) shape += "end ";
    }
    return shape;
}

std::string interlockExits(std::span<const std::uint32_t> words, std::size_t& kills) {
    struct Block {
        std::vector<spv::Op> ops;
        std::vector<std::uint32_t> next;
    };
    std::map<std::uint32_t, Block> blocks;
    std::uint32_t entry = 0, function = 0, label = 0, first = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        const auto end = at + (words[at] >> 16u);
        if (op == spv::OpEntryPoint) entry = words[at + 2];
        if (op == spv::OpFunction) {
            function = words[at + 2];
            label = 0;
        }
        if (op == spv::OpLabel) label = words[at + 1];
        if (op == spv::OpLabel && function == entry && first == 0) first = label;
        if (function != entry) {
            if (op == spv::OpBeginInvocationInterlockEXT || op == spv::OpEndInvocationInterlockEXT || op == spv::OpKill) return "an interlock instruction or OpKill outside the entry point";
            continue;
        }
        if (label == 0) continue;
        auto& block = blocks[label];
        block.ops.push_back(op);
        if (op == spv::OpBranch) block.next = {words[at + 1]};
        if (op == spv::OpBranchConditional) block.next = {words[at + 2], words[at + 3]};
        if (op == spv::OpSwitch) {
            block.next = {words[at + 2]};
            for (auto target = at + 4; target < end; target += 2) block.next.push_back(words[target]);
        }
    }
    std::map<std::uint32_t, int> state{{first, 0}};
    std::vector<std::uint32_t> work{first};
    while (!work.empty()) {
        const auto current = work.back();
        work.pop_back();
        auto stage = state[current];
        for (const auto op : blocks[current].ops) {
            if (op == spv::OpBeginInvocationInterlockEXT && stage++ != 0) return "block " + std::to_string(current) + " begins the interlock twice";
            if (op == spv::OpEndInvocationInterlockEXT && stage++ != 1) return "block " + std::to_string(current) + " ends the interlock outside it";
            if ((op == spv::OpKill || op == spv::OpReturn || op == spv::OpReturnValue || op == spv::OpUnreachable || op == spv::OpTerminateInvocation) && stage != 2) return "an exit in block " + std::to_string(current) + " skips OpEndInvocationInterlockEXT";
            if (op == spv::OpKill) ++kills;
        }
        for (const auto next : blocks[current].next) {
            const auto [known, added] = state.emplace(next, stage);
            if (!added && known->second != stage) return "block " + std::to_string(next) + " is reached both inside and outside the interlock";
            if (added) work.push_back(next);
        }
    }
    return {};
}

void orderedPixelShaderTests() {
    const std::array<std::uint32_t, 2> interlock{spv::CapabilityShader, spv::CapabilityFragmentShaderPixelInterlockEXT};
    const std::array<std::uint32_t, 1> plain{spv::CapabilityShader};
    for (const auto shaderControl : {0x30640u, 0x30600u}) {
        const auto queue = orderedPixelQueue(shaderControl);
        static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected DB_SHADER_CONTROL " + std::to_string(shaderControl));
        Require(AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports).orderedPixelShader, "PRIMITIVE_ORDERED_PIXEL_SHADER did not reach the pixel stage");
    }
    for (const auto shaderControl : {0x130600u, 0x430600u}) {
        const auto queue = orderedPixelQueue(shaderControl);
        expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(queue)); }, "ordered fragment execution");
        Require(!AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck accepted POPS_OVERLAP_NUM_SAMPLES in DB_SHADER_CONTROL " + std::to_string(shaderControl));
    }
    const ShaderRecompiler::RequestSerializer serializer;
    std::set<std::vector<std::uint64_t>> keys;
    for (const auto& [shaderControl, expected] : std::array<std::pair<std::uint32_t, std::string_view>, 2>{{{0x30600u, "capability extension ordered begin end "}, {0x20600u, ""}}}) {
        const auto request = orderedPixelRequest(shaderControl, interlock);
        const auto replayed = serializer.Deserialize(serializer.Serialize(request));
        Require(replayed.request.context.pixel->orderedPixelShader == (shaderControl == 0x30600u), "the serialized request lost PRIMITIVE_ORDERED_PIXEL_SHADER");
        const auto result = ShaderRecompiler::Recompile(request);
        Require(interlockShape(result.spirv.Words()) == expected && interlockShape(ShaderRecompiler::Recompile(replayed.request).spirv.Words()) == expected, "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " declared the wrong pixel interlock: " + interlockShape(result.spirv.Words()));
        std::vector<std::uint64_t> key;
        ShaderRecompiler::RecompileCacheKey::Build(request, key);
        keys.insert(key);
        AgcDriver::Graphics::State state{};
        state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &result, 0}}};
        AgcDriver::Graphics::ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
    }
    Require(keys.size() == 2, "the recompile cache key ignores PRIMITIVE_ORDERED_PIXEL_SHADER");
    const std::array<std::uint32_t, 5> bdaInterlock{spv::CapabilityShader, spv::CapabilityFragmentShaderPixelInterlockEXT, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> bdaExtensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    auto faulting = orderedPixelRequest(0x30600u, bdaInterlock, OrderedBdaPixelCode);
    faulting.target.supportedExtensions = bdaExtensions;
    faulting.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    std::size_t kills = 0;
    const auto exits = interlockExits(ShaderRecompiler::Recompile(faulting).spirv.Words(), kills);
    Require(exits.empty() && kills == 6u, "a primitive-ordered pixel shader with BDA reads: " + (exits.empty() ? std::to_string(kills) + " OpKill, not the valid-mask kill and 5 BDA fault kills" : exits));
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(orderedPixelRequest(0x30600u, plain))); }, "fragmentShaderPixelInterlock");
}

void ProvokingVertexTests() {
    using namespace AgcDriver::Graphics;
    constexpr std::uint32_t lastVertex = 1u << 19u;
    auto queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto first = DecodeState(queue);
    Require(first.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, "the default provoking vertex is not first");
    for (const auto primitive : {2u, 4u, 5u, 6u}) {
        queue.userConfig[0x242] = primitive;
        for (const auto last : {false, true, false}) {
            queue.context[0x205] = 0x240u | (last ? lastVertex : 0u);
            const auto state = DecodeState(queue);
            Require(state.provokingVertexMode == (last ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT), "PROVOKING_VTX_LAST did not select the requested vertex for primitive " + std::to_string(primitive));
            const auto rejection = DrawRejection(queue, false);
            Require(rejection.empty(), "the draw precheck rejected provoking vertex mode for primitive " + std::to_string(primitive) + ": " + rejection);
            Require(state.cullMode == first.cullMode && state.frontFace == first.frontFace && state.depthBias == first.depthBias, "PROVOKING_VTX_LAST changed another rasterization field");
        }
    }
    queue.context[0x205] |= lastVertex;
    queue.ClearContext();
    Require((queue.context.at(0x205) & lastVertex) == 0, "context reset kept last provoking vertex mode");
    Require(DrawKeyCovers({RegisterBank::Context, 0x205}), "the draw cache key does not cover provoking vertex mode");
    const Context unsupported{};
    ValidateProvokingVertex(unsupported, first);
    auto last = first;
    last.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
    expectFailure([&] { ValidateProvokingVertex(unsupported, last); }, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
    auto supported = unsupported;
    supported.provokingVertexLast = true;
    ValidateProvokingVertex(supported, last);
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN}) {
        auto state = last;
        state.topology = topology;
        ValidateProvokingVertex(supported, state);
    }
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_MAX_ENUM}) {
        auto state = last;
        state.topology = topology;
        expectFailure([&] { ValidateProvokingVertex(supported, state); }, "last provoking vertex is unsupported for this primitive topology");
    }
    const std::array<std::uint32_t, 7> flatColor{0xc8120002u, 0xc8160102u, 0xc81a0202u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto flatPixel = recompilePixel({0x400u}, flatColor);
    const auto inputs = locatedInputs(flatPixel.spirv.Words());
    Require(inputs.size() == 1 && inputs[0].location == 0 && inputs[0].flat && !inputs[0].perVertex, "flat v_interp_mov P0 did not compile to a flat input");
    Require(flatPixel.fragmentParameters.size() == 1 && flatPixel.fragmentParameters[0].flat && !flatPixel.fragmentParameters[0].perVertex, "flat v_interp_mov P0 has explicit per-vertex metadata");
    const std::array<CompiledShader, 1> flatShaders{{{ShaderRecompiler::ShaderStage::Fragment, &flatPixel, 0}}};
    ValidateProvokingVertex(supported, last, flatShaders);
    ShaderRecompiler::RecompileResult fragment{};
    const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN}) {
        auto state = last;
        state.topology = topology;
        for (const auto flat : {false, true}) {
            for (const auto perVertex : {false, true}) {
                fragment.fragmentParameters = {{0, 0, flat, perVertex}};
                auto firstState = state;
                firstState.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
                ValidateProvokingVertex(unsupported, firstState, shaders);
                if (flat && perVertex) {
                    expectFailure([&] { ValidateProvokingVertex(supported, state, shaders); }, "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
                } else if (perVertex && topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
                    expectFailure([&] { ValidateProvokingVertex(supported, state, shaders); }, "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
                } else {
                    ValidateProvokingVertex(supported, state, shaders);
                }
            }
        }
    }
    for (const auto path : {ShaderPath::Geometry, ShaderPath::Tessellation, ShaderPath::TessellationGeometry}) {
        auto generated = last;
        generated.stages.path = path;
        expectFailure([&] { ValidateProvokingVertex(supported, generated); }, "last provoking vertex is unsupported for generated primitive pipelines");
        generated.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
        ValidateProvokingVertex(unsupported, generated);
    }
    for (std::uint32_t generated = 0; generated < 3; ++generated) {
        auto state = last;
        if (generated == 0) state.rectList = true;
        if (generated == 1) state.stages.mesh.emplace();
        if (generated == 2) state.stages.tessellation.emplace();
        expectFailure([&] { ValidateProvokingVertex(supported, state); }, "last provoking vertex is unsupported for generated primitive pipelines");
    }
    last.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_MAX_ENUM_EXT;
    expectFailure([&] { ValidateProvokingVertex(supported, last); }, "invalid provoking vertex mode");
}

void ProvokingVertexRenderPassTests() {
    using namespace AgcDriver::Graphics;
    const auto first = DecodeState(makeState());
    const std::array<VkImageView, 1> views{reinterpret_cast<VkImageView>(std::uintptr_t{1})};
    const std::array<VkProvokingVertexModeEXT, 5> modes{
        VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT,
        VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT, VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT,
        VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT
    };
    for (const bool perPipeline : {false, true}) {
        Context context{};
        context.provokingVertexLast = true;
        context.provokingVertexModePerPipeline = perPipeline;
        auto state = first;
        const auto initialKey = DrawRenderPassKey(context, state, views);
        auto previousKey = initialKey;
        auto previousMode = modes.front();
        for (const auto mode : modes) {
            state.provokingVertexMode = mode;
            const auto key = DrawRenderPassKey(context, state, views);
            const bool reusable = key == previousKey;
            Require(reusable == (perPipeline || mode == previousMode), "render-pass reuse does not respect provokingVertexModePerPipeline");
            previousKey = key;
            previousMode = mode;
        }
        Require(previousKey == initialKey, "returning to first provoking vertex changed the render-pass identity");
        state.viewport.x += 1.0f;
        Require(DrawRenderPassKey(context, state, views) == initialKey, "dynamic viewport changes prevent render-pass reuse");
        const std::array<VkImageView, 1> differentViews{reinterpret_cast<VkImageView>(std::uintptr_t{2})};
        Require(DrawRenderPassKey(context, state, differentViews) != initialKey, "different attachments share a render-pass identity");
        ++state.renderExtent.width;
        Require(DrawRenderPassKey(context, state, views) != initialKey, "different render extents share a render-pass identity");
    }
}

void ConservativeRasterizationTests() {
    auto queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    Require(AgcDriver::Graphics::DecodeState(queue).conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT, "PA_SC_CONSERVATIVE_RASTERIZATION_CNTL 0x6000 enabled conservative rasterization");
    queue.context[0x313] = 0x6001;
    for (const auto primitive : {4u, 5u, 6u}) {
        queue.userConfig[0x242] = primitive;
        Require(AgcDriver::Graphics::DecodeState(queue).conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT, "OVER_RAST_ENABLE did not decode to overestimation for primitive type " + std::to_string(primitive));
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected OVER_RAST_ENABLE");
    }
    for (const auto& [primitive, rejected] : std::array<std::pair<std::uint32_t, std::string_view>, 3>{{{1u, "conservative rasterization of points is unsupported (VGT_PRIMITIVE_TYPE=0x1)"}, {2u, "conservative rasterization of lines is unsupported (VGT_PRIMITIVE_TYPE=0x2)"}, {17u, "conservative rasterization of rectangles is unsupported (VGT_PRIMITIVE_TYPE=0x11)"}}}) {
        queue.userConfig[0x242] = primitive;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, rejected);
    }
    queue.userConfig[0x242] = 4;
    auto geometry = queue;
    geometry.context[0x2d5] = 0x2020;
    geometry.userConfig[0x25b] = (64u << 9u) | 21u;
    geometry.context[0x1ff] = 64;
    geometry.context[0x2ce] = 3;
    geometry.context[0x29b] = 2;
    geometry.context[0x2ab] = 4;
    geometry.shader[0x8a] = 3u << 29u;
    geometry.shader[0x8b] = 3u << 16u;
    for (const auto input : {1u, 2u, 4u}) {
        geometry.userConfig[0x242] = input;
        const auto state = AgcDriver::Graphics::DecodeState(geometry);
        Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Geometry && state.conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT, "OVER_RAST_ENABLE did not overestimate the triangle strips of a geometry shader with input primitive type " + std::to_string(input));
    }
    for (const auto& [output, rejected] : std::array<std::pair<std::uint32_t, std::string_view>, 3>{{{0u, "conservative rasterization of points is unsupported (VGT_GS_OUT_PRIM_TYPE=0x0)"}, {1u, "conservative rasterization of lines is unsupported (VGT_GS_OUT_PRIM_TYPE=0x1)"}, {0x80000002u, "conservative rasterization of per-stream primitive types is unsupported (VGT_GS_OUT_PRIM_TYPE=0x80000002)"}}}) {
        geometry.context[0x29b] = output;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(geometry); }, rejected);
    }
    auto tessellation = queue;
    tessellation.context[0x2d5] = 0x200d;
    tessellation.userConfig[0x242] = 9;
    tessellation.context[0x2d6] = (3u << 8u) | (3u << 14u);
    tessellation.context[0x2db] = 1u | (2u << 2u) | (2u << 5u);
    const auto tessellated = AgcDriver::Graphics::DecodeState(tessellation);
    Require(tessellated.stages.path == AgcDriver::Graphics::ShaderPath::Tessellation && tessellated.conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT, "OVER_RAST_ENABLE did not overestimate clockwise tessellated triangles");
    for (const auto& [parameters, rejected] : std::array<std::pair<std::uint32_t, std::string_view>, 3>{{{1u | (2u << 2u), "conservative rasterization of points is unsupported (VGT_TF_PARAM=0x9)"}, {1u | (2u << 2u) | (1u << 5u), "conservative rasterization of lines is unsupported (VGT_TF_PARAM=0x29)"}, {(2u << 2u) | (1u << 5u), "conservative rasterization of lines is unsupported (VGT_TF_PARAM=0x28)"}}}) {
        tessellation.context[0x2db] = parameters;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(tessellation); }, rejected);
    }
    tessellation.context[0x2d5] = 0x202d;
    tessellation.context[0x2db] = 1u | (2u << 2u) | (2u << 5u);
    tessellation.context[0x29b] = 1;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(tessellation); }, "conservative rasterization of lines is unsupported (VGT_GS_OUT_PRIM_TYPE=0x1)");
    for (const auto inputs : {0x6u, 0x42u}) {
        queue.context[0x1b3] = inputs;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "conservative rasterization with centroid interpolation");
    }
    queue.context[0x1b3] = 2;
    for (const auto control : {0x6003u, 0x6020u, 0x6401u, 0xe06001u, 0x1u}) {
        queue.context[0x313] = control;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "PA_SC_CONSERVATIVE_RASTERIZATION_CNTL=0x");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("PA_SC_CONSERVATIVE_RASTERIZATION_CNTL=0x") != std::string::npos, "the precheck accepted PA_SC_CONSERVATIVE_RASTERIZATION_CNTL " + std::to_string(control));
    }
}

void floatControlsModeTests() {
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    for (const auto width : {16u, 32u, 64u}) {
        vertex.spirv = makeModule({.floatControlsWidth = width});
        pixel.spirv = makeModule({.fragment = true, .floatControlsWidth = width});
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false);
    }
    vertex.spirv = makeModule({.floatControlsWidth = 8u});
    pixel.spirv = makeModule({.fragment = true});
    expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "float controls execution modes");
}

void pixelParameterSlotTests() {
    using AgcDriver::Graphics::CompiledShader;
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    auto state = AgcDriver::Graphics::DecodeState(makeState());
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({.parameterOutput = true});
    const std::array<std::uint32_t, 8> mixed{0xc8100000u, 0xc8110001u, 0xc8160402u, 0xc81a0802u, 0xc81e0f02u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    auto pixel = recompilePixel({0x0u, 0x400u, 0x22u, 0x320u}, mixed);
    auto inputs = locatedInputs(pixel.spirv.Words());
    Require(inputs.size() == 1 && inputs[0].location == 0 && inputs[0].perVertex, "a slot read flat and interpolated did not become one per-vertex input");
    std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
    AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true);
    const std::array<std::uint32_t, 7> shared{0xc8100000u, 0xc8110001u, 0xc8140500u, 0xc8150501u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    pixel = recompilePixel({0x3u, 0x3u}, shared);
    inputs = locatedInputs(pixel.spirv.Words());
    Require(inputs.size() == 1 && inputs[0].location == 3 && inputs[0].perVertex && !inputs[0].flat, "inputs reading one slot were not declared once at the slot");
    pixel = recompilePixel({0x404u, 0x0u}, shared);
    inputs = locatedInputs(pixel.spirv.Words());
    Require(inputs.size() == 2 && inputs[0].location == 0 && inputs[0].perVertex && inputs[1].location == 4 && inputs[1].perVertex, "flat and interpolated inputs of different slots moved");
    pixel = recompilePixel({0x20u, 0x2320u}, shared);
    Require(locatedInputs(pixel.spirv.Words()).empty(), "a defaulted input was declared as a parameter");
    AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true);
    const std::array<std::uint32_t, 7> vertices{0xc8120002u, 0xc8160000u, 0xc81a0001u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto subtracts = [](std::span<const std::uint32_t> words) {
        std::size_t count = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) count += (words[at] & 0xffffu) == spv::OpFSub;
        return count;
    };
    pixel = recompilePixel({0x423u}, vertices);
    inputs = locatedInputs(pixel.spirv.Words());
    Require(inputs.size() == 1 && inputs[0].location == 3 && inputs[0].perVertex, "a pass-through input (OFFSET bit 5 with FLAT_SHADE) was not read per vertex at its slot");
    Require(subtracts(pixel.spirv.Words()) == 0, "v_interp_mov p10/p20 of a pass-through input subtracted vertex 0");
    ShaderRecompiler::RecompileResult slotVertex;
    slotVertex.spirv = makeModule({.parameterOutput = true, .parameterLocation = 3});
    const std::array<CompiledShader, 2> slotShaders{{{ShaderRecompiler::ShaderStage::Vertex, &slotVertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
    AgcDriver::Graphics::ValidateShaders(slotShaders, state, subgroup, true);
    pixel = recompilePixel({0x403u}, vertices);
    inputs = locatedInputs(pixel.spirv.Words());
    Require(inputs.size() == 1 && inputs[0].location == 3 && inputs[0].perVertex && subtracts(pixel.spirv.Words()) == 2, "v_interp_mov p10/p20 of a flat input did not read differences to vertex 0");
    expectFailure([&] { recompilePixel({0x423u, 0x3u}, shared); }, "passes its vertices through unchanged");
}

ShaderRecompiler::RecompileRequest packedExportRequest(const AgcDriver::QueueState& queue, const AgcDriver::Graphics::State& state, std::span<const std::uint32_t> code) {
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state));
    request.context.pixel->targetExportPacking = AgcDriver::Graphics::ExportPackings(state);
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = true;
    return request;
}

void unorm10_11_11TargetTests() {
    using ShaderRecompiler::ColorExportPacking;
    auto queue = makeState();
    queue.context[0x31c] = (queue.context.at(0x31c) & ~0x7cu) | (6u << 2u);
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.format == VK_FORMAT_R32_UINT && state.color.elementBytes == 4 && state.color.bytes == colorMemory.size() && state.color.packing == ColorExportPacking::Unorm10_11_11, "a 10_11_11 unorm target did not decode as a packed R32_UINT attachment");
    const auto packings = AgcDriver::Graphics::ExportPackings(state);
    Require(packings[0] == ColorExportPacking::Unorm10_11_11 && std::all_of(packings.begin() + 1, packings.end(), [](ColorExportPacking packing) { return packing == ColorExportPacking::None; }), "export 0 did not carry the 10_11_11 unorm packing alone");
    auto changed = queue;
    changed.context[0x31c] |= 7u << 8u;
    const auto floating = AgcDriver::Graphics::DecodeState(changed);
    Require(floating.color.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32 && floating.color.packing == ColorExportPacking::None, "a 10_11_11 float target changed");
    changed = queue;
    changed.context[0x31c] |= 1u << 8u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "unsupported color format 6 number type 1");
    changed = queue;
    changed.context[0x1e0] = 0x40010001u;
    for (std::uint32_t i = 0; i < 4; ++i) changed.context[0x105 + i] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "blending into a 10_11_11 unorm color target");
    changed = queue;
    changed.context[0x8e] = 3;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "partial writes of a 10_11_11 unorm color target");
    changed.context[0x8e] = 7;
    Require(AgcDriver::Graphics::DecodeState(changed).color.packing == ColorExportPacking::Unorm10_11_11, "an RGB write of a 10_11_11 unorm target was refused");
    for (const auto format : {5u, 6u}) {
        changed = queue;
        changed.context[0x1c5] = format;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "color export format " + std::to_string(format) + " into a 10_11_11 unorm target");
    }
    changed = queue;
    changed.context[0x1c5] = 4;
    Require(AgcDriver::Graphics::DecodeState(changed).color.packing == ColorExportPacking::Unorm10_11_11, "an FP16_ABGR export into a 10_11_11 unorm target was refused");
    changed = queue;
    changed.context[0x31c] |= 0x10000000u;
    changed.context[0x325] = 0x1234;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "DCC-compressed 10_11_11 unorm color targets");
    changed = queue;
    changed.context[0x31c] |= 0x40000u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "truncating (ROUND_MODE) 10_11_11 unorm color targets");

    constexpr std::array<std::uint32_t, 3> code{0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto request = packedExportRequest(queue, state, code);
    auto plain = request;
    plain.context.pixel->targetExportPacking.fill(ColorExportPacking::None);
    const ShaderRecompiler::RequestSerializer serializer;
    Require(serializer.Deserialize(serializer.Serialize(request)).request.context.pixel->targetExportPacking == request.context.pixel->targetExportPacking, "the 10_11_11 unorm packing was lost in serialization");
    std::vector<std::uint64_t> key;
    std::vector<std::uint64_t> plainKey;
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    ShaderRecompiler::BuildPreparedShaderKey(plain, plainKey);
    Require(key == plainKey, "the export packing entered the static shader ABI");
    const auto prepared = ShaderRecompiler::PrepareShader(plain);
    Require(ShaderRecompiler::MatchesPreparedShader(request, *prepared), "a packed draw did not match the registered pixel shader");
    const auto packed = ShaderRecompiler::Recompile(request);
    const auto unpacked = ShaderRecompiler::Recompile(plain);
    Require(packed.variantId == unpacked.variantId && packed.variantId == ShaderRecompiler::GetPreparedArtifact(*prepared).variantId, "the packed draw did not reuse the registered artifact");
    Require(packed.PipelineVariantId() != unpacked.PipelineVariantId() && packed.spirv.Words() != unpacked.spirv.Words(), "the export packing did not specialize the module");
    Require(ShaderRecompiler::Recompile(request).PipelineVariantId() == packed.PipelineVariantId(), "the packed specialization was not reused");
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    const auto validate = [&](const ShaderRecompiler::RecompileResult& pixel, const AgcDriver::Graphics::State& target) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        return AgcDriver::Graphics::ValidateShaders(shaders, target, subgroup, false);
    };
    Require(validate(packed, state) == std::set<std::uint32_t>{0u}, "the packed export did not reach attachment 0");
    expectFailure([&] { validate(unpacked, state); }, "uint4 words to its packed 10_11_11 unorm attachments");
    auto plainState = state;
    plainState.colors[0].packing = ColorExportPacking::None;
    expectFailure([&] { validate(packed, plainState); }, "float4 colors");
    auto uintExport = request;
    uintExport.context.pixel->targetOutputMode[0] = 7;
    expectFailure([&] { ShaderRecompiler::Recompile(uintExport); }, "fragment output 0 has no 10_11_11 unorm packing path");
}

AgcDriver::QueueState dualSourceQueue() {
    auto queue = makeState();
    queue.context[0x8f] = 0xffu;
    queue.context[0x1c5] = 0x44u;
    queue.context[0x1e0] = 0x40010f01u;
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = 0;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    return queue;
}

struct OutputDecorations {
    std::map<std::uint32_t, std::uint32_t> locations;
    std::map<std::uint32_t, std::uint32_t> indices;
};

OutputDecorations outputDecorations(std::span<const std::uint32_t> words) {
    OutputDecorations result;
    std::set<std::uint32_t> outputs;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpVariable && words[at + 3] == spv::StorageClassOutput) outputs.insert(words[at + 2]);
        if (op != spv::OpDecorate) continue;
        if (words[at + 2] == spv::DecorationLocation) result.locations[words[at + 1]] = words[at + 3];
        if (words[at + 2] == spv::DecorationIndex) result.indices[words[at + 1]] = words[at + 3];
    }
    std::erase_if(result.locations, [&](const auto& entry) { return !outputs.contains(entry.first); });
    return result;
}

void DualSourceBlendTests() {
    auto queue = dualSourceQueue();
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.dualSourceBlend && state.colors.size() == 1 && state.blends.size() == 1, "an ONE + SRC1_COLOR blend did not decode as dual-source blending into one target");
    Require(state.blends[0].srcColorBlendFactor == VK_BLEND_FACTOR_ONE && state.blends[0].dstColorBlendFactor == VK_BLEND_FACTOR_SRC1_COLOR && state.blends[0].srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE && state.blends[0].dstAlphaBlendFactor == VK_BLEND_FACTOR_SRC1_COLOR, "BLEND_SRC1_COLOR did not become VK_BLEND_FACTOR_SRC1_COLOR");
    const std::array<std::pair<std::uint32_t, VkBlendFactor>, 4> factors{{{15u, VK_BLEND_FACTOR_SRC1_COLOR}, {16u, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR}, {17u, VK_BLEND_FACTOR_SRC1_ALPHA}, {18u, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA}}};
    for (const auto& [value, factor] : factors) {
        auto changed = queue;
        changed.context[0x1e0] = 0x60000000u | (value << 24u) | (1u << 16u) | (value << 8u) | value;
        const auto decoded = AgcDriver::Graphics::DecodeState(changed);
        Require(decoded.dualSourceBlend && decoded.blends[0].srcColorBlendFactor == factor && decoded.blends[0].dstColorBlendFactor == factor && decoded.blends[0].srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE && decoded.blends[0].dstAlphaBlendFactor == factor, "blend factor " + std::to_string(value) + " decoded wrongly");
    }
    auto changed = queue;
    changed.context[0x1e0] = 0x00010f01u;
    Require(!AgcDriver::Graphics::DecodeState(changed).dualSourceBlend, "SRC1 factors of a disabled blend enabled dual-source blending");
    changed = queue;
    changed.context[0x1e0] = 0x40010001u;
    Require(!AgcDriver::Graphics::DecodeState(changed).dualSourceBlend, "a blend without SRC1 factors enabled dual-source blending");
    for (const auto operation : {2u, 3u}) {
        changed.context[0x1e0] = 0x40010f01u | (operation << 5u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MIN or MAX operation");
    }
    changed.context[0x1e0] = 0x40010f01u | (4u << 5u);
    Require(AgcDriver::Graphics::DecodeState(changed).blends[0].colorBlendOp == VK_BLEND_OP_REVERSE_SUBTRACT, "a reverse-subtract dual-source blend was refused");
    changed = queue;
    changed.context[0x8f] = 0xfu;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MRT slots 0 and 1");
    changed.context[0x8f] = 0xfffu;
    changed.context[0x1c5] = 0x444u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MRT slots 0 and 1");
    changed.context[0x8f] = 0xf000fu;
    changed.context[0x1c5] = 0x44u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MRT slots 0 and 1");
    alignas(256) static std::array<std::byte, 1024> slotOneMemory{};
    changed = queue;
    for (const auto offset : {0x31bu, 0x31cu, 0x31du}) changed.context[offset + 0xfu] = changed.context.at(offset);
    for (const auto offset : {0x3b0u, 0x3b8u}) changed.context[offset + 1u] = changed.context.at(offset);
    changed.context[0x318 + 0xfu] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(slotOneMemory.data()) >> 8u);
    changed.context[0x390 + 1u] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(slotOneMemory.data()) >> 40u);
    changed.context[0x1e1] = 0;
    changed.context[0x8e] = 0xffu;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "color writes to slot 0 alone");
    for (const auto format : {0x04u, 0x14u, 0x74u, 0x94u}) {
        changed = queue;
        changed.context[0x1c5] = format;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MRT1 export in the MRT0 export format");
    }
    changed = queue;
    changed.context[0x1c5] = 0x99u;
    Require(AgcDriver::Graphics::DecodeState(changed).dualSourceBlend, "a 32_ABGR MRT0 and MRT1 export pair was refused");
    for (const auto mask : {0x7fu, 0xf7u}) {
        changed = queue;
        changed.context[0x8f] = mask;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "MRT1 export with the MRT0 shader mask");
    }
    changed = queue;
    changed.context[0x31c] = 0x8804u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "component-swapped color target");

    constexpr std::array<std::uint32_t, 5> code{0xf800000fu, 0x07060504u, 0xf800181fu, 0x0b0a0908u, 0xbf810000u};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state));
    request.context.pixel->dualSourceBlend = state.dualSourceBlend;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = true;
    auto plain = request;
    plain.context.pixel->dualSourceBlend = false;
    const ShaderRecompiler::RequestSerializer serializer;
    Require(serializer.Deserialize(serializer.Serialize(request)).request.context.pixel->dualSourceBlend, "dual-source blending was lost in serialization");
    std::vector<std::uint64_t> key;
    std::vector<std::uint64_t> plainKey;
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    ShaderRecompiler::BuildPreparedShaderKey(plain, plainKey);
    Require(key == plainKey, "dual-source blending entered the static shader ABI");
    const auto prepared = ShaderRecompiler::PrepareShader(plain);
    Require(ShaderRecompiler::MatchesPreparedShader(request, *prepared), "a dual-source draw did not match the registered pixel shader");
    const auto dual = ShaderRecompiler::Recompile(request);
    const auto single = ShaderRecompiler::Recompile(plain);
    Require(dual.variantId == single.variantId && dual.variantId == ShaderRecompiler::GetPreparedArtifact(*prepared).variantId, "the dual-source draw did not reuse the registered artifact");
    Require(dual.PipelineVariantId() != single.PipelineVariantId() && ShaderRecompiler::Recompile(request).PipelineVariantId() == dual.PipelineVariantId(), "dual-source blending did not key its own specialization");
    const auto dualOutputs = outputDecorations(dual.spirv.Words());
    const auto singleOutputs = outputDecorations(single.spirv.Words());
    Require(dualOutputs.locations.size() == 2 && std::all_of(dualOutputs.locations.begin(), dualOutputs.locations.end(), [](const auto& entry) { return entry.second == 0u; }) && dualOutputs.indices.size() == 1 && dualOutputs.indices.begin()->second == 1u, "the MRT1 export of a dual-source draw was not the location 0 index 1 output");
    std::set<std::uint32_t> singleLocations;
    for (const auto& entry : singleOutputs.locations) singleLocations.insert(entry.second);
    Require(singleOutputs.indices.empty() && singleLocations == std::set<std::uint32_t>{0u, 1u}, "MRT0 and MRT1 without dual-source blending did not keep locations 0 and 1");
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    const auto validate = [&](const ShaderRecompiler::RecompileResult& pixel, const AgcDriver::Graphics::State& target) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        return AgcDriver::Graphics::ValidateShaders(shaders, target, subgroup, false);
    };
    Require(validate(dual, state) == std::set<std::uint32_t>{0u}, "the dual-source shader did not validate against its draw");
    expectFailure([&] { validate(single, state); }, "needs a float4 second color at location 0 index 1");
    auto singleState = state;
    singleState.dualSourceBlend = false;
    expectFailure([&] { validate(dual, singleState); }, "Index decoration");
}

void validationTests() {
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    ShaderRecompiler::RecompileResult fragment;
    fragment.spirv = makeModule({.fragment = true});
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.parameterOutput = true});
        ShaderRecompiler::RecompileResult pixel;
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        for (const auto noPerspective : {false, true}) {
            pixel.spirv = makeModule({.fragment = true, .barycentric = true, .barycentricNoPerspective = noPerspective, .perVertex = true});
            AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true);
            expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "fragmentShaderBarycentric");
        }
        pixel.spirv = makeModule({.fragment = true, .sampleId = true, .layer = true, .fragDepth = true, .sampleMaskLength = 1});
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, true, true);
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, true); }, "unsupported device capability 2");
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, true, false); }, "unsupported device capability 35");
        pixel.spirv = makeModule({.fragment = true, .sampleMaskLength = 2});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported fragment built-in");
        vertex.spirv = makeModule({.parameterOutput = true, .sampleId = true});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported device capability 35");
        vertex.spirv = makeModule({.parameterOutput = true});
        pixel.spirv = makeModule({.fragment = true, .barycentric = true, .barycentricComponents = 4});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true); }, "invalid barycentric built-in");
        pixel.spirv = makeModule({.fragment = true, .barycentric = true, .perVertex = true, .perVertexLength = 2});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true); }, "three vertices");
        pixel.spirv = makeModule({.fragment = true, .perVertex = true});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true); }, "PerVertexKHR requires");
        vertex.spirv = makeModule({.barycentric = true});
        pixel.spirv = makeModule({.fragment = true});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, true); }, "requires a fragment shader");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        ShaderRecompiler::RecompileResult pixel;
        pixel.spirv = makeModule({.fragment = true, .secondTarget = true});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        Require(state.colors.empty() && AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false) == std::set<std::uint32_t>{0u}, "an export past the attachments was not dropped");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.vertexInput = true});
        ShaderRecompiler::VertexAttribute attribute{0, 4, {{0x1000, 32u << 16u, 3, 77u << 12u}}, 0};
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "missing attribute metadata");
        vertex.vertexAttributes.push_back(attribute);
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false);
        vertex.vertexAttributes[0].components = 2;
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "metadata disagrees");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 2, 1) == 80, "incorrect strided vertex range");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1) == 112, "an index past the records still spans its fetch");
        attribute.fetchIndex = 1;
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 48, "instance attributes used the vertex index");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2, 1) == 80, "first instance was ignored");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 2, 2) == 112, "a first instance past the records still spans its fetch");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 2, 0xffffffffu); }, "instance range overflow");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 4) == 112, "an instance range past the records still spans its fetch");
        attribute.fetchIndex = 0;
        attribute.resource.fields[3] = 0x1004dfacu;
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1) == 112, "oob_select 1 past the records still spans its fetch");
        attribute.resource.fields[3] = 0x2004dfacu;
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1); }, "record count");
        attribute.resource.fields[3] = 0x3004dfacu;
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1); }, "record count");
        attribute.resource.fields[3] = 77u << 12u;
        attribute.fetchIndex = 1;
        attribute.resource.fields[1] = 0;
        attribute.resource.fields[2] = 16;
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 16, "zero stride must repeat one value");
        attribute.resource.fields[2] = 8;
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1); }, "byte range");
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "a structured zero-stride fetch was treated as out of range");
        attribute.resource.fields[3] = (77u << 12u) | (2u << 28u);
        attribute.resource.fields[2] = 1;
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 16, "OOB_SELECT 2 must read a zero-stride fetch whole while NUM_RECORDS is nonzero");
        Require(AgcDriver::Graphics::VertexBufferExtent(attribute) == 16, "OOB_SELECT 2 extent must cover the whole fetch");
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 2 with records was treated as out of range");
        attribute.resource.fields[2] = 0;
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 2 without records must read zero");
        attribute.resource.fields[3] = (77u << 12u) | (3u << 28u);
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 without records must read zero");
        attribute.resource.fields[2] = 8;
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 with records was treated as out of range");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1); }, "byte range");
        attribute.resource.fields[2] = 16;
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1) == 16, "OOB_SELECT 3 must read a fetch inside the range");
        attribute.resource.fields[3] = 113u << 12u;
        expectFailure([&] { AgcDriver::Graphics::DecodeVertexFormat(attribute); }, "unsupported vertex format");
        attribute.resource.fields[3] = 50u << 12u;
        Require(AgcDriver::Graphics::DecodeVertexFormat(attribute).format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && AgcDriver::Graphics::DecodeVertexFormat(attribute).bytes == 4, "2_10_10_10 vertex format was not decoded");
        attribute.resource.fields[3] = 55u << 12u;
        Require(AgcDriver::Graphics::DecodeVertexFormat(attribute).format == VK_FORMAT_A2B10G10R10_SINT_PACK32 && std::string_view(AgcDriver::Graphics::DecodeVertexFormat(attribute).scalar) == "i32", "2_10_10_10 sint vertex format was not decoded");
        attribute.resource.fields[3] = 36u << 12u;
        Require(AgcDriver::Graphics::DecodeVertexFormat(attribute).format == VK_FORMAT_B10G11R11_UFLOAT_PACK32, "10_11_11 float vertex format was not decoded");
        for (std::uint32_t format = 1; format < 128u; ++format) {
            attribute.resource.fields[3] = format << 12u;
            std::string_view scalar;
            try {
                scalar = AgcDriver::Graphics::DecodeVertexFormat(attribute).scalar;
            } catch (const std::exception&) {
                continue;
            }
            const auto numeric = ShaderRecompiler::VertexInputNumericClass(static_cast<ShaderRecompiler::IrBufferFormat>(format));
            Require((scalar == "u32") == (numeric == ShaderRecompiler::IrTextureNumericClass::Uint) && (scalar == "i32") == (numeric == ShaderRecompiler::IrTextureNumericClass::Sint), "vertex format " + std::to_string(format) + " declares a shader input of another numeric type than its attribute");
        }
        attribute.resource.fields[3] = 43u << 12u;
        expectFailure([&] { AgcDriver::Graphics::DecodeVertexFormat(attribute); }, "unsupported vertex format");
        attribute.resource.fields = {0x1000, 32u << 16u, 3, 77u << 12u};
        attribute.components = 2;
        AgcDriver::Graphics::Context context{};
        context.limits.maxVertexInputBindings = 16;
        context.limits.maxVertexInputAttributes = 16;
        context.limits.maxVertexInputBindingStride = 2048;
        context.formatProperties = [](VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
            *properties = {};
            if (format == VK_FORMAT_R32G32_SFLOAT) properties->bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
        };
        const auto layout = AgcDriver::Graphics::BuildVertexInputLayout(context, std::span(&attribute, 1));
        Require(layout.bindings.size() == 1 && layout.bindings[0].stride == 32 && layout.bindings[0].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE, "incorrect instance input binding");
        Require(layout.attributes[0].format == VK_FORMAT_R32G32_SFLOAT && layout.attributes[0].offset == 0 && layout.attributes[0].location == 0, "incorrect vertex attribute format or offset");
        attribute.components = 4;
        expectFailure([&] { AgcDriver::Graphics::BuildVertexInputLayout(context, std::span(&attribute, 1)); }, "device does not support vertex format");
        attribute.components = 2;
        attribute.resource.fields[1] |= 0x80000000u;
        expectFailure([&] { AgcDriver::Graphics::BuildVertexInputLayout(context, std::span(&attribute, 1)); }, "descriptor flags");
    }
    for (const auto capability : {spv::CapabilityInt64Atomics, spv::CapabilityInt64ImageEXT}) {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        vertex.spirv.insert(vertex.spirv.begin() + 5, {(2u << 16u) | spv::OpCapability, static_cast<std::uint32_t>(capability)});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported device capability");
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, true);
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        vertex.spirv.insert(vertex.spirv.begin() + 5, {(2u << 16u) | spv::OpCapability, static_cast<std::uint32_t>(spv::CapabilityInt64Atomics)});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported device capability 12");
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, true);
    }
    for (const auto capability : {spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle}) {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        vertex.spirv.insert(vertex.spirv.begin() + 5, {(2u << 16u) | spv::OpCapability, static_cast<std::uint32_t>(capability)});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        subgroup.supportedStages = VK_SHADER_STAGE_VERTEX_BIT;
        subgroup.supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false);
        subgroup.supportedStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported for shader stage");
        subgroup.supportedStages = VK_SHADER_STAGE_VERTEX_BIT;
        subgroup.supportedOperations = capability == spv::CapabilityGroupNonUniform ? 0u : VK_SUBGROUP_FEATURE_BASIC_BIT;
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "device lacks operations");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        vertex.spirv.insert(vertex.spirv.begin() + 5, {(2u << 16u) | spv::OpCapability, static_cast<std::uint32_t>(spv::CapabilityGroupNonUniformArithmetic)});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        subgroup.supportedStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        subgroup.supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "unsupported for shader stage");
        subgroup.supportedStages = VK_SHADER_STAGE_VERTEX_BIT;
        subgroup.supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "device lacks operations");
        subgroup.supportedOperations |= VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false);
    }
    const std::vector<std::uint32_t> words(8, 0);
    const auto validate = [&](const ShaderRecompiler::RecompileResult& vertex, std::uint32_t fragmentOffset) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, fragmentOffset}}};
        AgcDriver::Graphics::ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
    };
    const auto pushed = [&](const ModuleShape& shape) {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule(shape);
        vertex.pushConstants.assign(8, std::byte{1});
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, words));
        return vertex;
    };
    validate(pushed({.push = true, .bufferArray = 2}), 8);
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.shaderData = true});
        vertex.bindings.push_back(makeBinding(Role::ShaderData, 5, 1, {1, 2}));
        validate(vertex, 0);
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.bufferArray = 1, .shaderData = true});
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, {1, 2, 3, 4}));
        vertex.bindings.push_back(makeBinding(Role::ShaderData, 5, 1, {1, 2}));
        validate(vertex, 0);
    }
    expectFailure([&] { validate(pushed({.push = true, .pushLength = 16, .bufferArray = 2}), 8); }, "32 elements");
    expectFailure([&] { validate(pushed({.push = true, .pushStride = 8, .bufferArray = 2}), 8); }, "ArrayStride of 4");
    expectFailure([&] { validate(pushed({.push = false, .bufferArray = 2}), 8); }, "push constant metadata disagrees with SPIR-V");
    {
        auto vertex = pushed({.push = true, .bufferArray = 2});
        vertex.pushConstants.clear();
        expectFailure([&] { validate(vertex, 8); }, "invalid push constant interface");
    }
    {
        auto vertex = pushed({.push = true, .bufferArray = 3});
        expectFailure([&] { validate(vertex, 8); }, "descriptor array length disagrees");
    }
    {
        auto vertex = pushed({.push = true, .plainBuffer = true});
        expectFailure([&] { validate(vertex, 8); }, "must be declared as a descriptor array");
    }
    {
        auto vertex = pushed({.push = true, .bufferArray = 2});
        vertex.bindings.front().readOnly = true;
        expectFailure([&] { validate(vertex, 8); }, "read-only descriptor metadata is unsupported");
    }
    {
        auto vertex = pushed({.push = true, .bufferArray = 2});
        vertex.bindings.front().descriptorSet = 1;
        expectFailure([&] { validate(vertex, 8); }, "descriptor set other than zero");
    }
    {
        auto vertex = pushed({.push = true, .bufferArray = 2});
        vertex.bindings.front().kind = Kind::UniformBuffer;
        expectFailure([&] { validate(vertex, 8); }, "disagrees with recompiler binding metadata");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.shaderData = true});
        vertex.bindings.push_back(makeBinding(Role::ShaderData, 5, 2, {1, 2}));
        expectFailure([&] { validate(vertex, 0); }, "binding count of one");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.shaderData = true});
        vertex.bindings.push_back(makeBinding(Role::GuestSamplers, 5, 1, {1, 2}));
        expectFailure([&] { validate(vertex, 0); }, "descriptor role is unsupported");
    }
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.shaderData = true});
        expectFailure([&] { validate(vertex, 0); }, "absent from recompiler binding metadata");
    }
}


}

bool recompilesDebugBranch(std::uint32_t opcode) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x2u;
    queue.context[0x1b4] = 0x2u;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state));
    const std::array<std::uint32_t, 4> code{0xbf800000u | (opcode << 16u) | 1u, 0xf800180fu, 0x00000000u, 0xbf810000u};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return !ShaderRecompiler::Recompile(request).spirv.Words().empty();
}

void meshIndexBufferTests() {
    const AgcDriver::Pm4::DrawParameters automatic{0, 96, 0, 1, 0, false};
    const auto unindexed = AgcDriver::Graphics::MeshIndexBufferDescriptor(automatic);
    Require(unindexed[0] == 0 && unindexed[1] == 0 && unindexed[2] == 0 && unindexed[3] == 0x31016facu, "a non-indexed mesh draw did not get a null index buffer V#");
    const AgcDriver::Pm4::DrawParameters indexed{0x123456789a00ull, 5, 2, 1, 0, true};
    const auto words = AgcDriver::Graphics::MeshIndexBufferDescriptor(indexed);
    Require(words[0] == 0x56789a00u && words[1] == 0x1234u && words[2] == 12u && words[3] == 0x31016facu, "an indexed mesh draw's index buffer V# changed");
    expectFailure([] { static_cast<void>(AgcDriver::Graphics::MeshIndexBufferDescriptor(AgcDriver::Pm4::DrawParameters{0, 3, 2, 1, 0, true})); }, "invalid mesh index buffer range");
}

void stagingPoolBudgetTests() {
    if (std::getenv("APS5_STAGING_POOL_MIB") != nullptr) return;
    VkPhysicalDeviceMemoryProperties memory{};
    memory.memoryHeapCount = 2;
    memory.memoryHeaps[0] = {VkDeviceSize{32} << 30u, 0};
    memory.memoryHeaps[1] = {VkDeviceSize{16} << 30u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{2} << 30u, "the staging pool budget is not an eighth of the device-local heap");
    memory.memoryHeaps[1].size = VkDeviceSize{2} << 30u;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{512} << 20u, "the staging pool budget fell below 512 MiB");
    memory.memoryHeapCount = 1;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{512} << 20u, "a host heap sized the staging pool budget");
}

void meshArgumentTests() {
    using AgcDriver::Graphics::MeshArguments;
    using AgcDriver::Graphics::ResolveMeshArguments;
    AgcDriver::Graphics::Context context{};
    context.meshLimits.maxMeshWorkGroupCount[0] = 1000;
    context.meshLimits.maxMeshWorkGroupCount[1] = 600;
    context.meshLimits.maxMeshWorkGroupTotalCount = 4000;
    const ShaderRecompiler::MeshConfiguration points{1u, 1u, 1u, 1u, 1u, 64u, 1024u, 0u, 4u};
    const ShaderRecompiler::MeshConfiguration triangles{4u, 32u, 96u, 96u, 32u, 128u, 2048u, 0u, 4u};
    const ShaderRecompiler::MeshConfiguration strip{6u, 8u, 10u, 10u, 8u, 64u, 1024u, 0u, 4u};
    const ShaderRecompiler::MeshConfiguration fan{5u, 8u, 10u, 10u, 8u, 64u, 1024u, 0u, 4u};
    const auto same = [](const MeshArguments& a, const MeshArguments& b) { return a.groups == b.groups && a.instances == b.instances && a.layers == b.layers && a.indexCount == b.indexCount && a.firstIndex == b.firstIndex; };
    const auto rules = [&](const ShaderRecompiler::MeshConfiguration& mesh, std::uint32_t indexCount) { return AgcDriver::Graphics::MeshArgumentRulesFor(context, mesh, indexCount); };
    const auto record = [](std::uint32_t count, std::uint32_t instances, std::uint32_t first) { return AgcDriver::Pm4::DrawArguments{count, instances, first, 0, 0}; };
    Require(same(ResolveMeshArguments(record(1, 512, 0), rules(points, 1)), {1, 512, 1, 1, 0}), "one point, 512 instances");
    Require(same(ResolveMeshArguments(record(1, 0, 0), rules(points, 1)), {0, 0, 0, 1, 0}), "no instances draws nothing");
    Require(same(ResolveMeshArguments(record(0, 4, 0), rules(points, 1)), {0, 0, 0, 0, 0}), "no indices draws nothing");
    Require(same(ResolveMeshArguments(record(96, 2, 0), rules(triangles, 300)), {1, 2, 1, 96, 0}), "one full group");
    Require(same(ResolveMeshArguments(record(99, 2, 0), rules(triangles, 300)), {2, 2, 1, 99, 0}), "a partial second group");
    Require(same(ResolveMeshArguments(record(200, 3, 150), rules(triangles, 300)), {2, 3, 1, 150, 150}), "count clamped to the index buffer");
    Require(same(ResolveMeshArguments(record(9, 1, 300), rules(triangles, 300)), {0, 0, 0, 0, 300}), "first index past the index buffer draws nothing");
    Require(same(ResolveMeshArguments(record(2, 1, 0), rules(triangles, 300)), {0, 0, 0, 2, 0}), "no complete triangle draws nothing");
    Require(same(ResolveMeshArguments(record(10, 1, 0), rules(strip, 64)), {1, 1, 1, 10, 0}), "eight strip triangles are one group");
    Require(same(ResolveMeshArguments(record(11, 1, 0), rules(strip, 64)), {2, 1, 1, 11, 0}), "a ninth strip triangle starts a group");
    Require(same(ResolveMeshArguments(record(10, 1, 0), rules(fan, 64)), {1, 1, 1, 10, 0}), "eight fan triangles are one group");
    Require(same(ResolveMeshArguments(record(11, 1, 0), rules(fan, 64)), {2, 1, 1, 11, 0}), "a ninth fan triangle starts a group");
    Require(same(ResolveMeshArguments(record(1, 601, 0), rules(points, 1)), {0, 0, 0, 1, 0}), "instances over the device limit draw nothing");
    Require(same(ResolveMeshArguments(record(96 * 7, 600, 0), rules(triangles, 96 * 7)), {0, 0, 0, 96 * 7, 0}), "groups times instances over the device limit draw nothing");
    Require(same(ResolveMeshArguments(record(96 * 6, 600, 0), rules(triangles, 96 * 6)), {6, 600, 1, 96 * 6, 0}), "groups times instances at the device limit");
    Require(same(ResolveMeshArguments(record(0xffffffffu, 1, 0xfffffff0u), rules(points, 0xffffffffu)), {15, 1, 1, 15, 0xfffffff0u}), "first index near the end of a huge index buffer");
    Require(same(ResolveMeshArguments(record(0xffffffffu, 1, 0), rules(points, 0xffffffffu)), {0, 0, 0, 0xffffffffu, 0}), "groups over the device limit draw nothing");
}

void debugBranchTests() {
    for (const auto opcode : {0x17u, 0x18u, 0x19u, 0x1au}) Require(recompilesDebugBranch(opcode), "a conditional debug branch did not recompile");
}

void highestDrawIndexTests() {
    using AgcDriver::Graphics::HighestDrawIndex;
    const auto bytesOf = [](const auto& values) { return std::as_bytes(std::span(values)); };
    const std::array<std::uint16_t, 5> narrow{0, 7, 0xffff, 3, 0xffff};
    Require(HighestDrawIndex(bytesOf(narrow), 2, true) == 7, "a 16-bit restart index counted as a vertex");
    Require(HighestDrawIndex(bytesOf(narrow), 2, false) == 0xffff, "a 16-bit all-ones index without restart was skipped");
    const std::array<std::uint32_t, 4> wide{9, 0xffffffffu, 0xffff, 2};
    Require(HighestDrawIndex(bytesOf(wide), 4, true) == 0xffff, "a 32-bit restart index counted as a vertex, or 0xffff was taken for it");
    Require(HighestDrawIndex(bytesOf(wide), 4, false) == 0xffffffffu, "a 32-bit all-ones index without restart was skipped");
    const std::array<std::uint16_t, 4> restartOnly{0xffff, 0xffff, 0xffff, 0xffff};
    Require(!HighestDrawIndex(bytesOf(restartOnly), 2, true).has_value(), "a draw of only restart indices reached a vertex");
    Require(HighestDrawIndex(bytesOf(restartOnly), 2, false) == 0xffff, "a 16-bit all-ones index without restart was skipped");
    Require(HighestDrawIndex(bytesOf(narrow).first(4), 2, true) == 7 && HighestDrawIndex(bytesOf(narrow).first(2), 2, true) == 0, "the scan read past its index range");
    expectFailure([&] { HighestDrawIndex(bytesOf(narrow), 1, false); }, "unsupported index size");
}

void storeAtFlipTests() {
    using AgcDriver::Graphics::StorageTexture;
    Require(!StorageTexture::StoreAtFlipRequested(nullptr), "an unset APS5_STORE_AT_FLIP stored at each flip");
    Require(StorageTexture::StoreAtFlipRequested("1"), "APS5_STORE_AT_FLIP=1 did not store at each flip");
    for (const char* value : {"0", "", "true", "11"}) expectFailure([&] { StorageTexture::StoreAtFlipRequested(value); }, "expected 1");
}

void nullVertexDescriptorTests() {
    using AgcDriver::Graphics::DecodeVertexFormat;
    constexpr std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
    ShaderRecompiler::VertexAttribute attribute{};
    attribute.formatComponents = 1;
    for (std::uint32_t components = 1; components <= 4; ++components) {
        attribute.components = components;
        const auto format = DecodeVertexFormat(attribute);
        Require(format.format == formats[components - 1] && format.bytes == components * 4u && std::string_view(format.scalar) == "f32", "a null V# did not read as zero floats of the attribute's width");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100000, 3, 2) == format.bytes && AgcDriver::Graphics::VertexBufferExtent(attribute) == format.bytes, "a null V# was sized as a guest range");
    }
    attribute.resource.fields = {0x1000, 0, 0, 0};
    expectFailure([&] { DecodeVertexFormat(attribute); }, "unsupported vertex format");
}

void vertexCopyTests() {
    using AgcDriver::Graphics::PlanVertexCopies;
    using AgcDriver::Graphics::VertexFetch;
    {
        const std::array<VertexFetch, 3> fetches{{{0x1018, 0x1018 + 32 * 9 + 8, 32, 0, 4}, {0x1000, 0x1000 + 32 * 9 + 12, 32, 0, 4}, {0x100c, 0x100c + 32 * 9 + 12, 32, 0, 4}}};
        const auto plan = PlanVertexCopies(fetches);
        Require(plan.copies.size() == 1 && plan.copies[0].first == 0x1000 && plan.copies[0].second == 0x1018 + 32 * 9 + 8, "interleaved attributes were not copied as one union");
        Require(plan.copyOf == std::vector<std::size_t>{0, 0, 0} && plan.offsets == std::vector<std::uint64_t>{0x18, 0, 0xc}, "interleaved attribute offsets are wrong");
        Require(plan.alignments == std::vector<std::uint32_t>{4}, "the union's alignment is not its attributes'");
    }
    {
        const std::array<VertexFetch, 2> fetches{{{0x4000, 0x4100, 16, 0, 2}, {0x4004, 0x4104, 16, 0, 4}}};
        const auto plan = PlanVertexCopies(fetches);
        Require(plan.copies.size() == 1 && plan.alignments == std::vector<std::uint32_t>{4}, "a union did not take its strictest attribute alignment");
    }
    {
        const std::array<VertexFetch, 6> fetches{{
            {0x2000, 0x2100, 32, 0, 4},
            {0x2004, 0x2100, 16, 0, 4},
            {0x2020, 0x2120, 32, 0, 4},
            {0x2002, 0x2102, 32, 0, 4},
            {0x2008, 0x2108, 32, 1, 4},
            {0x2000, 0x2010, 0, 0, 4},
        }};
        const auto plan = PlanVertexCopies(fetches);
        Require(plan.copies.size() == 6, "fetches of other records, strides, rates or alignments shared a copy");
        for (std::size_t i = 0; i < fetches.size(); ++i) {
            const auto& copy = plan.copies[plan.copyOf[i]];
            Require(plan.offsets[i] == 0 && copy.first == fetches[i].begin && copy.second == fetches[i].end, "a lone fetch was not copied exactly");
        }
    }
    {
        const std::array<VertexFetch, 2> fetches{{{0x3000, 0x3100, 24, 0, 2}, {0x3002, 0x3102, 24, 0, 2}}};
        const auto plan = PlanVertexCopies(fetches);
        Require(plan.copies.size() == 1 && plan.copies[0].second == 0x3102 && plan.offsets[1] == 2, "aligned 16-bit attributes of one record were not merged");
    }
    {
        const std::array<VertexFetch, 1> empty{{{0x4000, 0x4000, 16, 0, 4}}};
        expectFailure([&] { PlanVertexCopies(empty); }, "empty vertex fetch");
    }
}

void vertexZeroFillTests() {
    using AgcDriver::Graphics::CopyZeroPaddedDrawInput;
    using AgcDriver::Graphics::VertexBufferReadSize;
    constexpr std::uint32_t stride = 24;
    constexpr std::uint32_t records = 3;
    const ShaderRecompiler::VertexAttribute attribute{0, 4, {{0x1000u, stride << 16u, records, 77u << 12u}}, 0};
    const auto extent = VertexBufferReadSize(attribute, records, 1);
    const auto valid = static_cast<std::size_t>(stride) * records;
    std::vector<std::byte> guest(extent);
    std::fill(guest.begin(), guest.begin() + static_cast<std::ptrdiff_t>(valid), std::byte{0x7a});
    std::fill(guest.begin() + static_cast<std::ptrdiff_t>(valid), guest.end(), std::byte{0x55});
    const auto copy = CopyZeroPaddedDrawInput(mockContext(), reinterpret_cast<std::uint64_t>(guest.data()), extent, valid);
    const auto bytes = copy.buffer->Bytes();
    Require(bytes.size() == extent, "a zero-padded vertex fetch changed its size");
    Require(std::all_of(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(valid), [](std::byte value) { return value == std::byte{0x7a}; }), "a vertex fetch did not copy the bytes within the record count");
    Require(std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(valid), bytes.end(), [](std::byte value) { return value == std::byte{0}; }), "a vertex fetch past the record count did not read as zeros");
}

void vertexZeroFillMergeTests() {
    using AgcDriver::Graphics::PlanVertexCopies;
    using AgcDriver::Graphics::SoloZeroPaddedFetchIndices;
    using AgcDriver::Graphics::VertexFetch;
    const std::vector<VertexFetch> fetches{{0x1000u, 0x1000u + 88u, 24u, 0u, 4u}, {0x1010u, 0x1010u + 80u, 24u, 0u, 4u}};
    Require(PlanVertexCopies(fetches).copies.size() == 1, "interleaved attributes share one vertex copy");
    Require(SoloZeroPaddedFetchIndices(fetches, {88u, 80u}).empty(), "fully valid fetches share the straight copy");
    const auto solo = SoloZeroPaddedFetchIndices(fetches, {88u, 72u});
    Require(solo.size() == 1 && solo[0] == 1, "a fetch past the records is copied alone");
}

void parallelCompareTests() {
    using AgcDriver::Graphics::CompareSpan;
    using AgcDriver::Graphics::CompareSpansFrom;
    constexpr std::size_t block = 65536;
    constexpr std::size_t count = 48;
    std::vector<std::byte> first(block * count + 100, std::byte{7});
    auto second = first;
    const std::array<std::size_t, 4> changedAt{3 * block + 17, 20 * block, 33 * block + block - 1, 47 * block + 120};
    for (const auto at : changedAt) second[at] = std::byte{9};
    std::vector<CompareSpan> spans;
    for (std::size_t k = 0; k < count; ++k) {
        const auto length = k + 1 == count ? block + 100 : block;
        spans.push_back({first.data() + k * block, second.data() + k * block, length});
    }
    const auto expected = [&](std::size_t k) {
        return std::none_of(changedAt.begin(), changedAt.end(), [&](std::size_t at) { return at >= k * block && at < k * block + spans[k].bytes; });
    };
    for (const std::size_t threshold : {std::size_t{0}, std::size_t{1} << 40u}) {
        for (int round = 0; round < 20; ++round) {
            std::vector<std::uint8_t> equal(count, 2);
            CompareSpansFrom(spans, equal, threshold);
            for (std::size_t k = 0; k < count; ++k) Require((equal[k] == 1) == expected(k) && equal[k] <= 1, "parallel compare misreported a span");
        }
    }
    std::vector<std::uint8_t> left(count, 2), right(count, 2);
    std::thread other([&] {
        for (int round = 0; round < 50; ++round) CompareSpansFrom(spans, right, 0);
    });
    for (int round = 0; round < 50; ++round) CompareSpansFrom(spans, left, 0);
    other.join();
    for (std::size_t k = 0; k < count; ++k) Require((left[k] == 1) == expected(k) && (right[k] == 1) == expected(k), "concurrent parallel compares misreported a span");
    std::vector<std::uint8_t> none;
    CompareSpansFrom({}, none, 0);
}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIN_WAIT_MS", "200");
    _putenv_s("APS5_HEAP_MIRROR_MIB", "4");
#else
    setenv("APS5_PIN_WAIT_MS", "200", 1);
    setenv("APS5_HEAP_MIRROR_MIB", "4", 1);
#endif
    try {
        {
            const AgcDriver::Graphics::Context context{};
            const AgcDriver::Graphics::State state{};
            AgcDriver::Pm4::DrawParameters draw{0, 0, 0, 1, 0, false};
            AgcDriver::Graphics::Draw(context, state, draw, {});
            draw.indexCount = 3;
            draw.instanceCount = 0;
            AgcDriver::Graphics::Draw(context, state, draw, {});
            draw.instanceCount = 2;
            draw.firstInstance = 0xffffffffu;
            expectFailure([&] { AgcDriver::Graphics::Draw(context, state, draw, {}); }, "instance range overflow");
            draw.firstInstance = 0;
            draw.firstVertex = 0xffffffffu;
            expectFailure([&] { AgcDriver::Graphics::Draw(context, state, draw, {}); }, "vertex range overflow");
            draw.firstVertex = 0;
            draw.indexAddress = 1;
            expectFailure([&] { AgcDriver::Graphics::Draw(context, state, draw, {}); }, "must not reference an index buffer");
            draw.indexAddress = 0;
            draw.flags = 1;
            expectFailure([&] { AgcDriver::Graphics::Draw(context, state, draw, {}); }, "draw modifiers");
            auto bounded = state;
            bounded.depthBoundsTest = true;
            bounded.minDepthBounds = 0.25f;
            AgcDriver::Graphics::ValidateDepthBounds(context, bounded);
            bounded.minDepthBounds = 1.5f;
            expectFailure([&] { AgcDriver::Graphics::ValidateDepthBounds(context, bounded); }, "depth bounds outside [0, 1]");
            auto unrestricted = context;
            unrestricted.depthRangeUnrestricted = true;
            AgcDriver::Graphics::ValidateDepthBounds(unrestricted, bounded);
        }
        RunGuestLeaseWaitTests();
        stateTests();
        depthMaintenanceTests();
        hardwareScreenOffsetTests();
        srgb8TargetTests();
        DepthClipTests();
        DepthStencilTests();
        OneDimensionalColorTests();
        htileDepthClearTests();
        ZExportTests();
        unorm10_11_11TargetTests();
        DepthBoundsBiasTests();
        conservativeZExportTests();
        orderedPixelShaderTests();
        ProvokingVertexTests();
        ProvokingVertexRenderPassTests();
        ConservativeRasterizationTests();
        DisabledColorTests();
        OpaqueDestinationAlphaTests();
        CompactedExportTests();
        ReversedComponentOrderTests();
        DualSourceBlendTests();
        metadataPassTests();
        cmaskTests();
        uint16ExportTests();
        uint8x4TargetTests();
        ShaderStageTests();
        TuningFieldTests();
        PixelInputLayoutTests();
        ColorPipeBankXorTests();
        ComputeScratchTests();
        shaderUserDataTailPaddingTests();
        InitialContextTests();
        pushConstantTests();
        resourceTests();
        descriptorSnapshotTests();
        descriptorCacheTests();
        misalignedShaderDataTests();
        debugBranchTests();
        textureCacheBudgetTests();
        sampledTextureBudgetTests();
        sampledBudgetReportTests();
        meshArgumentTests();
        meshIndexBufferTests();
        stagingPoolBudgetTests();
        validationTests();
        vertexCopyTests();
        highestDrawIndexTests();
        storeAtFlipTests();
        nullVertexDescriptorTests();
        vertexZeroFillTests();
        vertexZeroFillMergeTests();
        parallelCompareTests();
        pixelParameterSlotTests();
        rectListTests();
        floatControlsModeTests();
        barycentricEmulationTests();
        mock = MockVulkan{};
        auto bdaContext = mockContext();
        bdaContext.bufferDeviceAddress = true;
        bdaContext.limits.maxStorageBufferRange = 1u << 27;
        RunBdaResourceTests(bdaContext, {
            [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); },
            [](std::uint32_t binding) {
                for (auto it = mock.writes.rbegin(); it != mock.writes.rend(); ++it) {
                    if (it->binding == binding) return it->buffers.at(0);
                }
                throw std::runtime_error("missing BDA test descriptor");
            },
            [](VkDeviceAddress address) {
                const auto offset = address - 0x100000000000ULL;
                const auto buffer = reinterpret_cast<VkBuffer>(offset / 0x10000);
                return mockBufferMemory(buffer).subspan(offset % 0x10000);
            },
            [](std::optional<VkDeviceSize> headroom) {
                mock.memoryLimit = headroom.has_value() ? std::optional<VkDeviceSize>(mock.allocatedBytes + *headroom) : std::nullopt;
            },
            [] { return mock.allocationAttempts; }
        });
        Require(mock.live == 0, "BDA resources leaked Vulkan objects");
        RunGuestAllocationTests();
        RunUnmappedGapTests();
        RunColorTargetLayoutTests();
        RunLiveStackAccessTests();
        RunTextureFormatTests();
        RunTextureTilingTests();
        RunGuestTextureResourceTests();
        RunDepthSurfaceReuseTests();
        RunGuestSamplerResourceTests();
        mock = MockVulkan{};
        auto textureDetilerContext = mockContext();
        textureDetilerContext.limits.minStorageBufferOffsetAlignment = 16;
        textureDetilerContext.limits.maxStorageBufferRange = 256;
        RunTextureDetilerTests(textureDetilerContext, {
            [](std::uint64_t size) {
                VkBuffer buffer{};
                VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                bufferInfo.size = size;
                bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                mockCreateBuffer(VK_NULL_HANDLE, &bufferInfo, nullptr, &buffer);
                VkMemoryRequirements requirements{};
                mockGetBufferMemoryRequirements(VK_NULL_HANDLE, buffer, &requirements);
                VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                allocationInfo.allocationSize = requirements.size;
                VkDeviceMemory memory{};
                mockAllocateMemory(VK_NULL_HANDLE, &allocationInfo, nullptr, &memory);
                mockBindBufferMemory(VK_NULL_HANDLE, buffer, memory, 0);
                return buffer;
            },
            [](VkBuffer buffer) -> std::vector<std::byte>& { return mock.memories.at(mock.bufferMemory.at(buffer)); },
            [] {
                Require(mock.writes.size() >= 2, "expected texture detiling descriptor writes");
                const auto& destinationWrite = mock.writes.back();
                const auto& sourceWrite = mock.writes[mock.writes.size() - 2];
                DetilerCapture capture{};
                capture.groupsX = mock.lastDispatchGroups.x;
                capture.groupsY = mock.lastDispatchGroups.y;
                capture.groupsZ = mock.lastDispatchGroups.z;
                capture.pushConstants = mock.lastPushConstants;
                capture.sourceBuffer = sourceWrite.buffers.at(0).buffer;
                capture.sourceOffset = sourceWrite.buffers.at(0).offset;
                capture.sourceRange = sourceWrite.buffers.at(0).range;
                capture.destinationBuffer = destinationWrite.buffers.at(0).buffer;
                capture.destinationOffset = destinationWrite.buffers.at(0).offset;
                capture.destinationRange = destinationWrite.buffers.at(0).range;
                return capture;
            },
            [] { return mock.pipelineCreateCount; },
            [] { return mock.pipelineSpecializations.back(); }
        });
        std::cout << "Graphics validation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
