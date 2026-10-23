#include "BdaTests.hpp"
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "ShaderDiskCache.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "CacheKey.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "BdaAbi.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "ImageTableAbi.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include <spirv/unified1/spirv.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <initializer_list>
#include <iostream>
#include <limits>
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

extern "C" std::terminate_handler APS5_VABI _ZSt13set_terminatePFvvE_nid_postfix(std::terminate_handler handler);

namespace {

using AgcDriver::Graphics::Require;

alignas(256) std::array<std::byte, 1024> colorMemory{};
alignas(256) std::array<std::byte, 2048> sliceMemory{};
alignas(256) std::array<std::uint8_t, 4> volumeKeyMemory{};

AgcDriver::QueueState makeState() {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x1b8, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
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

void inactiveVertexPointSizeTests() {
    const auto pointSizeState = [] {
        auto queue = makeState();
        queue.context[0x1b3] = queue.context[0x1b4] = 2;
        queue.context[0x207] = 0x01210000;
        return queue;
    };
    for (const auto primitive : {2u, 4u, 5u, 6u, 7u, 17u, 19u, 21u}) {
        auto queue = pointSizeState();
        queue.userConfig[0x242] = primitive;
        for (const auto raster : {0u, 0x240u, 0x248u}) {
            queue.context[0x205] = raster;
            const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
            Require(rejection.empty(), "inactive vertex point size failed the draw precheck: " + rejection);
            static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
        }
    }
    for (const auto raster : {0u, 0x240u, 0x248u}) {
        auto queue = pointSizeState();
        queue.userConfig[0x242] = 3;
        queue.context[0x205] = raster;
        const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
        Require(rejection.empty(), "inactive line-strip point size failed the draw precheck: " + rejection);
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP && !state.primitiveRestart, "inactive point size changed line-strip decoding");
        Require(AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Line, "inactive point size changed the line primitive class");
        queue.userConfig[0x24b] = 1;
        queue.context[0x103] = 0xffffffffu;
        const auto indexedRejection = AgcDriver::Graphics::DrawRejection(queue, true);
        Require(indexedRejection.empty(), "inactive line-strip point size with primitive restart failed the draw precheck: " + indexedRejection);
        const auto indexedState = AgcDriver::Graphics::DecodeState(queue);
        Require(indexedState.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP && indexedState.primitiveRestart, "inactive point size changed indexed line-strip decoding");
    }
    const auto rejected = [](AgcDriver::QueueState queue) {
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("auxiliary vertex exports") != std::string::npos, "unsupported vertex exports passed the draw precheck");
        try {
            static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
        } catch (const std::runtime_error&) {
            return;
        }
        throw std::runtime_error("potentially active point size passed the state decoder");
    };
    {
        auto queue = pointSizeState();
        queue.userConfig[0x242] = 9;
        rejected(queue);
    }
    {
        auto queue = pointSizeState();
        queue.userConfig[0x242] = 1;
        queue.context[0x2d5] = 0x2020;
        rejected(queue);
    }
    for (const auto stages : {0x2004u, 0x2020u}) {
        auto queue = pointSizeState();
        queue.context[0x2d5] = stages;
        rejected(queue);
    }
    for (const auto raster : {8u, 0x208u, 0x48u}) {
        auto queue = pointSizeState();
        queue.context[0x205] = raster;
        rejected(queue);
    }
    for (const auto flag : {1u << 17u, 1u << 20u}) {
        auto queue = pointSizeState();
        queue.context[0x207] = 0x01210000 | flag;
        rejected(queue);
    }
    for (const auto distances : {1u, 0x100u, 0x00c000ffu, 0x00c0ff00u}) {
        auto queue = pointSizeState();
        queue.context[0x207] |= distances;
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "inactive point size rejected supported clip/cull distances");
        Require(AgcDriver::Graphics::DecodeState(queue).paClVsOutCntl == queue.context[0x207], "inactive point size discarded clip/cull distance state");
    }
    {
        auto queue = pointSizeState();
        queue.context[0x207] |= 0x004001ffu;
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("more than eight") != std::string::npos, "inactive point size bypassed the clip/cull count limit");
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "more than eight clip and cull distances");
    }
    for (const auto offset : {0x2d5u, 0x205u}) {
        auto queue = pointSizeState();
        queue.context.erase(offset);
        rejected(queue);
    }
    auto queue = pointSizeState();
    queue.userConfig.erase(0x242);
    rejected(queue);
}

void PolygonPrimitiveTests() {
    auto queue = makeState();
    queue.userConfig[0x242] = 21;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "a polygon did not decode to a triangle fan");
    Require(!state.primitiveRestart, "polygon decoding enabled primitive restart by default");
    Require(AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Triangle, "a polygon was not classified as a triangle primitive");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected a direct polygon draw");
    queue.userConfig[0x24b] = 1;
    queue.context[0x103] = 0xffffffffu;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "the precheck rejected an indexed polygon draw with restart");
    const auto indexedState = AgcDriver::Graphics::DecodeState(queue);
    Require(indexedState.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN && indexedState.primitiveRestart, "indexed polygon decoding did not keep primitive restart");
}

void vertexPointListTests() {
    const auto pointState = [](std::uint32_t control, std::uint32_t size, std::uint32_t limits) {
        auto queue = makeState();
        queue.userConfig[0x242] = 1;
        queue.context[0x1b3] = queue.context[0x1b4] = 2;
        queue.context[0x207] = control;
        queue.context[0x280] = size;
        queue.context[0x281] = limits;
        return queue;
    };
    {
        const auto queue = pointState(0x01210000u, 0x00080008u, 0x00080008u);
        const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
        Require(rejection.empty(), "a vertex-path point list with a vertex point size failed the draw precheck: " + rejection);
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && state.paClVsOutCntl == 0x01210000u, "a vertex-path point list with a vertex point size did not decode as a point list");
        Require(AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Point, "a vertex-path point list is not rasterized as points");
        Require(state.vertexPoints && state.pointSize == 1.0f && state.pointSizeMin == 1.0f && state.pointSizeMax == 1.0f, "PA_SU_POINT_SIZE and PA_SU_POINT_MINMAX of 8 did not decode as one-pixel points");
        ShaderRecompiler::ShaderVertexStageInfo vertex{};
        vertex.paClVsOutCntl = 1u << 21u;
        AgcDriver::Graphics::ApplyPointSize(state, vertex);
        Require(vertex.pointList && vertex.paClVsOutCntl == ((1u << 16u) | (1u << 21u)) && vertex.pointSize == 1.0f && vertex.pointSizeMin == 1.0f && vertex.pointSizeMax == 1.0f, "the vertex stage did not receive the vertex point size");
    }
    {
        const auto state = AgcDriver::Graphics::DecodeState(pointState(0, 0x00200020u, 0x00100004u));
        Require(state.vertexPoints && state.pointSize == 4.0f && state.pointSizeMin == 0.5f && state.pointSizeMax == 2.0f, "a register point size did not decode as diameters");
        ShaderRecompiler::ShaderVertexStageInfo vertex{};
        vertex.paClVsOutCntl = 1u << 21u;
        AgcDriver::Graphics::ApplyPointSize(state, vertex);
        Require(vertex.pointList && vertex.paClVsOutCntl == (1u << 21u) && vertex.pointSize == 4.0f, "a register point size turned on the vertex point size");
    }
    {
        const auto state = AgcDriver::Graphics::DecodeState(pointState(0x01210000u, 0x00200010u, 0x00400000u));
        Require(state.pointSizeMin == 0.0f && state.pointSizeMax == 8.0f, "a vertex point size refused a register size whose width and height differ");
    }
    {
        auto queue = makeState();
        queue.context[0x1b3] = queue.context[0x1b4] = 2;
        queue.context[0x207] = 0x01210000u;
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(!state.vertexPoints, "a triangle draw decoded as vertex points");
        ShaderRecompiler::ShaderVertexStageInfo vertex{};
        AgcDriver::Graphics::ApplyPointSize(state, vertex);
        Require(!vertex.pointList && vertex.paClVsOutCntl == 0, "a triangle draw received a point size");
    }
    expectFailure([&] { AgcDriver::Graphics::DecodeState(pointState(0, 0x00200010u, 0x00080008u)); }, "points whose PA_SU_POINT_SIZE width and height differ are unsupported");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(pointState(0, 0x00080008u, 0x00080010u)); }, "PA_SU_POINT_MINMAX with a minimum above its maximum is unsupported");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(pointState(1u << 16u, 0x00080008u, 0x00080008u)); }, "USE_VTX_POINT_SIZE without VS_OUT_MISC_VEC_ENA is unsupported");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(pointState(0, 0, 0x00080008u)); }, "points of size 0 (PA_SU_POINT_SIZE 0) are unsupported");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(pointState(0x01210000u, 0x00080008u, 0)); }, "a vertex point size clamped to 0 (PA_SU_POINT_MINMAX maximum 0) is unsupported");
    {
        const auto state = AgcDriver::Graphics::DecodeState(pointState(0x01210000u, 0, 0x00200000u));
        Require(state.pointSize == 0.0f && state.pointSizeMin == 0.0f && state.pointSizeMax == 4.0f, "a vertex point size refused a zero register size or a zero clamp minimum");
        static_cast<void>(AgcDriver::Graphics::DecodeState(pointState(0, 0x00080008u, 0)));
    }
    for (const auto sprite : {1u << 17u, 1u << 23u}) {
        auto queue = pointState(0x01210000u, 0x00080008u, 0x00080008u);
        queue.context[0x1b5] = 0x486bu;
        queue.context[0x1b6] = 2;
        queue.context[0x191] = 0;
        queue.context[0x192] = sprite;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "point sprite texture coordinates (SPI_PS_INPUT_CNTL PT_SPRITE_TEX) are not implemented");
        queue.context[0x1b5] = 0x4869u;
        Require(AgcDriver::Graphics::DecodeState(queue).vertexPoints, "PT_SPRITE_TEX was refused with point sprites off");
    }
}

void retainedPixelStageTests() {
    using namespace AgcDriver::Graphics;
    auto queue = makeState();
    queue.context[0x8e] = queue.context[0x8f] = queue.context[0x203] = 0;
    for (const auto format : {0u, 1u, 2u, 3u, 9u}) {
        queue.context[0x1c4] = format;
        Require(PixelProgramSkipped(queue), "a retained export format forced an unused pixel program to run");
        Require(DrawRejection(queue, true).empty(), "an unused pixel program with a retained export format was rejected");
        for (const auto control : {1u, 2u, 4u, 0x40u, 0x80u, 0x100u, 0x200u, 0x400u, 0x20000u}) {
            queue.context[0x203] = control;
            Require(!PixelProgramSkipped(queue), "an enabled export or execution flag was skipped");
        }
        queue.context[0x203] = 0;
        queue.context[0x8e] = 0xf0000000u;
        queue.context[0x8f] = 0x0f000000u;
        Require(PixelProgramSkipped(queue), "disjoint color masks ran an unused pixel program");
        queue.context[0x8f] = 0xf0000000u;
        Require(!PixelProgramSkipped(queue), "an enabled high render target lost its pixel program");
        queue.context[0x8e] = queue.context[0x8f] = 0;
    }
    queue.context[0x1b3] = queue.context[0x1b4] = 2;
    for (const auto [format, control] : {std::pair{1u, 1u}, std::pair{2u, 2u}, std::pair{9u, 0x100u}, std::pair{9u, 0x103u}}) {
        queue.context[0x1c4] = format;
        queue.context[0x203] = control;
        const auto state = DecodeState(queue);
        Require(DrawRejection(queue, true).empty(), "a valid pixel export combination was rejected");
        const auto pixel = DecodePixelStageInfo(queue.context, ExportMappings(state), PixelProgramSkipped(queue));
        Require(pixel.depthExportEnable == ((control & 1u) != 0) && pixel.stencilRefExportEnable == ((control & 2u) != 0) && pixel.sampleMaskExportEnable == ((control & 0x100u) != 0), "decoding lost an enabled pixel export");
    }
    queue.context[0x203] = 0;
    for (const auto alpha : {0xaa00u, 0xaa01u}) {
        queue.context[0x2dc] = alpha;
        Require(PixelProgramSkipped(queue) == ((alpha & 1u) == 0), "alpha-to-coverage enable was not respected");
    }
    queue.context[0x2dc] = 0xaa00u;
    for (const auto offset : {0x8eu, 0x8fu, 0x203u}) {
        auto missing = queue;
        missing.context.erase(offset);
        Require(!PixelProgramSkipped(missing), "incomplete execution state skipped a bound pixel program");
    }
}

void hdrComponentSwapTests() {
    using namespace AgcDriver::Graphics;
    for (const auto number : {0u, 1u, 4u, 7u}) {
        auto queue = makeState();
        queue.context[0x31c] = (queue.context[0x31c] & ~0x1f7cu) | (12u << 2u) | (number << 8u) | (1u << 11u);
        const auto address = reinterpret_cast<std::uintptr_t>(sliceMemory.data());
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        queue.context[0x1b3] = queue.context[0x1b4] = 2;
        auto state = DecodeState(queue);
        const auto expected = number == 0 ? VK_FORMAT_R16G16B16A16_UNORM : number == 1 ? VK_FORMAT_R16G16B16A16_SNORM : number == 4 ? VK_FORMAT_R16G16B16A16_UINT : VK_FORMAT_R16G16B16A16_SFLOAT;
        Require(state.color.format == expected && state.color.elementBytes == 8, "alternate 16-bit color format was not decoded");
        Require(state.color.componentMapping == 0xc6u && ExportMappings(state)[0] == 0xc6u, "alternate 16-bit color export did not swap red and blue");
        Require(DecodePixelStageInfo(queue.context, ExportMappings(state)).targetExportMapping[0] == 0xc6u, "pixel export lost the alternate color mapping");
        queue.context[0x8e] = 1;
        state = DecodeState(queue);
        Require(state.blend.colorWriteMask == VK_COLOR_COMPONENT_B_BIT, "alternate red-only write did not select the third storage component");
        queue.context[0x8e] = 0xf;
        queue.context[0x1e0] = (1u << 30u) | 13u;
        for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = std::bit_cast<std::uint32_t>(float(i + 1) / 8.0f);
        state = DecodeState(queue);
        Require(state.blendConstants == std::array<float, 4>{0.375f, 0.25f, 0.125f, 0.5f}, "alternate color blend constants were not swizzled with the export");
        queue.context[0x31c] &= ~(1u << 11u);
        state = DecodeState(queue);
        Require(state.color.componentMapping == 0xe4u && state.blendConstants == std::array<float, 4>{0.125f, 0.25f, 0.375f, 0.5f}, "standard color mapping changed");
        queue.context[0x31c] |= 2u << 11u;
        expectFailure([&] { DecodeState(queue); }, "reversed component order");
    }
    auto queue = makeState();
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1f7cu) | (12u << 2u) | (7u << 8u) | (1u << 11u);
    for (const auto offset : {0x318u, 0x31bu, 0x31cu, 0x31du}) queue.context[offset + 0xfu] = queue.context.at(offset);
    for (const auto offset : {0x390u, 0x3b0u, 0x3b8u}) queue.context[offset + 1u] = queue.context.at(offset);
    queue.context[0x31c + 0xfu] &= ~(1u << 11u);
    queue.context[0x1c5] = 0x99u;
    queue.context[0x8e] = queue.context[0x8f] = 0xffu;
    queue.context[0x1e0] = queue.context[0x1e1] = (1u << 30u) | 13u;
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = std::bit_cast<std::uint32_t>(float(i + 1) / 8.0f);
    expectFailure([&] { DecodeState(queue); }, "conflicting blend constants");
    queue.context[0x107] = queue.context[0x105];
    Require(DecodeState(queue).colors.size() == 2, "compatible constants rejected mixed component orders");
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
    queue.userConfig[0x242] = 21;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "primitive restart was rejected for an indexed polygon");
    Require(AgcDriver::Graphics::DecodeState(queue).primitiveRestart, "primitive restart was not decoded for a polygon");
    queue.context[0x103] = 5;
    Require(AgcDriver::Graphics::DrawRejection(queue, true).find("all ones") != std::string::npos, "a restart index other than all ones was accepted");
    queue = makeState();
    queue.context[0x293] = 0x06020000u;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("out-of-order") == std::string::npos, "per-engine primitive discard was rejected");
    queue.context[0x293] = 0x760201bcu;
    (void)AgcDriver::Graphics::DecodeState(queue);
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("out-of-order") == std::string::npos, "an out-of-order watermark without out-of-order rasterization was rejected");
    queue.context[0x293] = 0x7e0201bcu;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("out-of-order") != std::string::npos, "out-of-order rasterization was accepted");
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "out-of-order");
    queue.context[0x293] = 0x06030000u;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("out-of-order") == std::string::npos, "per-sample iteration of a single-sample target was rejected");
    Require(!AgcDriver::Graphics::DecodeState(queue).perSampleShading, "per-sample iteration of a single-sample target requested sample shading");
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
    queue.context[0x90] = 0x80040003;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.scissor.extent.height == 0, "a scissor whose top is below its bottom passes pixels");
    queue.context[0x90] = 0x80010021;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.scissor.extent.width == 0, "a scissor whose left edge is right of its right edge passes pixels");
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
    queue.context[0x010] = 0x80000183;
    queue.context[0x011] = 0x20000180;
    queue.context[0x012] = 0x00001000;
    queue.context[0x014] = 0x00001000;
    queue.context[0x007] = 0x0001001f;
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x00a] = 0;
    queue.context[0x00b] = 0x3f800000;
    queue.context[0x200] = 0x007007b1;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depth.has_value() && !state.stencilTest && state.renderExtent.width == state.color.extent.width && state.renderExtent.height == state.color.extent.height, "a stencil test without a stencil plane bound the depth plane it does not test");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a stencil test without a stencil plane beside an untested depth plane was rejected");
    queue.context[0x200] = 0x00700231;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depth.has_value() && !state.stencilTest && state.renderExtent.width == state.color.extent.width && state.renderExtent.height == state.color.extent.height, "a failing stencil test without a stencil plane bound the depth plane it does not test");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a failing stencil test without a stencil plane beside an untested depth plane was rejected");
    queue.context[0x200] = 0x007007b3;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depth.has_value() && state.depthTest && !state.stencilTest && state.renderExtent.width == 32 && state.renderExtent.height == 2, "a depth test on a bound depth plane lost its surface");
    queue = makeState();
    queue.context[0x10f] = 0x7fc00000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "non-finite");
    // The register facade: every register the decoders read is in DrawKeyRegisters (a wrong hit of
    // the draw key otherwise), and nothing is recorded without a log pointer.
    queue = makeState();
    std::vector<AgcDriver::Graphics::RegisterRead> log;
    {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&log);
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "precheck rejected the reference state");
        static_cast<void>(AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state)));
    }
    Require(!log.empty(), "the register facade recorded nothing");
    const auto recorded = log.size();
    static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
    Require(log.size() == recorded, "the register facade recorded without an open log");
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
    auto colorDisabled = queue;
    colorDisabled.context[0x202] = colorDisabled.context.at(0x202) & ~0x70u;
    Require(AgcDriver::Graphics::DrawRejection(colorDisabled, true).empty(), "a draw without a pixel program was rejected while CB_COLOR_CONTROL disables color");
    Require(!AgcDriver::Graphics::DecodeState(colorDisabled).hasColorTarget, "a draw with CB_COLOR_CONTROL disabling color decoded a color target");
    colorDisabled.context.erase(0x202);
    Require(AgcDriver::Graphics::DrawRejection(colorDisabled, true).find("writes color") != std::string::npos, "a draw without a pixel program and an unwritten CB_COLOR_CONTROL was accepted");
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
    Require(AgcDriver::Graphics::PixelProgramSkipped(queue), "a retained Z export format ran a pixel program with depth exports disabled");
    Require(AgcDriver::Graphics::DrawRejection(queue, true).empty(), "a depth-only draw with a retained Z export format was rejected");
    for (const auto control : {1u, 2u, 4u, 0x40u, 0x80u, 0x100u, 0x200u, 0x400u, 0x20000u}) {
        queue.context[0x203] = control;
        Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "an enabled pixel export or execution flag was ignored with a retained Z format");
    }
    queue.context[0x203] = 0;
    queue.context[0x2dc] = 0xaa01;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "alpha-to-coverage skipped the pixel program");
    queue.context[0x203] = 0x800;
    Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "the Vulkan alpha-to-coverage state lost its pixel program with DB's override set");
    queue.context[0x203] = queue.context[0x2dc] = 0;
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

void depthCopyFormatTests() {
    using AgcDriver::Graphics::Texture;
    Require(Texture::DepthCopyFormat(VK_FORMAT_R32_SFLOAT) == VK_FORMAT_D32_SFLOAT, "a 32-bit float color format did not map to the 32-bit float depth format");
    Require(Texture::DepthCopyFormat(VK_FORMAT_R16_UNORM) == VK_FORMAT_D16_UNORM, "a 16-bit unorm color format did not map to the 16-bit unorm depth format");
    for (const auto format : {VK_FORMAT_UNDEFINED, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT, VK_FORMAT_D32_SFLOAT}) {
        Require(Texture::DepthCopyFormat(format) == VK_FORMAT_UNDEFINED, "a color format without a depth counterpart mapped to a depth format");
    }
}

void UnwrittenSampleLocationTests() {
    using AgcDriver::Graphics::DecodeState;
    using AgcDriver::Graphics::DrawRejection;
    using AgcDriver::Graphics::SampleLocations;
    const auto fresh = [](std::uint32_t config, std::uint32_t attributes, std::uint32_t scanMode = 3) {
        auto written = makeState();
        written.context[0x2f8] = config;
        written.context[0x292] = scanMode;
        written.context[0x3b8] = 0x0dc00000u;
        written.context[0x31d] = attributes;
        written.context[0x1b3] = 2;
        written.context[0x1b4] = 2;
        AgcDriver::QueueState queue;
        queue.shader = written.shader;
        queue.userConfig = written.userConfig;
        written.context.ForEach([&](std::uint32_t offset, std::uint32_t value) { queue.context[offset] = value; });
        return queue;
    };
    AgcDriver::QueueState initial;
    for (std::uint32_t offset = 0x2fe; offset <= 0x30d; ++offset) Require(initial.context.contains(offset) && initial.context.at(offset) == 0, "an unwritten PA_SC_AA_SAMPLE_LOCS register did not hold its clear-state value 0");
    for (const auto samples : {2u, 4u, 8u}) {
        const auto log2 = static_cast<std::uint32_t>(std::countr_zero(samples));
        auto queue = fresh((log2 << 20u) | log2, (log2 << 12u) | (log2 << 15u));
        const auto state = DecodeState(queue);
        Require(state.samples == samples && state.colors.size() == 1 && state.colors[0].samples == samples, "a multisampled draw with unwritten sample locations did not decode its sample count");
        Require(state.customSampleLocations, "unwritten sample locations did not decode as the clear-state pattern");
        for (std::uint32_t sample = 0; sample < samples; ++sample) Require(state.sampleLocations[sample] == 0, "an unwritten sample location was not at the pixel center");
        const auto centered = SampleLocations(state);
        for (std::uint32_t sample = 0; sample < samples; ++sample) Require(centered[sample].x == 0.5f && centered[sample].y == 0.5f, "unwritten sample locations did not place every sample at the pixel center");
        const auto rejection = DrawRejection(queue, false);
        Require(rejection.empty(), "the precheck refused a multisampled draw with unwritten sample locations: " + rejection);
    }
    auto queue = fresh(0x00200002, 0x12000);
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x622ae6ae;
    auto state = DecodeState(queue);
    Require(state.samples == 4 && !state.customSampleLocations, "explicit standard 4x sample locations changed");
    queue = fresh(0x00200002, 0x12000);
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x22eea66a;
    state = DecodeState(queue);
    Require(state.samples == 4 && state.customSampleLocations, "explicit custom 4x sample locations changed");
    const auto custom = SampleLocations(state);
    const std::array<VkSampleLocationEXT, 4> expected{{{0.125f, 0.875f}, {0.875f, 0.125f}, {0.375f, 0.375f}, {0.625f, 0.625f}}};
    for (std::size_t sample = 0; sample < expected.size(); ++sample) Require(custom[sample].x == expected[sample].x && custom[sample].y == expected[sample].y, "explicit custom 4x sample coordinates changed");
    queue.ClearContext();
    for (std::uint32_t offset = 0x2fe; offset <= 0x30d; ++offset) Require(queue.context.at(offset) == 0, "CLEAR_STATE did not reset PA_SC_AA_SAMPLE_LOCS");
    expectFailure([&] { DecodeState(fresh(0x00400004, 0x12000)); }, "more than eight samples");
    expectFailure([&] { DecodeState(fresh(0x00100002, 0x12000)); }, "exposing fewer samples");
    queue = fresh(0, 0, 3);
    state = DecodeState(queue);
    Require(state.samples == 1 && DrawRejection(queue, false).empty(), "MSAA_ENABLE on a single-sample target with unwritten sample locations was refused");
}

void msaaEnableClearOneSampleTargetTests() {
    using AgcDriver::Graphics::DecodeState;
    using AgcDriver::Graphics::DrawRejection;
    const auto drawC = [](std::uint32_t config = 0x0020c002u) {
        auto queue = makeState();
        queue.context[0x2f8] = config;
        queue.context[0x292] = 2;
        for (std::uint32_t offset = 0x2fe; offset <= 0x30d; ++offset) queue.context[offset] = (offset - 0x2feu) % 4u == 0 ? 0x622ae6aeu : 0u;
        queue.context[0x31c] = 0x8028;
        queue.context[0x31d] = 0;
        queue.context[0x010] = 0x80000183;
        queue.context[0x200] = 0x710;
        queue.context[0x201] = 0x00010002;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        return queue;
    };
    const auto requireOneSample = [](const AgcDriver::QueueState& queue, const std::string& what) {
        const auto state = DecodeState(queue);
        Require(state.samples == 1 && state.colors.size() == 1 && state.colors[0].samples == 1 && !state.depth, what + " was not drawn as one sample");
        Require(!state.customSampleLocations && !state.sampleLocationGrid && !state.perSampleShading && state.sampleLocations == decltype(state.sampleLocations){}, what + " kept the multisampled rasterizer's sample state");
        const auto rejection = DrawRejection(queue, false);
        Require(rejection.empty(), "the precheck refused " + what + ": " + rejection);
    };
    auto queue = drawC();
    requireOneSample(queue, "PPSA18216's draw C (a one-sample color target under a 4x PA_SC_AA_CONFIG without MSAA_ENABLE)");
    auto reference = drawC(0);
    const auto state = DecodeState(queue);
    const auto expected = DecodeState(reference);
    Require(state.samples == expected.samples && state.customSampleLocations == expected.customSampleLocations && state.sampleLocations == expected.sampleLocations && state.perSampleShading == expected.perSampleShading && state.colors[0].format == expected.colors[0].format, "draw C decoded differently from the same draw with a one-sample PA_SC_AA_CONFIG");
    for (const auto config : {0x00100001u, 0x00306003u}) requireOneSample(drawC(config), "a one-sample color target under a 2x or 8x PA_SC_AA_CONFIG without MSAA_ENABLE");
    queue = drawC();
    queue.context[0x302] = 0xae62e62a;
    requireOneSample(queue, "a one-sample color target under a 4x pattern that varies between pixels");
    for (const auto input : {ShaderRecompiler::PixelInput::PerspectiveSample, ShaderRecompiler::PixelInput::PerspectiveCentroid, ShaderRecompiler::PixelInput::LinearSample, ShaderRecompiler::PixelInput::LinearCentroid}) {
        queue = drawC();
        queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(input);
        requireOneSample(queue, "draw C with sample or centroid inputs");
    }
    queue = drawC();
    queue.context[0x1b3] |= ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    requireOneSample(queue, "draw C with an enabled but unaddressed ANCILLARY input");
    constexpr std::string_view refused = "a one-sample color target under a multisampled rasterizer without MSAA_ENABLE is unsupported with PS_ITER_SAMPLE, a sample index or coverage input, or a sample-mask export";
    for (const auto input : {ShaderRecompiler::PixelInput::Ancillary, ShaderRecompiler::PixelInput::SampleCoverage}) {
        queue = drawC();
        queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(input);
        expectFailure([&] { DecodeState(queue); }, refused);
    }
    queue = drawC();
    queue.context[0x293] = 0x10000u;
    expectFailure([&] { DecodeState(queue); }, refused);
    queue = drawC();
    queue.context[0x1c4] = 9;
    queue.context[0x203] |= 0x100u;
    expectFailure([&] { DecodeState(queue); }, refused);
    queue = drawC();
    queue.context[0x2dc] = 0x0000aa01u;
    expectFailure([&] { DecodeState(queue); }, "without MSAA_ENABLE are unsupported for points or alpha-to-coverage");
    for (const auto primitive : {2u, 3u}) {
        queue = drawC();
        queue.userConfig[0x242] = primitive;
        requireOneSample(queue, "a line list or strip into PPSA18216's draw C target");
    }
    const auto twoTargets = [&](std::uint32_t first, std::uint32_t second) {
        auto pair = drawC();
        for (const auto offset : {0x318u, 0x31bu, 0x31cu}) pair.context[offset + 0xfu] = pair.context.at(offset);
        for (const auto offset : {0x390u, 0x3b0u, 0x3b8u}) pair.context[offset + 1u] = pair.context.at(offset);
        pair.context[0x31d] = first;
        pair.context[0x32c] = second;
        pair.context[0x1c5] = 0x99u;
        pair.context[0x8e] = pair.context[0x8f] = 0xffu;
        pair.context[0x1e1] = 0;
        return pair;
    };
    queue = twoTargets(0, 0);
    Require(DecodeState(queue).samples == 1 && DecodeState(queue).colors.size() == 2, "two one-sample color targets under a 4x PA_SC_AA_CONFIG without MSAA_ENABLE were not drawn as one sample");
    for (const auto [first, second] : {std::pair{0u, 0x12000u}, std::pair{0x12000u, 0u}}) {
        queue = twoTargets(first, second);
        expectFailure([&] { DecodeState(queue); }, "a color target's sample count differs from the rasterizer's");
    }
    const auto depthTarget = [&](bool color, std::uint32_t config = 0x0020c002u) {
        auto depth = drawC(config);
        depth.context[0x0] = 0;
        depth.context[0x002] = 0;
        depth.context[0x007] = (3u << 16u) | 63u;
        depth.context[0x00a] = 0;
        depth.context[0x00b] = 0;
        depth.context[0x011] = 0x20000180;
        for (const auto offset : {0x012u, 0x014u}) depth.context[offset] = 0x100;
        depth.context[0x200] = 0x16;
        if (!color) depth.context[0x8e] = 0;
        return depth;
    };
    const auto requireOneSampleDepth = [](const AgcDriver::QueueState& queue, std::size_t colors, const std::string& what) {
        const auto state = DecodeState(queue);
        Require(state.samples == 1 && state.depth && state.depth->samples == 1 && state.colors.size() == colors && std::ranges::all_of(state.colors, [](const auto& color) { return color.samples == 1; }), what + " was not drawn as one sample");
        Require(!state.customSampleLocations && !state.sampleLocationGrid && !state.perSampleShading && state.sampleLocations == decltype(state.sampleLocations){}, what + " kept the multisampled rasterizer's sample state");
        const auto rejection = DrawRejection(queue, false);
        Require(rejection.empty(), "the precheck refused " + what + ": " + rejection);
    };
    queue = depthTarget(false);
    requireOneSampleDepth(queue, 0, "PPSA18216's opening-movie draw (a one-sample depth target and no color target under a 4x PA_SC_AA_CONFIG without MSAA_ENABLE)");
    const auto movie = DecodeState(queue);
    const auto movieReference = DecodeState(depthTarget(false, 0));
    Require(!movie.hasColorTarget && movie.renderExtent.width == movieReference.renderExtent.width && movie.renderExtent.height == movieReference.renderExtent.height && movie.depth->format == movieReference.depth->format && movie.sampleLocations == movieReference.sampleLocations && movie.customSampleLocations == movieReference.customSampleLocations, "the opening-movie draw decoded differently from the same draw with a one-sample PA_SC_AA_CONFIG");
    requireOneSampleDepth(depthTarget(true), 1, "a one-sample depth and color pair under a 4x PA_SC_AA_CONFIG without MSAA_ENABLE");
    queue = depthTarget(false);
    queue.context[0x302] = 0xae62e62a;
    requireOneSampleDepth(queue, 0, "a one-sample depth target under a 4x pattern that varies between pixels");
    for (const auto config : {0x00100001u, 0x00306003u}) {
        requireOneSampleDepth(depthTarget(false, config), 0, "a one-sample depth target under a 2x or 8x PA_SC_AA_CONFIG without MSAA_ENABLE");
        requireOneSampleDepth(depthTarget(true, config), 1, "a one-sample depth and color pair under a 2x or 8x PA_SC_AA_CONFIG without MSAA_ENABLE");
    }
    for (const auto input : {ShaderRecompiler::PixelInput::PerspectiveSample, ShaderRecompiler::PixelInput::PerspectiveCentroid, ShaderRecompiler::PixelInput::LinearSample, ShaderRecompiler::PixelInput::LinearCentroid}) {
        queue = depthTarget(false);
        queue.context[0x203] |= 0x40u;
        queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(input);
        requireOneSampleDepth(queue, 0, "a one-sample depth target under a 4x PA_SC_AA_CONFIG without MSAA_ENABLE with sample or centroid inputs");
    }
    queue = depthTarget(false);
    queue.context[0x293] = 0x10000u;
    queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage);
    Require(AgcDriver::Graphics::PixelProgramSkipped(queue), "the depth-only draw did not skip its pixel program");
    requireOneSampleDepth(queue, 0, "a one-sample depth target whose skipped pixel program has PS_ITER_SAMPLE and sample index and coverage inputs");
    constexpr std::string_view depthRefused = "a one-sample depth target under a multisampled rasterizer without MSAA_ENABLE is unsupported for points or lines and with alpha-to-coverage, a sample index or coverage input, a sample-mask export, or DB_EQAA Z interpolation or overrasterization";
    for (const bool color : {false, true}) {
        for (const auto input : {ShaderRecompiler::PixelInput::Ancillary, ShaderRecompiler::PixelInput::SampleCoverage}) {
            queue = depthTarget(color);
            queue.context[0x203] |= 0x40u;
            queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(input);
            expectFailure([&] { DecodeState(queue); }, depthRefused);
        }
        queue = depthTarget(color);
        queue.context[0x203] |= 0x40u;
        queue.context[0x293] = 0x10000u;
        Require(!AgcDriver::Graphics::PixelProgramSkipped(queue), "the PS_ITER_SAMPLE draw skipped its pixel program");
        requireOneSampleDepth(queue, color ? 1u : 0u, "a one-sample depth target whose running pixel program has PS_ITER_SAMPLE and no sample index or coverage input");
        Require(!DecodeState(queue).perSampleShading, "a PS_ITER_SAMPLE program drawn as one sample still shades per sample");
        queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
        expectFailure([&] { DecodeState(queue); }, depthRefused);
        queue = depthTarget(color);
        queue.context[0x1c4] = 9;
        queue.context[0x203] |= 0x100u;
        expectFailure([&] { DecodeState(queue); }, depthRefused);
        queue = depthTarget(color);
        queue.context[0x2dc] = 0x0000aa01u;
        expectFailure([&] { DecodeState(queue); }, depthRefused);
        for (const auto primitive : {2u, 3u}) {
            queue = depthTarget(color);
            queue.userConfig[0x242] = primitive;
            expectFailure([&] { DecodeState(queue); }, depthRefused);
        }
        for (const auto eqaa : {0x00050002u, 0x00090002u, 0x01010002u, 0x08010002u}) {
            queue = depthTarget(color);
            queue.context[0x201] = eqaa;
            expectFailure([&] { DecodeState(queue); }, depthRefused);
        }
    }
    queue = depthTarget(true);
    queue.context[0x31d] = 0x12000;
    expectFailure([&] { DecodeState(queue); }, "a color target's sample count differs from the rasterizer's");
}

void UnwrittenDepthPlaneTests() {
    using AgcDriver::Graphics::DecodeState;
    using AgcDriver::Graphics::DrawRejection;
    const auto fresh = [](std::uint32_t depthControl) {
        auto written = makeState();
        written.context[0x200] = depthControl;
        written.context[0x1b3] = 2;
        written.context[0x1b4] = 2;
        AgcDriver::QueueState queue;
        queue.shader = written.shader;
        queue.userConfig = written.userConfig;
        written.context.ForEach([&](std::uint32_t offset, std::uint32_t value) { queue.context[offset] = value; });
        return queue;
    };
    auto queue = fresh(0x777);
    auto state = DecodeState(queue);
    Require(!state.depth && !state.depthTest && !state.depthWrite && !state.stencilTest, "depth and stencil tests on unwritten depth and stencil planes were kept (PPSA24040's DB_DEPTH_CONTROL)");
    auto rejection = DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused depth and stencil tests on unwritten depth and stencil planes: " + rejection);
    queue = fresh(0x77f);
    state = DecodeState(queue);
    Require(!state.depth && !state.depthBoundsTest, "a depth bounds test on unwritten depth and stencil planes was kept");
    rejection = DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused a depth bounds test on unwritten depth and stencil planes: " + rejection);
    queue = fresh(0x007007b3);
    queue.context[0x010] = 0x80000183;
    queue.context[0x012] = 0x00001000;
    queue.context[0x014] = 0x00001000;
    queue.context[0x007] = 0x0001001f;
    queue.context[0x00b] = 0x3f800000;
    state = DecodeState(queue);
    Require(state.depth && state.depthTest && !state.stencilTest && state.renderExtent.width == 32 && state.renderExtent.height == 2, "a depth plane beside an unwritten DB_STENCIL_INFO did not decode as depth only");
    rejection = DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused a depth plane beside an unwritten DB_STENCIL_INFO: " + rejection);
    queue = fresh(0x007007b3);
    queue.context[0x011] = 0x20000181;
    queue.context[0x013] = 0x00002000;
    queue.context[0x015] = 0x00002000;
    queue.context[0x007] = 0x003f003f;
    queue.context[0x00b] = 0x3f800000;
    queue.context[0x10c] = 0x01ffff00;
    queue.context[0x10d] = 0x01ffff00;
    state = DecodeState(queue);
    Require(!state.depthTest && state.stencilTest, "a stencil plane beside an unwritten DB_Z_INFO kept the depth test or lost the stencil test");
    rejection = DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused a stencil plane beside an unwritten DB_Z_INFO: " + rejection);
    queue.context[0x010] = 0x80000183;
    queue.ClearContext();
    for (const auto offset : {0x010u, 0x011u}) Require(queue.context.contains(offset) && queue.context.at(offset) == 0, "CLEAR_STATE did not reset DB_Z_INFO and DB_STENCIL_INFO to their clear-state value 0");
}

void MsaaEnableSingleSampleTests() {
    using AgcDriver::Graphics::DecodeState;
    using AgcDriver::Graphics::DrawRejection;
    const auto enabled = [] {
        auto queue = makeState();
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        queue.context[0x292] = 3;
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0;
        return queue;
    };
    const auto refused = [](const AgcDriver::QueueState& queue, std::string_view reason) {
        expectFailure([&] { DecodeState(queue); }, reason);
        const auto rejection = DrawRejection(queue, false);
        Require(rejection.find(reason) != std::string::npos, "the precheck did not refuse MSAA_ENABLE on a single-sample target (" + std::string(reason) + "): " + rejection);
    };
    auto queue = enabled();
    auto clear = queue;
    clear.context[0x292] = 2;
    const auto baseline = DecodeState(clear);
    for (const auto locations : {0x00000000u, 0xcc44cc00u}) {
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = locations;
        const auto state = DecodeState(queue);
        Require(state.samples == 1 && state.colors.size() == 1 && state.colors[0].samples == 1 && !state.alphaToCoverage, "MSAA_ENABLE on a single-sample target did not decode as single-sampled");
        Require(state.topology == baseline.topology && state.conservativeRasterization == baseline.conservativeRasterization && state.cullMode == baseline.cullMode && state.frontFace == baseline.frontFace, "MSAA_ENABLE on a single-sample target changed the rasterizer state");
        Require(state.scissor.offset.x == baseline.scissor.offset.x && state.scissor.offset.y == baseline.scissor.offset.y && state.scissor.extent.width == baseline.scissor.extent.width && state.scissor.extent.height == baseline.scissor.extent.height, "MSAA_ENABLE on a single-sample target changed the scissor");
        Require(state.colors[0].address == baseline.colors[0].address && state.colors[0].format == baseline.colors[0].format && state.blend.colorWriteMask == baseline.blend.colorWriteMask, "MSAA_ENABLE on a single-sample target changed the color target");
        const auto rejection = DrawRejection(queue, false);
        Require(rejection.empty(), "the precheck refused MSAA_ENABLE on a single-sample target: " + rejection);
    }
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        for (const auto location : {0x04u, 0x40u, 0xc4u}) {
            queue = enabled();
            queue.context[0x2fe + pixel * 4u] = location;
            refused(queue, "off the pixel center");
        }
    }
    queue = enabled();
    queue.context.erase(0x306);
    expectFailure([&] { DecodeState(queue); }, "missing register");
    queue = enabled();
    queue.context[0x203] &= ~0x800u;
    queue.context[0x2dc] = 0xaa01;
    refused(queue, "MSAA_ENABLE on a single-sample target with alpha-to-coverage");
    queue = enabled();
    queue.context[0x2dc] = 0xaa01;
    refused(queue, "MSAA_ENABLE on a single-sample target with alpha-to-coverage");
    queue.context[0x2dc] = 0x18701;
    Require(!DecodeState(queue).alphaToCoverage && DrawRejection(queue, false).empty(), "MSAA_ENABLE on a single-sample target ignored the dithered alpha disable override");
    queue = enabled();
    queue.context[0x313] = 0x6001;
    refused(queue, "MSAA_ENABLE on a single-sample target with conservative rasterization");
    for (const auto primitive : {2u, 3u}) {
        queue = enabled();
        queue.userConfig[0x242] = primitive;
        refused(queue, "primitives other than triangles and rectangles");
    }
    queue = enabled();
    queue.userConfig[0x242] = 1;
    Require(DrawRejection(queue, false).find("primitives other than triangles and rectangles") != std::string::npos, "the precheck accepted MSAA_ENABLE on a single-sample target for points");
    for (const auto primitive : {5u, 6u, 7u, 17u, 21u}) {
        queue = enabled();
        queue.userConfig[0x242] = primitive;
        Require(DecodeState(queue).samples == 1 && DrawRejection(queue, false).empty(), "MSAA_ENABLE on a single-sample target was refused for a filled triangle or rectangle primitive");
    }
    queue = enabled();
    queue.context[0x292] = 7;
    refused(queue, "scan conversion mode");
    clear.context[0x2fe] = 0x44;
    clear.context[0x203] &= ~0x800u;
    clear.context[0x2dc] = 0xaa01;
    Require(DecodeState(clear).alphaToCoverage && DrawRejection(clear, false).empty(), "a single-sample target without MSAA_ENABLE was refused over its sample locations or alpha-to-coverage");
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
    {
        auto vertex = makeState();
        vertex.context[0x2d5] = 0x02002008u;
        const auto state = AgcDriver::Graphics::DecodeState(vertex);
        Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Vertex
            && state.stages.registerValue == 0x02002008u,
            "the reported vertex-routing state did not decode as vertex");
        vertex.context[0x2d5] |= 0x10u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(vertex); }, "reserved LS_EN, ES_EN or VS_EN");
        vertex.context[0x2d5] = 0x02002008u | 0x800000u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(vertex); }, "unsupported vertex routing");
        vertex.context[0x2d5] = 0x2008u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(vertex); }, "unsupported vertex routing");
        vertex.context.erase(0x2d5);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(vertex); }, "missing register");
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
    queue.shader[0x8a] = 0x222c0007u;
    queue.shader[0x8b] = 0x00c00010u;
    stages = AgcDriver::Graphics::DecodeState(queue).stages;
    Require(stages.path == AgcDriver::Graphics::ShaderPath::Geometry && stages.mesh && stages.mesh->primitivesPerGroup == 21 && stages.mesh->esgsItemSize == 4, "a geometry program with fewer GS and ES input VGPRs did not decode");
    queue.shader[0x8a] = 3u << 29u;
    queue.shader[0x8b] = 3u << 16u;
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
    {
        auto fast = makeState();
        fast.userConfig[0x242] = 1;
        fast.context[0x2d5] = 0x482030;
        fast.userConfig[0x25b] = 0x201;
        fast.context[0x1ff] = 32;
        fast.context[0x2ce] = 32;
        fast.context[0x2d3] = 64;
        fast.context[0x29b] = 2;
        fast.context[0x2ab] = 4;
        fast.shader[0x8a] = 0x022c0245;
        fast.shader[0x8b] = 0x9002a;
        const auto state = AgcDriver::Graphics::DecodeState(fast);
        Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Geometry && state.stages.vertexWaveSize == 32 && state.stages.mesh, "a fast-launch draw did not decode as wave32 geometry");
        const auto& mesh = *state.stages.mesh;
        Require(mesh.inputPrimitive == 1 && mesh.primitivesPerGroup == 1 && mesh.verticesPerGroup == 1 && mesh.maxVertices == 32 && mesh.maxPrimitives == 64 && mesh.threadsPerGroup == 64 && mesh.ldsSizeDwords == 128, "a fast-launch subgroup did not decode as one point amplified by PRIM_AMP_FACTOR");
        fast.context[0x29b] = 1;
        const auto fastLines = AgcDriver::Graphics::DecodeState(fast);
        Require(fastLines.stages.mesh->outputPrimitiveSize == 2 && fastLines.stages.mesh->maxPrimitives == 64 && AgcDriver::Graphics::RasterPrimitiveOf(fastLines) == ShaderRecompiler::RasterPrimitive::Line, "a fast-launch line strip did not take its line count from PRIM_AMP_FACTOR");
        fast.context[0x29b] = 2;
        fast.context[0x2d5] = 0x502030;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fast); }, "unsupported geometry routing, fast launch or wave-ID state");
        fast.context[0x2d5] = 0x482030;
        fast.userConfig[0x25b] = 0x402;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fast); }, "fast launch other than one point per subgroup");
        fast.userConfig[0x25b] = 0x201;
        fast.userConfig[0x242] = 4;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fast); }, "fast launch other than one point per subgroup");
        fast.userConfig[0x242] = 1;
        fast.context[0x2d3] = 0;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(fast); }, "amplifies to no primitives");
    }
    {
        auto lines = makeState();
        lines.userConfig[0x242] = 1;
        lines.context[0x2d5] = 0x2030;
        lines.userConfig[0x25b] = 0x6633;
        lines.context[0x1ff] = 255;
        lines.context[0x2ce] = 5;
        lines.context[0x29b] = 1;
        lines.context[0x2ab] = 4;
        lines.shader[0x8a] = 3u << 29u;
        lines.shader[0x8b] = 3u << 16u;
        auto state = AgcDriver::Graphics::DecodeState(lines);
        Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Geometry && state.stages.mesh && AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Line, "a geometry program that outputs line strips did not decode as geometry rasterized as lines");
        auto mesh = *state.stages.mesh;
        Require(mesh.inputPrimitive == 1 && mesh.primitivesPerGroup == 51 && mesh.verticesPerGroup == 51 && mesh.maxVertices == 255 && mesh.maxPrimitives == 255 && mesh.threadsPerGroup == 256 && mesh.outputPrimitiveSize == 2, "a line-strip subgroup did not decode as 51 points with a line slot per output vertex");
        lines.context[0x29b] = 2;
        state = AgcDriver::Graphics::DecodeState(lines);
        mesh = *state.stages.mesh;
        Require(mesh.maxPrimitives == 153 && mesh.threadsPerGroup == 256 && mesh.outputPrimitiveSize == 3 && AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Triangle, "the triangle-strip subgroup of the same draw changed");
        Require(mesh.provokingVertex == 0, "a first-vertex geometry draw does not take its flat inputs and layer from the first vertex of each output primitive");
        const auto raster = lines.context.at(0x205);
        lines.context[0x205] = raster | (1u << 19u);
        Require(AgcDriver::Graphics::DecodeState(lines).stages.mesh->provokingVertex == 2 && AgcDriver::Graphics::DecodeShaderStages(lines).mesh->provokingVertex == 2, "a last-vertex geometry draw does not take its flat inputs and layer from the third vertex of each output triangle");
        lines.context[0x29b] = 1;
        Require(AgcDriver::Graphics::DecodeState(lines).stages.mesh->provokingVertex == 1, "a last-vertex geometry draw does not take its flat inputs and layer from the second vertex of each output line");
        lines.context[0x29b] = 0;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(lines); }, "missing register in context bank at DWORD 0x280");
        lines.context[0x280] = 0x00080008u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(lines); }, "missing register in context bank at DWORD 0x281");
        lines.context[0x281] = 0xffff0000u;
        state = AgcDriver::Graphics::DecodeState(lines);
        mesh = *state.stages.mesh;
        Require(mesh.outputPrimitiveSize == 1 && mesh.maxPrimitives == 255 && AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Point, "a point-list subgroup did not decode as points");
        Require(mesh.provokingVertex == 0, "a point draw has provoking vertex 0");
        Require(state.pointSize == 1.0f && state.pointSizeMin == 0.0f && state.pointSizeMax == 8191.875f, "geometry points did not decode the queue's default point size and limits");
        lines.context[0x205] = raster;
        auto points = lines;
        points.userConfig[0x242] = 6;
        points.userConfig[0x25b] = 0x5c2e;
        points.context[0x1ff] = 0x8a;
        points.context[0x2ce] = 3;
        points.context[0x29b] = 0;
        points.context[0x280] = 0x00180018u;
        points.context[0x281] = 0x00400008u;
        const auto pointState = AgcDriver::Graphics::DecodeState(points);
        const auto& pointMesh = *pointState.stages.mesh;
        ShaderRecompiler::ShaderVertexStageInfo pointInfo{};
        AgcDriver::Graphics::ApplyPointSize(pointState, pointInfo);
        Require(pointInfo.pointList && pointInfo.pointSize == 3.0f && pointInfo.pointSizeMin == 1.0f && pointInfo.pointSizeMax == 8.0f, "geometry points lost their register sizes");
        Require(pointMesh.outputPrimitiveSize == 1u && pointMesh.primitivesPerGroup == 44u && pointMesh.verticesPerGroup == 46u && pointMesh.maxVertices == 138u && pointMesh.maxPrimitives == 138u && pointMesh.threadsPerGroup == 192u && AgcDriver::Graphics::RasterPrimitiveOf(pointState) == ShaderRecompiler::RasterPrimitive::Point, "triangle-strip input with point output did not decode the reported subgroup");
        lines.context[0x29b] = 1;
        lines.context[0x2ce] = 1;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(lines); }, "unsupported geometry input or output assembly");
        lines.context[0x2ce] = 5;
        for (const auto output : {3u, 0x80000001u, 0x101u}) {
            lines.context[0x29b] = output;
            expectFailure([&] { AgcDriver::Graphics::DecodeState(lines); }, "VGT_GS_OUT_PRIM_TYPE other than a triangle strip, or a point list or line strip from a geometry program, is unsupported");
        }
        lines.userConfig[0x242] = 4;
        lines.context[0x205] = 0x2a40;
        lines.context[0x29b] = 2;
        Require(AgcDriver::Graphics::DecodeState(lines).stages.mesh->primitivesPerGroup == 17, "triangle input with a triangle-strip output and a triangle polygon offset did not decode");
        lines.context[0x29b] = 1;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(lines); }, "depth bias or nonstandard rasterization");
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
    queue.userConfig[0x25b] = 0x1fe55;
    queue.context[0x2d6] = 0xc355;
    queue.context[0x2db] = 0x40049;
    queue.context[0x286] = 0x42800000;
    queue.context[0x287] = 0x3f800000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported tessellation routing");
    queue.context[0x2d5] = 0x0200010d;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported tessellation routing");
    queue.context[0x2d5] = 0x0200210d;
    {
        std::vector<AgcDriver::Graphics::RegisterRead> tessellationLog;
        {
            const AgcDriver::Graphics::RegisterReadLogScope logScope(&tessellationLog);
            stages = AgcDriver::Graphics::DecodeState(queue).stages;
        }
        for (const auto read : tessellationLog) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a tessellation register the decoder reads: " + std::to_string(read.offset));
    }
    Require(stages.path == AgcDriver::Graphics::ShaderPath::Tessellation && stages.tessellation, "tessellation routing changed");
    const auto& tessellation = *stages.tessellation;
    Require(tessellation.inputControlPoints == 3 && tessellation.outputControlPoints == 3 && tessellation.domain == 1 && tessellation.partitioning == 2 && tessellation.outputTopology == 2 && stages.minTessellationLevel == 1.0f && stages.maxTessellationLevel == 64.0f && tessellation.patchesPerGroup == 85 && tessellation.primitiveGroupSize == 85 && !tessellation.breakAtInstance, "the passthrough triangle-patch configuration decoded differently");
    for (const auto [parameters, reason] : {std::pair{0x44049u, "VGT_TF_PARAM"}, std::pair{0x140049u, "VGT_TF_PARAM"}, std::pair{0x2040049u, "VGT_TF_PARAM"}, std::pair{0x40059u, "fractional-odd partitioning"}, std::pair{0x40045u, "fractional-odd partitioning"}, std::pair{0x40048u, "fractional-odd partitioning"}, std::pair{0x40029u, "fractional-odd partitioning"}, std::pair{0x4004bu, "fractional-odd partitioning"}}) {
        queue.context[0x2db] = parameters;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, reason);
    }
    queue.context[0x2db] = 0x40041;
    Require(AgcDriver::Graphics::DecodeState(queue).stages.tessellation->partitioning == 0, "integer-partitioned triangle patches decoded differently");
    queue.context[0x2db] = 0x4004d;
    Require(AgcDriver::Graphics::DecodeState(queue).stages.tessellation->partitioning == 3, "fractional-even-partitioned triangle patches decoded differently");
    queue.context[0x2d6] = 0x1043f;
    queue.context[0x2db] = 0x4006a;
    queue.context[0x286] = 0x41100000;
    queue.userConfig[0x25b] = 0x5fe3f;
    const auto quadStages = AgcDriver::Graphics::DecodeState(queue).stages;
    const auto& quads = quadStages.tessellation;
    Require(quads && quads->inputControlPoints == 4 && quads->outputControlPoints == 4 && quads->domain == 2 && quads->partitioning == 2 && quads->outputTopology == 3 && quadStages.minTessellationLevel == 1.0f && quadStages.maxTessellationLevel == 9.0f && quads->patchesPerGroup == 63 && quads->primitiveGroupSize == 63 && quads->breakAtInstance, "counter-clockwise quad patches with HOS levels [1, 9], 63 patches per group and BREAK_WAVE_AT_EOI decoded differently");
    queue.userConfig[0x25b] = 0x8040;
    const auto shared = AgcDriver::Graphics::DecodeState(queue).stages.tessellation;
    Require(shared && shared->primitiveGroupSize == 64 && !shared->breakAtInstance, "GE_CNTL 0x8040 did not decode as 64 primitives per group with BREAK_WAVE_AT_EOI clear");
    queue.userConfig.erase(0x25b);
    const auto missing = AgcDriver::Graphics::DecodeState(queue).stages.tessellation;
    Require(missing->primitiveGroupSize == 0 && !missing->breakAtInstance, "a draw without GE_CNTL decoded a primitive group size or BREAK_WAVE_AT_EOI");
    queue.context[0x2db] = 0x40069;
    Require(AgcDriver::Graphics::DecodeState(queue).stages.tessellation->outputTopology == 3, "counter-clockwise triangle patches decoded differently");
    for (const auto [maximum, minimum, reason] : {std::tuple{0x3f800000u, 0x40000000u, "tessellation level clamps"}, std::tuple{0x41100000u, 0xbf800000u, "tessellation level clamps"}, std::tuple{0x3f000000u, 0x00000000u, "tessellation level clamps"}, std::tuple{0x42800000u, 0x42800000u, "tessellation level clamps"}, std::tuple{0x7fc00000u, 0x3f800000u, "non-finite register"}, std::tuple{0x7f800000u, 0x3f800000u, "non-finite register"}}) {
        queue.context[0x286] = maximum;
        queue.context[0x287] = minimum;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, reason);
    }
    queue.context[0x2d6] = 0xc355;
    queue.context[0x2db] = 0x40049;
    queue.context[0x286] = 0x42800000;
    queue.context[0x287] = 0x3f800000;
    queue.context[0x2d5] = 0x202d;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "combined tessellation and geometry");
    queue.context[0x2d5] = 0x0200210d;
    queue.context[0x2d6] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "control-point counts");
    queue.context[0x2d6] = 0xc300;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "NUM_PATCHES is zero");
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

void UnsupportedTessellationFieldTests() {
    auto queue = makeState();
    queue.context[0x2d5] = 0x0200210d;
    queue.userConfig[0x242] = 9;
    queue.userConfig[0x25b] = 0x1fe55;
    queue.context[0x286] = 0x42800000;
    queue.context[0x287] = 0x3f800000;
    for (const auto [config, parameters, fields] : {
             std::tuple{0xc355u, 0x40045u, "VGT_TF_PARAM=0x40045 (TYPE=1 TESS_TRIANGLE, PARTITIONING=1 PART_POW2, TOPOLOGY=2 OUTPUT_TRIANGLE_CW), VGT_LS_HS_CONFIG=0xc355 (NUM_PATCHES=85, HS_NUM_INPUT_CP=3, HS_NUM_OUTPUT_CP=3)"},
             std::tuple{0x1033fu, 0x40066u, "VGT_TF_PARAM=0x40066 (TYPE=2 TESS_QUAD, PARTITIONING=1 PART_POW2, TOPOLOGY=3 OUTPUT_TRIANGLE_CCW), VGT_LS_HS_CONFIG=0x1033f (NUM_PATCHES=63, HS_NUM_INPUT_CP=3, HS_NUM_OUTPUT_CP=4)"},
             std::tuple{0xc355u, 0x40048u, "VGT_TF_PARAM=0x40048 (TYPE=0 TESS_ISOLINE, PARTITIONING=2 PART_FRAC_ODD, TOPOLOGY=2 OUTPUT_TRIANGLE_CW)"},
             std::tuple{0xc355u, 0x40029u, "(TYPE=1 TESS_TRIANGLE, PARTITIONING=2 PART_FRAC_ODD, TOPOLOGY=1 OUTPUT_LINE)"},
             std::tuple{0xc355u, 0x4000du, "(TYPE=1 TESS_TRIANGLE, PARTITIONING=3 PART_FRAC_EVEN, TOPOLOGY=0 OUTPUT_POINT)"},
             std::tuple{0xc355u, 0x4004bu, "(TYPE=3, PARTITIONING=2 PART_FRAC_ODD, TOPOLOGY=2 OUTPUT_TRIANGLE_CW)"},
             std::tuple{0xc355u, 0x400f1u, "(TYPE=1 TESS_TRIANGLE, PARTITIONING=4, TOPOLOGY=7)"}}) {
        queue.context[0x2d6] = config;
        queue.context[0x2db] = parameters;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "VGT_SHADER_STAGES_EN=0x200210d: VGT_TF_PARAM=0x");
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, fields);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "): only triangle and quad domains with integer, fractional-even or fractional-odd partitioning and triangle output are supported by the reference path");
    }
}

void NggVertexWorkgroupTests() {
    auto queue = makeState();
    queue.context[0x2d5] = 0x12010;
    queue.userConfig[0x242] = 4;
    queue.userConfig[0x25b] = 0x10080;
    queue.context[0x1ff] = 0x80;
    queue.context[0x2ce] = 0;
    queue.context[0x29b] = 2;
    queue.context[0x2ab] = 1;
    queue.shader[0x8a] = 0x222c0009;
    queue.shader[0x8b] = 0x43001e;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    Require(AgcDriver::Graphics::DecodeState(queue).stages.path == AgcDriver::Graphics::ShaderPath::Vertex, "an NGG vertex program without a workgroup left the vertex path");
    auto state = AgcDriver::Graphics::DecodeState(queue, true);
    Require(state.stages.path == AgcDriver::Graphics::ShaderPath::Geometry && state.stages.mesh, "an NGG vertex program that needs a workgroup did not take the mesh path");
    expectFailure([&] { AgcDriver::Graphics::ValidateShaders({}, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false, false, false, false, false, false, false, false, false, false, false, false, false); }, "device does not support mesh shaders");
    auto mesh = *state.stages.mesh;
    Require(mesh.inputPrimitive == 4 && mesh.primitivesPerGroup == 42 && mesh.verticesPerGroup == 126 && mesh.maxVertices == 128 && mesh.maxPrimitives == 128 && mesh.threadsPerGroup == 128 && mesh.ldsSizeDwords == 1024 && mesh.esgsItemSize == 1, "the NGG vertex subgroup of a triangle list changed");
    queue.userConfig[0x242] = 6;
    mesh = *AgcDriver::Graphics::DecodeState(queue, true).stages.mesh;
    Require(mesh.primitivesPerGroup == 126 && mesh.verticesPerGroup == 128 && mesh.maxPrimitives == 128, "the NGG vertex subgroup of a triangle strip is not bounded by its vertex budget");
    queue.shader[0x8a] = 0x222c0005;
    queue.shader[0x8b] = 0x400030;
    Require(AgcDriver::Graphics::DecodeState(queue, true).stages.mesh.has_value(), "an NGG vertex program that loads only its vertex ID was rejected");
    queue.shader[0x8a] = 0x222c0009;
    queue.shader[0x8b] = 0x43001e;
    queue.context[0x1ff] = 2;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue, true); }, "invalid geometry subgroup output");
    queue.context[0x1ff] = 0x80;
    for (const auto stages : {0x02012010u, 0x00012030u, 0x00010010u}) {
        queue.context[0x2d5] = stages;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue, true); }, "requires PRIMGEN_EN without GS_EN, PRIMGEN_PASSTHRU_EN or tessellation");
    }
    queue.context[0x2d5] = 0x12010;
    queue.context[0x29b] = 1;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue, true); }, "VGT_GS_OUT_PRIM_TYPE other than a triangle strip, or a point list or line strip from a geometry program, is unsupported");
    queue.context[0x29b] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue, true); }, "VGT_GS_OUT_PRIM_TYPE other than a triangle strip, or a point list or line strip from a geometry program, is unsupported");
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

std::size_t sampleMaskInputs(std::uint32_t inputs, std::uint16_t quadMask) {
    auto queue = makeState();
    queue.context[0x1b3] = inputs;
    queue.context[0x1b4] = inputs;
    auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
    pixel.quadSampleMask[0] = quadMask;
    const auto source = ShaderRecompiler::PixelInputVgpr(inputs, ShaderRecompiler::PixelInput::SampleCoverage);
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
    std::map<std::uint32_t, std::uint32_t> storage;
    std::vector<std::uint32_t> sampleMasks;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpVariable) storage[words[at + 2]] = words[at + 3];
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == spv::BuiltInSampleMask) sampleMasks.push_back(words[at + 1]);
    }
    return static_cast<std::size_t>(std::ranges::count_if(sampleMasks, [&](std::uint32_t id) { return storage[id] == spv::StorageClassInput; }));
}

bool readsBuiltin(const std::vector<spv::BuiltIn>& read, spv::BuiltIn builtin) {
    return std::find(read.begin(), read.end(), builtin) != read.end();
}

std::vector<std::uint32_t> pixelNoPerspectiveLocations(bool barycentricEnabled, std::uint32_t inputs = 0x22u) {
    auto queue = makeState();
    queue.context[0x1b3] = inputs;
    queue.context[0x1b4] = inputs;
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
    if (!barycentricEnabled) Require(!result.barycentricEmulation.active, "v_interp_p1/p2 of their own I/J pairs did not keep host interpolation");
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
    shader[0x207] = 0x00100040u;
    Require(AgcDriver::Graphics::DecodeComputeStageInfo(shader, {}).numThreads[0] == 64u, "the partial group size leaked into the group size");
    shader[0x207] = 64u;
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
    queue.context[0x3b8] = 0x0dc68000u;
    for (const std::uint32_t pipeBankXor : {0u, 0x4600u, 0xcf00u}) {
        const auto address = block + pipeBankXor;
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        const auto color = AgcDriver::Graphics::DecodeState(queue).color;
        Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::D64KBX && color.address == block && color.surfaceAddress == block && color.pipeBankXor == pipeBankXor, "a SW_64KB_D_X color base did not split its block address and XOR");
    }
    queue.context[0x3b8] = 0x4dc60000u;
    for (const std::uint32_t pipeBankXor : {0u, 0x4600u, 0xff00u}) {
        const auto address = block + pipeBankXor;
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        const auto color = AgcDriver::Graphics::DecodeState(queue).color;
        Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Z64KBX && color.address == block && color.surfaceAddress == block && color.pipeBankXor == pipeBankXor, "a SW_64KB_Z_X color base did not split its block address and XOR");
    }
    queue.context[0x3b8] = 0x0dc68000u;
    for (const std::uint32_t columnBits : {0x1000u, 0x2000u, 0x3000u}) {
        queue.context[0x318] = static_cast<std::uint32_t>((block + columnBits) >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>((block + columnBits) >> 40u);
        expectFailure([&queue] { AgcDriver::Graphics::DecodeColorBuffer(queue.context, 0); }, "unsupported display color pipe/bank XOR column bits");
    }
    static std::vector<std::byte> mipStorage(16u * blockBytes);
    const auto mipBase = (reinterpret_cast<std::uintptr_t>(mipStorage.data()) + blockBytes - 1u) / blockBytes * blockBytes;
    const auto mips = AgcDriver::Graphics::ComputeElementMipLayout(AgcDriver::Graphics::TextureTileMode::kD64KBX, 4, 256, 128, 5);
    const auto sliceBytes = AgcDriver::Graphics::ComputeSurfaceSize(mips, 1);
    queue.context[0x318] = static_cast<std::uint32_t>((mipBase + 0x0200u) >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>((mipBase + 0x0200u) >> 40u);
    queue.context[0x3b0] = (4u << 28u) | (255u << 14u) | 127u;
    queue.context[0x3b8] |= 1u;
    queue.context[0x31b] = (2u << 26u) | 1u | (1u << 13u);
    const auto mipColor = AgcDriver::Graphics::DecodeColorBuffer(queue.context, 0);
    Require(mipColor.mip == 2 && mipColor.mipCount == 5 && mipColor.extent.width == 64 && mipColor.extent.height == 32 && mipColor.surfaceAddress == mipBase + sliceBytes && mipColor.address == mipBase + sliceBytes + mips[2].tiledOffset && mipColor.mipTail == mips[2].tail && mipColor.pipeBankXor == 0x0200u, "a display XOR color view lost its array slice, mip placement or XOR");
}

void HiddenUserWordVertexTablesTests() {
    constexpr std::size_t userDataOffset = 288;
    constexpr std::size_t directOffset = 352;
    constexpr std::size_t semanticOffset = 384;
    static std::vector<std::byte> header(semanticOffset + sizeof(ShaderSemantic));
    static std::array<std::uint32_t, 4> bufferTable{0x1000u, 0x20000u, 64u, 0x3000u};
    static std::array<std::uint32_t, 1> attribTable{(0x2cu << 5u)};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    std::memcpy(header.data() + directOffset, direct.data(), sizeof(direct));
    ShaderSemantic semantic{};
    semantic.size_in_elements = 1;
    std::memcpy(header.data() + semanticOffset, &semantic, sizeof(semantic));
    ShaderUserData userData{};
    userData.direct_resource_offset = reinterpret_cast<std::uint16_t*>(header.data() + directOffset);
    userData.direct_resource_count = ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT;
    std::memcpy(header.data() + userDataOffset, &userData, sizeof(userData));
    Shader shader{};
    shader.user_data = reinterpret_cast<ShaderUserData*>(header.data() + userDataOffset);
    shader.input_semantics = reinterpret_cast<ShaderSemantic*>(header.data() + semanticOffset);
    shader.num_input_semantics = 1;
    std::memcpy(header.data(), &shader, sizeof(shader));
    const auto headerAddress = reinterpret_cast<std::uintptr_t>(header.data());
    const auto low = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    const auto high = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); };
    const std::array<std::uint32_t, 8> registers{0x31016facu, 0x7efff390u, 0x100002u, 0x49u, low(bufferTable.data()), high(bufferTable.data()), low(attribTable.data()), high(attribTable.data())};
    std::vector<std::uint32_t> merged(8, 0u);
    merged.insert(merged.end(), registers.begin(), registers.end());
    const auto info = AgcDriver::Graphics::DecodeVertexStageInfo(header, headerAddress, merged, nullptr, false, 8);
    Require(info.fetchEmbedded && info.fetchBufferReg == 4u && info.fetchAttribReg == 6u && info.resourcesNum == 1 && info.resources[0].fields[0] == 0x1000u && info.resources[0].fields[2] == 64u, "a merged program's vertex tables were not read past its eight hidden user words");
    expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeVertexStageInfo(header, headerAddress, merged, nullptr, false, 0)); }, "null vertex attribute table");
    const auto plain = AgcDriver::Graphics::DecodeVertexStageInfo(header, headerAddress, registers, nullptr, false, 0);
    Require(plain.fetchEmbedded && plain.fetchBufferReg == 4u && plain.resourcesNum == 1 && plain.resources[0].fields[0] == 0x1000u, "a vertex program's tables moved");
}

void HeaderlessVertexStageTests() {
    const std::array<std::uint32_t, 4> userData{1, 2, 3, 4};
    const auto info = AgcDriver::Graphics::DecodeVertexStageInfo({}, 0, userData);
    Require(!info.fetchEmbedded && info.resourcesNum == 0, "a vertex program without an AGC header received an embedded fetch table");
    const std::array<std::byte, 8> truncated{};
    expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeVertexStageInfo(truncated, 0x1000, userData)); }, "smaller than the fixed AGC header");
}

void UnexportedPixelInputTests() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 5;
    pixel.interpolatorSettings[0] = 0x000u;
    pixel.interpolatorSettings[1] = 0x401u;
    pixel.interpolatorSettings[2] = 0x302u;
    pixel.interpolatorSettings[3] = 0x123u;
    pixel.interpolatorSettings[4] = 0x680004u;
    pixel.interpolatorSettings[5] = 0x005u;
    const std::array<std::uint32_t, 1> exported{0u};
    const auto adjusted = AgcDriver::Graphics::DefaultUnexportedInputs(pixel, exported);
    Require(adjusted.interpolatorSettings[0] == 0x000u, "an input of an exported parameter was changed");
    Require(adjusted.interpolatorSettings[1] == 0x021u, "a flat input of an unexported parameter did not read zero");
    Require(adjusted.interpolatorSettings[2] == 0x022u, "an input of an unexported parameter did not read zero");
    Require(adjusted.interpolatorSettings[3] == 0x123u, "an input that already reads its default value was changed");
    Require(adjusted.interpolatorSettings[4] == 0x180024u, "an FP16 input of an unexported parameter did not read zero in both halves");
    Require(adjusted.interpolatorSettings[5] == 0x005u, "a setting past SPI_PS_IN_CONTROL's input count was changed");
    const std::array<std::uint32_t, 5> all{0u, 1u, 2u, 4u, 5u};
    const auto unchanged = AgcDriver::Graphics::DefaultUnexportedInputs(pixel, all);
    Require(std::ranges::equal(unchanged.interpolatorSettings, pixel.interpolatorSettings), "inputs of exported parameters were changed");
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
    Require(!pixel.frontFaceAllBits, "FRONT_FACE_ALL_BITS was decoded from a clear SPI_BARYC_CNTL");
    queue.context[0x1b8] = 1u << 24u;
    Require(decode(0x1146u, 0x1146u).frontFaceAllBits, "SPI_BARYC_CNTL FRONT_FACE_ALL_BITS was not decoded");
    Require(!decode(0x326u, 0x326u).frontFaceAllBits, "FRONT_FACE_ALL_BITS was decoded for a pixel program without the front-face input");
    using ShaderRecompiler::PixelPositionLocation;
    Require(decode(0x326u, 0x326u).positionLocation == PixelPositionLocation::Center, "POS_FLOAT_LOCATION 0 was not decoded as the pixel center");
    queue.context[0x1b8] = 0x10000u;
    Require(decode(0x326u, 0x326u).positionLocation == PixelPositionLocation::Centroid, "POS_FLOAT_LOCATION 1 was not decoded as the centroid");
    queue.context[0x1b8] = 0x20000u;
    Require(decode(0x326u, 0x326u).positionLocation == PixelPositionLocation::Sample, "POS_FLOAT_LOCATION 2 was not decoded as the iterated sample");
    queue.context[0x1b8] = 0x120000u;
    Require(decode(0x326u, 0x326u).positionLocation == PixelPositionLocation::UpperLeft, "POS_FLOAT_ULC did not override POS_FLOAT_LOCATION");
    queue.context[0x1b8] = 0x30000u;
    expectFailure([&] { static_cast<void>(decode(0x326u, 0x326u)); }, "POS_FLOAT_LOCATION uses the reserved value 3");
    Require(decode(0x1002u, 0x1002u).positionLocation == PixelPositionLocation::Center, "POS_FLOAT_LOCATION was decoded for a pixel program without positions");
    queue.context.erase(0x1b8);
    Require(decode(0x326u, 0x326u).positionLocation == PixelPositionLocation::Center, "positions without SPI_BARYC_CNTL were not at the pixel center");
    queue.context[0x1b8] = 0u;
    pixel = decode(0x506u, 0x7afu);
    Require(pixel.inputAddr == 0x7afu && !pixel.posY && pixel.posZ && PixelInputVgpr(pixel.inputAddr, PixelInput::PerspectiveCentroid) == 4u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionX) == 12u && PixelInputVgpr(pixel.inputAddr, PixelInput::PositionZ) == 14u, "ADDR-only inputs did not reserve their VGPRs");
    expectFailure([&] { static_cast<void>(decode(0xau, 0xau)); }, "unsupported SPI_PS_INPUT_ENA/ADDR");
    pixel = decode(0x4002u, 0x4002u);
    Require(pixel.inputAddr == 0x4002u && PixelInputVgpr(pixel.inputAddr, PixelInput::SampleCoverage) == 2u, "SAMPLE_COVERAGE did not take its VGPR after the perspective center");
    Require(readsBuiltin(pixelBuiltinsRead(0x4002u, 0x4002u, 2u), spv::BuiltInSampleMask), "SAMPLE_COVERAGE is not read from gl_SampleMaskIn");
    Require(sampleMaskInputs(0x4002u, 0xffffu) == 1u && sampleMaskInputs(0x4002u, 0x0003u) == 1u, "SAMPLE_COVERAGE beside a partial quad sample mask declared gl_SampleMaskIn more than once");
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
    for (const auto inputs : {0x11u, 0x12u, 0x21u}) {
        const auto sampleNoPerspective = pixelNoPerspectiveLocations(false, inputs);
        Require(sampleNoPerspective.size() == 1 && sampleNoPerspective[0] == 1u, "a parameter interpolated through a sample I/J pair lost its interpolation mode: inputs " + std::to_string(inputs));
        Require(pixelNoPerspectiveLocations(true, inputs).empty(), "explicit interpolation through a sample I/J pair interpolated parameter arrays a second time: inputs " + std::to_string(inputs));
    }
    for (const auto source : {0u, 1u}) {
        const auto read = pixelBuiltinsRead(0x3u, 0x3u, source);
        Require(readsBuiltin(read, spv::BuiltInBaryCoordKHR) && !readsBuiltin(read, spv::BuiltInFragCoord), "PERSP_SAMPLE does not hold the perspective barycentrics: v" + std::to_string(source));
    }
    for (const auto source : {2u, 3u}) {
        const auto read = pixelBuiltinsRead(0x12u, 0x12u, source);
        Require(readsBuiltin(read, spv::BuiltInBaryCoordNoPerspKHR) && !readsBuiltin(read, spv::BuiltInBaryCoordKHR), "LINEAR_SAMPLE does not hold the linear barycentrics: v" + std::to_string(source));
    }
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

void BlendBypassTests() {
    auto queue = makeState();
    for (std::uint32_t constant = 0x105; constant < 0x109; ++constant) queue.context[constant] = 0;
    queue.context[0x1e0] = (1u << 30u) | 4u | (5u << 8u);
    Require(AgcDriver::Graphics::DecodeState(queue).blends.at(0).blendEnable, "enabled blending was not decoded");
    queue.context[0x31c] |= 0x10000u;
    const auto bypassed = AgcDriver::Graphics::DecodeState(queue);
    Require(!bypassed.blends.at(0).blendEnable && bypassed.blends.at(0).colorWriteMask != 0, "a target with BLEND_BYPASS still blends");
}

void UnclampedNormalizedColorTests() {
    using AgcDriver::Graphics::DecodeState;
    constexpr std::uint32_t blending = (1u << 30u) | 4u | (5u << 8u);
    const auto target = [](std::uint32_t info) {
        auto queue = makeState();
        for (std::uint32_t constant = 0x105; constant < 0x109; ++constant) queue.context[constant] = 0;
        queue.context[0x31c] = info;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        return queue;
    };
    auto queue = target(0x0000000cu);
    auto state = DecodeState(queue);
    Require(state.colors.size() == 1 && state.colors[0].format == VK_FORMAT_R8G8_UNORM && !state.blends.at(0).blendEnable && state.blends.at(0).colorWriteMask != 0, "an 8_8 UNORM target with BLEND_CLAMP clear (CB_COLOR_INFO 0x0000000c, PPSA26344) was refused without blending");
    const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused an unclamped UNORM target that does not blend: " + rejection);
    Require(AgcDriver::Graphics::DecodeColorBuffer(queue.context, 0).format == VK_FORMAT_R8G8_UNORM, "an unclamped UNORM target was refused outside a draw");
    auto eliminate = target(0x0000000cu);
    eliminate.context[0x0] = 0;
    eliminate.context[0x202] = 0xcc0020;
    const auto pass = AgcDriver::Graphics::DecodeColorMetadataPass(eliminate);
    Require(pass.has_value() && pass->mode == AgcDriver::Graphics::ColorMetadataPass::Mode::EliminateFastClear && pass->targets.size() == 1 && pass->targets[0].format == VK_FORMAT_R8G8_UNORM, "a fast-clear eliminate over an unclamped UNORM target was refused");
    queue.context[0x1e0] = blending;
    expectFailure([&] { DecodeState(queue); }, "blending unclamped normalized color is not implemented (CB_COLOR_INFO 0x0000000c, CB_BLEND0_CONTROL 0x40000504)");
    queue.context[0x31c] |= 0x10000u;
    state = DecodeState(queue);
    Require(!state.blends.at(0).blendEnable && state.blends.at(0).colorWriteMask != 0, "an unclamped UNORM target with BLEND_BYPASS was refused or blended");
    queue = target(0x0000010cu);
    Require(DecodeState(queue).colors.at(0).format == VK_FORMAT_R8G8_SNORM, "an unclamped SNORM target that does not blend was refused");
    queue.context[0x1e0] = blending;
    expectFailure([&] { DecodeState(queue); }, "blending unclamped normalized color is not implemented");
    queue = target(0x00000604u);
    expectFailure([&] { DecodeState(queue); }, "unclamped normalized color is unsupported");
    queue = target(0x0000800cu);
    queue.context[0x1e0] = blending;
    Require(DecodeState(queue).blends.at(0).blendEnable, "a clamped UNORM target stopped blending");
    queue = target(0x00000708u);
    queue.context[0x1e0] = blending;
    state = DecodeState(queue);
    Require(state.colors.at(0).format == VK_FORMAT_R16_SFLOAT && state.blends.at(0).blendEnable, "a 16 float target, which keeps BLEND_CLAMP clear, stopped blending");
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
    const auto emptyScreen = AgcDriver::Graphics::DecodeState(queue);
    Require(emptyScreen.renderExtent.width == 1 && emptyScreen.renderExtent.height == 1, "an empty screen scissor without targets did not get a one-pixel framebuffer");
    Require(emptyScreen.scissor.extent.width == 0 && emptyScreen.scissor.extent.height == 0, "an empty screen scissor without targets rasterized pixels");
    queue.context[0xd] = 64u;
    const auto emptyRows = AgcDriver::Graphics::DecodeState(queue);
    Require(emptyRows.renderExtent.width == 64 && emptyRows.scissor.extent.height == 0, "a screen scissor without rows rasterized pixels");
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
    Require(depthOnly.depth->firstSlice == 0, "a depth view of slice 0 decoded another slice");
    queue.context[0x002] = 3;
    const auto thirdSlice = AgcDriver::Graphics::DecodeState(queue);
    Require(thirdSlice.depth && thirdSlice.depth->firstSlice == 3 && thirdSlice.depth->address == depthOnly.depth->address + 3u * AgcDriver::Graphics::DepthSliceBytes(depthOnly.depth->extent, 4u), "a depth view of slice 3 lost its slice or its Z offset");
    queue.context[0x002] = 1u << 26u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "on a surface without mips");
    queue.context[0x002] = 0;
    {
        using AgcDriver::Graphics::ComputeElementMipLayout;
        using AgcDriver::Graphics::TextureTileMode;
        auto mipped = queue;
        mipped.context[0x007] = 0x03ff03ff;
        mipped.context[0x010] = 0x80000181u | (2u << 16u);
        const auto mips = ComputeElementMipLayout(TextureTileMode::kZ64KBX, 2u, 1024u, 1024u, 3u);
        Require(!mips.at(1).tail && mips.at(1).width == 512u && mips.at(1).height == 512u, "the 1024x1024 Z_X layout does not give a 512x512 second mip outside the tail");
        const auto first = AgcDriver::Graphics::DecodeState(mipped);
        Require(first.depth && first.depth->address == depthOnly.depth->address + mips.at(0).tiledOffset && first.depth->extent.width == 1024u && first.depth->extent.height == 1024u, "view mip 0 of a mip chain is not its first level");
        mipped.context[0x002] = 1u << 26u;
        const auto second = AgcDriver::Graphics::DecodeState(mipped);
        Require(second.depth && second.depth->address == depthOnly.depth->address + mips.at(1).tiledOffset, "view mip 1 is not at the second level's offset in the Z_X mip chain");
        Require(second.depth->extent.width == 512u && second.depth->extent.height == 512u, "view mip 1 does not render at the second level's extent");
        mipped.context[0x002] = (1u << 26u) | 1u | (1u << 13u);
        const auto sliced = AgcDriver::Graphics::DecodeState(mipped);
        Require(sliced.depth && sliced.depth->address == second.depth->address + AgcDriver::Graphics::ComputeSurfaceSize(mips, 1u), "slice 1 of a mip chain does not step by the whole chain");
        mipped.context[0x002] = 3u << 26u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(mipped); }, "is beyond the surface's mips");
        mipped.context[0x002] = 2u << 26u;
        mipped.context[0x007] = 0x003f003f;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(mipped); }, "into the packed mip tail is unsupported");
        mipped.context[0x007] = 0x03ff03ff;
        mipped.context[0x002] = 1u << 26u;
        mipped.context[0x010] |= 1u << 29u;
        mipped.context[0x005] = 0x300;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(mipped); }, "HTILE for the depth view mip");
        mipped.context[0x010] = (0x80000181u | (2u << 16u)) & ~(31u << 4u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(mipped); }, "uses a swizzle other than 64 KiB Z_X");
    }
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

void ShadersWriteMemoryTests() {
    const auto binding = [](ShaderRecompiler::DescriptorKind kind, ShaderRecompiler::DescriptorRole role, std::vector<bool> written) {
        ShaderRecompiler::DescriptorBinding result{};
        result.kind = kind;
        result.role = role;
        result.count = 1;
        if (role == ShaderRecompiler::DescriptorRole::GuestImages) result.imageWritten = std::move(written);
        else result.bufferWritten = std::move(written);
        return result;
    };
    ShaderRecompiler::RecompileResult reads;
    reads.bindings.push_back(binding(ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::GuestBuffers, {false}));
    reads.bindings.push_back(binding(ShaderRecompiler::DescriptorKind::StorageImage, ShaderRecompiler::DescriptorRole::GuestImages, {false}));
    ShaderRecompiler::RecompileResult stores;
    stores.bindings.push_back(binding(ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::GuestBuffers, {true}));
    using Stage = ShaderRecompiler::ShaderStage;
    const auto writes = [](std::initializer_list<AgcDriver::Graphics::CompiledShader> shaders) { return AgcDriver::Graphics::ShadersWriteMemory(std::vector<AgcDriver::Graphics::CompiledShader>(shaders)); };
    Require(!writes({{Stage::Vertex, &reads, 0}, {Stage::Fragment, &reads, 0}}), "shaders that only read memory were reported as writing it");
    Require(writes({{Stage::Vertex, &reads, 0}, {Stage::Fragment, &stores, 0}}), "a pixel shader that stores to a buffer was not reported");
    Require(writes({{Stage::Vertex, &stores, 0}, {Stage::Fragment, &reads, 0}}), "a vertex shader that stores to a buffer was not reported");
    Require(writes({{Stage::Geometry, &stores, 0}, {Stage::Fragment, &reads, 0}}), "a geometry shader that stores to a buffer was not reported");
}

void ReversedComponentOrderTests() {
    {
        auto queue = makeState();
        queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u) | (3u << 11u))) | (3u << 2u) | (1u << 11u);
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.colors.size() == 1 && state.colors[0].format == VK_FORMAT_R8G8_UNORM && state.colors[0].componentMapping == 0xecu, "an 8_8 target with SWAP_ALT did not store the x and w exports in its two channels");
        queue.context[0x1e0] = 0x40010001u;
        for (const auto offset : {0x105u, 0x106u, 0x107u, 0x108u}) queue.context[offset] = 0;
        const auto blended = AgcDriver::Graphics::DecodeState(queue);
        Require(blended.blends.size() == 1 && blended.blends[0].blendEnable, "blending into an 8_8 SWAP_ALT target with one equation for color and alpha was rejected");
        Require(!AgcDriver::Graphics::SplitsAlternateAlpha(blended), "one blend equation for color and alpha split an 8_8 SWAP_ALT draw");
        queue.context[0x1e0] = 0x60000001u;
        Require(AgcDriver::Graphics::SplitsAlternateAlpha(AgcDriver::Graphics::DecodeState(queue)), "a separate alpha blend into an 8_8 SWAP_ALT target did not split its second channel");
        queue.context[0x1e0] = (1u << 30u) | 13u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "constant color factor");
        queue.context[0x1e0] = (1u << 30u) | (1u << 29u) | 1u | (1u << 16u) | (14u << 24u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "constant color factor");
        queue.context[0x1e0] = (1u << 30u) | (2u << 5u) | 13u;
        Require(AgcDriver::Graphics::DecodeState(queue).blends[0].colorBlendOp == VK_BLEND_OP_MIN, "a constant color factor of a MIN blend into an 8_8 SWAP_ALT target was rejected");
    }
    for (const auto& [swap, mapping] : {std::pair{2u, 0x1bu}, std::pair{3u, 0x93u}}) {
        auto queue = makeState();
        queue.context[0x31c] = (queue.context[0x31c] & ~(3u << 11u)) | (swap << 11u);
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.colors.size() == 1 && state.colors[0].format == VK_FORMAT_R8G8B8A8_UNORM && state.colors[0].componentMapping == mapping, "an 8_8_8_8 target with a reversed component order did not map exports onto RGBA8");
        Require(AgcDriver::Graphics::ExportMappings(state)[0] == mapping && state.blends[0].colorWriteMask == 0xfu, "a reversed 8_8_8_8 target did not write all four channels through its export mapping");
        queue.context[0x1e0] = 0x40010001u;
        for (const auto offset : {0x105u, 0x106u, 0x107u, 0x108u}) queue.context[offset] = 0;
        const auto blended = AgcDriver::Graphics::DecodeState(queue);
        Require(blended.blends[0].blendEnable && blended.blends[0].srcColorBlendFactor == VK_BLEND_FACTOR_ONE && blended.blends[0].dstColorBlendFactor == VK_BLEND_FACTOR_ZERO && blended.blends[0].srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE && blended.blends[0].dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO && blended.reversedBlend == 0u && !blended.dualSourceBlend, "a ONE + ZERO blend into a reversed 8_8_8_8 target did not stay a plain Vulkan blend");
        queue.context[0x1e0] = 0;
        queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u))) | (12u << 2u) | (7u << 8u);
        const auto wide = AgcDriver::Graphics::DecodeState(queue);
        Require(wide.colors.size() == 1 && wide.colors[0].format == VK_FORMAT_R16G16B16A16_SFLOAT && wide.colors[0].componentMapping == mapping && wide.blends[0].colorWriteMask == 0xfu, "a 16_16_16_16 float target with a reversed component order did not map exports onto RGBA16");
    }
}

AgcDriver::QueueState reversedBlendQueue(std::uint32_t format, std::uint32_t number, std::uint32_t swap, std::uint32_t control) {
    auto queue = makeState();
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1f7cu) | (format << 2u) | (number << 8u) | (swap << 11u);
    const auto address = reinterpret_cast<std::uintptr_t>(sliceMemory.data());
    queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    queue.context[0x1e0] = control;
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = 0;
    return queue;
}

void ReversedComponentOrderBlendTests() {
    using AgcDriver::Graphics::DecodeState;
    struct Target {
        std::uint32_t format;
        std::uint32_t number;
        VkFormat vkFormat;
    };
    struct Case {
        std::uint32_t control;
        VkBlendFactor source;
        VkBlendFactor destination;
        VkBlendFactor sourceAlpha;
        VkBlendFactor destinationAlpha;
        VkBlendOp operation;
        std::uint32_t code;
        bool opaqueDestination;
    };
    const std::array<Target, 3> targets{{
        {10u, 0u, VK_FORMAT_R8G8B8A8_UNORM}, {12u, 7u, VK_FORMAT_R16G16B16A16_SFLOAT}, {12u, 0u, VK_FORMAT_R16G16B16A16_UNORM}
    }};
    const std::array<Case, 10> cases{{
        {0x65010504u, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_OP_ADD, 0x570001u, false},
        {0x65810584u, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_OP_REVERSE_SUBTRACT, 0x570001u, false},
        {0x40000501u, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_OP_ADD, 0x540001u, false},
        {0x61000104u, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 0x560001u, false},
        {0x61020302u, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, 0x020001u, false},
        {0x61010504u, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_OP_ADD, 0x560301u, false},
        {0x60060008u, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0u, false},
        {0x40000101u, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 0u, false},
        {0x40000141u, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_MIN, 0u, false},
        {0x40000706u, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0u, true},
    }};
    const std::array<std::pair<std::uint32_t, const char*>, 10> refused{{
        {0x40000706u, "with a factor of the destination alpha in the color equation"},
        {0x4000000au, "with a factor of the destination alpha in the color equation"},
        {0x61000008u, "with factors the host blend cannot form from copies of the export is not implemented"},
        {0x61000501u, "with factors the host blend cannot form from copies of the export is not implemented"},
        {0x65050302u, "with factors the host blend cannot form from copies of the export is not implemented"},
        {0x4000000du, "with a constant blend factor"},
        {0x40000013u, "with a constant blend factor"},
        {0x4000000fu, "with a second-source factor is not implemented, as dual-source blending into a component-swapped color target is unsupported"},
        {0x65240504u, "with different color and alpha operations"},
        {0x40000544u, "with a MIN or MAX operation whose factors are not ONE"},
    }};
    for (const auto& [swap, mapping] : {std::pair{2u, std::uint8_t{0x1bu}}, std::pair{3u, std::uint8_t{0x93u}}}) {
        for (const auto& target : targets) {
            for (const auto& test : cases) {
                auto queue = reversedBlendQueue(target.format, target.number, swap, test.control);
                if (test.opaqueDestination) queue.context[0x31d] = 0x20000u;
                const auto state = DecodeState(queue);
                const auto code = test.code;
                char what[128];
                std::snprintf(what, sizeof(what), "CB_BLEND0_CONTROL 0x%08x into color format %u number type %u with component mapping 0x%02x", test.control, target.format, target.number, mapping);
                Require(state.colors.size() == 1 && state.colors[0].format == target.vkFormat && state.colors[0].componentMapping == mapping, std::string(what) + ": the target decoded differently");
                const auto& blend = state.blends.at(0);
                Require(blend.blendEnable && blend.srcColorBlendFactor == test.source && blend.dstColorBlendFactor == test.destination && blend.srcAlphaBlendFactor == test.sourceAlpha && blend.dstAlphaBlendFactor == test.destinationAlpha && blend.colorBlendOp == test.operation && blend.alphaBlendOp == test.operation && blend.colorWriteMask == 0xfu, std::string(what) + ": wrong Vulkan blend state");
                Require(state.reversedBlend == code && !state.dualSourceBlend, std::string(what) + ": wrong pixel shader blend code " + std::to_string(state.reversedBlend));
                Require(AgcDriver::Graphics::DecodePixelStageInfo(queue.context, AgcDriver::Graphics::ExportMappings(state)).targetExportMapping[0] == mapping, std::string(what) + ": the export mapping changed");
                const auto direct = AgcDriver::Graphics::TranslateReversedBlend(test.control, mapping, target.vkFormat, test.opaqueDestination);
                Require(direct.shaderCode == code && direct.blend.srcColorBlendFactor == blend.srcColorBlendFactor && direct.blend.dstColorBlendFactor == blend.dstColorBlendFactor && direct.blend.srcAlphaBlendFactor == blend.srcAlphaBlendFactor && direct.blend.dstAlphaBlendFactor == blend.dstAlphaBlendFactor && direct.blend.colorBlendOp == blend.colorBlendOp && direct.blend.alphaBlendOp == blend.alphaBlendOp, std::string(what) + ": TranslateReversedBlend disagrees with DecodeState");
            }
        }
        for (const auto& [control, reason] : refused) {
            const auto queue = reversedBlendQueue(10u, 0u, swap, control);
            expectFailure([&] { DecodeState(queue); }, std::string("blending into a color target with a reversed component order ") + reason);
        }
        char registers[96];
        std::snprintf(registers, sizeof(registers), "(CB_BLEND_CONTROL 0x65010504, component mapping 0x%02x, VkFormat %d)", mapping, static_cast<int>(VK_FORMAT_R8G8B8A8_SRGB));
        expectFailure([&] { DecodeState(reversedBlendQueue(10u, 6u, swap, 0x65010504u)); }, std::string("reversed component order is not implemented for sRGB targets ") + registers);
        expectFailure([&] { DecodeState(reversedBlendQueue(10u, 1u, swap, 0x65010504u)); }, "reversed component order is not implemented for signed normalized targets");
        expectFailure([&] { DecodeState(reversedBlendQueue(12u, 1u, swap, 0x65010504u)); }, "reversed component order is not implemented for signed normalized targets");
        expectFailure([&] { DecodeState(reversedBlendQueue(12u, 4u, swap, 0x65010504u)); }, "reversed component order is not implemented for integer targets");
        auto masked = reversedBlendQueue(10u, 0u, swap, 0x65010504u);
        masked.context[0x8e] = 7u;
        Require(DecodeState(masked).blends.at(0).colorWriteMask == 0xeu, "a reversed target's blend did not keep the export-order write mask of its first three exports");
        auto twoTargets = reversedBlendQueue(10u, 0u, swap, 0x65010504u);
        for (const auto offset : {0x318u, 0x31bu, 0x31cu, 0x31du}) twoTargets.context[offset + 0xfu] = twoTargets.context.at(offset);
        for (const auto offset : {0x390u, 0x3b0u, 0x3b8u}) twoTargets.context[offset + 1u] = twoTargets.context.at(offset);
        twoTargets.context[0x31c + 0xfu] &= ~(3u << 11u);
        twoTargets.context[0x1c5] = 0x99u;
        twoTargets.context[0x8e] = 0xffu;
        twoTargets.context[0x8f] = 0xffu;
        twoTargets.context[0x1e1] = 0;
        expectFailure([&] { DecodeState(twoTargets); }, "reversed component order through a second source color needs that target to be the shader's only color export");
        twoTargets.context[0x1e0] = 0x40000101u;
        const auto two = DecodeState(twoTargets);
        Require(two.colors.size() == 2 && two.reversedBlend == 0u && two.blends.at(0).srcColorBlendFactor == VK_BLEND_FACTOR_ONE && two.blends.at(0).dstColorBlendFactor == VK_BLEND_FACTOR_ONE, "a reversed target blended by Vulkan alone was refused next to a second target");
        twoTargets.context[0x31c + 0xfu] |= swap << 11u;
        twoTargets.context[0x1e0] = 0x65010504u;
        twoTargets.context[0x1e1] = 0x40000101u;
        expectFailure([&] { DecodeState(twoTargets); }, "reversed component order through a second source color needs that target to be the shader's only color export");
    }
}

void ReversedBlendRoutingTests() {
    namespace Code = ShaderRecompiler::ReversedBlendCode;
    using Term = std::pair<bool, std::uint32_t>;
    constexpr Term zero{false, 0u};
    constexpr Term one{true, 0u};
    const auto flip = [](Term term) { return Term{!term.first, term.second}; };
    const auto guest = [&](std::uint32_t factor, std::uint32_t component) -> std::optional<Term> {
        const std::uint32_t exported = 10u + component;
        const std::uint32_t stored = 20u + component;
        switch (factor) {
            case 0: return zero;
            case 1: return one;
            case 2: return Term{false, exported};
            case 3: return Term{true, exported};
            case 4: return Term{false, 10u};
            case 5: return Term{true, 10u};
            case 6: return Term{false, 20u};
            case 7: return Term{true, 20u};
            case 8: return Term{false, stored};
            case 9: return Term{true, stored};
            default: return component == 0u ? std::optional<Term>{one} : std::nullopt;
        }
    };
    const auto product = [&](Term first, Term second) {
        if (first == zero || second == zero) return std::vector<Term>{zero};
        std::vector<Term> factors;
        for (const auto term : {first, second}) {
            if (term != one) factors.push_back(term);
        }
        std::sort(factors.begin(), factors.end());
        return factors;
    };
    std::uint32_t routed = 0;
    std::uint32_t native = 0;
    for (std::uint32_t factors = 0; factors < 11u * 11u * 11u * 11u; ++factors) {
        const std::array<std::uint32_t, 4> guestFactors{factors % 11u, factors / 11u % 11u, factors / 121u % 11u, factors / 1331u};
        const auto control = guestFactors[0] | (guestFactors[1] << 8u) | (guestFactors[2] << 16u) | (guestFactors[3] << 24u) | 0x20000000u;
        std::optional<AgcDriver::Graphics::ReversedBlend> translated;
        try {
            translated = AgcDriver::Graphics::TranslateReversedBlend(control, 0x1bu, VK_FORMAT_R8G8B8A8_UNORM, false);
        } catch (const std::runtime_error& error) {
            Require(std::string(error.what()).find("reversed component order with") != std::string::npos, std::string("an unexpected reversed-order blend refusal: ") + error.what());
            continue;
        }
        const auto code = translated->shaderCode;
        Require(code == 0u || (code & Code::Enable) != 0u, "a routed reversed-order blend code lacks its enable bit");
        const auto route = [&](std::uint32_t shift, std::uint32_t component) {
            switch ((code >> (shift + Code::RouteBits * component)) & Code::RouteMask) {
                case Code::Own: return Term{false, 10u + component};
                case Code::Component0: return Term{false, 10u};
                case Code::Zero: return zero;
                default: return one;
            }
        };
        const auto host = [&](VkBlendFactor factor, std::uint32_t component) -> std::optional<Term> {
            switch (factor) {
                case VK_BLEND_FACTOR_ZERO: return zero;
                case VK_BLEND_FACTOR_ONE: return one;
                case VK_BLEND_FACTOR_SRC_COLOR: return route(Code::ColorShift, component);
                case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return flip(route(Code::ColorShift, component));
                case VK_BLEND_FACTOR_SRC_ALPHA: return route(Code::ColorShift, 3u);
                case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return flip(route(Code::ColorShift, 3u));
                case VK_BLEND_FACTOR_DST_COLOR: return Term{false, 20u + component};
                case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return Term{true, 20u + component};
                case VK_BLEND_FACTOR_DST_ALPHA: return Term{false, 23u};
                case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return Term{true, 23u};
                case VK_BLEND_FACTOR_SRC1_COLOR: return route(Code::SecondShift, component);
                case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return flip(route(Code::SecondShift, component));
                case VK_BLEND_FACTOR_SRC1_ALPHA: return route(Code::SecondShift, 3u);
                case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return flip(route(Code::SecondShift, 3u));
                default: return std::nullopt;
            }
        };
        const auto& blend = translated->blend;
        const auto secondColor = [](VkBlendFactor factor) { return factor == VK_BLEND_FACTOR_SRC1_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR || factor == VK_BLEND_FACTOR_SRC1_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA; };
        Require(code != 0u || !(secondColor(blend.srcColorBlendFactor) || secondColor(blend.dstColorBlendFactor) || secondColor(blend.srcAlphaBlendFactor) || secondColor(blend.dstAlphaBlendFactor)), "a reversed-order blend without a second color reads one");
        for (std::uint32_t component = 0; component < 4; ++component) {
            const auto wantedSource = guest(guestFactors[component == 0u ? 2u : 0u], component);
            const auto wantedDestination = guest(guestFactors[component == 0u ? 3u : 1u], component);
            const auto source = host(component == 3u ? blend.srcAlphaBlendFactor : blend.srcColorBlendFactor, component);
            const auto destination = host(component == 3u ? blend.dstAlphaBlendFactor : blend.dstColorBlendFactor, component);
            Require(wantedSource && wantedDestination && source && destination, "a reversed-order blend was accepted with a factor the routing check cannot express");
            char what[160];
            std::snprintf(what, sizeof(what), "CB_BLEND_CONTROL 0x%08x, code 0x%06x, component %u", control, code, component);
            Require(product(route(Code::ColorShift, component), *source) == product(Term{false, 10u + component}, *wantedSource), std::string(what) + ": the routed source term differs from the guest's");
            Require(*destination == *wantedDestination, std::string(what) + ": the routed destination factor differs from the guest's");
        }
        ++(code == 0u ? native : routed);
    }
    Require(native == 121u && routed == 932u, "the reversed-order blend routing accepts " + std::to_string(native) + " native and " + std::to_string(routed) + " routed factor combinations, not 121 and 932");
}

void Color565TargetTests() {
    auto queue = makeState();
    const auto info = [&](std::uint32_t number, std::uint32_t swap) {
        queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u) | (3u << 11u))) | (16u << 2u) | (number << 8u) | (swap << 11u);
    };
    info(0, 2);
    const auto reversed = AgcDriver::Graphics::DecodeState(queue);
    Require(reversed.colors.size() == 1 && reversed.colors[0].format == VK_FORMAT_R5G6B5_UNORM_PACK16 && reversed.colors[0].elementBytes == 2 && reversed.colors[0].componentMapping == 0xe4u, "a 5_6_5 target with the reversed component order did not render through R5G6B5 with X in its high bits");
    Require(reversed.blends[0].colorWriteMask == 0xfu, "a 5_6_5 target with the reversed component order did not write all its channels");
    queue.context[0x8e] = 0x1u;
    Require(AgcDriver::Graphics::DecodeState(queue).blends[0].colorWriteMask == VK_COLOR_COMPONENT_R_BIT, "a 5_6_5 target with the reversed component order writing only X did not write the R5G6B5 channel in its high bits");
    info(0, 0);
    const auto standard = AgcDriver::Graphics::DecodeState(queue);
    Require(standard.colors[0].format == VK_FORMAT_R5G6B5_UNORM_PACK16 && standard.colors[0].componentMapping == 0xc6u, "a 5_6_5 target with the standard component order did not render through R5G6B5 with X and Z exchanged");
    Require(AgcDriver::Graphics::ExportMappings(standard)[0] == 0xc6u, "a 5_6_5 target with the standard component order did not export through the X and Z exchange");
    Require(standard.blends[0].colorWriteMask == VK_COLOR_COMPONENT_B_BIT, "a 5_6_5 target with the standard component order writing only X did not write the R5G6B5 channel in its low bits");
    queue.context[0x8e] = 0xfu;
    info(0, 2);
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = 0;
    queue.context[0x1e0] = 0x40000504u;
    const auto blended = AgcDriver::Graphics::DecodeState(queue);
    Require(blended.blends[0].blendEnable && blended.blends[0].srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && blended.blends[0].dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, "alpha blending into a 5_6_5 target was not decoded");
    queue.context[0x1e0] = 0;
    for (const auto& [number, swap] : {std::pair{1u, 2u}, std::pair{4u, 0u}, std::pair{0u, 1u}, std::pair{0u, 3u}}) {
        info(number, swap);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color format 16 number type " + std::to_string(number) + " component swap " + std::to_string(swap));
    }
}

void Color1555TargetTests() {
    auto queue = makeState();
    queue.context[0x31c] = (queue.context[0x31c] & ~((0x1fu << 2u) | (7u << 8u) | (3u << 11u))) | (17u << 2u);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.colors.size() == 1 && state.colors[0].format == VK_FORMAT_A1R5G5B5_UNORM_PACK16 && state.colors[0].elementBytes == 2 && state.colors[0].componentMapping == 0xc6u, "a 1_5_5_5 target did not render through A1R5G5B5 with X and Z exchanged");
    Require(AgcDriver::Graphics::ExportMappings(state)[0] == 0xc6u && state.blends[0].colorWriteMask == 0xfu, "a 1_5_5_5 target did not write all four channels through its export mapping");
    queue.context[0x8e] = 0x1u;
    Require(AgcDriver::Graphics::DecodeState(queue).blends[0].colorWriteMask == VK_COLOR_COMPONENT_B_BIT, "a 1_5_5_5 target writing only X did not write the A1R5G5B5 channel in its low bits");
    queue.context[0x8e] = 0xfu;
    for (std::uint32_t i = 0; i < 4; ++i) queue.context[0x105 + i] = 0;
    queue.context[0x1e0] = 0x40000504u;
    const auto blended = AgcDriver::Graphics::DecodeState(queue);
    Require(blended.blends[0].blendEnable && blended.blends[0].srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && blended.blends[0].dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, "alpha blending into a 1_5_5_5 target was not decoded");
    queue.context[0x1e0] = 0;
    for (const auto& [number, swap] : {std::pair{1u, 0u}, std::pair{4u, 0u}, std::pair{0u, 1u}}) {
        queue.context[0x31c] = (queue.context[0x31c] & ~((7u << 8u) | (3u << 11u))) | (number << 8u) | (swap << 11u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "unsupported color format 17 number type " + std::to_string(number) + " component swap " + std::to_string(swap));
    }
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
    queue.context[0x005] = 0x300;
    queue.context[0x01e] = 0x1;
    {
        AgcDriver::QueueState unwritten;
        unwritten.shader = queue.shader;
        unwritten.userConfig = queue.userConfig;
        queue.context[0x200] = 0x00700791;
        for (int reset = 0; reset < 2; ++reset) {
            queue.context.ForEach([&](std::uint32_t offset, std::uint32_t value) { unwritten.context[offset] = value; });
            const auto state = AgcDriver::Graphics::DecodeState(unwritten);
            Require(state.stencilTest && state.depth, "unwritten stencil registers rejected a stencil draw");
            for (const auto face : {state.stencilFront, state.stencilBack}) {
                Require(face.failOp == VK_STENCIL_OP_KEEP && face.passOp == VK_STENCIL_OP_KEEP && face.depthFailOp == VK_STENCIL_OP_KEEP && face.compareOp == VK_COMPARE_OP_ALWAYS, "unwritten stencil operations changed");
                Require(face.compareMask == 0 && face.writeMask == 0 && face.reference == 0, "unwritten stencil reference or masks changed");
            }
            Require(AgcDriver::Graphics::DrawRejection(unwritten, false).empty(), "precheck rejected unwritten stencil registers");
            unwritten.context[0x10b] = 0x00070070;
            unwritten.context[0x10c] = unwritten.context[0x10d] = 0x01ffff09;
            const auto explicitState = AgcDriver::Graphics::DecodeState(unwritten);
            for (const auto face : {explicitState.stencilFront, explicitState.stencilBack}) Require(face.passOp == VK_STENCIL_OP_INVERT && face.compareMask == 0xff && face.writeMask == 0xff && face.reference == 9, "explicit stencil state was replaced by defaults");
            unwritten.ClearContext();
            for (const auto offset : {0x10bu, 0x10cu, 0x10du}) Require(unwritten.context.at(offset) == 0, "context clear did not reset stencil state");
        }
    }
    queue.context[0x10b] = 0x00050050;
    queue.context[0x10c] = 0x01ffff01;
    queue.context[0x10d] = 0x01000001;
    queue.context[0x200] = 0x00700711;
    std::vector<AgcDriver::Graphics::RegisterRead> log;
    auto state = [&] {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&log);
        return AgcDriver::Graphics::DecodeState(queue);
    }();
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a depth register the decoder reads: " + std::to_string(read.offset));
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
    Require(rejection.empty(), "precheck rejected a stencil draw with a surface: " + rejection);
    Require(state.depth && state.depth->address == 0x10000 && state.depth->stencilAddress == 0x20000 && state.depth->format == VK_FORMAT_D32_SFLOAT_S8_UINT && state.depth->clearStencil == 7 && state.depth->htileAddress == 0x10000030000ull && !state.depth->htileStencil, "depth surface decode changed");
    {
        auto split = queue;
        split.context[0x014] = 0;
        for (const auto control : {0x00700711u, 0x00700715u, 0x00700713u}) {
            split.context[0x200] = control;
            const auto splitState = AgcDriver::Graphics::DecodeState(split);
            Require(splitState.depth && splitState.depth->address == 0x10000 && !splitState.depthWrite, "a depth plane the draw does not write rejected another write address");
            Require(AgcDriver::Graphics::DrawRejection(split, false).empty(), "precheck rejected a depth plane the draw does not write at another write address");
        }
        split.context[0x200] = 0x00700717u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(split); }, "depth read and written at different addresses");
        split.context[0x200] = 0x00700711u;
        for (const auto control : {0x01u, 0x20u, 0x40u, 0x60u}) {
            split.context[0x000] = control;
            expectFailure([&] { AgcDriver::Graphics::DecodeState(split); }, "depth read and written at different addresses");
        }
    }
    for (const auto clear : {0u, 1u, 2u, 3u}) {
        auto retained = queue;
        retained.context[0x8e] = retained.context[0x8f] = retained.context[0x203] = 0;
        retained.context[0x000] = clear;
        retained.context[0x200] = 0x00700777u;
        auto unbound = retained;
        unbound.shader[0x008] = unbound.shader[0x009] = 0;
        const auto expected = AgcDriver::Graphics::DecodeState(unbound);
        for (const auto format : {1u, 2u, 3u, 9u}) {
            retained.context[0x1c4] = format;
            Require(AgcDriver::Graphics::PixelProgramSkipped(retained), "a retained format ran the pixel program during a depth/stencil pass");
            const auto actual = AgcDriver::Graphics::DecodeState(retained);
            Require(actual.depth && expected.depth && actual.depth->address == expected.depth->address && actual.depth->stencilAddress == expected.depth->stencilAddress && actual.depth->format == expected.depth->format && actual.depth->clearDepth == expected.depth->clearDepth && actual.depth->clearStencil == expected.depth->clearStencil, "null substitution changed the depth/stencil surface");
            Require(actual.depthTest == expected.depthTest && actual.depthWrite == expected.depthWrite && actual.depthCompare == expected.depthCompare && actual.depthBoundsTest == expected.depthBoundsTest && actual.stencilTest == expected.stencilTest, "null substitution changed fixed-function depth/stencil tests");
            Require(std::memcmp(&actual.stencilFront, &expected.stencilFront, sizeof(VkStencilOpState)) == 0 && std::memcmp(&actual.stencilBack, &expected.stencilBack, sizeof(VkStencilOpState)) == 0, "null substitution changed stencil operations");
            Require(actual.viewport.minDepth == expected.viewport.minDepth && actual.viewport.maxDepth == expected.viewport.maxDepth && actual.renderExtent.width == expected.renderExtent.width && actual.renderExtent.height == expected.renderExtent.height, "null substitution changed depth coverage");
            Require(AgcDriver::Graphics::DrawRejection(retained, true).empty(), "a retained-format depth/stencil pass was rejected");
        }
    }
    {
        queue.context[0x011] &= ~(1u << 29u);
        const auto stencilTiled = AgcDriver::Graphics::DecodeState(queue);
        Require(stencilTiled.depth && stencilTiled.depth->htileStencil, "HTILE holds the stencil state without TILE_STENCIL_DISABLE");
        queue.context[0x011] |= 1u << 29u;
        queue.context[0x010] &= ~(1u << 29u);
        const auto untiled = AgcDriver::Graphics::DecodeState(queue);
        Require(untiled.depth && untiled.depth->htileAddress == 0, "HTILE must be ignored without TILE_SURFACE_ENABLE");
        queue.context[0x010] |= 1u << 29u;
        using AgcDriver::Graphics::HtileFillClears;
        using AgcDriver::Graphics::HtileFillCovers;
        constexpr VkImageAspectFlags both = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        Require(HtileFillClears(0x0003fff0u, false) == VK_IMAGE_ASPECT_DEPTH_BIT, "a depth-only HTILE fill with ZMask 0 clears the depth");
        Require(HtileFillClears(0x000000f0u, true) == both, "a depth and stencil fast clear (ZMask 0, SMem 0) clears both aspects");
        Require(HtileFillClears(0x000003f0u, true) == VK_IMAGE_ASPECT_DEPTH_BIT, "SMem 3 leaves the stencil uncleared");
        Require(HtileFillClears(0xfffff0ffu, true) == VK_IMAGE_ASPECT_STENCIL_BIT, "ZMask 0xf with SMem 0 clears only the stencil");
        Require(HtileFillClears(0xffffffffu, true) == 0 && HtileFillClears(0xffffffffu, false) == 0, "an expanded fill clears nothing");
        const VkExtent2D extent{16, 8};
        Require(HtileFillCovers(0x10000, extent, 0x10000, 0x8000) && HtileFillCovers(0x10000, extent, 0xfff0, 0x8010), "a fill over the whole 32 KiB metadata block covers the surface");
        Require(!HtileFillCovers(0x10000, extent, 0x10000, 0x7ffc) && !HtileFillCovers(0x10000, extent, 0x10004, 0x8000) && !HtileFillCovers(0, extent, 0, 0x10000), "a partial fill, one starting past the HTILE base, or no HTILE covers nothing");
        // Pipe-aligned SW_64KB_Z_X places a 16x16 surface's HTILE words across its metadata block (a valid word at offset
        // 260, AMD PAL), so a fill of the words a linear layout would hold covers nothing.
        Require(!HtileFillCovers(0x10000, {16, 16}, 0x10000, 16) && !HtileFillCovers(0x10000, {16, 16}, 0x10000, 264), "a fill of the linear HTILE size of a 16x16 surface misses its pipe-aligned words");
        Require(AgcDriver::Graphics::HtileMetadataBytes({1024, 512}) == 0x8000u && AgcDriver::Graphics::HtileMetadataBytes({1025, 512}) == 0x10000u && AgcDriver::Graphics::HtileMetadataBytes({3840, 2160}) == 20u * 0x8000u, "the HTILE metadata is 32 KiB per 1024x512 block");
    }
    Require(state.renderExtent.width == 4 && state.renderExtent.height == 2, "render extent ignores the depth surface");
    Require(!state.depthTest && !state.depthWrite && state.stencilTest, "depth/stencil enables changed");
    queue.context.erase(0x005);
    Require(AgcDriver::Graphics::DecodeState(queue).depth->htileAddress == 0, "an HTILE surface without DB_HTILE_DATA_BASE decoded an HTILE address");
    queue.context[0x005] = 0x300;
    queue.context[0x01e] = 0x01;
    log.clear();
    {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&log);
        state = AgcDriver::Graphics::DecodeState(queue);
    }
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks an HTILE register the decoder reads: " + std::to_string(read.offset));
    Require(state.depth->htileAddress == (0x30000ull | (1ull << 40u)), "the HTILE address was not decoded from DB_HTILE_DATA_BASE and its high byte");
    {
        const auto view = queue.context[0x002];
        queue.context[0x002] = 2u | (2u << 13u);
        const auto slice = AgcDriver::Graphics::DecodeState(queue);
        Require(slice.depth->htileAddress == (0x30000ull | (1ull << 40u)) + 2u * AgcDriver::Graphics::HtileMetadataBytes(slice.depth->extent), "slice 2 of a depth array must use the third slice of its HTILE");
        queue.context[0x002] = view;
    }
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
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.passOp == VK_STENCIL_OP_INVERT && state.stencilFront.failOp == VK_STENCIL_OP_KEEP && state.stencilFront.depthFailOp == VK_STENCIL_OP_KEEP && state.stencilFront.compareOp == VK_COMPARE_OP_EQUAL, "an equal test replaced by another value is not drawn as an inversion");
    Require(state.stencilFront.reference == 2 && state.stencilFront.compareMask == 0xff && state.stencilFront.writeMask == 7, "an inverted replacement does not keep the test value and write only the bits that differ from it");
    queue.context[0x10b] = 0x440;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.passOp == VK_STENCIL_OP_INVERT && state.stencilFront.depthFailOp == VK_STENCIL_OP_INVERT && state.stencilFront.writeMask == 7, "a replacement on pass and depth fail is not drawn as one inversion");
    queue.context[0x10b] = 0x4;
    queue.context[0x200] = 0x00200511;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.failOp == VK_STENCIL_OP_INVERT && state.stencilFront.passOp == VK_STENCIL_OP_KEEP && state.stencilFront.writeMask == 7, "a not-equal test that fails is not replaced as an inversion");
    queue.context[0x10c] = 0x40ffffff;
    queue.context[0x10b] = 0x40;
    queue.context[0x200] = 0x00200311;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.passOp == VK_STENCIL_OP_INVERT && state.stencilFront.writeMask == 0xbf && state.stencilFront.reference == 0xff, "a less-or-equal test against 0xff, which passes only on 0xff, is not replaced as an inversion");
    queue.context[0x10b] = 0x4;
    queue.context[0x200] = 0x00200411;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.failOp == VK_STENCIL_OP_INVERT && state.stencilFront.writeMask == 0xbf, "a greater test against 0xff, which fails only on 0xff, is not replaced as an inversion");
    queue.context[0x10c] = 0x40ffff7f;
    queue.context[0x10b] = 0x40;
    queue.context[0x200] = 0x00200311;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil replacement");
    queue.context[0x10c] = 0x05ffff02;
    queue.context[0x200] = 0x00200511;
    queue.context[0x10b] = 0x40;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil replacement");
    queue.context[0x200] = 0x00200111;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil replacement");
    queue.context[0x200] = 0x00200211;
    queue.context[0x10c] = 0x05ff0f02;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "stencil replacement");
    queue.context[0x10c] = 0x05ffff02;
    queue.context[0x10b] = 0x41;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "beside another stencil operation");
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
    for (const auto select : {0xc0u, 0xf00u}) {
        queue.context[0x000] = 0x23u | select;
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.depthWrite && state.depthCompare == VK_COMPARE_OP_ALWAYS && clears(state.stencilFront), "a copy sample selection without a copy changed a clear draw");
    }
    for (const auto control : {4u, 8u, 0x84u, 0x108u}) {
        queue.context[0x000] = 0x22u | control;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "DB_RENDER_CONTROL");
    }
    queue.context[0x000] = 0x23;
    queue.context[0x011] = 0;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthWrite && state.depthCompare == VK_COMPARE_OP_ALWAYS && !state.stencilTest && state.colors.size() == 2, "a depth and stencil clear without a stencil plane does not clear depth alone");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "precheck rejected a stencil clear without a stencil plane");
    queue.context[0x000] = 0x22;
    queue.context[0x010] = 0;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(!state.depth && !state.depthTest && !state.depthWrite && !state.stencilTest && state.colors.size() == 2, "clear flags without depth/stencil planes changed color-only rendering");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "precheck rejected clear flags without depth/stencil planes");
    queue = makeState();
    queue.context[0x010] = 0x80000180;
    queue.context[0x011] = 0x20000180;
    queue.context[0x200] = 0x776;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto colorOnly = AgcDriver::Graphics::DecodeState(queue);
    for (const auto control : {1u, 2u, 3u}) {
        queue.context[0x000] = control;
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(!state.depth && !state.depthTest && !state.depthWrite && !state.stencilTest, "stale clear flags enabled absent depth/stencil planes");
        Require(state.color.address == colorOnly.color.address && state.blends[0].colorWriteMask == colorOnly.blends[0].colorWriteMask && state.viewport.minDepth == colorOnly.viewport.minDepth && state.viewport.maxDepth == colorOnly.viewport.maxDepth, "stale clear flags changed color writes or viewport depth");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "precheck rejected a color-only draw with stale clear flags: " + AgcDriver::Graphics::DrawRejection(queue, false));
    }
    queue = makeState();
    queue.context[0x31b] = 1u << 26u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "mip exceeds");
    queue.context[0x31b] = 0;
    const auto plain = AgcDriver::Graphics::DecodeState(queue);
    queue.context[0x31b] = 35u << 13u;
    const auto plainRange = AgcDriver::Graphics::DecodeState(queue);
    Require(plainRange.color.address == plain.color.address && plainRange.color.bytes == plain.color.bytes && plainRange.color.arrayLastSlice == plain.color.arrayLastSlice, "a color view of slices 0 to 35 of a target with no array slices did not render into its only slice");
    queue.context[0x31b] = 1u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "ends before its first slice");
    queue.context[0x31b] = 1u << 13u;
    const auto firstSlice = AgcDriver::Graphics::DecodeState(queue);
    Require(firstSlice.color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()), "a color view of several slices did not retain its first slice");
    queue.context[0x207] = 1u << 18u;
    const auto layeredSingle = AgcDriver::Graphics::DecodeState(queue);
    Require(layeredSingle.color.address == firstSlice.color.address && layeredSingle.color.arrayAddress == firstSlice.color.address && layeredSingle.color.arrayLastSlice == 1u && layeredSingle.color.layers == 2u && layeredSingle.renderLayers == 2u, "a layered draw into slices 0 to 1 of a surface of one slice did not render them as a two-slice array");
    queue.context[0x207] = 0;
    const auto sliced = reinterpret_cast<std::uintptr_t>(sliceMemory.data());
    queue.context[0x318] = static_cast<std::uint32_t>(sliced >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>(sliced >> 40u);
    queue.context[0x31b] = 1u | (1u << 13u);
    const auto slice = AgcDriver::Graphics::DecodeState(queue);
    Require(slice.color.address == sliced + 1024u && slice.color.bytes == 1024u, "a color view of one slice did not move the target by one slice");
    queue.context[0x3b8] = 0x09000001;
    const auto arraySlice = AgcDriver::Graphics::DecodeState(queue);
    Require(arraySlice.color.address == sliced + 1024u && arraySlice.color.bytes == 1024u && arraySlice.color.depth == 1u, "a color view of one slice of a 2D array target did not move the target by one slice");
    queue.context[0x31b] = 1u << 13u;
    const auto range = AgcDriver::Graphics::DecodeState(queue);
    Require(range.color.address == sliced && range.color.bytes == 1024u, "a color view of slices 0 to 1 of a 2D array target did not render into its first slice");
    queue.context[0x207] = 1u << 18u;
    const auto layeredArray = AgcDriver::Graphics::DecodeState(queue);
    Require(layeredArray.color.address == sliced && layeredArray.color.arraySlice == 0u && layeredArray.color.layers == 2u && layeredArray.renderLayers == 2u && layeredArray.layerExports != 0u, "a layeredArray draw into slices 0 to 1 of a 2D array target did not render two layers");
    queue.context[0x207] = 0;
    queue.context[0x31b] = 2u | (2u << 13u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "beyond the array surface");
    queue.context[0x31b] = 1u << 14u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "beyond the array surface");
    queue.context[0x3b8] = 0x0a000003;
    queue.context[0x31b] = 2u | (2u << 13u);
    const auto volume = AgcDriver::Graphics::DecodeState(queue);
    Require(volume.color.address == sliced && volume.color.depth == 4u && volume.color.depthSlice == 2u, "a color view of one 3D depth slice did not keep the surface address and select the slice");
    Require(volume.color.layers == 1 && volume.renderLayers == 1 && volume.layerExports == 0, "a color view of one 3D depth slice became layered");
    {
        auto mipmapped = queue;
        mipmapped.context[0x3b0] |= 2u << 28u;
        mipmapped.context[0x31b] = (1u << 26u) | (1u << 13u) | 1u;
        expectFailure([&] { AgcDriver::Graphics::DecodeColorBuffer(mipmapped.context, 0); }, "slot 0, 64x4x4, viewMip 1, maxMip 2, slices 1..1, CB_COLOR_INFO 0x00028028, ATTRIB2 0x200fc003, ATTRIB3 0x0a000003, VIEW 0x04002001");
    }
    queue.context[0x31b] = 4u | (4u << 13u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "outside the 3D surface");
    queue.context[0x31b] = 2u | (1u << 13u);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "ends before its first slice");
    queue.context[0x31b] = 1u | (3u << 13u);
    queue.context[0x207] = (1u << 18u) | (1u << 21u) | (1u << 24u);
    const auto layered = AgcDriver::Graphics::DecodeState(queue);
    Require(layered.color.depthSlice == 1u && layered.color.layers == 3u && layered.renderLayers == 3u && layered.layerExports == (1u << 18u), "a color view of three 3D depth slices did not render three layers from the vertex layer export");
    {
        auto pointSize = queue;
        pointSize.context[0x1b3] = pointSize.context[0x1b4] = 2;
        Require(AgcDriver::Graphics::DrawRejection(pointSize, false).empty(), "layered volume test state failed the draw precheck");
        pointSize.context[0x207] |= 1u << 16u;
        const auto rejection = AgcDriver::Graphics::DrawRejection(pointSize, false);
        Require(rejection.empty(), "inactive point size rejected a layered volume draw: " + rejection);
        const auto decoded = AgcDriver::Graphics::DecodeState(pointSize);
        Require(decoded.color.depthSlice == layered.color.depthSlice && decoded.color.layers == layered.color.layers && decoded.renderLayers == layered.renderLayers && decoded.layerExports == layered.layerExports, "inactive point size changed layered volume routing");
        Require(decoded.paClVsOutCntl == pointSize.context[0x207], "inactive point size discarded the layered export state");
    }
    queue.context[0x31b] = 1u | (8u << 13u);
    Require(AgcDriver::Graphics::DecodeState(queue).renderLayers == 3u, "a color view past the 3D surface was not clamped to its last slice");
    queue.context[0x207] = 0;
    queue.context[0x31b] = 0;
    queue.context[0x31c] |= 0x10000000;
    const auto volumeKeys = reinterpret_cast<std::uintptr_t>(volumeKeyMemory.data());
    queue.context[0x325] = static_cast<std::uint32_t>(volumeKeys >> 8u);
    queue.context[0x3a8] = static_cast<std::uint32_t>(volumeKeys >> 40u);
    Require(AgcDriver::Graphics::DecodeState(queue).color.dccAddress == volumeKeys, "a DCC 3D color target lost its keys");
}

void OneDimensionalColorTests() {
    auto queue = makeState();
    queue.context[0x3b8] = 0x08000000;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "taller than one row");
    queue.context[0x3b0] = 63u << 14u;
    const auto line = AgcDriver::Graphics::DecodeState(queue);
    Require(line.color.extent.width == 64u && line.color.extent.height == 1u && line.color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()) && line.color.depth == 1u, "a 1D color target did not decode as one row");
}

void multisampledSingleClearTests() {
    static constexpr std::array<std::uint32_t, 22> kernel{0xbe800303u, 0xbe830382u, 0xf40c0401u, 0xfa000000u, 0xd7460000u, 0x04010609u, 0xd7460001u, 0x0405060au, 0x7e060208u, 0x7e0c020bu, 0x7e0e0280u, 0xd5690004u, 0x02000100u, 0x7e000205u, 0xd5690005u, 0x02000901u, 0xd7000001u, 0x02000c80u, 0xbf8cc07fu, 0xf0201f38u, 0x00040004u, 0xbf810000u};
    const std::array<std::uint32_t, 9> userData{0u, 0u, 0x0f7f0008u, 8u, 8u, 0xff00ffccu, 0u, 0u, 0u};
    const std::array<std::uint32_t, 3> group{8, 8, 1};
    const auto match = [&](std::span<const std::uint32_t> code, std::span<const std::uint32_t> words, const std::array<std::uint32_t, 3>& threads) {
        return AgcDriver::Graphics::MatchMultisampledSingleClear(code, words, threads);
    };
    const auto clear = match(kernel, userData, group);
    Require(clear.has_value() && clear->descriptor == 0x20f7f0008ull && clear->color == std::array<std::uint32_t, 4>{0xff00ffccu, 0u, 0u, 0u}, "RADV's multisampled comp-to-single clear was not recognized");
    Require(!match(kernel, userData, {64, 1, 1}), "the comp-to-single clear matched with another group size");
    Require(!match(kernel, std::span(userData).first(8), group), "the comp-to-single clear matched without its clear color");
    auto unsized = userData;
    unsized[3] = 0;
    Require(!match(kernel, unsized, group), "a comp-to-single clear with no block size was recognized");
    auto other = kernel;
    other[17] ^= 1u;
    Require(!match(other, userData, group), "a different kernel was taken for RADV's comp-to-single clear");
    Require(!match(std::span(kernel).first(21), userData, group), "a truncated kernel was taken for RADV's comp-to-single clear");
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

void memoryValueFillTests() {
    static constexpr std::array<std::uint32_t, 9> kernel{0xd7460000u, 0x04010c08u, 0xf4201a82u, 0xfa000000u, 0xbf8cc07fu, 0x7e02026au, 0xe0102000u, 0x80000100u, 0xbf810000u};
    const std::array<std::uint32_t, 3> wave{64, 1, 1};
    const std::array<std::uint32_t, 8> userData{0x50b00000u, 0x00040020u, 0x20000u, 0x00014204u, 0x34be64e0u, 0x00100020u, 1u, 0x0004dfacu};
    const auto match = [&](std::span<const std::uint32_t> code, std::array<std::uint32_t, 8> words, std::array<std::uint32_t, 3> threads, std::array<std::uint32_t, 3> groups) {
        return AgcDriver::Graphics::MatchMemoryValueFill(code, words, threads, groups);
    };
    const auto fill = match(kernel, userData, wave, {2048, 1, 1});
    Require(fill.has_value() && fill->address == 0x2050b00000ull && fill->bytes == 0x80000u && fill->valueAddress == 0x2034be64e0ull, "Kena's HTILE fill from a value in memory was not recognized");
    const auto partial = match(kernel, userData, wave, {1024, 1, 1});
    Require(partial.has_value() && partial->bytes == 0x40000u, "a fill of half the groups did not cover half the buffer");
    auto records = userData;
    records[2] = 0x100u;
    const auto clipped = match(kernel, records, wave, {2048, 1, 1});
    Require(clipped.has_value() && clipped->bytes == 0x400u, "a fill past NUM_RECORDS was not clipped to it");
    Require(!match(kernel, userData, {32, 1, 1}, {2048, 1, 1}), "the memory value fill matched with another group size");
    Require(!match(kernel, userData, wave, {2048, 2, 1}), "a two-dimensional dispatch was taken for a linear fill");
    auto strided = userData;
    strided[1] = 0x00080020u;
    Require(!match(kernel, strided, wave, {2048, 1, 1}), "a fill with an 8-byte stride was taken for a dword fill");
    auto indexed = userData;
    indexed[3] |= 1u << 23u;
    Require(!match(kernel, indexed, wave, {2048, 1, 1}), "a buffer with ADD_TID_ENABLE was taken for a plain fill");
    auto nowhere = userData;
    nowhere[4] = 0;
    nowhere[5] = 0;
    Require(!match(kernel, nowhere, wave, {2048, 1, 1}), "a fill from a null value pointer was recognized");
    auto empty = userData;
    empty[6] = 0;
    Require(!match(kernel, empty, wave, {2048, 1, 1}), "a fill whose value buffer has no records was taken for a fill from memory");
    auto raw = userData;
    raw[5] = 0x20u;
    raw[6] = 4u;
    Require(match(kernel, raw, wave, {2048, 1, 1}).has_value(), "a fill from a 4-byte raw value buffer was not recognized");
    raw[6] = 3u;
    Require(!match(kernel, raw, wave, {2048, 1, 1}), "a fill from a value buffer shorter than a dword was taken for a fill from memory");
    for (const std::uint32_t format : {20u, 21u, 22u}) {
        auto typed = userData;
        typed[3] = (typed[3] & ~(0x7fu << 12u)) | (format << 12u);
        Require(match(kernel, typed, wave, {2048, 1, 1}).has_value(), "a fill through a 32-bit buffer format was not recognized");
    }
    for (const std::uint32_t format : {0x27u, 0x0du, 0x4bu}) {
        auto converted = userData;
        converted[3] = (converted[3] & ~(0x7fu << 12u)) | (format << 12u);
        Require(!match(kernel, converted, wave, {2048, 1, 1}), "a fill through a converting buffer format was taken for a raw dword fill");
    }
    auto other = kernel;
    other[6] ^= 1u;
    Require(!match(other, userData, wave, {2048, 1, 1}), "a different kernel was taken for the memory value fill");
    Require(!match(std::span(kernel).first(8), userData, wave, {2048, 1, 1}), "a truncated kernel was taken for the memory value fill");
}

void depthMaintenanceTests() {
    for (const auto mode : {0x4u, 0x8u, 0x84u, 0x108u, 0xf88u, 0x1000u, 0x4000u}) {
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
    for (const auto control : {0u, 1u, 2u, 3u, 0x20u, 0x40u, 0x80u, 0xc0u, 0x100u, 0xf00u, 0x2000u, 0x2063u, 0x2fe3u}) {
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
    auto readOnly = makeState();
    readOnly.context[0x000] = 0x10;
    readOnly.context[0x200] = 0x007007f3;
    readOnly.context[0x010] = 0xa0000183;
    readOnly.context[0x011] = 0x20000180;
    readOnly.context[0x8e] = 0xffffffff;
    readOnly.context[0x8f] = 0;
    readOnly.shader.erase(0x8);
    readOnly.shader.erase(0x9);
    Require(AgcDriver::Graphics::DepthMaintenanceRejection(readOnly).empty(), "a resummarize draw with pass-through read-only depth and no stencil plane was rejected");
    for (const auto [offset, value] : {std::pair{0x200u, 0x007007f7u}, {0x200u, 0x007007fbu}, {0x200u, 0x007007e3u}, {0x011u, 0x20000181u}, {0x8fu, 0xfu}, {0x000u, 0x11u}, {0x010u, 0xa0000182u}}) {
        auto changed = readOnly;
        changed.context[offset] = value;
        Require(!AgcDriver::Graphics::DepthMaintenanceRejection(changed).empty(), "a resummarize draw with surface work or unsupported depth passed");
    }
    auto pixel = readOnly;
    pixel.shader[0x8] = 1;
    pixel.shader[0x9] = 0;
    pixel.context[0x1c4] = 0;
    pixel.context[0x203] = 0x40;
    Require(!AgcDriver::Graphics::DepthMaintenanceRejection(pixel).empty(), "a resummarize draw with a running pixel stage passed");
    for (const auto offset : {0x200u, 0x010u, 0x011u, 0x8eu, 0x8fu}) {
        auto missing = readOnly;
        missing.context.erase(offset);
        Require(!AgcDriver::Graphics::DepthMaintenanceRejection(missing).empty(), "a resummarize draw without a required register passed");
    }
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
    const auto infinity = std::numeric_limits<float>::infinity();
    for (const auto bounds : {std::array{0.0f, infinity}, std::array{-infinity, 1.0f}, std::array{-infinity, infinity}}) {
        queue.context[0x008] = bits(bounds[0]);
        queue.context[0x009] = bits(bounds[1]);
        state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.depthBoundsTest && state.minDepthBounds == bounds[0] && state.maxDepthBounds == bounds[1], "infinite depth bounds changed");
        AgcDriver::Graphics::Context context{};
        expectFailure([&] { AgcDriver::Graphics::ValidateDepthBounds(context, state); }, "depth bounds outside [0, 1]");
        context.depthRangeUnrestricted = true;
        AgcDriver::Graphics::ValidateDepthBounds(context, state);
    }
    for (const auto offset : {0x008u, 0x009u}) {
        queue.context[0x008] = bits(0.25f);
        queue.context[0x009] = bits(0.75f);
        queue.context[offset] = bits(std::numeric_limits<float>::quiet_NaN());
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "NaN depth bound");
    }
    queue.context[0x008] = bits(0.25f);
    queue.context[0x009] = bits(0.75f);
    queue.context[0x00b] = bits(infinity);
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "non-finite register");
    queue.context[0x00b] = bits(0.0f);
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
    queue.context[0x205] = 0x00013a46u;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.depthBias && state.depthBiasConstant == 4.0f && state.cullMode == VK_CULL_MODE_BACK_BIT, "the window offset enable bit changed the raster decode while the window offset is zero");
    queue.context[0x80] = 1;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "window offset");
    queue.context[0x80] = 0;
    queue.userConfig[0x242] = 2;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "depth bias or nonstandard rasterization");
    queue.userConfig[0x242] = 3;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "depth bias or nonstandard rasterization");
    queue.userConfig[0x242] = 4;
    queue.context[0x280] = 0x80008u;
    queue.context[0x281] = 0xffff0000u;
    queue.context[0x282] = 8u;
    const auto polygonMode = [&](std::uint32_t raster) {
        queue.context[0x205] = raster;
        return AgcDriver::Graphics::DecodeState(queue).polygonMode;
    };
    Require(polygonMode(0x8u) == VK_POLYGON_MODE_POINT, "POLY_MODE with point fronts and backs did not decode to the point polygon mode");
    Require(polygonMode(0x128u) == VK_POLYGON_MODE_LINE, "POLY_MODE with line fronts and backs did not decode to the line polygon mode");
    Require(polygonMode(0x248u) == VK_POLYGON_MODE_FILL, "POLY_MODE with filled fronts and backs did not decode to the fill polygon mode");
    Require(polygonMode(0x20u) == VK_POLYGON_MODE_FILL, "polygon types without POLY_MODE did not keep the triangles filled");
    Require(polygonMode(0x4au) == VK_POLYGON_MODE_FILL, "the point type of culled back faces changed the filled front faces");
    Require(polygonMode(0x109u) == VK_POLYGON_MODE_LINE, "culled front faces did not leave the back faces' line type");
    expectFailure([&] { polygonMode(0x48u); }, "different front and back polygon modes");
    expectFailure([&] { polygonMode(0x18u); }, "a reserved POLY_MODE");
    expectFailure([&] { polygonMode(0x68u); }, "a reserved polygon type");
    expectFailure([&] { polygonMode(0x808u); }, "depth bias with a point or line polygon mode");
    queue.context[0x280] = 0x100010u;
    expectFailure([&] { polygonMode(0x8u); }, "a point polygon mode with a point size other than one pixel");
    queue.context[0x280] = 0x80008u;
    queue.context[0x281] = 0xffff0010u;
    expectFailure([&] { polygonMode(0x8u); }, "a point polygon mode whose size limits exclude one pixel");
    queue.context[0x281] = 0xffff0000u;
    queue.context[0x282] = 0x10u;
    expectFailure([&] { polygonMode(0x128u); }, "a line polygon mode wider or narrower than one pixel");
    queue.context[0x282] = 8u;
    queue.userConfig[0x242] = 2;
    Require(polygonMode(0x8u) == VK_POLYGON_MODE_FILL, "a line list took the point polygon mode of its triangles");
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

void depthCopyPassTests() {
    using AgcDriver::Graphics::DecodeDepthCopyPass;
    auto queue = makeState();
    queue.context[0x0] = 0;
    Require(!DecodeDepthCopyPass(queue).has_value(), "a draw without DEPTH_COPY decoded as a DB depth copy");
    queue.context[0x000] = 0x84;
    queue.context[0x010] = 0x80000187;
    queue.context[0x011] = 0x20000180;
    queue.context[0x012] = 0x00001000;
    queue.context[0x007] = 0x0003003f;
    queue.context[0x31c] = 0x40710;
    queue.context[0x8e] = 1;
    auto pass = DecodeDepthCopyPass(queue);
    Require(pass.has_value() && pass->depthAddress == 0x100000 && pass->samples == 2 && pass->sample == 0 && pass->extent.width == 64 && pass->extent.height == 4, "a centroid DB depth copy of a two-sample depth plane decoded differently");
    Require(pass->color.format == VK_FORMAT_R32_SFLOAT && pass->color.samples == 1 && pass->color.address == reinterpret_cast<std::uintptr_t>(colorMemory.data()), "the DB depth copy target decoded differently");
    queue.context[0x000] = 0x104;
    Require(DecodeDepthCopyPass(queue)->sample == 1, "COPY_SAMPLE without COPY_CENTROID did not select the sample");
    queue.context[0x000] = 0x204;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "a sample the depth plane does not have");
    queue.context[0x000] = 0x8c;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "stencil copy");
    queue.context[0x000] = 0x84;
    queue.context[0x30e] = 0xfffffffe;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "sample mask");
    queue.context[0x30e] = 0xffffffff;
    queue.context[0x200] = 2;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "depth or stencil tests");
    queue.context[0x200] = 0;
    queue.context[0x010] = 0x80000185;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "Z_32_FLOAT");
    queue.context[0x010] = 0x80000187;
    queue.context[0x8e] = 0xf;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "red channel");
    queue.context[0x8e] = 1;
    queue.context[0x31c] = 0x28028;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "single-sample R32 float");
    queue.context[0x31c] = 0x40710;
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(16.0f);
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "over part of the depth surface");
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x010] = 0x80001187;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "mipmapped or partially resident");
    queue.context[0x010] = 0x80000187;
    queue.context[0x012] = 0x00001003;
    pass = DecodeDepthCopyPass(queue);
    Require(pass.has_value() && pass->depthAddress == 0x100000, "a DB depth copy kept the pipe-bank XOR of a two-sample depth plane in its address");
    queue.context[0x012] = 0x00001000;
    queue.context[0x002] = 1u | (1u << 13u);
    pass = DecodeDepthCopyPass(queue);
    Require(pass.has_value() && pass->depthAddress == 0x100000 + AgcDriver::Graphics::DepthSliceBytes({64, 4}, 4u), "a DB depth copy of slice 1 did not address the slice the depth surface registers");
    queue.context[0x002] = 0;
    Require(DecodeDepthCopyPass(queue).has_value(), "the restored DB depth copy was refused");
    queue.context[0x010] = 0x80000183;
    pass = DecodeDepthCopyPass(queue);
    Require(pass.has_value() && pass->samples == 1 && pass->sample == 0, "a centroid DB depth copy of a one-sample depth plane decoded differently");
    queue.context[0x000] = 0x104;
    expectFailure([&] { (void)DecodeDepthCopyPass(queue); }, "a sample the depth plane does not have");
}

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
    queue.context[0x10] = 0x80000180;
    queue.context[0x11] = 0x20000180;
    queue.context[0x200] = 0x700763;
    queue.context[0x0] = 0x20;
    Require(DecodeColorMetadataPass(queue).has_value(), "depth and stencil state without depth or stencil planes refused the pass");
    queue.context[0x10] = 0x80000183;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "depth or stencil work");
    queue.context[0x10] = 0x80000180;
    queue.context[0x11] = 0x20000181;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "depth or stencil work");
    queue.context[0x11] = 0x20000180;
    queue.context.erase(0x200);
    queue.context.erase(0x0);
    Require(DecodeColorMetadataPass(queue).has_value(), "unwritten depth and render control refused a pass without depth or stencil planes");
    queue.context.erase(0x10);
    queue.context.erase(0x11);
    queue.context[0x0] = 0;
    queue.context[0x200] = 0x70;
    Require(DecodeColorMetadataPass(queue).has_value(), "an always-pass depth function without a test refused the pass");
    auto bound = queue;
    bound.context[0x010] = 0x3;
    bound.context[0x011] = 0x1;
    bound.context[0x200] = 0x72;
    Require(DecodeColorMetadataPass(bound).has_value(), "an always-pass depth test without writes refused the pass");
    bound.context[0x200] = 0x76;
    expectFailure([&] { DecodeColorMetadataPass(bound); }, "depth or stencil work");
    bound.context[0x200] = 0x7a;
    expectFailure([&] { DecodeColorMetadataPass(bound); }, "depth or stencil work");
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
        std::vector<std::uint32_t> original(tiledBytes / 4);
        for (std::size_t index = 0; index < original.size(); ++index) original[index] = 0x01000000u + static_cast<std::uint32_t>(index);
        const auto resetSingle = [&] {
            std::memcpy(tiledTexels, original.data(), tiledBytes);
            std::memset(tiledKeys, 0x10, keyCount);
        };
        resetSingle();
        expectFailure([&] { AgcDriver::Graphics::RunColorMetadataPass(context, *tiledPass); }, "fast-clear eliminate over comp-to-single");
        Require(std::memcmp(tiledTexels, original.data(), tiledBytes) == 0 && std::all_of(tiledKeys, tiledKeys + keyCount, [](std::uint8_t value) { return value == 0x10; }), "a refused fast-clear eliminate over comp-to-single keys changed the " + alignment + " target");
        resetSingle();
        AgcDriver::Graphics::RunColorMetadataPass(context, {AgcDriver::Graphics::ColorMetadataPass::Mode::DccDecompress, tiledPass->targets});
        const AgcDriver::Graphics::ColorTargetLayout layout(tiledSide, tiledSide, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
        bool expanded = true;
        for (std::uint32_t y = 0; y < tiledSide && expanded; ++y) {
            for (std::uint32_t x = 0; x < tiledSide && expanded; ++x) {
                std::uint32_t texel = 0;
                std::memcpy(&texel, tiledTexels + layout.Offset(x, y), 4);
                expanded = texel == original[layout.Offset(x & ~7u, y & ~7u) / 4];
            }
        }
        const auto stored = static_cast<std::size_t>(std::count(tiledKeys, tiledKeys + keyCount, std::uint8_t{0xff}));
        Require(expanded && stored == expected, "a DCC decompress of comp-to-single keys over a 128x128 SW_64KB_R_X target with " + alignment + " DCC did not give every 8x8 block its first texel (" + std::to_string(stored) + " of " + std::to_string(keyCount) + " keys uncompressed, expected " + std::to_string(expected) + ")");
    }
}

alignas(4096) std::array<std::uint8_t, 4096> cmaskMemory{};

void cmaskTests() {
    using AgcDriver::Graphics::DecodeColorBuffer;
    using AgcDriver::Graphics::CmaskLayout;
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
    {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&log);
        static_cast<void>(DecodeColorBuffer(queue.context, 0));
    }
    Require(!log.empty(), "the register facade recorded nothing for a fast-clear target");
    for (const auto read : log) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a register the fast-clear target decode reads: " + std::to_string(read.offset));
    Require(DecodeColorBuffer(makeState().context, 0).cmaskAddress == 0, "a target without FAST_CLEAR got a CMASK");
    using AgcDriver::Graphics::CmaskBytes;
    Require(CmaskBytes(1920, 1080) == 0x6000 && CmaskBytes(960, 544) == 0x2000 && CmaskBytes(800, 450) == 0x1000 && CmaskBytes(1024, 512) == 0x1000 && CmaskBytes(3840, 2160) == 0x14000, "a CMASK does not span whole 1024x512 metablocks");
    constexpr std::size_t blockBytes = 65536u;
    static std::vector<std::byte> mipStorage(16u * blockBytes);
    const auto mipBase = (reinterpret_cast<std::uintptr_t>(mipStorage.data()) + blockBytes - 1u) / blockBytes * blockBytes;
    auto tailMipmapped = queue;
    tailMipmapped.context[0x3b8] = 0x0dc6c000u;
    tailMipmapped.context[0x318] = static_cast<std::uint32_t>(mipBase >> 8u);
    tailMipmapped.context[0x390] = static_cast<std::uint32_t>(mipBase >> 40u);
    tailMipmapped.context[0x3b0] |= 1u << 28u;
    const auto tailBaseMipTarget = DecodeColorBuffer(tailMipmapped.context, 0);
    Require(tailBaseMipTarget.cmaskFastClear && tailBaseMipTarget.cmaskAddress == cmaskAddress && tailBaseMipTarget.cmaskBytes == cmaskMemory.size(), "tail-bound mip 0 of a mipmapped fast-clear target did not decode at CMASK base");
    auto arrayTarget = queue;
    arrayTarget.context[0x3b8] |= 1u;
    expectFailure([&] { DecodeColorBuffer(arrayTarget.context, 0); }, "3D or array");
    auto volumeTarget = queue;
    volumeTarget.context[0x3b8] = (queue.context.at(0x3b8) & ~0x03000000u) | 0x02000000u;
    expectFailure([&] { DecodeColorBuffer(volumeTarget.context, 0); }, "3D or array");
    static std::vector<std::byte> cmaskChainStorage(16u * CmaskLayout::Alignment);
    const auto cmaskChainBase = (reinterpret_cast<std::uintptr_t>(cmaskChainStorage.data()) + CmaskLayout::Alignment - 1u) / CmaskLayout::Alignment * CmaskLayout::Alignment;
    auto mipmapped = queue;
    mipmapped.context[0x3b8] = 0x0dc6c000u;
    mipmapped.context[0x318] = static_cast<std::uint32_t>(mipBase >> 8u);
    mipmapped.context[0x390] = static_cast<std::uint32_t>(mipBase >> 40u);
    mipmapped.context[0x31f] = static_cast<std::uint32_t>(cmaskChainBase >> 8u);
    mipmapped.context[0x398] = static_cast<std::uint32_t>(cmaskChainBase >> 40u);
    mipmapped.context[0x3b0] = (4u << 28u) | (255u << 14u) | 127u;
    const auto baseMipTarget = DecodeColorBuffer(mipmapped.context, 0);
    Require(baseMipTarget.cmaskFastClear && baseMipTarget.cmaskAddress == cmaskChainBase + 0x2000 && baseMipTarget.cmaskBytes == 4096, "base mip 0 of a mipmapped fast-clear target did not match addrlib CMASK offset");
    static std::vector<std::byte> largeMipStorage(50331648u + blockBytes);
    const auto largeMipBase = (reinterpret_cast<std::uintptr_t>(largeMipStorage.data()) + blockBytes - 1u) / blockBytes * blockBytes;
    static std::vector<std::byte> largeCmaskChainStorage(26u * CmaskLayout::Alignment);
    const auto largeCmaskChainBase = (reinterpret_cast<std::uintptr_t>(largeCmaskChainStorage.data()) + CmaskLayout::Alignment - 1u) / CmaskLayout::Alignment * CmaskLayout::Alignment;
    auto largeMipmapped = queue;
    largeMipmapped.context[0x3b8] = 0x0dc6c000u;
    largeMipmapped.context[0x318] = static_cast<std::uint32_t>(largeMipBase >> 8u);
    largeMipmapped.context[0x390] = static_cast<std::uint32_t>(largeMipBase >> 40u);
    largeMipmapped.context[0x31f] = static_cast<std::uint32_t>(largeCmaskChainBase >> 8u);
    largeMipmapped.context[0x398] = static_cast<std::uint32_t>(largeCmaskChainBase >> 40u);
    largeMipmapped.context[0x3b0] = (7u << 28u) | (4095u << 14u) | 2047u;
    const auto largeBaseMipTarget = DecodeColorBuffer(largeMipmapped.context, 0);
    Require(largeBaseMipTarget.cmaskFastClear && largeBaseMipTarget.cmaskAddress == largeCmaskChainBase + 0x9000 && largeBaseMipTarget.cmaskBytes == 16u * CmaskLayout::Alignment, "multiblock base mip 0 did not match addrlib CMASK offset");
    auto nonPowerOfTwo = queue;
    nonPowerOfTwo.context[0x3b8] = 0x0dc6c000u;
    nonPowerOfTwo.context[0x318] = static_cast<std::uint32_t>(largeMipBase >> 8u);
    nonPowerOfTwo.context[0x390] = static_cast<std::uint32_t>(largeMipBase >> 40u);
    nonPowerOfTwo.context[0x31f] = static_cast<std::uint32_t>(largeCmaskChainBase >> 8u);
    nonPowerOfTwo.context[0x398] = static_cast<std::uint32_t>(largeCmaskChainBase >> 40u);
    nonPowerOfTwo.context[0x3b0] = (1u << 28u) | (2048u << 14u) | 1023u;
    const auto nonPowerOfTwoTarget = DecodeColorBuffer(nonPowerOfTwo.context, 0);
    Require(nonPowerOfTwoTarget.cmaskFastClear && nonPowerOfTwoTarget.cmaskAddress == largeCmaskChainBase + 0x2000 && nonPowerOfTwoTarget.cmaskBytes == 6u * CmaskLayout::Alignment, "non-power-of-two base mip 0 did not round up mip dimensions");
    auto smallBppMipmapped = mipmapped;
    smallBppMipmapped.context[0x31c] = (mipmapped.context[0x31c] & ~0x7cu) | 4u;
    expectFailure([&] { DecodeColorBuffer(smallBppMipmapped.context, 0); }, "8 or 16 bpp");
    auto higherMip = mipmapped;
    higherMip.context[0x31b] = 1u << 26u;
    const auto higherTarget = DecodeColorBuffer(higherMip.context, 0);
    Require(!higherTarget.cmaskFastClear && higherTarget.cmaskAddress == 0, "a higher mip of a fast-clear target decoded a CMASK");
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
    AgcDriver::Graphics::RunColorMetadataPass(context, *pass);
    Require(tiles([](std::uint32_t tile) { return tile == 2; }) && cmaskIs(0xff), "a tile with a CMASK code other than 0 or 15 did not keep its texels");

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

void uint8x2TargetTests() {
    auto queue = makeState();
    queue.context[0x1c5] = 7;
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1ffcu) | (3u << 2u) | (4u << 8u);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.format == VK_FORMAT_R8G8_UINT && state.color.elementBytes == 2 && state.color.componentMapping == 0xe4u && state.color.uintExport, "an 8_8 UINT color target was rejected");
    queue.context[0x31c] = (queue.context[0x31c] & ~0x1800u) | (1u << 11u);
    const auto alt = AgcDriver::Graphics::DecodeState(queue);
    Require(alt.color.format == VK_FORMAT_R8G8_UINT && alt.color.componentMapping == 0xecu, "an 8_8 UINT SWAP_ALT color target was rejected");
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

VkSampleCountFlags depthSampleCounts = VK_SAMPLE_COUNT_4_BIT;
VkResult depthSampleResult = VK_SUCCESS;
std::uint32_t depthSampleQueries = 0;

VKAPI_ATTR VkResult VKAPI_CALL sampleLocationDepthProperties(VkPhysicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    Require(format == VK_FORMAT_D32_SFLOAT && type == VK_IMAGE_TYPE_2D && tiling == VK_IMAGE_TILING_OPTIMAL, "sample location query changed the depth image format or type");
    Require(usage == (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) && flags == VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT, "sample location query differs from depth image creation");
    ++depthSampleQueries;
    properties->sampleCounts = depthSampleCounts;
    return depthSampleResult;
}

void msaaEnableClearDepthTests() {
    using AgcDriver::Graphics::DecodeState;
    const auto drawB = [] {
        auto queue = makeState();
        queue.context[0x2f8] = 0x0020c002;
        queue.context[0x292] = 2;
        for (std::uint32_t offset = 0x2fe; offset <= 0x30d; ++offset) queue.context[offset] = (offset - 0x2feu) % 4u == 0 ? 0x622ae6aeu : 0u;
        queue.context[0x31c] = 0x8028;
        queue.context[0x31d] = 0x12000;
        queue.context[0x3b8] = 0x0dc00000u;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        queue.context[0x000] = 0;
        queue.context[0x002] = 0;
        queue.context[0x007] = (3u << 16u) | 63u;
        queue.context[0x00a] = 0;
        queue.context[0x00b] = 0;
        queue.context[0x010] = 0xa290098b;
        queue.context[0x011] = 0x20000180;
        for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
        queue.context[0x200] = 0x16;
        queue.context[0x201] = 0x00010002;
        return queue;
    };
    const auto requireCentered = [](const AgcDriver::Graphics::State& state, const std::string& what) {
        Require(state.samples == 4 && state.depth && state.depth->samples == 4 && state.customSampleLocations, what + " did not keep the 4x depth target");
        const auto locations = AgcDriver::Graphics::SampleLocations(state);
        for (std::uint32_t sample = 0; sample < state.samples; ++sample) Require(locations[sample].x == 0.5f && locations[sample].y == 0.5f, what + " did not place every sample at the pixel center");
    };
    auto queue = drawB();
    queue.context[0x200] = 0x711;
    queue.context[0x011] = 0x20000181;
    for (const auto offset : {0x013u, 0x015u}) queue.context[offset] = 0x200;
    queue.context[0x10b] = 0x333;
    queue.context[0x10c] = 0x00ff0000;
    queue.context[0x10d] = 0;
    auto state = DecodeState(queue);
    Require(state.colors.size() == 1 && state.colors[0].samples == 4, "PPSA18216's draw B lost its 4x color target");
    Require(state.stencilTest && !state.depthTest && state.depth && state.depth->format == VK_FORMAT_D32_SFLOAT_S8_UINT && state.stencilFront.compareOp == VK_COMPARE_OP_ALWAYS, "PPSA18216's draw B (DB_DEPTH_CONTROL 0x711) lost its 4x stencil plane");
    Require(state.stencilFront.passOp == VK_STENCIL_OP_REPLACE && state.stencilFront.failOp == VK_STENCIL_OP_REPLACE && state.stencilFront.depthFailOp == VK_STENCIL_OP_REPLACE && state.stencilFront.writeMask == 0xffu && state.stencilFront.reference == 0u, "PPSA18216's draw B did not decode as a stencil clear to 0");
    requireCentered(state, "a 4x depth and stencil target rasterized without MSAA_ENABLE (PPSA18216's draw B)");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck refused PPSA18216's draw B");
    queue.context[0x200] = 0x710;
    state = DecodeState(queue);
    Require(!state.depth && state.samples == 4 && state.colors.size() == 1, "draw B with DB_DEPTH_CONTROL 0x710 bound a depth target");
    queue.context[0x201] = 0x00050002u;
    Require(!DecodeState(queue).depth, "DB_EQAA Z interpolation refused a 4x draw without a depth target");
    queue = drawB();
    state = DecodeState(queue);
    Require(state.depthTest && state.colors.size() == 1, "a depth-tested 4x depth and color pair lost a target");
    requireCentered(state, "a depth-tested 4x depth and color pair rasterized without MSAA_ENABLE");
    queue.context[0x8e] = 0;
    state = DecodeState(queue);
    Require(state.colors.empty(), "a depth-only 4x draw without MSAA_ENABLE decoded a color target");
    requireCentered(state, "a 4x depth target rasterized without MSAA_ENABLE");
    for (const auto primitive : {2u, 3u}) {
        queue = drawB();
        queue.userConfig[0x242] = primitive;
        state = DecodeState(queue);
        Require(state.topology == (primitive == 2u ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) && state.colors.size() == 1 && state.colors[0].samples == 4, "PPSA18216's line list or strip into its 4x depth and color pair lost its topology or color target");
        requireCentered(state, "a line list or strip into PPSA18216's 4x depth and color pair without MSAA_ENABLE");
        Require(!state.sampleLocationGrid && !state.perSampleShading, "a line list or strip without MSAA_ENABLE kept a sample location grid or sample shading");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck refused a line list or strip into PPSA18216's 4x depth and color pair");
    }
    queue = drawB();
    queue.userConfig[0x242] = 1;
    queue.context[0x2d5] = 0x2030;
    queue.userConfig[0x25b] = 0x6633;
    queue.context[0x1ff] = 255;
    queue.context[0x2ce] = 5;
    queue.context[0x29b] = 2;
    queue.context[0x2ab] = 4;
    queue.shader[0x8a] = 3u << 29u;
    queue.shader[0x8b] = 3u << 16u;
    state = DecodeState(queue);
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && state.stages.mesh && state.stages.mesh->outputPrimitiveSize == 3u && AgcDriver::Graphics::RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Triangle && state.colors.size() == 1 && state.colors[0].samples == 4, "a point list into a geometry program that outputs triangle strips into PPSA18216's 4x depth and color pair did not decode as triangles");
    requireCentered(state, "triangles a geometry program makes from a point list into PPSA18216's 4x depth and color pair without MSAA_ENABLE");
    for (const auto primitive : {4u, 2u, 1u}) {
        queue = drawB();
        queue.userConfig[0x242] = primitive;
        queue.context[0x2d5] = 0x2030;
        queue.userConfig[0x25b] = 0x6633;
        queue.context[0x1ff] = 255;
        queue.context[0x2ce] = 5;
        queue.context[0x29b] = 0;
        queue.context[0x2ab] = 4;
        queue.context[0x280] = 0x00080008u;  // point-output geometry programs decode PA_SU_POINT_SIZE / PA_SU_POINT_MINMAX before this rejection
        queue.context[0x281] = 0x00080008u;
        queue.shader[0x8a] = 3u << 29u;
        queue.shader[0x8b] = 3u << 16u;
        expectFailure([&] { DecodeState(queue); }, "multisampled targets rasterized without MSAA_ENABLE are unsupported for points or alpha-to-coverage");
    }
    for (const auto eqaa : {0x00050002u, 0x00090002u, 0x01010002u, 0x08010002u}) {
        queue = drawB();
        queue.context[0x201] = eqaa;
        expectFailure([&] { DecodeState(queue); }, "a multisampled depth target rasterized without MSAA_ENABLE with Z interpolation or overrasterization (DB_EQAA)");
    }
    queue = drawB();
    queue.context[0x1c4] = 9;
    queue.context[0x203] |= 0x100u;
    expectFailure([&] { DecodeState(queue); }, "a multisampled depth target rasterized without MSAA_ENABLE with a sample-mask export");
    queue = drawB();
    queue.context[0x2dc] = 0x0000aa01u;
    expectFailure([&] { DecodeState(queue); }, "multisampled targets rasterized without MSAA_ENABLE are unsupported for points or alpha-to-coverage");
    queue = drawB();
    queue.context[0x010] = 0xa2900987;
    expectFailure([&] { DecodeState(queue); }, "the depth target's sample count differs from the rasterizer's");
}

void multisampleTests() {
    using AgcDriver::Graphics::ColorMetadataPass;
    using AgcDriver::Graphics::DecodeColorBuffer;
    using AgcDriver::Graphics::DecodeColorMetadataPass;
    using AgcDriver::Graphics::DecodeState;
    const auto multisampled = [] {
        auto queue = makeState();
        queue.context[0x2f8] = 0x00100001;
        queue.context[0x292] = 3;
        queue.context[0x3b8] = 0x0dc00000u;
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0xcc44;
        queue.context[0x31d] = 0x9000;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        return queue;
    };
    const auto cmaskAddress = reinterpret_cast<std::uintptr_t>(cmaskMemory.data());
    const auto setCmaskAddress = [&](auto& target, std::uint32_t slot = 0u) {
        const auto stride = slot * 0xfu;
        target.context[0x31f + stride] = static_cast<std::uint32_t>(cmaskAddress >> 8u);
        target.context[0x398 + slot] = static_cast<std::uint32_t>(cmaskAddress >> 40u);
    };
    auto queue = multisampled();
    auto state = DecodeState(queue);
    Require(state.samples == 2 && state.colors.size() == 1 && state.colors[0].samples == 2 && state.colors[0].cmaskAddress == 0, "2x multisampling did not decode");
    const auto rejection = AgcDriver::Graphics::DrawRejection(queue, false);
    Require(rejection.empty(), "the precheck refused 2x multisampling: " + rejection);
    Require(!state.perSampleShading, "2x multisampling requested sample shading without PS_ITER_SAMPLE");
    queue.context[0x293] = 0x10000u;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "PS_ITER_SAMPLE on a 2x target was refused");
    Require(DecodeState(queue).perSampleShading, "PS_ITER_SAMPLE on a 2x target did not request sample shading");
    queue.context[0x1b3] = 0x4002u;
    queue.context[0x1b4] = 0x4002u;
    expectFailure([&] { DecodeState(queue); }, "SAMPLE_COVERAGE in a multisampled pixel shader that runs per sample");
    queue.context[0x293] = 0;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty() && !DecodeState(queue).perSampleShading, "SAMPLE_COVERAGE with center interpolation on a 2x target was refused");
    for (const auto sampleRate : {0x1u, 0x10u, 0x2000u}) {
        queue.context[0x1b3] = 0x4002u | sampleRate;
        queue.context[0x1b4] = 0x4002u | sampleRate;
        expectFailure([&] { DecodeState(queue); }, "SAMPLE_COVERAGE in a multisampled pixel shader that runs per sample");
        queue.context[0x1b4] = 0x4002u;
        Require(DecodeState(queue).samples == 2, "a sample-rate input enabled but not addressed beside SAMPLE_COVERAGE decoded differently");
    }
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    {
        auto smooth = multisampled();
        smooth.context[0x2f8] = 0x0020c002u;
        smooth.context[0x31d] = 0u;
        smooth.context[0x201] = 0x02130000u;
        smooth.context[0x1b3] = 0x4002u;
        smooth.context[0x1b4] = 0x4002u;
        const auto merged = DecodeState(smooth);
        Require(merged.samples == 4 && merged.mergedCoverage && merged.colors.size() == 1 && merged.colors[0].samples == 1 && !merged.perSampleShading, "a radeonsi smoothing draw (4 raster samples, OVERRASTERIZATION_AMOUNT 2, one-sample target) did not decode as merged coverage");
        smooth.context[0x201] = 0x01130000u;
        expectFailure([&] { DecodeState(smooth); }, "a color target's sample count differs from the rasterizer's");
        smooth.context[0x201] = 0x02130000u;
        smooth.context[0x293] = 0x10000u;
        expectFailure([&] { DecodeState(smooth); }, "SAMPLE_COVERAGE in a multisampled pixel shader");
        smooth.context[0x1b3] = 2u;
        smooth.context[0x1b4] = 2u;
        const auto iteratedSmooth = DecodeState(smooth);
        Require(iteratedSmooth.samples == 1 && !iteratedSmooth.mergedCoverage && !iteratedSmooth.perSampleShading, "an overrasterized draw that runs per sample into a one-sample target did not rasterize sample 0 alone");
        smooth.context[0x293] = 0u;
        smooth.context[0x292] = 2u;
        const auto unmerged = DecodeState(smooth);
        Require(unmerged.samples == 1 && !unmerged.mergedCoverage, "an overrasterized draw into a one-sample target without MSAA_ENABLE did not rasterize as one sample");
    }
    queue.context[0x302] = 0x44cc;
    state = DecodeState(queue);
    Require(state.customSampleLocations && state.sampleLocationGrid, "a standard 2x pattern with its samples swapped in one pixel did not decode as a grid");
    for (const auto [pixel, word] : {std::pair{0u, 0x0c04u}, std::pair{1u, 0x0c04u}, std::pair{2u, 0x040cu}, std::pair{3u, 0x040cu}}) queue.context[0x2fe + pixel * 4u] = word;
    state = DecodeState(queue);
    Require(state.samples == 2 && state.customSampleLocations && state.sampleLocationGrid, "the 2x checkerboard pattern (PPSA27836 and PPSA01556) did not decode as a grid");
    const auto grid = AgcDriver::Graphics::SampleLocationGrid(state);
    Require(grid.width == 2 && grid.height == 2, "a pattern varying between pixels did not use a 2x2 grid");
    const auto checkerboard = AgcDriver::Graphics::SampleLocations(state);
    const std::array<VkSampleLocationEXT, 8> expectedCheckerboard{{{0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}}};
    for (std::size_t index = 0; index < expectedCheckerboard.size(); ++index) Require(checkerboard[index].x == expectedCheckerboard[index].x && checkerboard[index].y == expectedCheckerboard[index].y, "grid sample locations are not in Vulkan's (x + y * width) * samples + sample order");
    AgcDriver::Graphics::Context gridContext{};
    gridContext.sampleLocationSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    Require(AgcDriver::Graphics::ValidateSampleLocations(gridContext, state), "a pattern varying between pixels was refused on a device without a 2x2 sample location grid");
    gridContext.sampleLocationGridSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    Require(AgcDriver::Graphics::ValidateSampleLocations(gridContext, state), "a 2x pattern varying between pixels was refused on a device with a 2x2 grid for 4 samples only");
    gridContext.sampleLocationGridSampleCounts |= VK_SAMPLE_COUNT_2_BIT;
    Require(AgcDriver::Graphics::ValidateSampleLocations(gridContext, state), "a supported 2x2 sample location grid was refused");
    queue.context[0x31d] = 0;
    state = DecodeState(queue);
    Require(state.samples == 1 && state.colors.size() == 1 && state.colors[0].samples == 1 && state.customSampleLocations && state.sampleLocationGrid, "2x checkerboard rasterization into a one-sample target (PPSA27836) did not rasterize sample 0 alone");
    const auto sampleZero = AgcDriver::Graphics::SampleLocations(state);
    const std::array<VkSampleLocationEXT, 4> expectedSampleZero{{{0.75f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f}}};
    for (std::size_t pixel = 0; pixel < expectedSampleZero.size(); ++pixel) Require(sampleZero[pixel].x == expectedSampleZero[pixel].x && sampleZero[pixel].y == expectedSampleZero[pixel].y, "a one-sample target did not take sample 0's location in each pixel of the grid");
    gridContext.sampleLocationSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    gridContext.sampleLocationGridSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleLocations(gridContext, state); }, "VK_EXT_sample_locations");
    gridContext.sampleLocationSampleCounts |= VK_SAMPLE_COUNT_1_BIT;
    Require(AgcDriver::Graphics::ValidateSampleLocations(gridContext, state), "a one-sample pattern varying between pixels was refused on a device without a one-sample 2x2 grid");
    gridContext.sampleLocationGridSampleCounts |= VK_SAMPLE_COUNT_1_BIT;
    Require(AgcDriver::Graphics::ValidateSampleLocations(gridContext, state), "one-sample grid locations were refused on a device that supports them");
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0xcc44;
    state = DecodeState(queue);
    Require(state.samples == 1 && state.customSampleLocations && !state.sampleLocationGrid, "a uniform 2x pattern into a one-sample target did not use sample 0's location");
    auto stenciled = queue;
    for (const auto [pixel, word] : {std::pair{0u, 0x0c04u}, std::pair{1u, 0x0c04u}, std::pair{2u, 0x040cu}, std::pair{3u, 0x040cu}}) stenciled.context[0x2fe + pixel * 4u] = word;
    for (const auto [offset, word] : {std::pair{0x010u, 0x80000180u}, std::pair{0x011u, 0x20000181u}, std::pair{0x012u, 0x00001000u}, std::pair{0x013u, 0x00002000u}, std::pair{0x015u, 0x00002000u}, std::pair{0x007u, 0x003f003fu}, std::pair{0x000u, 0u}, std::pair{0x002u, 0u}, std::pair{0x00au, 0u}, std::pair{0x00bu, 0x3f800000u}, std::pair{0x10bu, 0u}, std::pair{0x10cu, 0x01ffff00u}, std::pair{0x10du, 0x01ffff00u}, std::pair{0x200u, 0x007007b3u}}) stenciled.context[offset] = word;
    state = DecodeState(stenciled);
    Require(state.samples == 1 && state.depth && state.depth->samples == 1 && state.stencilTest && state.sampleLocationGrid && state.colors.size() == 1 && state.colors[0].samples == 1, "2x checkerboard rasterization into one-sample stencil and color targets (PPSA27836) did not rasterize sample 0 alone");
    stenciled.context[0x31d] = 0x9000;
    expectFailure([&] { DecodeState(stenciled); }, "a color target's sample count differs from the rasterizer's");
    stenciled.context[0x31d] = 0;
    stenciled.context[0x1b3] |= ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid);
    stenciled.context[0x1b4] |= ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid);
    expectFailure([&] { DecodeState(stenciled); }, "the depth target's sample count differs from the rasterizer's");
    const auto withInputs = [](AgcDriver::QueueState base, std::initializer_list<ShaderRecompiler::PixelInput> inputs) {
        for (const auto input : inputs) {
            base.context[0x1b3] |= ShaderRecompiler::PixelInputBit(input);
            base.context[0x1b4] |= ShaderRecompiler::PixelInputBit(input);
        }
        return base;
    };
    for (const auto input : {ShaderRecompiler::PixelInput::PerspectiveCentroid, ShaderRecompiler::PixelInput::LinearCentroid, ShaderRecompiler::PixelInput::SampleCoverage}) expectFailure([&] { DecodeState(withInputs(queue, {input})); }, "sample count differs from the rasterizer's");
    for (const auto input : {ShaderRecompiler::PixelInput::PerspectiveSample, ShaderRecompiler::PixelInput::LinearSample, ShaderRecompiler::PixelInput::Ancillary}) {
        state = DecodeState(withInputs(queue, {input}));
        Require(state.samples == 1 && !state.perSampleShading, "a sample or sample-index input into a one-sample target did not rasterize sample 0 alone");
    }
    for (const auto position : {ShaderRecompiler::PixelInput::PositionX, ShaderRecompiler::PixelInput::PositionY}) {
        state = DecodeState(withInputs(queue, {ShaderRecompiler::PixelInput::Ancillary, position}));
        Require(state.samples == 1 && !state.perSampleShading, "a sample-index and position input into a one-sample target did not rasterize sample 0 alone");
    }
    auto iterated = queue;
    iterated.context[0x293] = 0x06030000u;
    state = DecodeState(iterated);
    Require(state.samples == 1 && !state.perSampleShading, "PS_ITER_SAMPLE into a one-sample target (PPSA27836) did not rasterize sample 0 alone");
    state = DecodeState(withInputs(iterated, {ShaderRecompiler::PixelInput::PerspectiveSample, ShaderRecompiler::PixelInput::PositionX, ShaderRecompiler::PixelInput::PositionY}));
    Require(state.samples == 1 && !state.perSampleShading, "PS_ITER_SAMPLE with sample and position inputs (PPSA27836) did not rasterize sample 0 alone");
    state = DecodeState(withInputs(iterated, {ShaderRecompiler::PixelInput::PerspectiveCentroid, ShaderRecompiler::PixelInput::LinearCentroid}));
    Require(state.samples == 1, "PS_ITER_SAMPLE with centroid inputs did not rasterize sample 0 alone");
    expectFailure([&] { DecodeState(withInputs(iterated, {ShaderRecompiler::PixelInput::SampleCoverage})); }, "SAMPLE_COVERAGE in a multisampled pixel shader that runs per sample");
    state = DecodeState(withInputs(iterated, {ShaderRecompiler::PixelInput::Ancillary, ShaderRecompiler::PixelInput::PositionX}));
    Require(state.samples == 1 && !state.perSampleShading, "PS_ITER_SAMPLE with sample-index and position inputs did not rasterize sample 0 alone");
    auto iteratedStencil = withInputs(stenciled, {});
    iteratedStencil.context[0x1b3] &= ~ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid);
    iteratedStencil.context[0x1b4] &= ~ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid);
    state = DecodeState(withInputs(iteratedStencil, {ShaderRecompiler::PixelInput::PerspectiveSample, ShaderRecompiler::PixelInput::Ancillary}));
    Require(state.samples == 1 && state.depth && state.depth->samples == 1, "sample and sample-index inputs into a one-sample depth target (PPSA07411) did not rasterize sample 0 alone");
    iteratedStencil.context[0x293] = 0x06030000u;
    state = DecodeState(iteratedStencil);
    Require(state.samples == 1 && state.depth && state.depth->samples == 1 && !state.perSampleShading, "PS_ITER_SAMPLE into one-sample depth and color targets did not rasterize sample 0 alone");
    auto singleScan = queue;
    singleScan.context[0x292] = 2;
    state = DecodeState(singleScan);
    Require(state.samples == 1 && state.colors.size() == 1 && state.colors[0].samples == 1 && !state.customSampleLocations, "a one-sample target under a 2x scan without MSAA_ENABLE did not rasterize as one sample");
    queue = multisampled();
    queue.context[0x2f8] = 0;
    queue.context[0x292] = 2;
    queue.context[0x31d] = 0x12000;
    state = DecodeState(queue);
    Require(state.samples == 4 && state.colors.size() == 1 && state.colors[0].samples == 4 && state.customSampleLocations, "single-sample scan conversion into a 4x target did not rasterize the target's samples");
    const auto centered = AgcDriver::Graphics::SampleLocations(state);
    for (std::uint32_t sample = 0; sample < state.samples; ++sample) Require(centered[sample].x == 0.5f && centered[sample].y == 0.5f, "single-sample scan conversion did not place every sample at the pixel center");
    queue.context[0x292] = 3;
    expectFailure([&] { DecodeState(queue); }, "MSAA_ENABLE on a single-sample target");
    queue = multisampled();
    queue.context[0x2f8] = 0x00300003;
    queue.context[0x31d] = 0x1b000;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        queue.context[0x2fe + pixel * 4u] = 0xbd153fd1u;
        queue.context[0x2ff + pixel * 4u] = 0x9773f95bu;
    }
    state = DecodeState(queue);
    Require(state.samples == 8 && state.colors.size() == 1 && state.colors[0].samples == 8, "the standard 8x pattern (sample 7 at (7, -7), PPSA99902's registers) did not decode as standard");
    queue.context[0x2f8] = 0x00200002;
    queue.context[0x31d] = 0x12000;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x22eea66a;
    state = DecodeState(queue);
    Require(state.samples == 4 && state.customSampleLocations && !state.sampleLocationGrid, "uniform custom sample locations were not decoded as one pixel's pattern");
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    const std::array<VkSampleLocationEXT, 4> expected{{{0.125f, 0.875f}, {0.875f, 0.125f}, {0.375f, 0.375f}, {0.625f, 0.625f}}};
    for (std::size_t sample = 0; sample < expected.size(); ++sample) Require(locations[sample].x == expected[sample].x && locations[sample].y == expected[sample].y, "custom sample coordinates or indices changed");
    AgcDriver::Graphics::Context sampleContext{};
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state); }, "VK_EXT_sample_locations");
    sampleContext.sampleLocationSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state); }, "sample count");
    sampleContext.sampleLocationSampleCounts |= VK_SAMPLE_COUNT_4_BIT;
    AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state);
    sampleContext.imageFormatProperties = sampleLocationDepthProperties;
    using AgcDriver::Graphics::SampleLocationsCompatibleDepth;
    Require(SampleLocationsCompatibleDepth(sampleContext, VK_FORMAT_D32_SFLOAT, 4), "supported custom sample depth format was refused");
    state.depth = AgcDriver::Graphics::DepthTarget{};
    state.depth->format = VK_FORMAT_D32_SFLOAT;
    state.depth->samples = 4;
    Require(AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state), "supported custom depth sample locations were disabled");
    depthSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    Require(!SampleLocationsCompatibleDepth(sampleContext, VK_FORMAT_D32_SFLOAT, 4), "unsupported custom sample depth count was accepted");
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state); }, "compatible depth format and sample count");
    depthSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    depthSampleResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
    Require(!SampleLocationsCompatibleDepth(sampleContext, VK_FORMAT_D32_SFLOAT, 4), "unsupported custom sample depth format was accepted");
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state); }, "compatible depth format and sample count");
    state.customSampleLocations = false;
    Require(!AgcDriver::Graphics::ValidateSampleLocations(sampleContext, state), "standard locations did not fall back for an incompatible depth format");
    depthSampleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    expectFailure([&] { SampleLocationsCompatibleDepth(sampleContext, VK_FORMAT_D32_SFLOAT, 4); }, "Vulkan result");
    depthSampleResult = VK_SUCCESS;
    const auto queries = depthSampleQueries;
    Require(!SampleLocationsCompatibleDepth({}, VK_FORMAT_D32_SFLOAT, 4) && !SampleLocationsCompatibleDepth(sampleContext, VK_FORMAT_D32_SFLOAT, 1), "sample location depth compatibility requires the extension and multisampling");
    Require(depthSampleQueries == queries, "ineligible sample location depth formats were queried");
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x622ae6ae;
    state = DecodeState(queue);
    Require(!state.customSampleLocations, "standard sample locations require the extension");
    const auto standard = AgcDriver::Graphics::SampleLocations(state);
    const std::array<VkSampleLocationEXT, 4> expectedStandard{{{0.375f, 0.125f}, {0.875f, 0.375f}, {0.125f, 0.625f}, {0.625f, 0.875f}}};
    for (std::size_t sample = 0; sample < expectedStandard.size(); ++sample) Require(standard[sample].x == expectedStandard[sample].x && standard[sample].y == expectedStandard[sample].y, "standard sample coordinate scale or Y direction changed");
    AgcDriver::Graphics::ValidateSampleLocations({}, state);
    for (const auto samples : {2u, 8u}) {
        queue = multisampled();
        const auto log2 = samples == 2 ? 1u : 3u;
        queue.context[0x2f8] = (log2 << 20u) | log2;
        queue.context[0x31d] = (log2 << 12u) | (log2 << 15u);
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
            queue.context[0x2fe + pixel * 4u] = 0x77887788;
            queue.context[0x2ff + pixel * 4u] = 0x77887788;
        }
        state = DecodeState(queue);
        Require(state.customSampleLocations && state.samples == samples, "a custom 2x or 8x pattern did not decode");
        const auto endpoints = AgcDriver::Graphics::SampleLocations(state);
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            const auto expected = sample % 2u == 0 ? 0.0f : 15.0f / 16.0f;
            Require(endpoints[sample].x == expected && endpoints[sample].y == expected, "signed sample coordinate endpoints changed");
        }
    }
    queue.context[0x2f8] = 0x00300003;
    queue.context[0x31d] = 0x1b000;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        queue.context[0x2fe + pixel * 4u] = 0xbd153fd1u;
        queue.context[0x2ff + pixel * 4u] = 0x9773f95bu;
    }
    state = DecodeState(queue);
    Require(state.samples == 8 && !state.customSampleLocations && state.colors.size() == 1 && state.colors[0].samples == 8, "the standard 8x pattern (sample 7 at (7, -7), PPSA99902's registers) did not decode as standard");
    AgcDriver::Graphics::ValidateSampleLocations({}, state);
    for (const auto log2 : {0u, 1u, 2u, 3u}) {
        queue = log2 == 0 ? makeState() : multisampled();
        queue.context[0x1b3] = 2u;
        queue.context[0x1b4] = 2u;
        queue.context[0x2f8] = (log2 << 20u) | log2 | 0x20000000u;
        queue.context[0x31d] = (log2 << 12u) | (log2 << 15u);
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
            queue.context[0x2fe + pixel * 4u] = log2 == 3 ? 0xbd153fd1u : log2 == 2 ? 0x622ae6aeu : 0xcc44u;
            queue.context[0x2ff + pixel * 4u] = log2 == 3 ? 0x9773f95bu : 0u;
        }
        std::vector<AgcDriver::Graphics::RegisterRead> sampleReads;
        std::optional<AgcDriver::Graphics::RegisterReadLogScope> sampleLog(std::in_place, &sampleReads);
        const auto coveredCenter = DecodeState(queue);
        const auto coveredRejection = AgcDriver::Graphics::DrawRejection(queue, false);
        sampleLog.reset();
        Require(coveredCenter.samples == (1u << log2) && !coveredCenter.customSampleLocations, "covered-centroid mode changed the sample count or locations without centroid inputs");
        for (const auto read : sampleReads) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a covered-centroid register");
        Require(coveredRejection.empty(), "covered-centroid mode without centroid inputs was refused: " + coveredRejection);
        for (const auto centroid : {4u, 64u, 68u}) {
            queue.context[0x1b3] = 2u | centroid;
            queue.context[0x1b4] = 2u | centroid;
            expectFailure([&] { DecodeState(queue); }, "COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs");
            Require(AgcDriver::Graphics::DrawRejection(queue, false).find("COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs") != std::string::npos, "the precheck accepted active covered-centroid interpolation");
            queue.context[0x1b4] = 2u;
            Require(DecodeState(queue).samples == (1u << log2) && AgcDriver::Graphics::DrawRejection(queue, false).empty(), "an unaddressed centroid input changed covered-centroid mode");
            queue.context[0x1b3] = 2u;
            queue.context[0x1b4] = 2u | centroid;
            Require(DecodeState(queue).samples == (1u << log2) && AgcDriver::Graphics::DrawRejection(queue, false).empty(), "a disabled centroid input changed covered-centroid mode");
        }
        queue.context[0x1b3] = 2u;
        queue.context[0x1b4] = 2u;
        for (const auto offset : {0x1b3u, 0x1b4u}) {
            queue.context.erase(offset);
            expectFailure([&] { DecodeState(queue); }, "COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs");
            Require(AgcDriver::Graphics::DrawRejection(queue, false).find("COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs") != std::string::npos, "the precheck accepted unspecified centroid inputs");
            queue.context[offset] = 2u;
        }
        const auto withProgram = queue;
        queue.shader[0x008] = 0;
        queue.context[0x8e] = 0;
        queue.context.erase(0x1b3);
        queue.context.erase(0x1b4);
        Require(DecodeState(queue).samples == (1u << log2) && AgcDriver::Graphics::DrawRejection(queue, false).empty(), "covered-centroid mode was refused with no pixel program");
        queue = withProgram;
        queue.context[0x2f8] |= 0x10u;
        expectFailure([&] { DecodeState(queue); }, "coverage conversion or nonstandard multisampling");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("coverage conversion or nonstandard multisampling") != std::string::npos, "covered-centroid handling admitted unrelated AA bits");
    }
    queue = multisampled();
    queue.context[0x31d] = 0x1000;
    expectFailure([&] { DecodeState(queue); }, "fewer fragments than samples");
    queue.context[0x31d] = 0;
    queue.context[0x293] = 0x10000u;
    Require(DecodeState(queue).samples == 1 && !DecodeState(queue).perSampleShading, "PS_ITER_SAMPLE into a one-sample target did not rasterize sample 0 alone");
    queue = multisampled();
    queue.context[0x292] = 2;
    state = DecodeState(queue);
    Require(state.samples == 2 && state.colors.size() == 1 && state.colors[0].samples == 2 && state.customSampleLocations, "a 2x target rasterized without MSAA_ENABLE did not rasterize the target's samples");
    const auto aliased = AgcDriver::Graphics::SampleLocations(state);
    for (std::uint32_t sample = 0; sample < state.samples; ++sample) Require(aliased[sample].x == 0.5f && aliased[sample].y == 0.5f, "a 2x target rasterized without MSAA_ENABLE did not place every sample at the pixel center");
    queue.context[0x2dc] = 0x0000aa01u;
    expectFailure([&] { DecodeState(queue); }, "without MSAA_ENABLE are unsupported for points or alpha-to-coverage");
    queue = multisampled();
    queue.context[0x292] = 2;
    queue.context[0x302] = 0x44cc;
    state = DecodeState(queue);
    Require(state.customSampleLocations && !state.sampleLocationGrid, "a 2x target rasterized without MSAA_ENABLE kept the per-pixel sample location grid of its centred samples");
    queue = multisampled();
    queue.context[0x2f8] |= 0x10;
    expectFailure([&] { DecodeState(queue); }, "coverage conversion");
    queue = multisampled();
    queue.context[0x31c] |= 0x6000;
    setCmaskAddress(queue);
    Require(DecodeState(queue).colors[0].cmaskAddress == cmaskAddress, "the CMASK address of a multisampled target did not decode");
    queue = makeState();
    queue.context[0x31c] |= 0x4000;
    expectFailure([&] { DecodeState(queue); }, "color compression");
    queue = multisampled();
    queue.context[0x31c] |= 0x2000;
    setCmaskAddress(queue);
    const auto fastCleared = DecodeState(queue).colors[0];
    Require(fastCleared.cmaskAddress == cmaskAddress && fastCleared.cmaskFastClear && fastCleared.cmaskBytes == 0x1000, "a multisampled FAST_CLEAR target did not decode its CMASK at the metablock size");
    queue = multisampled();
    queue.context[0x31c] |= 0x4000;
    setCmaskAddress(queue);
    const auto compressed = DecodeState(queue).colors[0];
    Require(compressed.cmaskAddress == cmaskAddress && !compressed.cmaskFastClear && compressed.cmaskBytes == 0x1000, "a multisampled COMPRESSION target did not decode its CMASK at the metablock size");
    queue = multisampled();
    queue.context[0x31c] |= 0x2000;
    queue.context[0x31f] = 0;
    expectFailure([&] { DecodeState(queue); }, "without a CMASK address");
    queue = multisampled();
    queue.context[0x31c] |= 0x10000000u;
    queue.context[0x325] = 0;
    expectFailure([&] { DecodeState(queue); }, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
    expectFailure([&] { DecodeColorBuffer(queue.context, 0); }, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
    queue = multisampled();
    queue.context[0x31c] |= 0x2000;
    setCmaskAddress(queue);
    std::vector<AgcDriver::Graphics::RegisterRead> reads;
    {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&reads);
        static_cast<void>(DecodeState(queue));
    }
    for (const auto read : reads) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a CMASK register the decoder reads: " + std::to_string(read.offset));

    queue = multisampled();
    queue.context[0x0] = 0;
    queue.context[0x202] = 0xcc0030;
    for (const auto offset : {0x318u, 0x31bu, 0x31cu, 0x390u}) queue.context[offset + (offset >= 0x390u ? 1u : 0xfu)] = queue.context[offset];
    queue.context[0x32c] = 0;
    queue.context[0x3b1] = queue.context[0x3b0];
    queue.context[0x3b9] = queue.context[0x3b8];
    setCmaskAddress(queue);
    setCmaskAddress(queue, 1);
    auto pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->mode == ColorMetadataPass::Mode::Resolve && pass->source.has_value() && pass->source->samples == 2 && pass->targets.size() == 1 && pass->targets[0].samples == 1, "a CB resolve of a multisampled target did not decode");
    const auto rasterizedResolve = queue;
    queue.context[0x2f8] = 0;
    pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->mode == ColorMetadataPass::Mode::Resolve && pass->source.has_value() && pass->source->samples == 2 && pass->targets.size() == 1, "a CB resolve with a single-sample rasterizer did not take its sample count from the source target");
    queue = rasterizedResolve;
    queue.context[0x2f8] = 0x00200002;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x622ae6aeu;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "differs from the rasterizer's");
    queue.context[0x292] &= ~1u;
    pass = DecodeColorMetadataPass(queue);
    Require(pass.has_value() && pass->mode == ColorMetadataPass::Mode::Resolve && pass->source.has_value() && pass->source->samples == 2 && pass->targets.size() == 1, "a CB resolve with MSAA_ENABLE off did not take its sample count from the source target despite a stale PA_SC_AA_CONFIG");
    queue = rasterizedResolve;
    std::vector<std::byte> resolvedBlock(2u * 65536u);
    const auto resolvedAddress = (reinterpret_cast<std::uintptr_t>(resolvedBlock.data()) + 65535u) / 65536u * 65536u;
    for (const auto mode : {AgcDriver::Graphics::ColorTileMode::Standard4KB, AgcDriver::Graphics::ColorTileMode::Standard64KB, AgcDriver::Graphics::ColorTileMode::RenderTarget}) {
        auto tiled = queue;
        tiled.context[0x327] = static_cast<std::uint32_t>(resolvedAddress >> 8u);
        tiled.context[0x391] = static_cast<std::uint32_t>(resolvedAddress >> 40u);
        tiled.context[0x3b9] = (tiled.context[0x3b9] & ~(0x1fu << 14u)) | (static_cast<std::uint32_t>(mode) << 14u);
        const auto tiledPass = DecodeColorMetadataPass(tiled);
        Require(tiledPass.has_value() && tiledPass->mode == ColorMetadataPass::Mode::Resolve && tiledPass->targets.size() == 1 && tiledPass->targets[0].tileMode == mode && tiledPass->targets[0].samples == 1, "a CB resolve into a tile mode " + std::to_string(static_cast<std::uint32_t>(mode)) + " destination did not decode");
    }
    queue.context[0x32b] = queue.context[0x31c] ^ (1u << 8u);
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "different formats or extents");
    for (const auto [packedInfo, name] : {std::pair{6u << 2u, "10_11_11 unorm"}, std::pair{(7u << 2u) | (7u << 8u) | (2u << 11u) | 0x40000u, "11_11_10 float"}}) {
        auto packed = rasterizedResolve;
        for (const auto offset : {0x31cu, 0x32bu}) packed.context[offset] = (packed.context[offset] & ~0x41f7cu) | packedInfo;
        expectFailure([&] { DecodeColorMetadataPass(packed); }, std::string("CB resolves of ") + name + " color targets");
    }
    queue = rasterizedResolve;
    queue.context[0x31c] |= 0x10000000u;
    queue.context[0x325] = 0;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
    queue = multisampled();
    queue.context[0x0] = 0;
    queue.context[0x202] = 0xcc0030;
    for (const auto offset : {0x318u, 0x31bu, 0x31cu, 0x390u}) queue.context[offset + (offset >= 0x390u ? 1u : 0xfu)] = queue.context[offset];
    queue.context[0x32c] = 0;
    queue.context[0x3b1] = queue.context[0x3b0];
    queue.context[0x3b9] = queue.context[0x3b8];
    queue.context[0x32b] = queue.context[0x31c] | 0x2000u;
    setCmaskAddress(queue, 1);
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "fast-clear color target");
    queue = multisampled();
    queue.context[0x2f8] = 0;
    queue.context[0x0] = 0;
    queue.context[0x202] = 0xcc0020;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "multisampled color target");
    queue.context[0x31c] |= 0x10000000u;
    queue.context[0x325] = 0;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
    queue = multisampled();
    queue.context[0x2f8] = 0;
    queue.context[0x0] = 0;
    queue.context[0x202] = 0x00cc0050;
    queue.context[0x31c] = 0x1002e028;
    queue.context[0x31d] = 0x00012000;
    queue.context[0x31e] = 0x00100038u;
    queue.context[0x325] = 0x2000;
    queue.context[0x323] = 0x3f800000;
    queue.context[0x324] = 0x3f800000;
    setCmaskAddress(queue);
    auto fmaskPass = DecodeColorMetadataPass(queue);
    Require(fmaskPass.has_value() && fmaskPass->mode == ColorMetadataPass::Mode::FmaskDecompress && fmaskPass->targets.size() == 1 && fmaskPass->targets[0].samples == 4 && fmaskPass->targets[0].cmaskAddress == cmaskAddress && fmaskPass->targets[0].dccAddress != 0, "an FMASK decompress of a 4x multisampled target with CMASK and DCC did not decode");
    Require(fmaskPass->targets[0].clearWords[0] == 0x3f800000 && fmaskPass->targets[0].clearWords[1] == 0x3f800000, "fast clear words changed");
    Require(fmaskPass->targets[0].dccControl == 0x00100038u, "FMASK decompress lost its DCC control");
    queue.context[0x31e] |= 0x80u;
    Require(DecodeColorMetadataPass(queue)->targets[0].dccControl == 0x001000b8u, "FMASK decompress hid an unsupported DCC control from the importer");
    queue = makeState();
    queue.context[0x0] = 0;
    queue.context[0x202] = 0x00cc0050;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "over a single-sample target");
    queue = multisampled();
    queue.context[0x2f8] = 0;
    queue.context[0x0] = 0;
    queue.context[0x202] = 0x00cc0050;
    queue.context[0x31c] = 0x00008028;
    queue.context[0x31d] = 0x00012000;
    expectFailure([&] { DecodeColorMetadataPass(queue); }, "FMASK decompress of a target without a CMASK address");

    using AgcDriver::Graphics::DccKeys;
    using AgcDriver::Graphics::MultisampledCmask;
    MultisampledCmask cmask(0x10000, 0x1000);
    cmask.NoteFill(0x10000, 0x1000, 0x11111111u);
    expectFailure([&] { cmask.TakeClear(); }, "leaves its tiles mixed");
    cmask.NoteFill(0x10800, 0x1000, 0);
    expectFailure([&] { cmask.TakeClear(); }, "partly covers");
    cmask.NoteFill(0x10000, 0x1000, 0);
    Require(cmask.TakeClear() && !cmask.TakeClear(), "a zero fill of the whole CMASK did not clear once");
    cmask.NoteFill(0x10000, 0x1000, 0xffffffffu);
    Require(!cmask.TakeClear(), "an expanded CMASK cleared");
    cmask.NoteFill(0x20000, 0x1000, 0x11111111u);
    Require(!cmask.TakeClear() && cmask.Keys() == DccKeys::Uncompressed, "a fill of another range changed the CMASK");
    cmask.CheckAddress(0x10000);
    expectFailure([&] { cmask.CheckAddress(0x20000); }, "used CMASK 0x20000");
    for (unsigned value = 0; value < 256u; ++value) {
        std::optional<DccKeys> expected;
        if (value == 0u) expected = DccKeys::Clear0000;
        else if (value == 0xccu || value == 0xddu || value == 0xeeu || value == 0xffu) expected = DccKeys::Uncompressed;
        Require(AgcDriver::Graphics::MultisampledCmaskKeys(static_cast<std::uint8_t>(value)) == expected, "MSAA CMASK byte classification differs at " + std::to_string(value));
    }
    std::cout << "All 256 MSAA CMASK byte classifications passed\n";
    MultisampledCmask seeded(0x30000, 0x1000);
    seeded.SeedCode(0x00u);
    Require(seeded.TakeClear(), "a fast-cleared seed did not clear its first use");
    Require(!seeded.TakeClear(), "a subsequent use applied the clear again");
    MultisampledCmask mixedSeed(0x30000, 0x1000);
    mixedSeed.SeedCode(std::nullopt);
    expectFailure([&] { mixedSeed.TakeClear(); }, "reads as mixed, which is not modeled");
    for (const std::uint8_t expanded : {0xccu, 0xddu, 0xeeu, 0xffu}) {
        MultisampledCmask expandedSeed(0x30000, 0x1000);
        expandedSeed.SeedCode(expanded);
        Require(!expandedSeed.TakeClear() && expandedSeed.Keys() == DccKeys::Uncompressed, "a color-expanded CMASK seed applied a fast clear or was refused");
        MultisampledCmask expandedFill(0x30000, 0x1000);
        expandedFill.NoteFill(0x30000, 0x1000, expanded * 0x01010101u);
        Require(!expandedFill.TakeClear() && expandedFill.Keys() == DccKeys::Uncompressed, "a color-expanded CMASK fill applied a fast clear or was refused");
    }
    for (const std::uint8_t unmodeled : {0x11u, 0x33u, 0x44u, 0x88u, 0xcdu}) {
        MultisampledCmask unmodeledSeed(0x30000, 0x1000);
        unmodeledSeed.SeedCode(unmodeled);
        expectFailure([&] { unmodeledSeed.TakeClear(); }, "reads as code 0x");
    }
    MultisampledCmask unevenFill(0x30000, 0x1000);
    unevenFill.NoteFill(0x30000, 0x1000, 0xccccccdcu);
    expectFailure([&] { unevenFill.TakeClear(); }, "leaves its tiles mixed");
    MultisampledCmask unaddressed;
    unaddressed.NoteFill(0, 0x1000, 0);
    Require(!unaddressed.TakeClear(), "a target without a CMASK cleared");

    using AgcDriver::Graphics::ColorTarget;
    using AgcDriver::Graphics::ColorTileMode;
    using AgcDriver::Graphics::MultisampledColorBytes;
    using AgcDriver::Graphics::MultisampledColorFill;
    using AgcDriver::Graphics::MultisampledFillClearColor;
    ColorTarget halfFloat{};
    halfFloat.address = 0x40000000;
    halfFloat.extent = {960, 540};
    halfFloat.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    halfFloat.tileMode = ColorTileMode::RenderTarget;
    halfFloat.elementBytes = 8;
    halfFloat.samples = 4;
    Require(MultisampledColorBytes(halfFloat) == 0xff0000, "a 4x 960x540 8-byte target is not 64x32 pixels per 64 KiB block");
    auto srgb = halfFloat;
    srgb.extent = {1536, 1536};
    srgb.format = VK_FORMAT_R8G8B8A8_SRGB;
    srgb.elementBytes = 4;
    Require(MultisampledColorBytes(srgb) == 0x2400000, "a 4x 1536x1536 4-byte target is not 64x64 pixels per 64 KiB block");
    auto narrow = srgb;
    narrow.extent = {130, 60};
    narrow.samples = 2;
    Require(MultisampledColorBytes(narrow) == 0x30000, "a 2x 4-byte target is not 64x128 pixels per 64 KiB block");
    narrow.samples = 8;
    Require(MultisampledColorBytes(narrow) == 0x50000, "an 8x 4-byte target is not 32x64 pixels per 64 KiB block");
    srgb.extent = {100, 100};
    Require(MultisampledColorBytes(srgb) == 0x40000, "a 4x 100x100 4-byte target does not span 2x2 64 KiB blocks");
    for (const auto mode : {ColorTileMode::Z64KBX, ColorTileMode::D64KBX}) {
        auto swizzled = srgb;
        swizzled.tileMode = mode;
        Require(MultisampledColorBytes(swizzled) == 0x40000, "a 4x 100x100 4-byte surface of another 64 KiB swizzle does not span 2x2 64 KiB blocks");
    }
    srgb.tileMode = ColorTileMode::Standard4KB;
    Require(MultisampledColorBytes(srgb) == 0x31000, "a 4x 4-byte target is not 16x16 pixels per 4 KiB block");
    srgb.tileMode = ColorTileMode::Linear;
    Require(MultisampledColorBytes(srgb) == 0, "a linear multisampled target has a footprint");

    MultisampledColorFill fill(halfFloat);
    Require(!fill.TakeFill(), "an unfilled target cleared");
    fill.NoteFill(0x40000000, 0xff0000, 0);
    const auto zero = fill.TakeFill();
    Require(zero && *zero == 0 && !fill.TakeFill(), "a fill of the whole color memory did not clear once");
    fill.NoteFill(0x40ff0000, 0x10000, 0);
    fill.NoteFill(0x3fff0000, 0x10000, 0);
    Require(!fill.TakeFill(), "a fill next to the color memory cleared it");
    fill.NoteFill(0x40000000, 0xfe0000, 0);
    expectFailure([&] { fill.TakeFill(); }, "partly covers");
    fill.NoteFill(0x3fff0000, 0x1010000, 0x3c003c00u);
    const auto one = fill.TakeFill();
    Require(one && *one == 0x3c003c00u, "a fill over and around the color memory did not clear it");
    MultisampledColorFill linear(srgb);
    linear.NoteFill(0x40001000, 0x1000, 0);
    Require(!linear.TakeFill(), "a fill past the start of a target with no modeled footprint was taken");
    linear.NoteFill(0x40000000, 0x1000, 0);
    expectFailure([&] { linear.TakeFill(); }, "footprint is not modeled");

    VkClearColorValue clear{};
    Require(MultisampledFillClearColor(halfFloat, 0x3c003c00u, clear) && clear.float32[0] == 1.0f && clear.float32[3] == 1.0f, "a half-float fill of 1.0 did not clear to 1.0");
    Require(MultisampledFillClearColor(srgb, 0, clear) && clear.float32[0] == 0.0f && clear.float32[3] == 0.0f, "an sRGB fill of 0 did not clear to 0");
    Require(MultisampledFillClearColor(srgb, 0x80ffffffu, clear) && clear.float32[2] == 1.0f && clear.float32[3] == 128.0f / 255.0f, "an sRGB fill of white with linear alpha 0x80 did not clear to it");
    Require(!MultisampledFillClearColor(srgb, 0x00000080u, clear), "an sRGB fill whose color decodes inexactly cleared");

    using AgcDriver::Graphics::MultisampledDccClear;
    auto single = srgb;
    single.format = VK_FORMAT_R8G8B8A8_UNORM;
    single.tileMode = ColorTileMode::RenderTarget;
    single.extent = {581, 729};
    single.address = single.surfaceAddress = 0x40010000;
    single.dccAddress = 0x50010000;
    single.dccPipeAligned = true;
    const std::vector<std::byte> singleKeys(AgcDriver::Graphics::MultisampledDccBytes(single), std::byte{0x10});
    expectFailure([&] { MultisampledDccClear(single, singleKeys, {}); }, "comp-to-single keys without a recorded clear color");
    AgcDriver::Graphics::GuestTextureResource cleared{};
    cleared.baseAddress = single.surfaceAddress;
    cleared.width = 581;
    cleared.height = 729;
    cleared.samples = 4;
    AgcDriver::Graphics::NoteMultisampledSingleClear(cleared, {0xff00ffccu, 0u, 0u, 0u});
    clear = MultisampledDccClear(single, singleKeys, {});
    Require(clear.float32[0] == 204.0f / 255.0f && clear.float32[1] == 1.0f && clear.float32[2] == 0.0f && clear.float32[3] == 1.0f, "comp-to-single keys did not clear to the recorded RGBA8 color");
    auto resized = single;
    resized.extent = {580, 729};
    const std::vector<std::byte> resizedKeys(AgcDriver::Graphics::MultisampledDccBytes(resized), std::byte{0x10});
    expectFailure([&] { MultisampledDccClear(resized, resizedKeys, {}); }, "comp-to-single keys without a recorded clear color");
    auto wide = halfFloat;
    wide.surfaceAddress = 0x40000000;
    wide.dccAddress = 0x50000000;
    wide.dccPipeAligned = true;
    AgcDriver::Graphics::GuestTextureResource wideCleared{};
    wideCleared.baseAddress = wide.surfaceAddress;
    wideCleared.width = wide.extent.width;
    wideCleared.height = wide.extent.height;
    wideCleared.samples = wide.samples;
    AgcDriver::Graphics::NoteMultisampledSingleClear(wideCleared, {0x3c003c00u, 0x3c003c00u, 0u, 0u});
    const std::vector<std::byte> wideKeys(AgcDriver::Graphics::MultisampledDccBytes(wide), std::byte{0x10});
    clear = MultisampledDccClear(wide, wideKeys, {});
    Require(clear.float32[0] == 1.0f && clear.float32[1] == 1.0f && clear.float32[2] == 1.0f && clear.float32[3] == 1.0f, "comp-to-single keys did not clear to the recorded RGBA16F color");
    auto widest = wide;
    widest.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    widest.elementBytes = 16;
    widest.address = widest.surfaceAddress = 0x48000000;
    widest.dccAddress = 0x58000000;
    auto widestCleared = wideCleared;
    widestCleared.baseAddress = widest.surfaceAddress;
    AgcDriver::Graphics::NoteMultisampledSingleClear(widestCleared, {0x3f800000u, 0x3f000000u, 0u, 0x3f800000u});
    const std::vector<std::byte> widestKeys(AgcDriver::Graphics::MultisampledDccBytes(widest), std::byte{0x10});
    clear = MultisampledDccClear(widest, widestKeys, {});
    Require(clear.float32[0] == 1.0f && clear.float32[1] == 0.5f && clear.float32[2] == 0.0f && clear.float32[3] == 1.0f, "comp-to-single keys did not clear to the recorded RGBA32F color");
    auto swizzled = single;
    swizzled.pipeBankXor = 0x4000;
    expectFailure([&] { MultisampledDccClear(swizzled, singleKeys, {}); }, "comp-to-single keys without a recorded clear color");
    constexpr std::size_t reusedBytes = 0x10000;
    auto* reusedBlock = static_cast<std::byte*>(::operator new(reusedBytes, std::align_val_t{reusedBytes}));
    const auto mapReused = [&] {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, reusedBlock, reusedBytes, true, true, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto unmapReused = [&] {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, reusedBlock);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    mapReused();
    auto reused = single;
    reused.address = reused.surfaceAddress = reinterpret_cast<std::uintptr_t>(reusedBlock);
    auto reusedCleared = cleared;
    reusedCleared.baseAddress = reused.surfaceAddress;
    AgcDriver::Graphics::NoteMultisampledSingleClear(reusedCleared, {0xff00ffccu, 0u, 0u, 0u});
    clear = MultisampledDccClear(reused, singleKeys, {});
    Require(clear.float32[0] == 204.0f / 255.0f && clear.float32[3] == 1.0f, "comp-to-single keys on mapped guest memory did not clear to the recorded color");
    unmapReused();
    mapReused();
    expectFailure([&] { MultisampledDccClear(reused, singleKeys, {}); }, "comp-to-single keys without a recorded clear color");
    clear = MultisampledDccClear(single, singleKeys, {});
    Require(clear.float32[0] == 204.0f / 255.0f, "unmapping one surface dropped another surface's comp-to-single clear");
    unmapReused();
    ::operator delete(reusedBlock, std::align_val_t{reusedBytes});

    using AgcDriver::Graphics::TakeMultisampledClear;
    halfFloat.clearWords = {0x3c003c00u, 0x3c003c00u};
    MultisampledCmask clearedCmask(0x50000000, 0x1000);
    MultisampledColorFill pending(halfFloat);
    Require(!TakeMultisampledClear(clearedCmask, pending, halfFloat, clear), "a target with no CMASK clear and no fill cleared");
    pending.NoteFill(0x40000000, 0xff0000, 0);
    Require(TakeMultisampledClear(clearedCmask, pending, halfFloat, clear) && clear.float32[0] == 0.0f, "a fill of the whole color memory did not clear to its value");
    pending.NoteFill(0x40000000, 0xfe0000, 0);
    expectFailure([&] { TakeMultisampledClear(clearedCmask, pending, halfFloat, clear); }, "partly covers");
    clearedCmask.NoteFill(0x50000000, 0x1000, 0);
    Require(TakeMultisampledClear(clearedCmask, pending, halfFloat, clear) && clear.float32[0] == 1.0f, "a pending CMASK clear did not take precedence over a partial fill of the color memory");
    Require(!TakeMultisampledClear(clearedCmask, pending, halfFloat, clear), "a partial fill of the color memory outlived the CMASK clear over it");
    pending.NoteFill(0x40000000, 0xff0000, 0);
    clearedCmask.NoteFill(0x50000000, 0x1000, 0);
    Require(TakeMultisampledClear(clearedCmask, pending, halfFloat, clear) && clear.float32[0] == 1.0f && !TakeMultisampledClear(clearedCmask, pending, halfFloat, clear), "a pending CMASK clear and a whole fill did not clear once to the CMASK clear color");
}

void multisampleDccStateTests() {
    static std::array<std::byte, 131072> color{};
    const auto address = (reinterpret_cast<std::uintptr_t>(color.data()) + 65535u) & ~std::uintptr_t{65535u};
    for (const auto log2 : {1u, 2u, 3u}) {
        auto queue = makeState();
        queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
        queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
        queue.context[0x31c] |= 0x10000000u;
        queue.context[0x31d] = (log2 << 12u) | (log2 << 15u);
        queue.context[0x31e] = 0x00100038u;
        queue.context[0x325] = 0x1234u;
        queue.context[0x3b8] = 0x4dc6c000u;
        queue.context[0x2f8] = (log2 << 20u) | log2;
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
            queue.context[0x2fe + pixel * 4u] = 0;
            queue.context[0x2ff + pixel * 4u] = 0;
        }
        queue.context[0x292] = 3;
        queue.context[0x1b3] = queue.context[0x1b4] = 2;
        const auto state = AgcDriver::Graphics::DecodeState(queue);
        Require(state.colors.size() == 1 && state.colors.front().samples == (1u << log2) && state.colors.front().dccAddress == 0x123400u && state.colors.front().dccPipeAligned, "normal-render multisampled DCC state did not decode");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "normal-render multisampled DCC state failed the draw precheck");
        auto changed = queue;
        changed.context[0x31e] |= 0x80u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "color transforms or lossy");
        changed = queue;
        changed.context[0x3b8] &= ~0x40000000u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "pipe-aligned single-layer");
        changed = queue;
        changed.context[0x325] = 0;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "pipe-aligned single-layer");
        changed = queue;
        changed.context[0x3b8] |= 1u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "pipe-aligned single-layer");
        changed = queue;
        changed.context[0x3b0] |= 1u << 28u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "multisampled color mips");
    }
}

void multisampledDepthTests() {
    using AgcDriver::Graphics::DecodeState;
    const auto singleSampleScan = [] {
        auto queue = makeState();
        queue.context[0x2f8] = 0;
        queue.context[0x292] = 2;
        queue.context[0x31d] = 0x12000;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        queue.context[0x000] = 0;
        queue.context[0x002] = 0;
        queue.context[0x007] = (3u << 16u) | 63u;
        queue.context[0x00a] = 0;
        queue.context[0x00b] = 0;
        queue.context[0x010] = 0x2290098b;
        queue.context[0x011] = 0x20000180;
        for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
        queue.context[0x200] = 0x16;
        return queue;
    };
    const auto requireCentered = [](const AgcDriver::Graphics::State& state, const std::string& what) {
        Require(state.samples == 4 && state.depth && state.depth->samples == 4 && state.customSampleLocations && !state.sampleLocationGrid && !state.perSampleShading, what + " did not rasterize the depth target's samples");
        const auto locations = AgcDriver::Graphics::SampleLocations(state);
        for (std::uint32_t sample = 0; sample < state.samples; ++sample) Require(locations[sample].x == 0.5f && locations[sample].y == 0.5f, what + " did not place every sample at the pixel center");
    };
    const auto depthOnly = [&] {
        auto queue = singleSampleScan();
        queue.context[0x8e] = 0;
        queue.context[0x010] = 0xa290098b;
        queue.context[0x200] = 0x777;
        queue.context[0x201] = 0;
        return queue;
    };
    auto queue = depthOnly();
    auto state = DecodeState(queue);
    Require(state.colors.empty(), "a depth-only single-sample scan decoded a color target");
    requireCentered(state, "a single-sample scan into a 4x depth target (PPSA18216's registers)");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck refused a single-sample scan into a 4x depth target");
    for (const auto primitive : {5u, 6u, 7u, 17u}) {
        queue = depthOnly();
        queue.userConfig[0x242] = primitive;
        requireCentered(DecodeState(queue), "a single-sample scan of primitive type " + std::to_string(primitive) + " into a 4x depth target");
    }
    queue = depthOnly();
    queue.context[0x000] = 1;
    queue.context[0x00b] = std::bit_cast<std::uint32_t>(0.25f);
    state = DecodeState(queue);
    requireCentered(state, "a depth clear scanned single-sampled into a 4x depth target");
    Require(state.depthWrite && state.depthCompare == VK_COMPARE_OP_ALWAYS, "a depth clear into a 4x depth target did not write every covered sample");
    queue = depthOnly();
    queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    requireCentered(DecodeState(queue), "a single-sample scan whose pixel program is skipped");
    queue = depthOnly();
    queue.context[0x201] = 0x00040000u;
    expectFailure([&] { DecodeState(queue); }, "a single-sample scan into a multisampled depth target with Z interpolation or overrasterization (DB_EQAA)");
    queue = singleSampleScan();
    state = DecodeState(queue);
    Require(state.colors.size() == 1 && state.colors[0].samples == 4, "a single-sample scan into 4x depth and color targets lost its color target");
    requireCentered(state, "a single-sample scan into 4x depth and color targets");
    queue.context[0x293] = 0x10000u;
    Require(!DecodeState(queue).perSampleShading, "PS_ITER_SAMPLE requested sample shading for a single-sample scan");
    queue = singleSampleScan();
    queue.context[0x31d] = 0;
    state = DecodeState(queue);
    Require(state.truncatedCoverage && !state.mergedCoverage && state.colors.size() == 1 && state.colors[0].samples == 1, "a single-sample scan into a 4x depth target and a one-sample color target did not take the color from sample 0");
    requireCentered(state, "a single-sample scan into a 4x depth target and a one-sample color target");
    const auto truncatedTwoSample = [&] {
        auto queue = singleSampleScan();
        queue.context[0x2f8] = 0x00100001;
        queue.context[0x292] = 3;
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0xcc44;
        queue.context[0x010] = 0x22900987;
        queue.context[0x31d] = 0;
        return queue;
    };
    queue = truncatedTwoSample();
    state = DecodeState(queue);
    Require(state.samples == 2 && state.depth && state.depth->samples == 2 && state.colors.size() == 1 && state.colors[0].samples == 1 && state.truncatedCoverage && !state.mergedCoverage && !state.perSampleShading, "a one-sample color target beside a 2x depth target under a 2x rasterizer with MSAA_ENABLE (PPSA02225's registers) did not take the color from sample 0");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck refused a one-sample color target beside a 2x depth target");
    queue = truncatedTwoSample();
    queue.context[0x293] = 0x10000u;
    queue.context[0x1b3] = queue.context[0x1b4] = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveSample) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::FrontFace);
    state = DecodeState(queue);
    Require(state.truncatedCoverage && state.perSampleShading && state.samples == 2 && state.colors[0].samples == 1, "PS_ITER_SAMPLE into a one-sample color target beside a 2x depth target (PPSA02225's registers) did not run per sample with sample 0's color");
    queue = truncatedTwoSample();
    queue.context[0x31d] = 0x12000;
    expectFailure([&] { DecodeState(queue); }, "a color target's sample count differs from the rasterizer's");
    queue = singleSampleScan();
    queue.context[0x010] = 0x22900983;
    expectFailure([&] { DecodeState(queue); }, "a color target's sample count differs from the rasterizer's");
    queue = singleSampleScan();
    queue.context[0x2dc] = 0x0000aa01u;
    expectFailure([&] { DecodeState(queue); }, "a single-sample scan into a multisampled depth target with alpha-to-coverage");
    for (const auto primitive : {2u, 3u}) {
        queue = singleSampleScan();
        queue.userConfig[0x242] = primitive;
        expectFailure([&] { DecodeState(queue); }, "a single-sample scan into a multisampled depth target for primitives other than triangles and rectangles");
    }
    queue = singleSampleScan();
    queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    expectFailure([&] { DecodeState(queue); }, "a single-sample scan into a multisampled depth target with a pixel program reading the sample index");
    queue = singleSampleScan();
    queue.context[0x1b3] = queue.context[0x1b4] = 2u | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveSample);
    requireCentered(DecodeState(queue), "a single-sample scan with a pixel program reading sample-interpolated inputs");
    queue = singleSampleScan();
    queue.context[0x1c4] = 9;
    queue.context[0x203] |= 0x100u;
    expectFailure([&] { DecodeState(queue); }, "a single-sample scan into a multisampled depth target with a sample-mask export");
    queue = singleSampleScan();
    queue.context[0x292] = 3;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0;
    expectFailure([&] { DecodeState(queue); }, "the depth target's sample count differs from the rasterizer's");
    queue = singleSampleScan();
    queue.context[0x2f8] = 0x00100001;
    queue.context[0x292] = 3;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0xcc44;
    queue.context[0x31d] = 0x9000;
    expectFailure([&] { DecodeState(queue); }, "the depth target's sample count differs from the rasterizer's");
}

void clipDistanceTests() {
    auto queue = makeState();
    queue.context[0x207] = 0x0040000f;
    Require(AgcDriver::Graphics::DecodeState(queue).paClVsOutCntl == 0x0040000fu, "DecodeState dropped the PA_CL_VS_OUT_CNTL value the validator quotes");
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "four clip distances were rejected by the precheck");
    queue.context[0x207] = 0x004001ff;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "more than eight clip and cull distances");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).find("more than eight") != std::string::npos, "nine clip distances passed the precheck");
}

void framebufferLayerTests() {
    AgcDriver::Graphics::Context context{};
    context.limits.maxFramebufferWidth = 64;
    context.limits.maxFramebufferHeight = 64;
    context.limits.maxFramebufferLayers = 3;
    AgcDriver::Graphics::Framebuffer accepted(context, VK_NULL_HANDLE, {}, {64, 64}, 3);
    for (const VkRenderPass pass : std::array<VkRenderPass, 2>{VK_NULL_HANDLE, reinterpret_cast<VkRenderPass>(1)}) {
        expectFailure([&] { AgcDriver::Graphics::Framebuffer rejected(context, pass, {}, {64, 64}, 0); }, "framebuffer layers exceed device limits");
        expectFailure([&] { AgcDriver::Graphics::Framebuffer rejected(context, pass, {}, {64, 64}, 4); }, "framebuffer layers exceed device limits");
    }
}

void quadPixelMaskTests() {
    using AgcDriver::Graphics::DecodeState;
    using AgcDriver::Graphics::DrawRejection;
    using AgcDriver::Graphics::HostSampleMask;
    using AgcDriver::Graphics::PartialSampleMask;
    using AgcDriver::Graphics::QuadPixelMask;
    using AgcDriver::Graphics::UniformSampleMask;
    using Masks = std::array<std::uint16_t, 4>;
    const auto masked = [](std::uint32_t top, std::uint32_t bottom, std::uint32_t log2 = 0u) {
        auto queue = makeState();
        queue.context[0x30e] = top;
        queue.context[0x30f] = bottom;
        queue.context[0x1b3] = 2;
        queue.context[0x1b4] = 2;
        if (log2 != 0u) {
            queue.context[0x2f8] = (log2 << 20u) | log2;
            queue.context[0x292] = 3;
            queue.context[0x3b8] = 0x0dc00000u;
            queue.context[0x31d] = (log2 << 12u) | (log2 << 15u);
            for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = log2 == 1u ? 0xcc44u : 0x622ae6aeu;
        }
        return queue;
    };
    const auto accepted = [](const AgcDriver::QueueState& queue, const std::string& what) {
        std::vector<AgcDriver::Graphics::RegisterRead> reads;
        AgcDriver::Graphics::RegisterReadLog() = &reads;
        const auto state = DecodeState(queue);
        const auto rejection = DrawRejection(queue, false);
        AgcDriver::Graphics::RegisterReadLog() = nullptr;
        Require(rejection.empty(), what + " was refused by the precheck: " + rejection);
        for (const auto read : reads) Require(AgcDriver::Graphics::DrawKeyCovers(read), "DrawKeyRegisters lacks a register decoded for " + what);
        return state;
    };
    const auto refused = [](const AgcDriver::QueueState& queue, std::string_view reason) {
        expectFailure([&] { DecodeState(queue); }, reason);
        Require(DrawRejection(queue, false).find(reason) != std::string::npos, "the precheck accepted a draw DecodeState refuses for " + std::string(reason));
    };
    auto state = accepted(masked(0x0000ffffu, 0u), "the X0Y0 single-sample mask");
    Require(state.quadSampleMask == Masks{1, 0, 0, 0} && PartialSampleMask(state) && !UniformSampleMask(state) && QuadPixelMask(state) == 0x1u, "PA_SC_AA_MASK 0x0000ffff/0 did not cover only the even-x, even-y pixel of each quad");
    state = accepted(masked(0xffff0000u, 0u), "the X1Y0 single-sample mask");
    Require(QuadPixelMask(state) == 0x2u, "PA_SC_AA_MASK 0xffff0000/0 did not cover only the odd-x, even-y pixel of each quad");
    state = accepted(masked(0xfffe0001u, 0x0001fffeu), "a single-sample mask with bits above sample 0");
    Require(state.quadSampleMask == Masks{1, 0, 0, 1} && QuadPixelMask(state) == 0x9u, "a single-sample mask did not keep only bit 0 of each pixel's field");
    state = accepted(masked(0x00010001u, 0x00010001u), "a single-sample mask with only sample 0 set");
    Require(!PartialSampleMask(state) && QuadPixelMask(state) == 0xfu && HostSampleMask(state) == 1u, "PA_SC_AA_MASK 0x00010001 at one sample is not a full mask");
    state = accepted(masked(0u, 0u), "an empty single-sample mask");
    Require(PartialSampleMask(state) && UniformSampleMask(state) && HostSampleMask(state) == 0u && QuadPixelMask(state) == 0xfu, "an empty mask equal for the whole quad did not become a host sample mask of 0");
    Require(AgcDriver::Graphics::DecodePixelStageInfo(masked(0x0000ffffu, 0u).context, {}).quadPixelMask == 0xfu, "the pixel stage decode read the sample mask registers instead of the decoded state");
    auto queue = masked(0x0000ffffu, 0u);
    queue.context[0x203] = 0x10u;
    Require(accepted(queue, "an early-Z masked draw").pixelEarlyZ, "DB_SHADER_CONTROL Z_ORDER EARLY_Z_THEN_LATE_Z did not decode as early Z");
    queue.context[0x203] = 0x50u;
    Require(!accepted(queue, "a killing masked draw").pixelEarlyZ, "a pixel program with kill decoded as early Z");
    queue.context[0x313] = 0x6001u;
    refused(queue, "sample masks with conservative rasterization");
    queue = masked(0xffffffffu, 0xffffffffu);
    queue.context[0x313] = 0x6001u;
    Require(!PartialSampleMask(accepted(queue, "a full mask with conservative rasterization")), "a full mask with conservative rasterization decoded as partial");
    state = accepted(masked(0x00050005u, 0x00050005u, 2u), "a 4x mask equal for the whole quad");
    Require(state.samples == 4 && PartialSampleMask(state) && UniformSampleMask(state) && HostSampleMask(state) == 0x5u && QuadPixelMask(state) == 0xfu, "a 4x mask 0x5 equal for the whole quad did not become the host sample mask 0x5");
    state = accepted(masked(0xfff5fff5u, 0xfff5fff5u, 2u), "a 4x mask with bits above sample 3");
    Require(HostSampleMask(state) == 0x5u && state.quadSampleMask == Masks{5, 5, 5, 5}, "a 4x mask kept bits of samples the rasterizer does not have");
    state = accepted(masked(0x0000000fu, 0u, 2u), "a 4x mask that covers whole pixels");
    Require(!UniformSampleMask(state) && QuadPixelMask(state) == 0x1u && state.quadSampleMask == Masks{0xf, 0, 0, 0}, "a 4x mask covering all samples of one quad pixel did not become a quad pixel mask");
    state = accepted(masked(0x00000003u, 0u, 1u), "a 2x mask that covers whole pixels");
    Require(QuadPixelMask(state) == 0x1u, "a 2x mask covering both samples of one quad pixel did not become a quad pixel mask");
    using AgcDriver::Graphics::QuadSampleMasks;
    constexpr Masks NoSampleMask{0xffffu, 0xffffu, 0xffffu, 0xffffu};
    state = accepted(masked(0x00020001u, 0x00080004u, 2u), "a 4x mask with one sample per quad pixel");
    Require(QuadPixelMask(state) == 0xfu && QuadSampleMasks(state) == Masks{1, 2, 4, 8}, "a 4x mask of sample q for quad pixel q did not become per-pixel sample masks");
    state = accepted(masked(0x0000000fu, 0x00030000u, 2u), "a 4x mask mixing full, empty and partial pixels");
    Require(QuadPixelMask(state) == 0x9u && QuadSampleMasks(state) == Masks{0xffff, 0, 0, 3}, "a 4x mask mixing full, empty and partial pixels did not keep full pixels as all samples");
    Require(QuadSampleMasks(accepted(masked(0x0000000fu, 0u, 2u), "a whole-pixel 4x mask")) == NoSampleMask && QuadSampleMasks(accepted(masked(0x00050005u, 0x00050005u, 2u), "a uniform 4x mask")) == NoSampleMask && QuadSampleMasks(accepted(masked(0x0000ffffu, 0u), "a single-sample mask")) == NoSampleMask, "a mask without partial pixels that differ produced per-pixel sample masks");
    for (const auto centroid : {4u, 64u}) {
        queue = masked(0x00020001u, 0x00080004u, 2u);
        queue.context[0x1b3] = 2u | centroid;
        queue.context[0x1b4] = 2u | centroid;
        Require(QuadSampleMasks(accepted(queue, "centroid interpolation under per-pixel sample masks")) == Masks{1, 2, 4, 8}, "centroid interpolation refused per-pixel sample masks");
    }
    for (const auto centroid : {4u, 64u}) {
        queue = masked(0x00050005u, 0x00050005u, 2u);
        queue.context[0x1b3] = 2u | centroid;
        queue.context[0x1b4] = 2u | centroid;
        refused(queue, "sample masks that remove some samples of a pixel with centroid interpolation");
        queue.context[0x1b4] = 2u;
        Require(HostSampleMask(accepted(queue, "an unaddressed centroid input")) == 0x5u, "an unaddressed centroid input refused a 4x mask");
        queue = masked(0x0000000fu, 0u, 2u);
        queue.context[0x1b3] = 2u | centroid;
        queue.context[0x1b4] = 2u | centroid;
        Require(QuadPixelMask(accepted(queue, "centroid interpolation under a whole-pixel mask")) == 0x1u, "centroid interpolation refused a mask that covers whole pixels");
    }
    queue = masked(0x0000ffffu, 0u);
    queue.context[0x31d] = 0x12000;
    state = accepted(queue, "a single-sample mask into a 4x target");
    Require(state.samples == 4 && state.quadSampleMask == Masks{0xf, 0, 0, 0} && QuadPixelMask(state) == 0x1u, "single-sample rasterization into a 4x target did not apply each pixel's sample 0 bit to all of its samples");
    queue.context[0x30e] = queue.context[0x30f] = 0xfffefffeu;
    state = accepted(queue, "an empty single-sample mask into a 4x target");
    Require(UniformSampleMask(state) && HostSampleMask(state) == 0u, "an empty single-sample mask into a 4x target did not remove every sample");

    AgcDriver::Graphics::Context context{};
    ShaderRecompiler::RecompileResult early;
    early.spirv = std::vector<std::uint32_t>{spv::MagicNumber, 0x10300u, 0u, 2u, 0u, (3u << 16u) | spv::OpExecutionMode, 1u, spv::ExecutionModeEarlyFragmentTests, (5u << 16u) | spv::OpFunction, 0u, 1u, 0u, 0u};
    ShaderRecompiler::RecompileResult late;
    late.spirv = std::vector<std::uint32_t>{spv::MagicNumber, 0x10300u, 0u, 2u, 0u, (3u << 16u) | spv::OpExecutionMode, 1u, spv::ExecutionModeOriginUpperLeft};
    Require(AgcDriver::Graphics::DeclaresEarlyFragmentTests(early) && !AgcDriver::Graphics::DeclaresEarlyFragmentTests(late), "early fragment tests were not read from the SPIR-V execution modes");
    using Stage = ShaderRecompiler::ShaderStage;
    AgcDriver::Graphics::State uniform{};
    uniform.samples = 4;
    uniform.quadSampleMask = {5, 5, 5, 5};
    const std::array<AgcDriver::Graphics::CompiledShader, 1> earlyShaders{{{Stage::Fragment, &early, 0}}};
    const std::array<AgcDriver::Graphics::CompiledShader, 1> lateShaders{{{Stage::Fragment, &late, 0}}};
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleMask(context, uniform, earlyShaders); }, "earlyFragmentSampleMaskTestBeforeSampleCounting");
    AgcDriver::Graphics::ValidateSampleMask(context, uniform, lateShaders);
    context.earlyFragmentSampleMaskTestBeforeSampleCounting = true;
    AgcDriver::Graphics::ValidateSampleMask(context, uniform, earlyShaders);
    ShaderRecompiler::RecompileResult stores;
    ShaderRecompiler::DescriptorBinding written{};
    written.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    written.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    written.count = 1;
    written.bufferWritten = {true};
    stores.bindings.push_back(written);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> storing{{{Stage::Vertex, &late, 0}, {Stage::Fragment, &stores, 0}}};
    const std::array<AgcDriver::Graphics::CompiledShader, 2> vertexStoring{{{Stage::Vertex, &stores, 0}, {Stage::Fragment, &late, 0}}};
    AgcDriver::Graphics::State varying{};
    varying.quadSampleMask = {1, 0, 0, 0};
    varying.pixelEarlyZ = true;
    varying.depth = AgcDriver::Graphics::DepthTarget{};
    varying.depthTest = true;
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleMask(context, varying, storing); }, "differs between the pixels of a quad");
    AgcDriver::Graphics::ValidateSampleMask(context, varying, vertexStoring);
    for (const auto change : {0, 1, 2}) {
        auto relaxed = varying;
        if (change == 0) relaxed.pixelEarlyZ = false;
        if (change == 1) relaxed.depthTest = false;
        if (change == 2) relaxed.quadSampleMask = {1, 1, 1, 1};
        AgcDriver::Graphics::ValidateSampleMask(context, relaxed, storing);
    }
    varying.depthTest = false;
    varying.stencilTest = true;
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleMask(context, varying, storing); }, "differs between the pixels of a quad");
    varying.stencilTest = false;
    varying.depthBoundsTest = true;
    expectFailure([&] { AgcDriver::Graphics::ValidateSampleMask(context, varying, storing); }, "differs between the pixels of a quad");
}

void DepthRemapTests() {
    auto queue = makeState();
    queue.context[0x204] = 0;
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    AgcDriver::Graphics::Context remapping{};
    remapping.depthClamp = true;
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.negativeOneToOne && state.viewport.minDepth == -1 && state.viewport.maxDepth == 1 && state.depthRemap.mode == 0, "a negative-one-to-one depth transform decoded with a remap");
    AgcDriver::Graphics::ApplyDepthRemap(remapping, state);
    Require(state.depthRemap.mode == 2 && state.depthRemap.scale == 1.0f && state.depthRemap.offset == 0.0f, "the remap did not take the viewport's depth scale and offset");
    Require(state.viewport.minDepth == 0 && state.viewport.maxDepth == 1 && !state.negativeOneToOne && state.depthClamp, "the remapped viewport does not clamp at the console's bounds in zero-to-one clip space");
    AgcDriver::Graphics::Context unrestricted{};
    unrestricted.depthRangeUnrestricted = true;
    auto kept = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::ApplyDepthRemap(unrestricted, kept);
    Require(kept.depthRemap.mode == 0 && kept.viewport.minDepth == -1 && kept.negativeOneToOne && !kept.depthClamp, "an unrestricted device remapped the depth range");
    AgcDriver::Graphics::Context noClamp{};
    auto refused = AgcDriver::Graphics::DecodeState(queue);
    expectFailure([&] { AgcDriver::Graphics::ApplyDepthRemap(noClamp, refused); }, "depth clamping");
    queue.context[0x113] = std::bit_cast<std::uint32_t>(0.5f);
    queue.context[0x114] = std::bit_cast<std::uint32_t>(0.5f);
    auto inRange = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::ApplyDepthRemap(remapping, inRange);
    Require(inRange.depthRemap.mode == 0 && inRange.negativeOneToOne && !inRange.depthClamp, "an in-range depth transform was remapped");
    queue.context[0x204] = 0x80000u;
    queue.context[0x113] = std::bit_cast<std::uint32_t>(2.0f);
    queue.context[0x114] = std::bit_cast<std::uint32_t>(-0.5f);
    queue.context[0xb4] = std::bit_cast<std::uint32_t>(0.25f);
    queue.context[0xb5] = std::bit_cast<std::uint32_t>(0.75f);
    auto bounded = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::ApplyDepthRemap(remapping, bounded);
    Require(bounded.depthRemap.mode == 1 && bounded.depthRemap.scale == 4.0f && bounded.depthRemap.offset == -1.5f && bounded.viewport.minDepth == 0.25f && bounded.viewport.maxDepth == 0.75f, "a zero-to-one remap did not divide its transform by the clamp range it gives the viewport");
    queue.context[0xb4] = std::bit_cast<std::uint32_t>(-1.0f);
    queue.context[0xb5] = std::bit_cast<std::uint32_t>(2.0f);
    auto clipped = AgcDriver::Graphics::DecodeState(queue);
    AgcDriver::Graphics::ApplyDepthRemap(remapping, clipped);
    Require(clipped.depthRemap.mode == 1 && clipped.viewport.minDepth == 0 && clipped.viewport.maxDepth == 1, "clamp bounds beyond [0, 1] were not clipped to the representable depth range");
    Require(clipped.depthRemap.scale == 2.0f && clipped.depthRemap.offset == -0.5f, "clamp bounds clipped to [0, 1] changed the depth transform");
    auto unorm = AgcDriver::Graphics::DecodeState(queue);
    unorm.depth = AgcDriver::Graphics::DepthTarget{0x10000, 0, {64, 64}, VK_FORMAT_D16_UNORM, 0.0f, 0};
    unorm.depthTest = unorm.depthWrite = true;
    AgcDriver::Graphics::ApplyDepthRemap(remapping, unorm);
    Require(unorm.viewport.minDepth == 0 && unorm.viewport.maxDepth == 1, "a normalized depth target refused clipped clamp bounds");
    auto floating = AgcDriver::Graphics::DecodeState(queue);
    floating.depth = AgcDriver::Graphics::DepthTarget{0x10000, 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 0.0f, 0};
    floating.depthWrite = true;
    AgcDriver::Graphics::ApplyDepthRemap(remapping, floating);
    Require(floating.depthRemap.mode == 1 && floating.viewport.minDepth == 0 && floating.viewport.maxDepth == 1 && floating.depthClamp, "a written float depth target refused clipped clamp bounds");
}

void UnusedViewportDepthTests() {
    auto queue = makeState();
    queue.context[0x204] = 0;
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    const auto decoded = AgcDriver::Graphics::DecodeState(queue);
    Require(!decoded.depth && decoded.viewport.minDepth == -1 && decoded.viewport.maxDepth == 1, "unused depth fixture changed");
    AgcDriver::Graphics::Context context{};
    context.limits.maxViewportDimensions[0] = context.limits.maxViewportDimensions[1] = 16384;
    context.limits.viewportBoundsRange[0] = -32768;
    context.limits.viewportBoundsRange[1] = 32767;
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.posX = pixel.posY = pixel.posW = true;
    auto color = decoded;
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(color, pixel);
    AgcDriver::Graphics::ValidateViewport(context, color.viewport);
    Require(color.viewport.minDepth == 0 && color.viewport.maxDepth == 1, "unused viewport depth is not representable");
    AgcDriver::Graphics::Context remapping{};
    remapping.depthClamp = true;
    const auto normalized = color;
    AgcDriver::Graphics::ApplyDepthRemap(remapping, color);
    Require(color.depthRemap.mode == 0 && color.viewport.minDepth == 0 && color.viewport.maxDepth == 1 && color.negativeOneToOne == normalized.negativeOneToOne && color.depthClamp == normalized.depthClamp, "the depth remap acted on a normalized viewport");
    Require(color.viewport.x == decoded.viewport.x && color.viewport.y == decoded.viewport.y && color.viewport.width == decoded.viewport.width && color.viewport.height == decoded.viewport.height && color.negativeOneToOne == decoded.negativeOneToOne && color.depthClamp == decoded.depthClamp, "unused depth changed coverage or clipping");
    auto observed = decoded;
    pixel.posZ = true;
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(observed, pixel);
    Require(observed.viewport.minDepth == -1, "fragment position depth was changed");
    expectFailure([&] { AgcDriver::Graphics::ValidateViewport(context, observed.viewport); }, "viewport depth outside");
    pixel.posZ = false;
    auto attached = decoded;
    attached.depth.emplace();
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(attached, pixel);
    Require(attached.viewport.minDepth == -1, "attached depth was changed");
    expectFailure([&] { AgcDriver::Graphics::ValidateViewport(context, attached.viewport); }, "viewport depth outside");
    auto reversed = decoded;
    reversed.viewport.minDepth = 2;
    reversed.viewport.maxDepth = -1;
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(reversed, pixel);
    AgcDriver::Graphics::ValidateViewport(context, reversed.viewport);
    Require(reversed.viewport.minDepth == 1 && reversed.viewport.maxDepth == 0, "reversed unused depth is not representable");
    auto bounded = decoded;
    bounded.viewport.minDepth = 0.25f;
    bounded.viewport.maxDepth = 0.75f;
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(bounded, pixel);
    Require(bounded.viewport.minDepth == 0.25f && bounded.viewport.maxDepth == 0.75f, "an in-range viewport changed");
    auto invalid = decoded;
    invalid.viewport.minDepth = std::numeric_limits<float>::infinity();
    AgcDriver::Graphics::NormalizeUnusedViewportDepth(invalid, pixel);
    expectFailure([&] { AgcDriver::Graphics::ValidateViewport(context, invalid.viewport); }, "non-finite viewport");
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
        if (bit == 16 || bit == 19 || bit == 24 || bit == 26 || bit == 27) continue;
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
        for (const auto extraBit : {0x00010000u, 0x01000000u, 0x01010000u}) {
            queue.context[0x204] = clip | extraBit;
            const auto decoded = AgcDriver::Graphics::DecodeState(queue);
            Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected CLIP_DISABLE or DX_LINEAR_ATTR_CLIP_ENA");
            Require(decoded.negativeOneToOne == plain.negativeOneToOne && decoded.depthClamp == plain.depthClamp && decoded.viewport.minDepth == plain.viewport.minDepth && decoded.viewport.maxDepth == plain.viewport.maxDepth, "CLIP_DISABLE or DX_LINEAR_ATTR_CLIP_ENA changed the clip space, depth clamping or depth range");
        }
    }
}

void UnditheredAlphaToCoverageTests() {
    auto queue = makeState();
    queue.context[0x203] = 0x8010;
    queue.context[0x2dc] = 0xaa01;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    for (const auto samples : {1u, 8u}) {
        if (samples == 8) {
            queue.context[0x201] = 0x00133303;
            queue.context[0x2f8] = 0x0030e003;
            queue.context[0x292] = 3;
            queue.context[0x31d] = 0x1b000;
            for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
                queue.context[0x2fe + pixel * 4u] = 0xbd153fd1u;
                queue.context[0x2ff + pixel * 4u] = 0x9773f95bu;
            }
        }
        for (const auto disabled : {false, true}) {
            queue.context[0x203] = 0x8010u | (disabled ? 0x800u : 0u);
            const auto state = AgcDriver::Graphics::DecodeState(queue);
            Require(state.alphaToCoverage && !state.ditheredAlphaToCoverage && state.samples == samples, "the existing undithered alpha-to-coverage state changed");
            Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck changed the existing undithered alpha-to-coverage state");
        }
    }
}

void AlphaToCoverageTests() {
    UnditheredAlphaToCoverageTests();
    auto dithered = makeState();
    dithered.context[0x203] = 0x8010;
    dithered.context[0x201] = 0x00133303;
    dithered.context[0x2dc] = 0x18701;
    dithered.context[0x2f8] = 0x0030e003;
    dithered.context[0x292] = 3;
    dithered.context[0x31d] = 0x1b000;
    dithered.context[0x1b3] = 2;
    dithered.context[0x1b4] = 2;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        dithered.context[0x2fe + pixel * 4u] = 0xbd153fd1u;
        dithered.context[0x2ff + pixel * 4u] = 0x9773f95bu;
    }
    const auto rejection = AgcDriver::Graphics::DrawRejection(dithered, false);
    Require(rejection.empty(), "eight-sample default dither refused: " + rejection);
    const auto ditheredState = AgcDriver::Graphics::DecodeState(dithered);
    Require(ditheredState.alphaToCoverage && ditheredState.samples == 8, "default dither did not enable eight-sample coverage");
    Require(ditheredState.ditheredAlphaToCoverage, "default dither did not reach the shader state");
    for (const auto eqaa : {0x00133333u, 0x00132303u, 0x00133203u, 0x00333303u}) {
        dithered.context[0x201] = eqaa;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(dithered); }, "DB_EQAA");
        Require(AgcDriver::Graphics::DrawRejection(dithered, false).find("DB_EQAA") != std::string::npos, "the precheck accepted unmatched dithered coverage samples");
    }
    dithered.context[0x201] = 0x00133303;
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) dithered.context[0x2fe + pixel * 4u] ^= 1u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(dithered); }, "custom sample locations");
    Require(AgcDriver::Graphics::DrawRejection(dithered, false).find("custom sample locations") != std::string::npos, "the precheck accepted unmeasured dithered sample locations");
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) dithered.context[0x2fe + pixel * 4u] ^= 1u;
    for (const auto format : {3u, 5u, 6u}) {
        dithered.context[0x1c5] = format;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(dithered); }, "FP16_ABGR or 32_ABGR");
        Require(AgcDriver::Graphics::DrawRejection(dithered, false).find("FP16_ABGR or 32_ABGR") != std::string::npos, "the precheck accepted an unmeasured dithered export format");
    }
    dithered.context[0x1c5] = 4;
    Require(AgcDriver::Graphics::DecodeState(dithered).ditheredAlphaToCoverage, "FP16 default dither was refused");
    dithered.context[0x203] |= 0x800u;
    Require(!AgcDriver::Graphics::DecodeState(dithered).alphaToCoverage, "the shader disable override was ignored");
    Require(AgcDriver::Graphics::DrawRejection(dithered, false).empty(), "the precheck ignored the shader disable override");
    dithered.context[0x2dc] |= 2u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(dithered); }, "alpha-to-coverage");
    auto queue = makeState();
    queue.context[0x203] = 0;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    Require(!AgcDriver::Graphics::DecodeState(queue).alphaToCoverage, "alpha-to-coverage decoded while disabled");
    queue.context[0x2dc] = 0xaa01;
    Require(AgcDriver::Graphics::DecodeState(queue).alphaToCoverage, "undithered alpha-to-coverage was not decoded");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected undithered alpha-to-coverage");
    queue.context[0x203] |= 0x800u;
    Require(AgcDriver::Graphics::DecodeState(queue).alphaToCoverage, "the existing undithered single-sample state changed under the shader disable override");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck changed the existing undithered single-sample state under the shader disable override");
    queue.context[0x203] &= ~0x800u;
    for (const auto control : {0x8701u, 0x1aa01u, 0xaa03u}) {
        queue.context[0x2dc] = control;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "alpha-to-coverage");
        Require(AgcDriver::Graphics::DrawRejection(queue, false).find("alpha-to-coverage") != std::string::npos, "the precheck accepted unsupported alpha-to-coverage");
    }
    queue.context[0x2dc] = 0xaa01;
    const std::uint32_t exportFormat = queue.context.at(0x1c5);
    for (const auto format : {3u, 4u, 5u, 6u, 9u}) {
        queue.context[0x1c5] = (exportFormat & ~0xfu) | format;
        Require(AgcDriver::Graphics::DecodeState(queue).alphaToCoverage, "alpha-to-coverage rejected an MRT0 export format with a floating-point alpha");
    }
    for (const auto format : {1u, 2u}) {
        queue.context[0x1c5] = (exportFormat & ~0xfu) | format;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "floating-point alpha");
    }
    const auto targetMask = queue.context.at(0x8e);
    queue.context[0x8e] = 0;
    queue.context[0x1c5] = (exportFormat & ~0xfu) | 3u;
    const auto depthOnly = AgcDriver::Graphics::DecodeState(queue);
    Require(depthOnly.alphaToCoverage && depthOnly.colors.empty(), "alpha-to-coverage without a written color target and a 32_AR MRT0 export was refused or kept a target");
    Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected alpha-to-coverage without a written color target");
    queue.context[0x1c5] = exportFormat & ~0xfu;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(queue); }, "floating-point alpha");
    queue.context[0x8e] = targetMask;
    queue.context[0x1c5] = exportFormat;
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
    queue.context[0x203] &= ~0x800u;
    queue.context[0x2dc] |= 1;
    Require(AgcDriver::Graphics::DecodeState(queue).alphaToCoverage, "undithered alpha-to-coverage was not decoded");
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
    std::vector<VkDescriptorImageInfo> images;
};

struct LiveCounter {
    std::atomic<std::int64_t> value{0};
    LiveCounter() = default;
    LiveCounter(const LiveCounter& other) : value(other.value.load()) {}
    LiveCounter& operator=(const LiveCounter& other) {
        value.store(other.value.load());
        return *this;
    }
    operator std::int64_t() const { return value.load(); }
    LiveCounter& operator++() {
        ++value;
        return *this;
    }
    LiveCounter& operator--() {
        --value;
        return *this;
    }
    LiveCounter& operator+=(std::int64_t count) {
        value += count;
        return *this;
    }
    LiveCounter& operator-=(std::int64_t count) {
        value -= count;
        return *this;
    }
};

struct MockVulkan {
    std::uint64_t next = 1;
    LiveCounter live;
    bool deferFences = false;
    std::map<VkFence, bool> signaled;
    std::uint64_t fenceWaits = 0;
    struct BufferCopy { VkBuffer source; VkBuffer destination; VkBufferCopy region; };
    std::vector<BufferCopy> bufferCopies;
    struct ImageBufferCopy { VkImage image; VkBuffer buffer; VkBufferImageCopy region; bool fromImage; };
    std::vector<ImageBufferCopy> imageBufferCopies;
    std::set<VkImage> destroyedImages;
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
    std::optional<VkDeviceSize> importLimit;
    std::uint32_t bufferMemoryTypeBits = 1;
    std::vector<std::uint32_t> allocationTypes;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    VkDescriptorSetLayoutCreateFlags layoutFlags = 0;
    std::vector<VkDescriptorBindingFlags> layoutBindingFlags;
    std::map<VkDescriptorSetLayout, std::uint32_t> variableLayoutCounts;
    std::vector<VkDescriptorPoolSize> poolSizes;
    std::uint32_t poolMaxSets = 0;
    VkDescriptorPoolCreateFlags poolFlags = 0;
    std::uint32_t freedDescriptorSets = 0;
    struct DescriptorPool {
        VkDescriptorPoolCreateFlags flags;
        std::uint32_t maxSets;
        std::uint32_t allocated = 0;
        std::uint32_t attempts = 0;
        std::uint32_t resets = 0;
    };
    std::map<VkDescriptorPool, DescriptorPool> descriptorPools;
    std::uint32_t descriptorPoolLimit = 1000000;
    VkResult descriptorResetResult = VK_SUCCESS;
    std::optional<std::uint32_t> variableDescriptorCount;
    std::vector<MockDescriptorWrite> writes;
    std::map<VkSampler, VkSamplerCreateFlags> samplerFlags;
    std::uint32_t samplerCreateCount = 0;
    std::vector<VkCopyDescriptorSet> copies;
    bool allowCopies = false;
    std::uint32_t boundSets = 0;
    std::uint32_t boundFirst = 0;
    VkPipelineBindPoint boundPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    std::map<VkPipelineLayout, VkDeviceSize> pipelineLayoutPushConstantSize;
    std::map<VkPipelineLayout, std::vector<VkDescriptorSetLayout>> pipelineLayoutSets;
    std::map<VkPipelineLayout, VkPipelineLayoutCreateFlags> pipelineLayoutFlags;
    std::map<VkPipelineLayout, std::vector<VkPushConstantRange>> pipelineLayoutPushRanges;
    std::map<VkGraphicsPipelineLibraryFlagsEXT, VkPipelineLayout> libraryLayouts;
    std::uint32_t pipelineCreateCount = 0;
    std::vector<std::array<std::uint32_t, 3>> pipelineSpecializations;
    VkPipeline boundPipeline = VK_NULL_HANDLE;
    std::vector<std::byte> lastPushConstants;
    struct { std::uint32_t x = 0, y = 0, z = 0; } lastDispatchGroups;
};

MockVulkan mock;
std::mutex pipelineCreateMutex;
std::condition_variable pipelineCreateCondition;
bool blockLibraryCreate = false;
bool libraryCreateEntered = false;
bool releaseLibraryCreate = false;

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
    *requirements = {mock.bufferSizes.at(buffer), 1, mock.bufferMemoryTypeBits};
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    ++mock.allocationAttempts;
    mock.allocationTypes.push_back(info->memoryTypeIndex);
    if (mock.memoryLimit.has_value() && mock.allocatedBytes + info->allocationSize > *mock.memoryLimit) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if (mock.importLimit.has_value() && info->allocationSize > *mock.importLimit && info->pNext != nullptr && static_cast<const VkMemoryAllocateFlagsInfo*>(info->pNext)->pNext != nullptr) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
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

VKAPI_ATTR VkResult VKAPI_CALL mockCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage* image) { *image = makeHandle<VkImage>(); ++mock.live; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements) { *requirements = {4096, 1, 1}; }
VKAPI_ATTR VkResult VKAPI_CALL mockBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockDestroyImage(VkDevice, VkImage image, const VkAllocationCallbacks*) { mock.destroyedImages.insert(image); --mock.live; }
VKAPI_ATTR VkResult VKAPI_CALL mockCreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view) { *view = makeHandle<VkImageView>(); ++mock.live; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockDestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*) { --mock.live; }
VKAPI_ATTR void VKAPI_CALL mockFormatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyImageToBuffer(VkCommandBuffer, VkImage image, VkImageLayout, VkBuffer buffer, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) mock.imageBufferCopies.push_back({image, buffer, regions[index], true});
}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyBufferToImage(VkCommandBuffer, VkBuffer buffer, VkImage image, VkImageLayout, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) mock.imageBufferCopies.push_back({image, buffer, regions[index], false});
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo* info, const VkAllocationCallbacks*, VkDescriptorSetLayout* layout) {
    *layout = makeHandle<VkDescriptorSetLayout>();
    mock.layoutBindings.assign(info->pBindings, info->pBindings + info->bindingCount);
    mock.layoutFlags = info->flags;
    const auto* bindingFlags = static_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(info->pNext);
    if (bindingFlags != nullptr) mock.layoutBindingFlags.assign(bindingFlags->pBindingFlags, bindingFlags->pBindingFlags + bindingFlags->bindingCount);
    else mock.layoutBindingFlags.clear();
    for (std::size_t index = 0; index < mock.layoutBindingFlags.size(); ++index) {
        if ((mock.layoutBindingFlags[index] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0) mock.variableLayoutCounts[*layout] = info->pBindings[index].descriptorCount;
    }
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout layout, const VkAllocationCallbacks*) {
    mock.variableLayoutCounts.erase(layout);
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo* info, const VkAllocationCallbacks*, VkDescriptorPool* pool) {
    *pool = makeHandle<VkDescriptorPool>();
    mock.poolSizes.assign(info->pPoolSizes, info->pPoolSizes + info->poolSizeCount);
    mock.poolMaxSets = info->maxSets;
    mock.poolFlags = info->flags;
    mock.descriptorPools.emplace(*pool, MockVulkan::DescriptorPool{info->flags, std::min(info->maxSets, mock.descriptorPoolLimit)});
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyDescriptorPool(VkDevice, VkDescriptorPool pool, const VkAllocationCallbacks*) {
    mock.descriptorPools.erase(pool);
    --mock.live;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo* info, VkDescriptorSet* sets) {
    Require(info->descriptorSetCount == 1, "exactly one descriptor set must be allocated");
    auto& pool = mock.descriptorPools.at(info->descriptorPool);
    ++pool.attempts;
    if (pool.allocated == pool.maxSets) return VK_ERROR_OUT_OF_POOL_MEMORY;
    ++pool.allocated;
    mock.variableDescriptorCount.reset();
    if (info->pNext != nullptr) {
        const auto* variable = static_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo*>(info->pNext);
        Require(variable->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO && variable->descriptorSetCount == 1, "variable descriptor allocation is malformed");
        mock.variableDescriptorCount = variable->pDescriptorCounts[0];
        Require(*mock.variableDescriptorCount <= mock.variableLayoutCounts.at(info->pSetLayouts[0]), "variable descriptor count exceeds its layout binding");
    }
    sets[0] = makeHandle<VkDescriptorSet>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetDescriptorPool(VkDevice, VkDescriptorPool handle, VkDescriptorPoolResetFlags) {
    auto& pool = mock.descriptorPools.at(handle);
    const auto result = mock.descriptorResetResult;
    mock.descriptorResetResult = VK_SUCCESS;
    if (result != VK_SUCCESS) return result;
    pool.allocated = 0;
    ++pool.resets;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockFreeDescriptorSets(VkDevice, VkDescriptorPool handle, std::uint32_t count, const VkDescriptorSet*) {
    auto& pool = mock.descriptorPools.at(handle);
    Require((pool.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0, "a descriptor set was freed from a pool without free support");
    Require(count <= pool.allocated, "more descriptor sets were freed than allocated");
    pool.allocated -= count;
    mock.freedDescriptorSets += count;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateSampler(VkDevice, const VkSamplerCreateInfo* info, const VkAllocationCallbacks*, VkSampler* sampler) {
    *sampler = makeHandle<VkSampler>();
    mock.samplerFlags[*sampler] = info->flags;
    ++mock.samplerCreateCount;
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroySampler(VkDevice, VkSampler sampler, const VkAllocationCallbacks*) {
    mock.samplerFlags.erase(sampler);
    --mock.live;
}

VKAPI_ATTR void VKAPI_CALL mockUpdateDescriptorSets(VkDevice, std::uint32_t count, const VkWriteDescriptorSet* writes, std::uint32_t copyCount, const VkCopyDescriptorSet* copies) {
    Require(copyCount == 0 || mock.allowCopies, "descriptor copies are not expected");
    if (copyCount != 0) mock.copies.insert(mock.copies.end(), copies, copies + copyCount);
    for (std::uint32_t i = 0; i < count; ++i) {
        MockDescriptorWrite write{writes[i].dstBinding, writes[i].dstArrayElement, writes[i].descriptorCount, writes[i].descriptorType, {}, {}};
        if (writes[i].pBufferInfo != nullptr) write.buffers.assign(writes[i].pBufferInfo, writes[i].pBufferInfo + writes[i].descriptorCount);
        if (writes[i].pImageInfo != nullptr) write.images.assign(writes[i].pImageInfo, writes[i].pImageInfo + writes[i].descriptorCount);
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
    if (info->setLayoutCount != 0) mock.pipelineLayoutSets[*layout].assign(info->pSetLayouts, info->pSetLayouts + info->setLayoutCount);
    mock.pipelineLayoutFlags[*layout] = info->flags;
    if (info->pushConstantRangeCount != 0) mock.pipelineLayoutPushRanges[*layout].assign(info->pPushConstantRanges, info->pPushConstantRanges + info->pushConstantRangeCount);
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateGraphicsPipelines(VkDevice, VkPipelineCache, std::uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks*, VkPipeline* pipelines) {
    {
        std::unique_lock lock(pipelineCreateMutex);
        if (blockLibraryCreate) {
            libraryCreateEntered = true;
            pipelineCreateCondition.notify_all();
            pipelineCreateCondition.wait(lock, [] { return releaseLibraryCreate; });
        }
    }
    Require(count == 1, "mock expects exactly one graphics pipeline per call");
    if ((infos[0].flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) == 0) {
        const auto* linked = static_cast<const VkPipelineLibraryCreateInfoKHR*>(infos[0].pNext);
        Require(linked != nullptr && linked->sType == VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR && linked->libraryCount != 0, "mock expects a graphics pipeline library or a pipeline linked from libraries");
        *pipelines = makeHandle<VkPipeline>();
        ++mock.pipelineCreateCount;
        ++mock.live;
        return VK_SUCCESS;
    }
    const auto* library = static_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(infos[0].pNext);
    Require(library != nullptr && library->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, "missing graphics pipeline library description");
    mock.libraryLayouts[library->flags] = infos[0].layout;
    *pipelines = makeHandle<VkPipeline>();
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

VKAPI_ATTR void VKAPI_CALL mockDestroyShaderModule(VkDevice, VkShaderModule module, const VkAllocationCallbacks*) {
    if (module != VK_NULL_HANDLE) --mock.live;
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
VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) { *fence = makeHandle<VkFence>(); mock.signaled[*fence] = !mock.deferFences; ++mock.live; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) { mock.signaled.erase(fence); --mock.live; }
VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) { return mock.signaled.at(fence) ? VK_SUCCESS : VK_NOT_READY; }
VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) { for (std::uint32_t index = 0; index < count; ++index) mock.signaled.at(fences[index]) = !mock.deferFences; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) { for (std::uint32_t index = 0; index < count; ++index) mock.signaled.at(fences[index]) = true; ++mock.fenceWaits; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockCmdCopyBuffer(VkCommandBuffer, VkBuffer source, VkBuffer destination, std::uint32_t count, const VkBufferCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) mock.bufferCopies.push_back({source, destination, regions[index]});
}
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
        {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyBuffer)},
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
        {"vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(mockResetDescriptorPool)},
        {"vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(mockCreateSampler)},
        {"vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(mockDestroySampler)},
        {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockUpdateDescriptorSets)},
        {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindDescriptorSets)},
        {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(mockCreateGraphicsPipelines)},
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

PFN_vkVoidFunction VKAPI_CALL mockDepthCopyProc(VkDevice device, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(mockCreateImage)},
        {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetImageMemoryRequirements)},
        {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindImageMemory)},
        {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyImage)},
        {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(mockCreateImageView)},
        {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyImageView)},
        {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyImageToBuffer)},
        {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyBufferToImage)},
    };
    const auto found = table.find(name);
    return found != table.end() ? found->second : mockProc(device, name);
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
    context.limits.maxImageArrayLayers = 2048;
    return context;
}

struct CommandBatchFailureProbe {
    VkResult allocation = VK_SUCCESS;
    VkResult creation = VK_SUCCESS;
    VkResult begin = VK_SUCCESS;
    VkFence failedFence = VK_NULL_HANDLE;
    std::uint32_t freed = 0;
    std::uint32_t creates = 0;
    std::uint32_t destroys = 0;
    std::uint32_t invalidDestroys = 0;
    std::uint32_t begins = 0;
};

CommandBatchFailureProbe batchProbe;

VKAPI_ATTR VkResult VKAPI_CALL batchAllocateCommands(VkDevice device, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* commands) {
    if (batchProbe.allocation != VK_SUCCESS) {
        std::fill_n(commands, info->commandBufferCount, VK_NULL_HANDLE);
        return batchProbe.allocation;
    }
    return mockAllocateCommandBuffers(device, info, commands);
}

VKAPI_ATTR VkResult VKAPI_CALL batchCreateFence(VkDevice device, const VkFenceCreateInfo* info, const VkAllocationCallbacks* allocator, VkFence* fence) {
    ++batchProbe.creates;
    if (batchProbe.creation != VK_SUCCESS) {
        *fence = batchProbe.failedFence;
        return batchProbe.creation;
    }
    return mockCreateFence(device, info, allocator, fence);
}

VKAPI_ATTR void VKAPI_CALL batchFreeCommands(VkDevice device, VkCommandPool pool, std::uint32_t count, const VkCommandBuffer* commands) {
    batchProbe.freed += count;
    mockFreeCommandBuffers(device, pool, count, commands);
}

VKAPI_ATTR void VKAPI_CALL batchDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator) {
    ++batchProbe.destroys;
    if (fence != VK_NULL_HANDLE && fence == batchProbe.failedFence) {
        ++batchProbe.invalidDestroys;
        return;
    }
    mockDestroyFence(device, fence, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL batchBeginCommands(VkCommandBuffer commands, const VkCommandBufferBeginInfo* info) {
    ++batchProbe.begins;
    if (batchProbe.begin != VK_SUCCESS) return batchProbe.begin;
    return mockBeginCommandBuffer(commands, info);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL batchFailureProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkAllocateCommandBuffers") == 0) return reinterpret_cast<PFN_vkVoidFunction>(batchAllocateCommands);
    if (std::strcmp(name, "vkCreateFence") == 0) return reinterpret_cast<PFN_vkVoidFunction>(batchCreateFence);
    if (std::strcmp(name, "vkFreeCommandBuffers") == 0) return reinterpret_cast<PFN_vkVoidFunction>(batchFreeCommands);
    if (std::strcmp(name, "vkDestroyFence") == 0) return reinterpret_cast<PFN_vkVoidFunction>(batchDestroyFence);
    if (std::strcmp(name, "vkBeginCommandBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(batchBeginCommands);
    return mockProc(device, name);
}

void commandBatchFenceFailureTests() {
    using namespace AgcDriver::Graphics;
    const std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    std::uint32_t invalidDestroys = 0;
    for (const auto result : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
        for (const bool nonnull : {false, true}) {
            mock = MockVulkan{};
            batchProbe = {};
            batchProbe.creation = result;
            batchProbe.failedFence = nonnull ? reinterpret_cast<VkFence>(std::uintptr_t{0xdeadbeef}) : VK_NULL_HANDLE;
            auto context = mockContext();
            context.deviceProc = batchFailureProc;
            bool failed = false;
            try {
                CommandBatch batch(context);
            } catch (const std::runtime_error& error) {
                const std::string message = error.what();
                failed = message.find("vkCreateFence") != std::string::npos && message.find("Vulkan result " + std::to_string(result)) != std::string::npos;
            }
            Require(failed && batchProbe.creates == 1 && batchProbe.begins == 0, "fence creation failure lost its error or continued construction");
            Require(mock.live == 0 && batchProbe.freed == 1, "fence creation failure leaked the allocated command buffer");
            invalidDestroys += batchProbe.invalidDestroys;
            std::printf("command batch fence result=%d nonnull=%d invalid destroys=%u\n", result, nonnull, batchProbe.invalidDestroys);
            const auto destroyed = batchProbe.destroys;
            const auto invalid = batchProbe.invalidDestroys;
            batchProbe.creation = VK_SUCCESS;
            {
                CommandBatch recovered(context);
                Require(recovered.Handle() != VK_NULL_HANDLE, "command batch did not recover after fence creation failed");
            }
            Require(mock.live == 0 && batchProbe.freed == 2 && batchProbe.destroys == destroyed + 1 && batchProbe.invalidDestroys == invalid, "recovered command batch mishandled its owned objects");
        }
    }
    for (const bool allocated : {false, true}) {
        mock = MockVulkan{};
        batchProbe = {};
        batchProbe.allocation = allocated ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
        batchProbe.begin = VK_ERROR_OUT_OF_HOST_MEMORY;
        auto context = mockContext();
        context.deviceProc = batchFailureProc;
        bool failed = false;
        try {
            CommandBatch batch(context);
        } catch (const std::runtime_error& error) {
            failed = std::string(error.what()).find(allocated ? "vkBeginCommandBuffer" : "vkAllocateCommandBuffers") != std::string::npos;
        }
        Require(failed && mock.live == 0 && batchProbe.freed == (allocated ? 1u : 0u) && batchProbe.destroys == (allocated ? 1u : 0u) && batchProbe.invalidDestroys == 0, "command batch constructor failure mishandled its owned objects");
    }
    Require(invalidDestroys == 0, "command batch destroyed undefined failed fence outputs");
}

void addressSpaceTeardownTests() {
#ifdef _WIN32
    _putenv_s("APS5_NO_LEASE_MIRROR", "1");
#else
    setenv("APS5_NO_LEASE_MIRROR", "1", 1);
#endif
    auto context = mockContext();
    context.device = makeHandle<VkDevice>();
    context.bufferDeviceAddress = true;
    context.limits.maxStorageBufferRange = 1u << 27;
    void* block = GuestHeap::GuestHeapAlignForGpu_nid_postfix(16, 64);
    const auto before = AgcDriver::Graphics::LeaseCounters().cacheDrops;
    {
        AgcDriver::Graphics::GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        Require(memory.HoldsLease(), "address-based build holds no lease before device teardown");
        memory.Upload(true);
        memory.WriteBack();
        Require(!memory.HoldsLease(), "completed address-based build kept its own lease");
    }
    AgcDriver::Graphics::ClearImageMirrors(context.device);
    GuestHeap::GuestHeapFree_nid_postfix(block);
    Require(AgcDriver::Graphics::LeaseCounters().cacheDrops == before, "device teardown left the cached address space pinning guest allocations");
    Require(mock.live == 0, "address-space teardown test leaked Vulkan objects");
}

using Role = ShaderRecompiler::DescriptorRole;

void bufferBudgetPlacementTests() {
    using namespace AgcDriver::Graphics;
    for (const std::size_t bytes : {std::size_t{512}, std::size_t{3} << 20u}) {
        for (const bool fail : {false, true}) {
            mock = MockVulkan{};
            mock.bufferMemoryTypeBits = 3;
            if (fail) mock.memoryLimit = 0;
            {
                auto context = mockContext();
                constexpr auto coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                context.memory.memoryTypeCount = 2;
                context.memory.memoryHeapCount = 2;
                context.memory.memoryHeaps[0] = {4u << 20u, 0};
                context.memory.memoryHeaps[1] = {8u << 20u, 0};
                context.memory.memoryTypes[0] = {coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0};
                context.memory.memoryTypes[1] = {coherent, 1};
                context.physical = reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{1});
                context.memoryProperties2 = [](VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
                    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
                    reported->heapBudget[0] = 1u << 20u;
                    reported->heapUsage[0] = reported->heapBudget[0] - 1024u;
                    reported->heapBudget[1] = 8u << 20u;
                };
                const auto allocate = [&] {
                    Buffer buffer(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                    Require(buffer.Mapped() && buffer.Bytes().size() == bytes, "a budget-selected buffer lost its mapping or size");
                    buffer.Bytes().front() = std::byte{0x5a};
                    Require(mockBufferMemory(buffer.Handle()).front() == std::byte{0x5a}, "a budget-selected buffer bound the wrong allocation");
                };
                if (fail) expectFailure(allocate, "Vulkan result -2");
                else allocate();
                Require(mock.allocationTypes == std::vector<std::uint32_t>{1} && mock.allocationAttempts == 1, "a coherent buffer did not select the other heap before its single allocation attempt");
                if (!fail) Require(mock.allocatedBytes == (bytes == 512 ? BufferPool::SlabBlockBytes(512) : bytes), "a coherent buffer budget check lost its full allocation extent");
            }
            Require(mock.live == 0, "a budget-selected buffer leaked Vulkan objects");
        }
    }
}

void copyInputKeptBytesTests() {
    using namespace AgcDriver::Graphics;
    constexpr std::size_t piece = 65536;
    constexpr std::size_t bytes = 32;
    constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto block = std::unique_ptr<std::byte[], void(*)(std::byte*)>(static_cast<std::byte*>(::operator new(piece, std::align_val_t{piece})), [](std::byte* value) { ::operator delete(value, std::align_val_t{piece}); });
    std::fill_n(block.get(), piece, std::byte{0});
    const auto base = reinterpret_cast<std::uint64_t>(block.get());
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block.get(), piece, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block.get());
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    mock = MockVulkan{};
    mock.deferFences = true;
    {
        auto context = mockContext();
        context.device = makeHandle<VkDevice>();
        context.bufferDeviceAddress = true;
        context.hostImportAlignment = piece;
        context.limits.minStorageBufferOffsetAlignment = 256;
        context.limits.maxMemoryAllocationCount = 4096;
        SetImportWatch(context, ImportWatch::Unwatch);
        const auto pool = GetBufferPool(context);
        registry(true);
        const auto unregister = [&](std::byte*) { registry(false); };
        std::unique_ptr<std::byte, decltype(unregister)> registered(block.get(), unregister);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Recorder recorder(context);
            recorder.Activate();
            {
                GuestBufferMemory aligned(context);
                aligned.AddReadable(base + 256, bytes);
                aligned.Upload(false);
                Require(mock.bufferCopies.empty() && recorder.OpenKeptBytes() == 0, "an aligned imported view made or charged an input snapshot");
                aligned.WriteBack();
            }
            std::vector<VkBuffer> snapshots;
            for (unsigned batch = 0; batch < 3; ++batch) {
                recorder.Keep(std::make_shared<int>(0), Recorder::KeptBytesBudget - bytes);
                block[4] = static_cast<std::byte>(batch + 1);
                {
                    GuestBufferMemory memory(context);
                    memory.AddReadable(base + 4, bytes);
                    memory.Upload(false);
                    Require(mock.bufferCopies.size() == batch + 1, "a misaligned imported view did not record exactly one GPU copy");
                    const auto& copy = mock.bufferCopies.back();
                    const auto* imported = HostImportFor(context, base + 4, bytes);
                    Require(imported != nullptr && copy.source != imported->buffer && copy.region.srcOffset == 0 && copy.region.size == bytes, "the read-only GPU copy did not use its own exact input snapshot");
                    snapshots.push_back(copy.source);
                    Require(mockBufferMemory(copy.source).front() == static_cast<std::byte>(batch + 1), "the input snapshot missed the guest bytes at record time");
                    block[4] = std::byte{0x7f};
                    Require(mockBufferMemory(copy.source).front() == static_cast<std::byte>(batch + 1), "a later CPU store changed the recorded input snapshot");
                    memory.WriteBack();
                }
                Require(recorder.OpenKeptBytes() == Recorder::KeptBytesBudget, "the actual GPU-copy input snapshot did not complete the kept-byte budget");
                Require(!pool->Take(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties), "an input snapshot became reusable before its GPU fence");
                recorder.BoundKeptBytes();
                Require(recorder.Submissions() == batch + 1 && !recorder.Recording(), "the copy input charge did not submit a full budget batch");
                if (batch < 2) {
                    Require(!recorder.Reap() && mock.fenceWaits == 0, "a pending copy batch was reaped or waited before exceeding two budgets");
                    Require(recorder.InFlightKeptBytes() == (batch + 1) * Recorder::KeptBytesBudget, "submitted copy input bytes were lost from the in-flight count");
                }
            }
            Require(mock.fenceWaits == 1 && recorder.InFlightBatches() == 2 && recorder.InFlightKeptBytes() == 2 * Recorder::KeptBytesBudget, "three copy input batches did not retire just the oldest budget");
            const auto released = pool->Take(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties);
            Require(released.has_value() && released->buffer == snapshots.front(), "the completed batch did not release its input snapshot for reuse");
            Require(!pool->Take(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties), "an unfinished copy batch released its input snapshot");
            pool->Put(*released);
            recorder.Sync();
            Require(recorder.InFlightKeptBytes() == 0, "completed copy inputs still count as in flight");
        }
        registered.reset();
        ClearHostImports(context.device);
    }
    Require(mock.live == 0, "copy input accounting tests leaked Vulkan objects");
}

void depthCopyKeptBytesTests() {
    using namespace AgcDriver::Graphics;
    constexpr std::size_t bytes = (8 * 8 + 4 * 4) * sizeof(float);
    constexpr auto properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const auto usage = BufferPool::Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, properties);
    alignas(256) std::array<std::byte, 16384> guest{};
    mock = MockVulkan{};
    {
        auto sourceContext = mockContext();
        sourceContext.device = makeHandle<VkDevice>();
        sourceContext.deviceProc = mockDepthCopyProc;
        sourceContext.formatProperties = mockFormatProperties;
        sourceContext.memory.memoryTypes[0].propertyFlags |= properties;
        sourceContext.memory.memoryHeapCount = 1;
        sourceContext.memory.memoryHeaps[0] = {std::uint64_t{4} << 30u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
        sourceContext.limits.maxMemoryAllocationCount = 4096;
        GetBufferPool(sourceContext);
        TextureDetiler detiler(sourceContext);
        GuestTextureResource resource{};
        resource.baseAddress = reinterpret_cast<std::uint64_t>(guest.data());
        resource.width = 8;
        resource.height = 8;
        resource.mipCount = 2;
        resource.lastLevel = 1;
        resource.tileMode = TextureTileMode::kLinear;
        resource.dimension = TextureDimension::k2D;
        resource.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32Float);
        auto source = StorageTexture::Create(sourceContext, detiler, resource, 0);
        const auto sourceImage = source->Image();
        std::weak_ptr<StorageTexture> sourceLifetime = source;
        auto context = sourceContext;
        context.bufferPool = std::make_shared<BufferPool>(context);
        const auto pool = context.bufferPool;
        mock.bufferCopies.clear();
        mock.imageBufferCopies.clear();
        mock.fenceWaits = 0;
        mock.deferFences = true;
        std::vector<VkBuffer> linears;
        std::vector<VkImage> depths;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Recorder recorder(context);
            recorder.Activate();
            for (unsigned batch = 0; batch < 3; ++batch) {
                recorder.Keep(std::make_shared<int>(0), Recorder::KeptBytesBudget - bytes);
                {
                    Texture depth(context, source, resource, {}, Texture::DepthCopyTag{});
                    Require(depth.RefreshDepthCopy(), "a fresh depth copy did not record its linear transfer");
                    const auto begin = batch * 4;
                    Require(mock.imageBufferCopies.size() == begin + 4, "a depth refresh did not copy both mips through its linear buffer");
                    const auto& first = mock.imageBufferCopies[begin];
                    linears.push_back(first.buffer);
                    depths.push_back(mock.imageBufferCopies[begin + 2].image);
                    for (unsigned mip = 0; mip < 2; ++mip) {
                        const auto& from = mock.imageBufferCopies[begin + mip];
                        const auto& to = mock.imageBufferCopies[begin + 2 + mip];
                        Require(from.fromImage && !to.fromImage && from.image == sourceImage && to.image == depths.back() && from.buffer == linears.back() && to.buffer == linears.back(), "the depth refresh did not retain one linear buffer between its image transfers");
                        Require(from.region.imageSubresource.mipLevel == mip && to.region.imageSubresource.mipLevel == mip && from.region.imageSubresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT && to.region.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT && from.region.bufferOffset == (mip == 0 ? 0 : 8 * 8 * sizeof(float)) && to.region.bufferOffset == from.region.bufferOffset, "the depth refresh's mip offsets or aspects changed");
                    }
                    Require(!depth.RefreshDepthCopy() && mock.imageBufferCopies.size() == begin + 4, "an unchanged storage version made another linear depth copy");
                }
                Require(recorder.OpenKeptBytes() == Recorder::KeptBytesBudget, "the real depth-copy linear buffer did not complete the kept-byte budget");
                Require(!pool->Take(bytes, usage, properties) && !mock.destroyedImages.contains(depths.back()), "a recorded depth transfer released its linear buffer or destination before its fence");
                recorder.BoundKeptBytes();
                Require(recorder.Submissions() == batch + 1 && !recorder.Recording(), "the depth-copy charge did not submit a full budget batch");
                if (batch < 2) Require(!recorder.Reap() && mock.fenceWaits == 0, "a pending depth-copy batch was reaped or waited too soon");
            }
            source.reset();
            Require(!sourceLifetime.expired() && !mock.destroyedImages.contains(sourceImage), "a pending depth copy released its storage source");
            Require(mock.fenceWaits == 1 && recorder.InFlightBatches() == 2 && recorder.InFlightKeptBytes() == 2 * Recorder::KeptBytesBudget, "three depth-copy budgets did not retire just the oldest batch");
            const auto released = pool->Take(bytes, usage, properties);
            Require(released.has_value() && released->buffer == linears.front() && mock.destroyedImages.contains(depths.front()), "the completed depth-copy batch did not release its linear buffer and destination");
            Require(!pool->Take(bytes, usage, properties) && !mock.destroyedImages.contains(depths[1]) && !mock.destroyedImages.contains(depths[2]), "an unfinished depth-copy batch released its resources");
            pool->Put(*released);
            recorder.Sync();
            Require(recorder.InFlightKeptBytes() == 0, "completed depth-copy buffers still count as in flight");
        }
        Require(sourceLifetime.expired() && mock.destroyedImages.contains(sourceImage) && mock.destroyedImages.contains(depths[1]) && mock.destroyedImages.contains(depths[2]), "the completed depth-copy batches kept their source or destinations after teardown");
    }
    Require(mock.live == 0, "depth copy accounting tests leaked Vulkan objects");
}
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
    auto context = mockContext();
    context.emptyBuffer = reinterpret_cast<VkBuffer>(std::uintptr_t{0x1000});
    const auto color = AgcDriver::Graphics::DecodeState(makeState()).color;
    { AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, color, 0, 0); }
    Require(mock.live == 0, std::string(what) + " leaked Vulkan objects");
}

void oversizedDescriptorTests() {
    constexpr std::size_t blockBytes = 65536;
    constexpr std::size_t tailBytes = 8192;
    void* block = ::operator new(blockBytes, std::align_val_t{blockBytes});
    std::memset(block, 0x5a, blockBytes);
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, blockBytes, true, true, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    const auto oversized = [](const void* pointer) { return makeBinding(Role::GuestBuffers, 0, 1, vsharp(pointer, 0xffffffffu)); };
    const auto* tail = static_cast<const std::byte*>(block) + blockBytes - tailBytes;
    {
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        vertex.bindings.push_back(oversized(tail));
        mock = MockVulkan{};
        const auto context = mockContext();
        const auto color = AgcDriver::Graphics::DecodeState(makeState()).color;
        {
            AgcDriver::Graphics::ShaderResources resources(context, vertex, fragment, color, 0, 0);
            const auto& write = findWrite(0);
            Require(write.buffers.size() == 1 && write.buffers[0].range == tailBytes && sameBytes(bufferBytes(write.buffers[0].buffer), tail, tailBytes), "a V# past the range limit was not bound up to the end of its registered allocation");
        }
        Require(mock.live == 0, "a V# bound up to its registered allocation leaked Vulkan objects");
    }
    expectSingleFailure(oversized(block), "descriptor range limit");
    expectSingleAccepted(oversized(reinterpret_cast<const void*>(0x1000)), "an unregistered V# past the range limit");
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    ::operator delete(block, std::align_val_t{blockBytes});
}

void guestBufferViewBytesTests() {
    using AgcDriver::Graphics::GuestBufferMemory;
    for (const std::uint64_t bytes : {2ull, 3ull, 4ull, 6ull, 7ull, 16ull, 4096ull, 4098ull}) {
        for (const std::uint32_t adjustment : {0u, 4u, 16u, 256u}) {
            Require(GuestBufferMemory::ViewBytes(bytes, adjustment) == bytes + adjustment, "a view whose adjustment is a whole number of DWORDs did not add the adjustment");
        }
        for (const std::uint32_t adjustment : {1u, 2u, 3u, 5u, 7u}) {
            const auto expected = 4 * ((adjustment + bytes + 3) / 4);
            Require(GuestBufferMemory::ViewBytes(bytes, adjustment) == expected, "an off-DWORD view did not cover every byte in its range");
        }
    }
}

void descriptorLayoutFlagTests() {
    constexpr VkDescriptorBindingFlags indexed = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    mock = MockVulkan{};
    {
        AgcDriver::Graphics::DescriptorCache cache(mockContext());
        const VkDescriptorSetLayoutBinding binding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        const std::array<std::uint32_t, 4> key{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, VK_SHADER_STAGE_COMPUTE_BIT};
        const std::array<VkDescriptorBindingFlags, 1> flags{indexed};
        const auto layout = cache.Layout(key, std::span(&binding, 1), false, flags);
        Require(mock.layoutFlags == 0 && mock.layoutBindingFlags == std::vector<VkDescriptorBindingFlags>{indexed}, "native layout lost binding flags without update-after-bind");
        Require(cache.Layout(key, std::span(&binding, 1), false, flags) == layout, "identical binding flags missed the layout cache");
        Require(cache.Layout(key, std::span(&binding, 1)) != layout && mock.layoutBindingFlags.empty(), "layout cache ignored binding flags");
    }
    Require(mock.live == 0, "binding flag layout cache leaked Vulkan objects");
    for (const bool updateAfterBind : {false, true}) {
        mock = MockVulkan{};
        auto context = mockContext();
        context.emptyBuffer = reinterpret_cast<VkBuffer>(std::uintptr_t{0x1000});
        context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 17;
        context.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 128;
        context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 32;
        {
            ShaderRecompiler::RecompileResult program;
            const std::uint32_t count = updateAfterBind ? 17 : 1;
            program.bindings.push_back(makeBinding(Role::GuestBuffers, 0, count, std::vector<std::uint32_t>(4 * count)));
            const AgcDriver::Graphics::CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
            AgcDriver::Graphics::ShaderResources resources(context, compute);
            const auto flags = resources.LayoutBindingFlags();
            Require(flags.size() == resources.LayoutBindings().size() && flags.size() == 1, "exported binding flag count differs from native layout");
            Require(flags[0] == (updateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0), "exported binding flags differ from native layout");
            Require(updateAfterBind ? mock.layoutBindingFlags == std::vector<VkDescriptorBindingFlags>(flags.begin(), flags.end()) : mock.layoutBindingFlags.empty(), "native layout did not use the exported binding flag vector");
        }
        Require(mock.live == 0, "exported binding flag fixture leaked Vulkan objects");
    }
}

void descriptorPoolTests() {
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    mock.descriptorPoolLimit = 2;
    {
        DescriptorCache cache(mockContext());
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        const std::array<std::uint32_t, 4> key{binding.binding, binding.descriptorType, binding.descriptorCount, binding.stageFlags};
        const auto layout = cache.Layout(key, std::span(&binding, 1));
        const VkDescriptorPoolSize size{binding.descriptorType, binding.descriptorCount};
        const auto allocate = [&] { return cache.Allocate(layout, std::span(&size, 1)); };
        const auto first = allocate();
        const auto second = allocate();
        Require(first.pool == second.pool, "descriptor allocations did not share a pool");
        Require(mock.descriptorPools.at(first.pool).flags == 0, "descriptor pools use the individually freed allocation path");
        const auto third = allocate();
        const auto fourth = allocate();
        const auto fifth = allocate();
        Require(third.pool == fourth.pool && third.pool != first.pool && fifth.pool != third.pool, "exhausted descriptor pools did not advance the chain");
        Require(mock.descriptorPools.at(first.pool).attempts == 3, "an exhausted descriptor pool was retried before reset");
        cache.Free(first);
        Require(mock.descriptorPools.at(first.pool).resets == 0, "a descriptor pool reset while another set was live");
        cache.Free(second);
        Require(mock.descriptorPools.at(first.pool).resets == 1, "an empty descriptor pool was not reset");
        const auto sixth = allocate();
        const auto seventh = allocate();
        Require(sixth.pool == fifth.pool && seventh.pool == first.pool && cache.Counters().pools == 3, "an empty descriptor pool was not recycled");
        cache.Free(third);
        cache.Free(fourth);
        cache.Free(fifth);
        cache.Free(sixth);
        cache.Free(seventh);
        Require(mock.descriptorPools.at(first.pool).resets == 2 && mock.descriptorPools.at(third.pool).resets == 1 && mock.descriptorPools.at(fifth.pool).resets == 1, "descriptor pool ownership was lost after recycling");
    }
    Require(mock.live == 0 && mock.descriptorPools.empty(), "descriptor pool cache leaked Vulkan objects");
}

void descriptorPoolResetFailureTest() {
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    DescriptorCache cache(mockContext());
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    const std::array<std::uint32_t, 4> key{binding.binding, binding.descriptorType, binding.descriptorCount, binding.stageFlags};
    const auto layout = cache.Layout(key, std::span(&binding, 1));
    const VkDescriptorPoolSize size{binding.descriptorType, binding.descriptorCount};
    const auto allocation = cache.Allocate(layout, std::span(&size, 1));
    mock.descriptorResetResult = VK_ERROR_DEVICE_LOST;
    const auto previous = _ZSt13set_terminatePFvvE_nid_postfix([] {
        std::_Exit(mock.descriptorResetResult == VK_SUCCESS ? 0 : 1);
    });
    cache.Free(allocation);
    _ZSt13set_terminatePFvvE_nid_postfix(previous);
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
    {
        ShaderRecompiler::RecompileResult reading;
        reading.readsDepthRemapPush = true;
        std::vector<std::byte> encoded;
        ShaderRecompiler::ShaderDiskCache::EncodeResult(reading, encoded);
        ShaderRecompiler::RecompileResult replayed;
        Require(ShaderRecompiler::ShaderDiskCache::DecodeResult(encoded, replayed), "a result that reads the depth remap push words does not decode");
        const std::array<AgcDriver::Graphics::CompiledShader, 1> remapped{{{ShaderRecompiler::ShaderStage::Vertex, &replayed, 0}}};
        Require(replayed.pushConstants.empty() && AgcDriver::Graphics::PushConstantStages(remapped) == VK_SHADER_STAGE_VERTEX_BIT, "a journalled vertex shader that reads the depth remap push words lost its push stage");
    }
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
            const auto originalThird = findWrite(4).buffers;
            std::vector<ShaderResources::MovedBuffer> moved;
            std::array<std::vector<std::uint32_t>, 2> words;
            for (std::size_t index = 0; index < 2; ++index) {
                if ((selected & (1u << index)) == 0) continue;
                const std::size_t bytes = index == 0 ? 24 : 12;
                words[index].assign(bytes / 4, static_cast<std::uint32_t>(0x12340000u + index));
                moved.push_back({index, 0, bytes, words[index]});
            }
            mock.writes.clear();
            const auto snapshots = resources.PrepareDrawBindings(recorder, moved);
            Require(snapshots != nullptr && snapshots->snapshots.size() == moved.size(), "draw snapshots are missing");
            Require(mock.copies.empty(), "snapshot descriptor set must be written without descriptor copies");
            Require(mock.writes.size() == 2, "snapshot update must write every binding once");
            const auto& write = mock.writes.front();
            const auto& untouched = mock.writes.back();
            Require(untouched.binding == 4 && untouched.count == 1 && untouched.buffers[0].buffer == originalThird[0].buffer && untouched.buffers[0].offset == originalThird[0].offset && untouched.buffers[0].range == originalThird[0].range, "snapshot update changed a binding without snapshots");
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

alignas(16) std::array<std::uint32_t, 8> guestMovedFirst{0x51515151, 0x52525252, 0x53535353, 0x54545454, 0x55555555, 0x56565656, 0x57575757, 0x58585858};
alignas(16) std::array<std::uint32_t, 8> guestMovedSecond{0x61616161, 0x62626262, 0x63636363, 0x64646464, 0x65656565, 0x66666666, 0x67676767, 0x68686868};

void dispatchMoveTests() {
    using AgcDriver::Graphics::CompiledShader;
    using AgcDriver::Graphics::DescriptorCache;
    using AgcDriver::Graphics::Recorder;
    using AgcDriver::Graphics::ShaderResources;
    using Refusal = ShaderResources::MoveRefusal;
    mock = MockVulkan{};
    mock.allowCopies = true;
    auto context = mockContext();
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context);
        ShaderRecompiler::RecompileResult built;
        auto inputs = makeBinding(Role::GuestBuffers, 0, 2, join(vsharp(guestFirst.data(), 16), vsharp(guestSecond.data(), 32)));
        inputs.bufferWritten = {false, false};
        built.bindings.push_back(inputs);
        auto output = makeBinding(Role::GuestBuffers, 4, 1, vsharp(guestThird.data(), 8));
        output.bufferWritten = {true};
        built.bindings.push_back(output);
        const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &built, 0};
        ShaderResources resources(context, shader);
        const auto moveTo = [&](std::size_t element, const void* where, std::uint32_t bytes) {
            auto program = built;
            const auto words = vsharp(where, bytes);
            std::copy(words.begin(), words.end(), program.bindings[0].guestDescriptor.begin() + static_cast<std::ptrdiff_t>(element) * 4);
            return program;
        };
        const auto prove = [&](const ShaderRecompiler::RecompileResult& program, std::size_t maxBytes, ShaderResources::MoveProof& proof) {
            return resources.MovedDispatchBuffers(CompiledShader{ShaderRecompiler::ShaderStage::Compute, &program, 0}, &recorder, maxBytes, proof);
        };
        const auto refuses = [&](const ShaderRecompiler::RecompileResult& program, std::size_t maxBytes, Refusal why, const char* what) {
            ShaderResources::MoveProof refusal;
            const auto listed = prove(program, maxBytes, refusal);
            Require(!listed.has_value() && refusal.refusal == why, what);
        };

        ShaderResources::MoveProof proof;
        auto moved = prove(built, 0, proof);
        Require(moved.has_value() && moved->empty() && proof.refusal == Refusal::None && proof.buffers == 0, "a dispatch with every buffer where it was built moved something");

        const auto relocated = moveTo(1, guestMovedSecond.data(), 24);
        const CompiledShader relocatedShader{ShaderRecompiler::ShaderStage::Compute, &relocated, 0};
        Require(ShaderResources::ContentKey(shader, false, true) == ShaderResources::ContentKey(relocatedShader, false, true), "moving a read-only buffer changed the movable content key");
        Require(ShaderResources::ContentKey(shader, false, false) != ShaderResources::ContentKey(relocatedShader, false, false), "the fixed content key ignored a moved buffer");

        moved = prove(relocated, 0, proof);
        Require(moved.has_value() && moved->size() == 1 && proof.refusal == Refusal::None && proof.buffers == 1 && proof.bytes >= 24, "a moved read-only buffer was not listed");
        Require(moved->front().address == reinterpret_cast<std::uintptr_t>(guestMovedSecond.data()) && moved->front().size == 24 && moved->front().words.empty(), "the listed buffer is not the moved range");

        moved = prove(moveTo(0, guestFirst.data(), 8), 0, proof);
        Require(moved.has_value() && moved->size() == 1 && moved->front().address == reinterpret_cast<std::uintptr_t>(guestFirst.data()) && moved->front().size == 8, "a buffer that only changed its size was not listed");

        auto both = relocated;
        const auto firstWords = vsharp(guestMovedFirst.data(), 12);
        std::copy(firstWords.begin(), firstWords.end(), both.bindings[0].guestDescriptor.begin());
        moved = prove(both, 1024, proof);
        Require(moved.has_value() && moved->size() == 2 && proof.buffers == 2 && proof.bytes >= 36, "two moved buffers were not both listed");
        refuses(both, 30, Refusal::Limit, "moved buffers past the byte budget together were served");

        auto written = relocated;
        written.bindings[0].bufferWritten[1] = true;
        refuses(written, 0, Refusal::Written, "a moved buffer the dispatch writes was served");
        refuses(moveTo(1, guestMovedSecond.data(), 20000), 0, Refusal::Written, "a moved buffer over the storage buffer range that no registered allocation holds was served");
        refuses(relocated, 16, Refusal::Limit, "a moved buffer over the byte budget was served");
        refuses(moveTo(1, reinterpret_cast<const void*>(std::uintptr_t{0x10}), 24), 0, Refusal::Inaccessible, "a moved buffer over unmapped memory was served");
        refuses(moveTo(1, guestThird.data(), 8), 0, Refusal::Overlap, "a moved buffer over the dispatch's own written range was served");
        auto shaped = relocated;
        shaped.bindings[0].guestDescriptor.resize(4);
        refuses(shaped, 0, Refusal::Shape, "a dispatch with another binding shape was served");
        recorder.Sync();
    }
    {
        constexpr std::size_t blockBytes = 65536;
        void* block = ::operator new(blockBytes, std::align_val_t{blockBytes});
        std::memset(block, 0x5a, blockBytes);
        {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, blockBytes, true, true, true);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        }
        const auto* end = static_cast<const std::byte*>(block) + blockBytes;
        const auto strided = [](const void* pointer, std::uint32_t stride, std::uint32_t records) {
            auto words = vsharp(pointer, records);
            words[1] |= stride << 16u;
            return words;
        };
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context);
        const auto proveAgainst = [&](const ShaderRecompiler::RecompileResult& built, const std::vector<std::uint32_t>& incoming, ShaderResources::MoveProof& proof, std::optional<std::vector<ShaderResources::MovedBuffer>>& moved) {
            const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &built, 0};
            ShaderResources resources(context, shader);
            auto program = built;
            program.bindings[0].guestDescriptor = incoming;
            moved = resources.MovedDispatchBuffers(CompiledShader{ShaderRecompiler::ShaderStage::Compute, &program, 0}, &recorder, 0, proof);
        };
        const auto readOnly = [&](const std::vector<std::uint32_t>& words) {
            ShaderRecompiler::RecompileResult built;
            auto binding = makeBinding(Role::GuestBuffers, 0, 1, words);
            binding.bufferWritten = {false};
            built.bindings.push_back(binding);
            return built;
        };
        const auto address = [](const void* pointer) { return reinterpret_cast<std::uintptr_t>(pointer); };
        ShaderResources::MoveProof proof;
        std::optional<std::vector<ShaderResources::MovedBuffer>> moved;

        const auto* tail = end - 8192;
        const auto* nearEnd = end - 4096;
        const auto oversized = readOnly(vsharp(tail, 0xffffffffu));
        proveAgainst(oversized, vsharp(tail, 0xffffffffu), proof, moved);
        Require(moved.has_value() && moved->empty() && proof.refusal == Refusal::None && proof.buffers == 0, "an unchanged V# the build cut to its allocation end was not an exact hit");
        proveAgainst(oversized, vsharp(nearEnd, 0xffffffffu), proof, moved);
        Require(moved.has_value() && moved->size() == 1 && proof.buffers == 1 && proof.bytes == 4096 && moved->front().address == address(nearEnd) && moved->front().size == 4096, "a moved V# past the range limit was not snapshotted at its clamped range");
        proveAgainst(oversized, vsharp(nearEnd, 0xfffffff0u), proof, moved);
        Require(moved.has_value() && moved->size() == 1 && moved->front().size == 4096, "another oversized size over the same allocation end listed another range");
        proveAgainst(oversized, vsharp(tail, 4096), proof, moved);
        Require(moved.has_value() && moved->size() == 1 && moved->front().address == address(tail) && moved->front().size == 4096, "a V# that now fits the range limit was not listed at its own size");

        proveAgainst(oversized, vsharp(block, 0xffffffffu), proof, moved);
        Require(!moved.has_value() && proof.refusal == Refusal::Limit, "a moved V# the allocation holds past the range limit was served");

        const auto overhang = readOnly(strided(tail, 24, 342));
        proveAgainst(overhang, strided(tail, 24, 342), proof, moved);
        Require(moved.has_value() && moved->empty() && proof.refusal == Refusal::None, "an unchanged V# with a stride overhang past the allocation was not an exact hit");
        proveAgainst(overhang, strided(nearEnd, 24, 171), proof, moved);
        Require(moved.has_value() && moved->size() == 1 && moved->front().address == address(nearEnd) && moved->front().size == 4096 && proof.bytes == 4096, "a moved V# with a stride overhang was snapshotted past the allocation end");
        proveAgainst(overhang, strided(nearEnd, 24, 100), proof, moved);
        Require(moved.has_value() && moved->size() == 1 && moved->front().size == 2400, "a moved V# inside the allocation was not listed at its own size");
        recorder.Sync();
        {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        }
        ::operator delete(block, std::align_val_t{blockBytes});
    }
    Require(mock.live == 0, "dispatch move resources leaked Vulkan objects");
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
    expectSingleAccepted(changed([](auto& binding) { binding.guestDescriptor[1] |= 0x40000000u; }), "a V# with reserved bit 30");
    expectSingleFailure(changed([](auto& binding) { binding.guestDescriptor[3] |= 0x40000000u; }), "unsupported type");
    expectSingleAccepted(changed([](auto& binding) { binding.guestDescriptor[1] |= 0x3fffu << 16u; binding.guestDescriptor[2] = 0xffffffffu; }), "an unregistered V# past the range limit");
    expectSingleAccepted(changed([](auto& binding) { binding.guestDescriptor[2] = 32768; }), "an unregistered V# past the range limit");
    expectSingleAccepted(changed([](auto& binding) { binding.guestDescriptor = vsharp(reinterpret_cast<const void*>(0x1000), 8); }), "an unmapped V#");
    expectSingleFailure(changed([&](auto& binding) { binding.guestDescriptor = vsharp(reinterpret_cast<const void*>(state.color.address), 64); }), "aliases the render target");
    const auto stridedBefore = [&](std::uint32_t past) {
        constexpr std::uint32_t stride = 60;
        constexpr std::uint32_t records = 4;
        auto descriptor = vsharp(reinterpret_cast<const void*>(state.color.address - (stride * records - past)), records);
        descriptor[1] |= stride << 16u;
        return descriptor;
    };
    expectSingleAccepted(changed([&](auto& binding) { binding.guestDescriptor = stridedBefore(8); binding.bufferWritten = {false}; }), "a read-only strided V# whose last record runs 8 bytes into the render target");
    expectSingleFailure(changed([&](auto& binding) { binding.guestDescriptor = stridedBefore(8); binding.bufferWritten = {true}; }), "aliases the render target");
    expectSingleFailure(changed([&](auto& binding) { binding.guestDescriptor = stridedBefore(60); }), "aliases the render target");
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
        mock = MockVulkan{};
        auto limited = mockContext();
        limited.limits.maxPerStageDescriptorSamplers = 16;
        ShaderRecompiler::RecompileResult vertex;
        ShaderRecompiler::RecompileResult fragment;
        auto samplers = makeBinding(Role::GuestSamplers, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers), 17, std::vector<std::uint32_t>(17u * 4u));
        samplers.kind = Kind::Sampler;
        vertex.bindings.push_back(std::move(samplers));
        expectFailure([&] { AgcDriver::Graphics::ShaderResources resources(limited, vertex, fragment, state.color, 0, 0); }, "shader sampler descriptors exceed per-stage limits: required 17, device limit 16");
        Require(mock.live == 0, "sampler limit rejection leaked Vulkan objects");
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
        Require(mock.live == live + 1 && cache.Counters().keptSets == 1, "freeing an oversized set did not keep its set and pool");
        const auto again = cache.Allocate(large, oversized);
        Require(again.set == dedicated.set && again.pool == dedicated.pool && mock.live == live + 1 && cache.Counters().reusedSets == 1 && cache.Counters().dedicatedPools == 1, "an oversized set of the kept set's layout and sizes did not take the kept set");
        cache.Free(again);
        const std::array<VkDescriptorPoolSize, 2> otherSizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4098}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3}}};
        const auto other = cache.Allocate(large, otherSizes);
        Require(other.pool != dedicated.pool && mock.live == live + 2 && cache.Counters().dedicatedPools == 2, "an oversized set of other sizes took a kept set");
        cache.Free(other);
        Require(mock.live == live + 2 && cache.Counters().keptSets == 3, "freeing a second oversized set did not keep it");
        const std::array<VkDescriptorPoolSize, 1> fitting{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096}}};
        const auto chained = cache.Allocate(layout(4096), fitting);
        Require(chained.set != VK_NULL_HANDLE && mock.poolMaxSets == 1024 && mock.poolFlags == 0 && cache.Counters().pools == 1, "a set the chain pool holds left the chain");
        cache.Free(chained);
        Require(mock.descriptorPools.at(chained.pool).resets == 1 && mock.live == live + 4, "a chain pool was not reset after its last set");
        const std::array<VkDescriptorPoolSize, 1> regular{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096}}};
        const auto first = cache.Allocate(layout(4096), regular);
        const auto oversizedMixed = cache.Allocate(large, oversized);
        const auto second = cache.Allocate(layout(4096), regular);
        Require(first.pool == chained.pool && second.pool == chained.pool && oversizedMixed.pool != chained.pool && mock.descriptorPools.at(chained.pool).resets == 1, "an oversized set disturbed the chain pool");
        cache.Free(oversizedMixed);
        Require(oversizedMixed.pool == dedicated.pool && mock.live == live + 4 && mock.descriptorPools.count(oversizedMixed.pool) == 1 && mock.descriptorPools.at(chained.pool).resets == 1, "freeing an oversized set between chain sets touched the chain pool");
        cache.Free(first);
        Require(mock.descriptorPools.at(chained.pool).resets == 1, "a chain pool reset while a set was live");
        cache.Free(second);
        Require(mock.descriptorPools.at(chained.pool).resets == 2, "a chain pool was not reset after the oversized set was freed");
        const std::array<VkDescriptorPoolSize, 1> beyondDevice{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8193}}};
        expectFailure([&] { cache.Allocate(large, beyondDevice); }, "descriptor set exceeds the device's per-set descriptor limit");
        const std::array<VkDescriptorPoolSize, 1> storageImages{{{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}}};
        expectFailure([&] { cache.Allocate(large, storageImages); }, "descriptor set exceeds the device's per-set descriptor limit");
        const std::array<VkDescriptorPoolSize, 1> uniformBuffers{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}}};
        expectFailure([&] { cache.Allocate(large, uniformBuffers); }, "descriptor set uses an unsupported descriptor type 6");
        const auto keptBefore = mock.live;
        std::vector<AgcDriver::Graphics::DescriptorCache::SetAllocation> spread;
        for (std::uint32_t index = 0; index < 10; ++index) {
            const std::array<VkDescriptorPoolSize, 1> sized{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4100 + index}}};
            spread.push_back(cache.Allocate(large, sized));
        }
        Require(mock.live == keptBefore + 10, "ten oversized sets of new sizes did not get ten pools");
        for (const auto& set : spread) cache.Free(set);
        Require(mock.live == keptBefore - 2 + 8, "freeing more oversized sets than the cache keeps did not destroy the oldest pools");
    }
    Require(mock.live == 0, "the descriptor cache leaked a pool or layout");
    mock = MockVulkan{};
    context.limits.maxDescriptorSetStorageBuffers = 16;
    context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 8192;
    context.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 16384;
    context.limits.maxPerStageDescriptorSampledImages = 2048;
    {
        AgcDriver::Graphics::DescriptorCache cache(context);
        const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4097, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
        constexpr auto tableFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        const std::array<VkDescriptorBindingFlags, 2> flags{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, tableFlags};
        const std::array<std::uint32_t, 9> key{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4097, VK_SHADER_STAGE_FRAGMENT_BIT, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT};
        const auto layout = cache.Layout(key, bindings, true, flags);
        Require(mock.layoutFlags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT && mock.layoutBindingFlags == std::vector<VkDescriptorBindingFlags>{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, tableFlags}, "a variable image table lost its flags when the storage binding used update-after-bind");
        Require(cache.Layout(key, bindings, true, flags) == layout && cache.Counters().layoutHits == 1, "the combined descriptor layout was not reused");
        const std::array<VkDescriptorPoolSize, 2> oversized{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4097}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3}}};
        const auto dedicated = cache.Allocate(layout, oversized, true, 3u);
        Require(mock.poolMaxSets == 1 && mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT && mock.variableDescriptorCount == 3u, "a dedicated update-after-bind pool lost the variable image count");
        cache.Free(dedicated);
        auto fittingBindings = bindings;
        fittingBindings[0].descriptorCount = 17;
        auto fittingKey = key;
        fittingKey[2] = 17;
        const auto fittingLayout = cache.Layout(fittingKey, fittingBindings, true, flags);
        const std::array<VkDescriptorPoolSize, 2> fitting{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 17}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3}}};
        const auto chained = cache.Allocate(fittingLayout, fitting, true, 3u);
        Require(mock.poolMaxSets == 1024 && mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT && mock.variableDescriptorCount == 3u, "a chained update-after-bind pool lost the variable image count");
        cache.Free(chained);
        const auto reused = cache.Allocate(fittingLayout, fitting, true, 0u);
        Require(reused.pool == chained.pool && mock.variableDescriptorCount == 0u, "reusing an update-after-bind pool lost an empty variable image table");
        cache.Free(reused);
        const auto rewritten = cache.AllocateRewritten(fittingLayout, fitting, true, 3u);
        cache.Recycle(fittingLayout, rewritten);
        const std::array<VkDescriptorPoolSize, 2> grown{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 17}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5}}};
        const auto larger = cache.AllocateRewritten(fittingLayout, grown, true, 5u);
        Require(larger.set != rewritten.set && mock.variableDescriptorCount == 5u, "a recycled descriptor set served a larger variable image table than it was allocated for");
        const auto same = cache.AllocateRewritten(fittingLayout, fitting, true, 3u);
        Require(same.set == rewritten.set, "a recycled descriptor set was not reused for a variable image table of its own size");
        cache.Free(larger);
        cache.Free(same);
        expectFailure([&] { cache.Allocate(fittingLayout, fitting, false, 3u); }, "descriptor set exceeds the device's per-set descriptor limit");
    }
    Require(mock.live == 0, "the combined descriptor cache leaked a pool or layout");
    mock = MockVulkan{};
    auto lavapipeContext = mockContext();
    lavapipeContext.individuallyFreeDescriptorSets = true;
    lavapipeContext.limits.maxDescriptorSetStorageBuffers = 16;
    {
        AgcDriver::Graphics::DescriptorCache cache(lavapipeContext);
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        const std::array<std::uint32_t, 4> key{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
        const auto layout = cache.Layout(key, std::span(&binding, 1));
        const std::array<VkDescriptorPoolSize, 1> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
        const auto first = cache.Allocate(layout, sizes);
        const auto second = cache.Allocate(layout, sizes);
        Require(first.pool == second.pool && mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, "lavapipe chain pools do not enable individual descriptor-set frees");
        cache.Free(first);
        Require(mock.freedDescriptorSets == 1 && mock.descriptorPools.at(first.pool).resets == 0, "freeing one of two live sets reset its chain pool");
        cache.Free(second);
        Require(mock.freedDescriptorSets == 2 && mock.descriptorPools.at(second.pool).resets == 1, "freeing the last live set did not free it and reset its chain pool");
        const auto reused = cache.Allocate(layout, sizes);
        Require(reused.pool == first.pool && mock.descriptorPools.at(reused.pool).attempts == 3, "an emptied lavapipe chain pool was not reused");
        cache.Free(reused);
    }
    Require(mock.live == 0, "the lavapipe descriptor cache leaked a pool or layout");
}

void imageTableAllocationTests() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    for (const bool cached : {false, true}) {
        for (const std::uint32_t count : {0u, 2u, 3u}) {
            mock = MockVulkan{};
            auto context = mockContext();
            context.imageTableCapacity = 2;
            context.limits.maxDescriptorSetSampledImages = 2;
            context.limits.maxPerStageDescriptorSampledImages = 2;
            {
                AgcDriver::Graphics::DescriptorCache cache(context);
                if (cached) context.descriptorCache = &cache;
                ShaderRecompiler::RecompileResult program;
                auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords + count));
                map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
                map.guestDescriptor[Abi::HeaderElementCount] = count;
                map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
                map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + count;
                program.bindings.push_back(map);
                auto images = makeBinding(Role::ImageTable, ShaderRecompiler::RuntimeAbi::ImageTableBinding, count, std::vector<std::uint32_t>(count * 8u));
                images.kind = Kind::SampledImage;
                images.imageShapes.resize(count);
                images.imageDepthCompare.resize(count);
                images.imageSamplers.resize(count);
                program.bindings.push_back(std::move(images));
                const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
                {
                    AgcDriver::Graphics::ShaderResources resources(context, shader, {}, true);
                    Require(findLayoutBinding(ShaderRecompiler::RuntimeAbi::ImageTableBinding).descriptorCount == 2u, "the image table layout changed its device capacity");
                    const auto physicalCount = std::max(1u, std::min(count, 2u));
                    Require(mock.variableDescriptorCount == physicalCount, "image table allocation did not reserve its bounded physical descriptor count");
                    const auto sampled = std::find_if(mock.poolSizes.begin(), mock.poolSizes.end(), [](const auto& size) { return size.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; });
                    Require(sampled != mock.poolSizes.end() && sampled->descriptorCount >= physicalCount && (cached || sampled->descriptorCount == physicalCount), "the image table pool did not cover its bounded allocation");
                    resources.Complete();
                    const auto& bytes = bufferBytes(findWrite(map.binding).buffers.at(0).buffer);
                    std::vector<std::uint32_t> actual(bytes.size() / sizeof(std::uint32_t));
                    std::memcpy(actual.data(), bytes.data(), bytes.size());
                    Require(actual[Abi::HeaderElementCount] == count, "bounding the Vulkan allocation truncated the guest table domain");
                    if (count > context.imageTableCapacity) Require((actual[Abi::HeaderWords + 2u] & Abi::PoisonFlag) != 0u, "an image beyond Vulkan capacity lost its deferred fault");
                }
            }
            Require(mock.live == 0, "the bounded image table leaked Vulkan objects");
        }
    }
}

void imageTableSnapshotAllocationTests() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    auto context = mockContext();
    context.imageTableCapacity = 2;
    context.limits.maxDescriptorSetSampledImages = 2;
    context.limits.maxPerStageDescriptorSampledImages = 2;
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context);
        ShaderRecompiler::RecompileResult vertex, fragment;
        auto buffer = makeBinding(Role::GuestBuffers, 0, 1, vsharp(guestFirst.data(), 16));
        buffer.bufferWritten = {false};
        vertex.bindings.push_back(buffer);
        auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords));
        map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
        map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
        map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords;
        vertex.bindings.push_back(map);
        auto images = makeBinding(Role::ImageTable, ShaderRecompiler::RuntimeAbi::ImageTableBinding, 0, {});
        images.kind = Kind::SampledImage;
        vertex.bindings.push_back(images);
        const auto state = DecodeState(makeState());
        ShaderResources resources(context, vertex, fragment, state.color, 0, 0);
        const auto originalMap = findWrite(map.binding).buffers.at(0);
        const std::array<std::uint32_t, 4> movedWords{11, 22, 33, 44};
        const std::array<ShaderResources::MovedBuffer, 1> moved{{{0, 0, 16, movedWords}}};
        const auto sets = cache.Counters().sets;
        mock.writes.clear();
        mock.variableDescriptorCount.reset();
        const auto snapshots = resources.PrepareDrawBindings(recorder, moved);
        Require(snapshots != nullptr && snapshots->snapshots.size() == 1 && cache.Counters().sets == sets + 1, "an empty image table prevented draw snapshot allocation");
        Require(mock.variableDescriptorCount == 1u, "an empty draw snapshot table did not reserve one physical image descriptor");
        Require(sameBytes(bufferBytes(snapshots->snapshots[0].buffer->Handle()), moved[0].words.data(), moved[0].size), "an empty image table changed draw snapshot contents");
        const auto& currentMap = findWrite(map.binding).buffers.at(0);
        Require(currentMap.buffer == originalMap.buffer && currentMap.offset == originalMap.offset && currentMap.range == originalMap.range, "draw snapshots replaced the empty image table map");
        std::array<std::uint32_t, Abi::HeaderWords> header{};
        std::memcpy(header.data(), bufferBytes(currentMap.buffer).data() + currentMap.offset, sizeof(header));
        Require(header[Abi::HeaderElementCount] == 0u, "draw snapshots changed the empty guest table domain");
        Require(std::none_of(mock.writes.begin(), mock.writes.end(), [](const auto& write) { return write.binding == ShaderRecompiler::RuntimeAbi::ImageTableBinding; }), "an empty draw snapshot table wrote an image descriptor");
        recorder.Sync();
    }
    Require(mock.live == 0, "the empty image table draw snapshot leaked Vulkan objects");
}

void samplerTableNonSeamlessCubeTests() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    constexpr auto count = 2u * Abi::SamplerEntryElements;
    for (const bool supported : {false, true}) {
        for (const std::uint32_t flaggedEntry : {0u, 1u}) {
            mock = MockVulkan{};
            auto context = mockContext();
            context.nonSeamlessCubeMap = supported;
            context.samplerTableCapacity = count;
            context.limits.maxDescriptorSetSamplers = count;
            context.limits.maxPerStageDescriptorSamplers = count;
            context.limits.maxSamplerAnisotropy = 1.0f;
            ShaderRecompiler::RecompileResult program;
            auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords + 2u));
            map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
            map.guestDescriptor[Abi::HeaderSamplerCount] = count;
            map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
            map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords;
            program.bindings.push_back(map);
            auto samplers = makeBinding(Role::SamplerTable, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::SamplerTable), count, {});
            samplers.kind = Kind::Sampler;
            for (std::uint32_t entry = 0; entry < 2u; ++entry) {
                const std::array<std::uint32_t, 4> words{0x92u | (entry == flaggedEntry ? 1u << 28u : 0u), (4u * 256u) << 12u, 0u, 0u};
                for (std::uint32_t variant = 0; variant < Abi::SamplerEntryElements; ++variant) {
                    samplers.guestDescriptor.insert(samplers.guestDescriptor.end(), words.begin(), words.end());
                    samplers.samplerDepthCompare.push_back(variant >= 2u);
                }
            }
            const auto samplerBinding = samplers.binding;
            program.bindings.push_back(std::move(samplers));
            const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
            {
                AgcDriver::Graphics::ShaderResources resources(context, shader, {}, true);
                resources.Complete();
                const auto& bytes = bufferBytes(findWrite(map.binding).buffers.at(0).buffer);
                std::vector<std::uint32_t> actual(bytes.size() / sizeof(std::uint32_t));
                std::memcpy(actual.data(), bytes.data(), bytes.size());
                Require(actual[Abi::HeaderSamplerCount] == count, "non-seamless sampler rejection changed the guest table domain");
                Require(actual[Abi::HeaderPoisonCount] == (supported ? 0u : 1u), "non-seamless sampler table has the wrong deferred poison count");
                for (std::uint32_t entry = 0; entry < 2u; ++entry) {
                    const auto expected = !supported && entry == flaggedEntry ? Abi::PoisonCode(0u) : 0u;
                    Require(actual[Abi::HeaderWords + entry] == expected, "non-seamless sampler rejection poisoned the wrong entry");
                }
                const auto& write = findWrite(samplerBinding);
                const auto first = supported ? 0u : (1u - flaggedEntry) * Abi::SamplerEntryElements;
                const auto written = supported ? count : Abi::SamplerEntryElements;
                Require(findLayoutBinding(samplerBinding).descriptorCount == count && write.type == VK_DESCRIPTOR_TYPE_SAMPLER && write.arrayElement == first && write.count == written && write.images.size() == written, "non-seamless sampler rejection compacted or truncated descriptor indices");
                Require(mock.samplerFlags.size() == written, "a rejected sampler table entry allocated a Vulkan sampler");
                for (std::uint32_t index = 0; index < written; ++index) {
                    const auto expected = supported && (first + index) / Abi::SamplerEntryElements == flaggedEntry ? VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT : 0u;
                    Require(mock.samplerFlags.at(write.images[index].sampler) == expected, "a sampler table variant has the wrong non-seamless cube creation flag");
                }
            }
            Require(mock.live == 0 && mock.samplerFlags.empty(), "the non-seamless sampler table leaked Vulkan objects");
        }
    }
}

void samplerTableDeviceLimitTests(std::optional<std::uint32_t> selected = std::nullopt) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    constexpr auto count = 2u * Abi::SamplerEntryElements;
    const std::array<std::uint32_t, 4> valid{0x92u, (4u * 256u) << 12u, 0u, 0u};
    const std::array<std::array<std::uint32_t, 4>, 3> invalid{{
        {0x92u, valid[1], 256u, 0u},
        {0x92u | (2u << 9u), valid[1], (2u << 20u) | (2u << 22u), 0u},
        {0x92u | (1u << 29u), valid[1], 0u, 0u}
    }};
    for (std::uint32_t kind = 0; kind < invalid.size(); ++kind) {
        if (selected.has_value() && kind != *selected) continue;
        for (const std::uint32_t badEntry : {0u, 1u}) {
            mock = MockVulkan{};
            auto context = mockContext();
            context.samplerTableCapacity = count;
            context.limits.maxDescriptorSetSamplers = count;
            context.limits.maxPerStageDescriptorSamplers = count;
            context.samplerAnisotropy = true;
            context.samplerFilterMinmax = true;
            context.limits.maxSamplerAnisotropy = 2.0f;
            context.limits.maxSamplerLodBias = 0.5f;
            ShaderRecompiler::RecompileResult program;
            auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords + 2u));
            map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
            map.guestDescriptor[Abi::HeaderSamplerCount] = count;
            map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
            map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords;
            program.bindings.push_back(map);
            auto samplers = makeBinding(Role::SamplerTable, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::SamplerTable), count, {});
            samplers.kind = Kind::Sampler;
            for (std::uint32_t entry = 0; entry < 2u; ++entry) {
                const auto& words = entry == badEntry ? invalid[kind] : valid;
                for (std::uint32_t variant = 0; variant < Abi::SamplerEntryElements; ++variant) {
                    samplers.guestDescriptor.insert(samplers.guestDescriptor.end(), words.begin(), words.end());
                    samplers.samplerDepthCompare.push_back(variant >= 2u);
                }
            }
            const auto samplerBinding = samplers.binding;
            program.bindings.push_back(std::move(samplers));
            const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
            {
                AgcDriver::Graphics::ShaderResources resources(context, shader, {}, true);
                resources.Complete();
                const auto& bytes = bufferBytes(findWrite(map.binding).buffers.at(0).buffer);
                std::vector<std::uint32_t> actual(bytes.size() / sizeof(std::uint32_t));
                std::memcpy(actual.data(), bytes.data(), bytes.size());
                Require(actual[Abi::HeaderSamplerCount] == count && actual[Abi::HeaderPoisonCount] == 1u, "sampler device validation changed the domain or deferred poison count");
                for (std::uint32_t entry = 0; entry < 2u; ++entry) {
                    Require(actual[Abi::HeaderWords + entry] == (entry == badEntry ? Abi::PoisonCode(0u) : 0u), "sampler device validation poisoned the valid entry");
                }
                const auto& write = findWrite(samplerBinding);
                Require(write.type == VK_DESCRIPTOR_TYPE_SAMPLER && write.arrayElement == (1u - badEntry) * Abi::SamplerEntryElements && write.count == Abi::SamplerEntryElements && write.images.size() == Abi::SamplerEntryElements, "sampler device validation changed valid descriptor indices");
                Require(mock.samplerCreateCount == Abi::SamplerEntryElements && mock.samplerFlags.size() == Abi::SamplerEntryElements, "a rejected sampler table entry created a Vulkan sampler");
            }
            Require(mock.live == 0 && mock.samplerFlags.empty(), "sampler device validation leaked Vulkan objects");
        }
    }
}

void hostImportBudgetTests() {
    using AgcDriver::Graphics::HostImportBudget;
    constexpr std::uint64_t MiB = 1ull << 20u;
    constexpr std::uint64_t GiB = 1ull << 30u;
    VkPhysicalDeviceMemoryProperties memory{};
    Require(HostImportBudget(memory, 0) == 6 * GiB, "a device without memory heaps does not keep the 6 GiB import budget");
    Require(HostImportBudget(memory, 32 * GiB) == 16 * GiB, "a device without a host heap does not take half the physical memory");
    memory.memoryHeapCount = 3;
    memory.memoryHeaps[0] = {7920 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory.memoryHeaps[1] = {8128 * MiB, 0};
    memory.memoryHeaps[2] = {256 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    Require(HostImportBudget(memory, 16 * GiB) == 4064 * MiB, "imports on a 16 GiB machine take more than half its 8128 MiB host heap");
    memory.memoryHeaps[1].size = 16 * GiB;
    Require(HostImportBudget(memory, 32 * GiB) == 8 * GiB, "imports on a 32 GiB machine take more than half its 16 GiB host heap");
    memory.memoryHeaps[1].size = 32 * GiB;
    Require(HostImportBudget(memory, 64 * GiB) == 16 * GiB, "imports on a 64 GiB machine take more than half its 32 GiB host heap");
    memory.memoryHeaps[1].size = 3 * GiB;
    Require(HostImportBudget(memory, 16 * GiB) == 1 * GiB, "a 3 GiB host heap does not keep its 2 GiB reserve");
    memory.memoryHeaps[1].size = 8128 * MiB;
    memory.memoryHeaps[0].size = 64 * GiB;
    Require(HostImportBudget(memory, 16 * GiB) == 4064 * MiB, "a device-local heap counted as the host heap");
    memory.memoryHeapCount = 1;
    Require(HostImportBudget(memory, 16 * GiB) == 8 * GiB, "a host heap past memoryHeapCount limits the import budget");
}

void tableUpdateAfterBindTests() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    using namespace AgcDriver::Graphics;
    auto argumentLimits = mockContext();
    argumentLimits.runtimeDescriptorArray = true;
    argumentLimits.partiallyBound = true;
    argumentLimits.variableDescriptorCount = true;
    argumentLimits.descriptorIndexing = true;
    argumentLimits.limits.maxPerStageDescriptorSamplers = 16;
    argumentLimits.limits.maxPerStageDescriptorSampledImages = 256;
    argumentLimits.limits.maxPerStageResources = 287;
    argumentLimits.limits.maxDescriptorSetSamplers = 80;
    argumentLimits.limits.maxDescriptorSetSampledImages = 1280;
    argumentLimits.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSamplers = 500000;
    argumentLimits.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSampledImages = 1000000;
    argumentLimits.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 1000000;
    argumentLimits.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSamplers = 500000;
    argumentLimits.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSampledImages = 1000000;
    Require(ImageTableCapacity(argumentLimits) == 0u && SamplerTableCapacity(argumentLimits) == 0u && !ImageTableUpdateAfterBind(argumentLimits) && !SamplerTableUpdateAfterBind(argumentLimits), "tables beyond the regular descriptor limits were enabled without update-after-bind sampled images");
    argumentLimits.sampledImageUpdateAfterBind = true;
    Require(ImageTableCapacity(argumentLimits) == ShaderRecompiler::RuntimeAbi::StageCount * Abi::ImageElementBudget && ImageTableUpdateAfterBind(argumentLimits), "an image table beyond the regular descriptor limits did not take the update-after-bind limits");
    Require(SamplerTableCapacity(argumentLimits) == Abi::SamplerElementBudget && SamplerTableUpdateAfterBind(argumentLimits), "a sampler table beyond the regular descriptor limits did not take the update-after-bind limits");
    auto regularLimits = argumentLimits;
    regularLimits.limits.maxPerStageDescriptorSamplers = 1048576;
    regularLimits.limits.maxPerStageDescriptorSampledImages = 1048576;
    regularLimits.limits.maxPerStageResources = 1048576;
    regularLimits.limits.maxDescriptorSetSamplers = 1048576;
    regularLimits.limits.maxDescriptorSetSampledImages = 1048576;
    Require(ImageTableCapacity(regularLimits) == ShaderRecompiler::RuntimeAbi::StageCount * Abi::ImageElementBudget && !ImageTableUpdateAfterBind(regularLimits), "an image table within the regular descriptor limits took the update-after-bind path");
    Require(SamplerTableCapacity(regularLimits) == Abi::SamplerElementBudget && !SamplerTableUpdateAfterBind(regularLimits), "a sampler table within the regular descriptor limits took the update-after-bind path");

    const auto tableContext = [] {
        auto context = mockContext();
        context.sampledImageUpdateAfterBind = true;
        context.limits.maxPerStageDescriptorSampledImages = 1;
        context.limits.maxDescriptorSetSampledImages = 1;
        context.limits.maxPerStageDescriptorSamplers = 1;
        context.limits.maxDescriptorSetSamplers = 1;
        context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSampledImages = 2;
        context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSampledImages = 2;
        context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSamplers = 2u * Abi::SamplerEntryElements;
        context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSamplers = 2u * Abi::SamplerEntryElements;
        context.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 128;
        context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 32;
        context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 16;
        return context;
    };
    const auto requireTableLayout = [](std::uint32_t tableBinding, VkDescriptorBindingFlags tableFlags, std::string_view what) {
        Require(mock.layoutFlags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, std::string(what) + " did not get an update-after-bind layout");
        Require(mock.layoutBindingFlags.size() == mock.layoutBindings.size(), std::string(what) + " lost its binding flags");
        for (std::size_t index = 0; index < mock.layoutBindings.size(); ++index) {
            const auto expected = mock.layoutBindings[index].binding == tableBinding ? tableFlags | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0u;
            Require(mock.layoutBindingFlags[index] == expected, std::string(what) + " put update-after-bind on the wrong bindings");
        }
        Require(mock.poolFlags == VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, std::string(what) + " did not come from an update-after-bind pool");
    };
    for (const bool cached : {false, true}) {
        mock = MockVulkan{};
        auto context = tableContext();
        context.imageTableCapacity = 2;
        context.imageTableUpdateAfterBind = true;
        {
            DescriptorCache cache(context);
            if (cached) context.descriptorCache = &cache;
            ShaderRecompiler::RecompileResult program;
            auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords + 2u));
            map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
            map.guestDescriptor[Abi::HeaderElementCount] = 2u;
            map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
            map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + 2u;
            program.bindings.push_back(map);
            auto images = makeBinding(Role::ImageTable, ShaderRecompiler::RuntimeAbi::ImageTableBinding, 2u, std::vector<std::uint32_t>(16u));
            images.kind = Kind::SampledImage;
            images.imageShapes.resize(2u);
            images.imageDepthCompare.resize(2u);
            images.imageSamplers.resize(2u);
            program.bindings.push_back(std::move(images));
            const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
            {
                ShaderResources resources(context, shader, {}, true);
                requireTableLayout(ShaderRecompiler::RuntimeAbi::ImageTableBinding, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, "an image table beyond the regular limits");
                if (cached) Require(DescribeSetLayouts(context, std::span(&shader, 1)).layouts.main == resources.Layout(), "the described image table layout differs from the built one");
            }
        }
        Require(mock.live == 0, "the update-after-bind image table leaked Vulkan objects");
    }
    for (const bool cached : {false, true}) {
        mock = MockVulkan{};
        auto context = tableContext();
        constexpr auto count = 2u * Abi::SamplerEntryElements;
        context.samplerTableCapacity = count;
        context.samplerTableUpdateAfterBind = true;
        context.samplerAnisotropy = true;
        context.samplerFilterMinmax = true;
        context.limits.maxSamplerAnisotropy = 2.0f;
        context.limits.maxSamplerLodBias = 0.5f;
        {
            DescriptorCache cache(context);
            if (cached) context.descriptorCache = &cache;
            ShaderRecompiler::RecompileResult program;
            auto map = makeBinding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap), 1, std::vector<std::uint32_t>(Abi::HeaderWords + 2u));
            map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
            map.guestDescriptor[Abi::HeaderSamplerCount] = count;
            map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
            map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords;
            program.bindings.push_back(map);
            const auto samplerBinding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::SamplerTable);
            auto samplers = makeBinding(Role::SamplerTable, samplerBinding, count, {});
            samplers.kind = Kind::Sampler;
            const std::array<std::uint32_t, 4> valid{0x92u, (4u * 256u) << 12u, 0u, 0u};
            for (std::uint32_t element = 0; element < count; ++element) {
                samplers.guestDescriptor.insert(samplers.guestDescriptor.end(), valid.begin(), valid.end());
                samplers.samplerDepthCompare.push_back(false);
            }
            program.bindings.push_back(std::move(samplers));
            const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
            {
                ShaderResources resources(context, shader, {}, true);
                resources.Complete();
                requireTableLayout(samplerBinding, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, "a sampler table beyond the regular limits");
                Require(findWrite(samplerBinding).count == count, "the update-after-bind sampler table did not write its samplers");
                if (cached) Require(DescribeSetLayouts(context, std::span(&shader, 1)).layouts.main == resources.Layout(), "the described sampler table layout differs from the built one");
            }
        }
        Require(mock.live == 0 && mock.samplerFlags.empty(), "the update-after-bind sampler table leaked Vulkan objects");
    }
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

void storageTextureBudgetTests() {
    using AgcDriver::Graphics::StorageTextureBudget;
    constexpr std::uint64_t GiB = 1ull << 30u;
    constexpr std::uint64_t MiB = 1ull << 20u;
    VkPhysicalDeviceMemoryProperties memory{};
    memory.memoryHeapCount = 3;
    memory.memoryHeaps[0] = {256 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory.memoryHeaps[1] = {32 * GiB, 0};
    memory.memoryHeaps[2] = {16 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    Require(StorageTextureBudget(memory, nullptr, 0, 0) == 4 * GiB, "without VK_EXT_memory_budget the storage image cache does not keep a quarter of the 16 GiB heap");
    reported.heapBudget[0] = 100 * GiB;
    reported.heapBudget[1] = 100 * GiB;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 4 * GiB, "a zero budget for the largest device-local heap did not fall back to a quarter of the heap");
    reported.heapBudget[2] = 10 * GiB;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 2560 * MiB, "a 10 GiB budget on the 16 GiB heap does not limit the storage image cache to a quarter of the budget");
    reported.heapUsage[2] = 5 * GiB;
    Require(StorageTextureBudget(memory, &reported, 1 * GiB, 2 * GiB) == 2560 * MiB, "2 GiB used outside the texture caches on a 10 GiB budget lowered the storage image cache below its quarter");
    reported.heapUsage[2] = 8 * GiB + 512 * MiB;
    Require(StorageTextureBudget(memory, &reported, 1 * GiB, 2 * GiB) == 2304 * MiB, "a 10 GiB budget with 5632 MiB used outside the texture caches does not leave 2304 MiB after 1 GiB of sampled textures and 1280 MiB of headroom");
    reported.heapUsage[2] = 9 * GiB;
    Require(StorageTextureBudget(memory, &reported, 3 * GiB, 2 * GiB) == 1792 * MiB, "3 GiB of sampled textures were not reserved in full");
    reported.heapUsage[2] = 1 * GiB;
    Require(StorageTextureBudget(memory, &reported, 1 * GiB, 2 * GiB) == 2560 * MiB, "usage below the cached bytes is not read as no other device memory in use");
    Require(StorageTextureBudget(memory, &reported, 7 * GiB, 1 * GiB) == 1792 * MiB, "7 GiB of sampled textures with the usage below the cached bytes do not leave 1792 MiB after their reservation and 1280 MiB of headroom");
    reported.heapUsage[2] = 9 * GiB + 512 * MiB;
    Require(StorageTextureBudget(memory, &reported, 2 * GiB, 512 * MiB) == 512 * MiB, "a heap whose other users leave no room does not keep the 512 MiB floor");
    reported.heapUsage[2] = 0;
    reported.heapBudget[2] = 6 * GiB;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 1536 * MiB, "a 6 GiB budget on the 16 GiB heap does not limit the storage image cache to a quarter of the budget");
    reported.heapBudget[2] = 1 * GiB;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 512 * MiB, "a 1 GiB budget does not keep the 512 MiB floor");
    reported.heapBudget[2] = 40 * GiB;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 4 * GiB, "a budget above the heap size raised the storage image cache past a quarter of the heap");
    memory.memoryHeaps[2].size = 8 * GiB;
    reported.heapBudget[2] = 7189 * MiB;
    reported.heapUsage[2] = 4491 * MiB;
    Require(StorageTextureBudget(memory, &reported, 49 * MiB, 506 * MiB) == 7189 * MiB / 4u, "49 MiB of sampled textures on an 8 GiB heap with a 7189 MiB budget and 3936 MiB used outside the texture caches held the storage image cache below a quarter of the budget");
    reported.heapUsage[2] = 6490 * MiB;
    Require(StorageTextureBudget(memory, &reported, 2 * GiB, 506 * MiB) == 512 * MiB, "2 GiB of sampled textures on an 8 GiB heap with a 7189 MiB budget and 3936 MiB used outside the texture caches did not take the storage image cache back to its 512 MiB floor");
    memory.memoryHeaps[2].size = 10 * GiB;
    reported.heapBudget[2] = 8767 * MiB;
    reported.heapUsage[2] = 7981 * MiB;
    Require(StorageTextureBudget(memory, &reported, 1427 * MiB, 490 * MiB) == 512 * MiB, "a 10 GiB heap at 7981 of its 8767 MiB budget with 1427 MiB of sampled textures does not keep the storage image cache at its 512 MiB floor");
    for (const auto heapGiB : {2u, 4u, 6u, 8u, 16u, 32u}) {
        memory.memoryHeaps[2].size = heapGiB * GiB;
        for (std::uint64_t budget = GiB; budget <= heapGiB * GiB; budget += 512 * MiB) {
            reported.heapBudget[2] = budget;
            for (std::uint64_t sampled = 0; sampled <= budget; sampled += 256 * MiB) {
                std::uint64_t previous = 0;
                for (std::uint64_t outside = 0; outside <= budget + GiB; outside += 128 * MiB) {
                    reported.heapUsage[2] = outside + sampled + 512 * MiB;
                    const auto limit = StorageTextureBudget(memory, &reported, sampled, 512 * MiB);
                    Require(limit >= 512 * MiB, "pressure sweep lost the storage cache floor");
                    Require(limit <= std::max<std::uint64_t>(512 * MiB, budget / 4u), "pressure sweep exceeded a quarter of the reported budget above the floor");
                    Require(limit <= std::max<std::uint64_t>(2 * GiB, heapGiB * GiB / 4u), "pressure sweep exceeded the heap storage cache cap");
                    if (outside != 0) Require(limit <= previous, "additional non-cache memory usage grew the storage cache budget");
                    if (limit > 512 * MiB) Require(outside + sampled + budget / 8u + limit <= budget, "storage cache budget consumed live sampled memory or pressure headroom");
                    previous = limit;
                }
            }
        }
    }
    memory.memoryHeaps[0].flags = 0;
    memory.memoryHeaps[2].flags = 0;
    Require(StorageTextureBudget(memory, &reported, 0, 0) == 2 * GiB, "a device without a device-local heap does not keep 2 GiB of storage images");
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
    std::uint32_t clipDistanceLength = 0;
    std::uint32_t cullDistanceLength = 0;
    bool layerOutput = false;
    bool terminateInvocation = false;
    bool omitTerminateExtension = false;
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
    if (shape.layerOutput) {
        const auto pointer = id();
        const auto layer = id();
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassOutput, uintType});
        emit(declarations, spv::OpVariable, {pointer, layer, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {layer, spv::DecorationBuiltIn, spv::BuiltInLayer});
        extraInterface.push_back(layer);
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
    if (shape.clipDistanceLength != 0) {
        const auto length = id();
        const auto array = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpConstant, {uintType, length, shape.clipDistanceLength});
        emit(declarations, spv::OpTypeArray, {array, floatType, length});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassOutput, array});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, spv::BuiltInClipDistance});
        extraInterface.push_back(variable);
    }
    if (shape.cullDistanceLength != 0) {
        const auto length = id();
        const auto array = id();
        const auto pointer = id();
        const auto variable = id();
        emit(declarations, spv::OpConstant, {uintType, length, shape.cullDistanceLength});
        emit(declarations, spv::OpTypeArray, {array, floatType, length});
        emit(declarations, spv::OpTypePointer, {pointer, spv::StorageClassOutput, array});
        emit(declarations, spv::OpVariable, {pointer, variable, spv::StorageClassOutput});
        emit(annotations, spv::OpDecorate, {variable, spv::DecorationBuiltIn, spv::BuiltInCullDistance});
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
    emit(function, shape.terminateInvocation ? spv::OpTerminateInvocation : spv::OpReturn, {});
    emit(function, spv::OpFunctionEnd, {});
    std::vector<std::uint32_t> words{spv::MagicNumber, 0x10300, 0, next, 0};
    emit(words, spv::OpCapability, {spv::CapabilityShader});
    if (shape.sampleId) emit(words, spv::OpCapability, {spv::CapabilitySampleRateShading});
    if (shape.layer) emit(words, spv::OpCapability, {spv::CapabilityGeometry});
    if (shape.clipDistanceLength != 0) emit(words, spv::OpCapability, {spv::CapabilityClipDistance});
    if (shape.cullDistanceLength != 0) emit(words, spv::OpCapability, {spv::CapabilityCullDistance});
    if (shape.layerOutput) {
        emit(words, spv::OpCapability, {spv::CapabilityShaderViewportIndexLayerEXT});
        const std::string extension = "SPV_EXT_shader_viewport_index_layer";
        const auto count = (extension.size() + 4) / 4;
        words.push_back((static_cast<std::uint32_t>(count + 1) << 16u) | spv::OpExtension);
        const auto start = words.size();
        words.resize(start + count, 0);
        for (std::size_t i = 0; i < extension.size(); ++i) words[start + i / 4] |= static_cast<std::uint32_t>(static_cast<unsigned char>(extension[i])) << ((i % 4) * 8);
    }
    if (shape.barycentric) {
        emit(words, spv::OpCapability, {spv::CapabilityFragmentBarycentricKHR});
        const std::string extension = "SPV_KHR_fragment_shader_barycentric";
        const auto count = (extension.size() + 4) / 4;
        words.push_back((static_cast<std::uint32_t>(count + 1) << 16u) | spv::OpExtension);
        const auto start = words.size();
        words.resize(start + count, 0);
        for (std::size_t i = 0; i < extension.size(); ++i) words[start + i / 4] |= static_cast<std::uint32_t>(static_cast<unsigned char>(extension[i])) << ((i % 4) * 8);
    }
    if (shape.terminateInvocation && !shape.omitTerminateExtension) {
        const std::string extension = "SPV_KHR_terminate_invocation";
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

struct SampleLocationsUse {
    VkExtent2D grid{};
    std::vector<VkSampleLocationEXT> locations;
};

std::vector<SampleLocationsUse> pipelineSampleLocations;
std::vector<SampleLocationsUse> passSampleLocations;

void recordSampleLocations(std::vector<SampleLocationsUse>& uses, const VkSampleLocationsInfoEXT& info) {
    uses.push_back({info.sampleLocationGridSize, {info.pSampleLocations, info.pSampleLocations + info.sampleLocationsCount}});
}

VKAPI_ATTR VkResult VKAPI_CALL sampleLocationCreateGraphicsPipelines(VkDevice, VkPipelineCache, std::uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks*, VkPipeline* pipelines) {
    Require(count == 1, "the sample location mock expects one pipeline per call");
    const auto* locations = infos[0].pMultisampleState != nullptr ? static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(infos[0].pMultisampleState->pNext) : nullptr;
    if (locations != nullptr && locations->sType == VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT && locations->sampleLocationsEnable) recordSampleLocations(pipelineSampleLocations, locations->sampleLocationsInfo);
    *pipelines = makeHandle<VkPipeline>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL sampleLocationCreateRenderPass(VkDevice, const VkRenderPassCreateInfo*, const VkAllocationCallbacks*, VkRenderPass* pass) {
    *pass = makeHandle<VkRenderPass>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL sampleLocationCreateFramebuffer(VkDevice, const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer* framebuffer) {
    *framebuffer = makeHandle<VkFramebuffer>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL sampleLocationDestroyRenderPass(VkDevice, VkRenderPass, const VkAllocationCallbacks*) { --mock.live; }
VKAPI_ATTR void VKAPI_CALL sampleLocationDestroyFramebuffer(VkDevice, VkFramebuffer, const VkAllocationCallbacks*) { --mock.live; }
VKAPI_ATTR void VKAPI_CALL sampleLocationSetViewport(VkCommandBuffer, std::uint32_t, std::uint32_t, const VkViewport*) {}
VKAPI_ATTR void VKAPI_CALL sampleLocationSetScissor(VkCommandBuffer, std::uint32_t, std::uint32_t, const VkRect2D*) {}

VKAPI_ATTR void VKAPI_CALL sampleLocationBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo* begin, VkSubpassContents) {
    const auto* locations = static_cast<const VkRenderPassSampleLocationsBeginInfoEXT*>(begin->pNext);
    if (locations == nullptr || locations->sType != VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT) return;
    for (std::uint32_t index = 0; index < locations->attachmentInitialSampleLocationsCount; ++index) recordSampleLocations(passSampleLocations, locations->pAttachmentInitialSampleLocations[index].sampleLocationsInfo);
    for (std::uint32_t index = 0; index < locations->postSubpassSampleLocationsCount; ++index) recordSampleLocations(passSampleLocations, locations->pPostSubpassSampleLocations[index].sampleLocationsInfo);
}

PFN_vkVoidFunction VKAPI_CALL sampleLocationProc(VkDevice device, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationCreateGraphicsPipelines)},
        {"vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationCreateRenderPass)},
        {"vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationDestroyRenderPass)},
        {"vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationCreateFramebuffer)},
        {"vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationDestroyFramebuffer)},
        {"vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationBeginRenderPass)},
        {"vkCmdSetViewport", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationSetViewport)},
        {"vkCmdSetScissor", reinterpret_cast<PFN_vkVoidFunction>(sampleLocationSetScissor)}
    };
    const auto it = table.find(name);
    return it != table.end() ? it->second : mockProc(device, name);
}

void sampleLocationGridFallbackTests() {
    using namespace AgcDriver::Graphics;
    auto queue = makeState();
    queue.context[0x2f8] = 0x00100001;
    queue.context[0x292] = 3;
    queue.context[0x3b8] = 0x0dc00000u;
    queue.context[0x31d] = 0x9000;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    for (const auto [pixel, word] : {std::pair{0u, 0x0c04u}, std::pair{1u, 0x0c04u}, std::pair{2u, 0x040cu}, std::pair{3u, 0x040cu}}) queue.context[0x2fe + pixel * 4u] = word;
    const auto checkerboard = DecodeState(queue);
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = 0x0c04u;
    const auto uniform = DecodeState(queue);
    Require(checkerboard.samples == 2 && checkerboard.customSampleLocations && checkerboard.sampleLocationGrid && uniform.samples == 2 && uniform.customSampleLocations && !uniform.sampleLocationGrid, "the 2x checkerboard (PPSA01556) or the uniform pattern of its pixel (0,0) did not decode");
    auto sampleZero = checkerboard;
    sampleZero.samples = 1;
    sampleZero.sampleLocations = {};
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) sampleZero.sampleLocations[pixel * 8u] = checkerboard.sampleLocations[pixel * 8u];
    const auto previousDepthSamples = depthSampleCounts;
    depthSampleCounts = VK_SAMPLE_COUNT_2_BIT;
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.spirv = makeModule({});
    fragment.spirv = makeModule({.fragment = true});
    vertex.variantId = 1;
    fragment.variantId = 2;
    const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
    const std::array<std::uint32_t, 1> mainKey{41};
    const auto draw = [&](const State& decoded, VkSampleCountFlags gridSamples, bool libraries, bool depth) {
        mock = MockVulkan{};
        pipelineSampleLocations.clear();
        passSampleLocations.clear();
        auto context = mockContext();
        context.deviceProc = sampleLocationProc;
        context.device = makeHandle<VkDevice>();
        context.graphicsPipelineLibrary = libraries;
        context.sampleLocationSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT;
        context.sampleLocationGridSampleCounts = gridSamples;
        context.imageFormatProperties = sampleLocationDepthProperties;
        context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT;
        context.limits.framebufferDepthSampleCounts = VK_SAMPLE_COUNT_2_BIT;
        context.limits.maxFramebufferWidth = 64;
        context.limits.maxFramebufferHeight = 64;
        context.limits.maxFramebufferLayers = 1;
        State state{};
        state.samples = decoded.samples;
        state.sampleLocations = decoded.sampleLocations;
        state.customSampleLocations = decoded.customSampleLocations;
        state.sampleLocationGrid = decoded.sampleLocationGrid;
        if (depth) {
            state.depth = DepthTarget{0, 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
            state.depth->samples = state.samples;
            state.depthTest = true;
            state.depthCompare = VK_COMPARE_OP_LESS;
        }
        const PipelineSetLayouts resources{makeHandle<VkDescriptorSetLayout>(), VK_NULL_HANDLE, false, mainKey, {}};
        if (libraries) {
            Require(Pipeline::Prebuild(context, state, VertexInputLayout{}, resources, shaders), "the sample location pipeline's libraries were not prebuilt");
        } else {
            Pipeline pipeline(context, state, VertexInputLayout{}, resources, shaders, VK_IMAGE_LAYOUT_GENERAL);
            const std::array<VkImageView, 1> depthView{makeHandle<VkImageView>()};
            const auto framebuffer = pipeline.AcquireFramebuffer(std::span(depthView).first(depth ? 1u : 0u), {}, {64, 64});
            pipeline.Begin(makeHandle<VkCommandBuffer>(), *framebuffer, {64, 64}, state);
        }
        ClearPipelineLibraries(context.device);
        Require(mock.live == 0, "sample location grid fallback tests leaked Vulkan objects");
        Require(pipelineSampleLocations.size() == (libraries ? 2u : 1u) && passSampleLocations.size() == (libraries ? 0u : depth ? 2u : 1u), "the pipeline or render pass did not take its sample locations once per library or attachment");
        auto uses = pipelineSampleLocations;
        uses.insert(uses.end(), passSampleLocations.begin(), passSampleLocations.end());
        return uses;
    };
    const auto expect = [](const std::vector<SampleLocationsUse>& uses, VkExtent2D grid, std::span<const VkSampleLocationEXT> locations, const std::string& what) {
        for (const auto& use : uses) {
            Require(use.grid.width == grid.width && use.grid.height == grid.height && use.locations.size() == locations.size(), what + " took the wrong sample location grid");
            for (std::size_t index = 0; index < locations.size(); ++index) Require(use.locations[index].x == locations[index].x && use.locations[index].y == locations[index].y, what + " took the wrong sample locations");
        }
    };
    const std::array<VkSampleLocationEXT, 2> pixelZero{{{0.75f, 0.5f}, {0.25f, 0.5f}}};
    const std::array<VkSampleLocationEXT, 8> grid{{{0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}}};
    const std::array<VkSampleLocationEXT, 1> sampleZeroPixelZero{{{0.75f, 0.5f}}};
    const std::array<VkSampleLocationEXT, 4> sampleZeroGrid{{{0.75f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f}}};
    for (const bool libraries : {true, false}) {
        for (const bool depth : {false, true}) {
            for (const VkSampleCountFlags gridSamples : {VkSampleCountFlags{0}, VkSampleCountFlags{VK_SAMPLE_COUNT_4_BIT}}) expect(draw(checkerboard, gridSamples, libraries, depth), {1, 1}, pixelZero, "a 2x checkerboard on a device without a 2x2 grid for 2 samples");
            expect(draw(checkerboard, VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT, libraries, depth), {2, 2}, grid, "a 2x checkerboard on a device with a 2x2 grid for 2 samples");
            for (const VkSampleCountFlags gridSamples : {VkSampleCountFlags{0}, VkSampleCountFlags{VK_SAMPLE_COUNT_2_BIT}}) expect(draw(uniform, gridSamples, libraries, depth), {1, 1}, pixelZero, "a uniform 2x pattern");
        }
        expect(draw(sampleZero, VK_SAMPLE_COUNT_2_BIT, libraries, false), {1, 1}, sampleZeroPixelZero, "one sample at sample 0's checkerboard locations on a device without a one-sample grid");
        expect(draw(sampleZero, VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT, libraries, false), {2, 2}, sampleZeroGrid, "one sample at sample 0's checkerboard locations on a device with a one-sample grid");
    }
    depthSampleCounts = previousDepthSamples;
    mock = MockVulkan{};
}

std::string varyingIndex(std::span<const std::uint32_t> code) {
    Require(code.size() > 5 && code[0] == spv::MagicNumber, "a sample order shader is not a SPIR-V module");
    std::set<std::uint32_t> constants;
    for (std::size_t at = 5; at < code.size();) {
        const auto count = code[at] >> 16u;
        const auto opcode = static_cast<spv::Op>(code[at] & 0xffffu);
        Require(count != 0 && at + count <= code.size(), "a sample order shader holds a malformed instruction");
        if (opcode == spv::OpConstant) constants.insert(code[at + 2]);
        if (opcode == spv::OpVectorExtractDynamic || opcode == spv::OpVectorInsertDynamic) return "a vector component chosen per invocation (opcode " + std::to_string(opcode) + ")";
        if (opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain || opcode == spv::OpPtrAccessChain || opcode == spv::OpInBoundsPtrAccessChain) {
            for (std::size_t operand = 4; operand < count; ++operand) {
                if (!constants.contains(code[at + operand])) return "an access chain into %" + std::to_string(code[at + 3]) + " through the non-constant index %" + std::to_string(code[at + operand]);
            }
        }
        at += count;
    }
    return {};
}

VkSampleCountFlags orderStorageCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
bool orderSixteenBitStencil = true;
std::vector<VkSampleMask> pipelineSampleMasks;

VKAPI_ATTR VkResult VKAPI_CALL sampleOrderImageProperties(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags usage, VkImageCreateFlags, VkImageFormatProperties* properties) {
    *properties = {};
    properties->maxExtent = {16384, 16384, 1};
    properties->maxMipLevels = 1;
    properties->maxArrayLayers = 1;
    properties->sampleCounts = (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0 ? orderStorageCounts : VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL sampleOrderFormatProperties(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format == VK_FORMAT_D16_UNORM_S8_UINT && !orderSixteenBitStencil) return;
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
}

VKAPI_ATTR VkResult VKAPI_CALL sampleOrderCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache, std::uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks* allocator, VkPipeline* pipelines) {
    const auto* samples = infos[0].pMultisampleState;
    pipelineSampleMasks.push_back(samples != nullptr && samples->pSampleMask != nullptr ? *samples->pSampleMask : ~VkSampleMask{0});
    return sampleLocationCreateGraphicsPipelines(device, cache, count, infos, allocator, pipelines);
}

PFN_vkVoidFunction VKAPI_CALL sampleOrderProc(VkDevice device, const char* name) {
    if (std::string_view(name) == "vkCreateGraphicsPipelines") return reinterpret_cast<PFN_vkVoidFunction>(sampleOrderCreateGraphicsPipelines);
    return sampleLocationProc(device, name);
}

struct DepthOrderEvent {
    bool copy = false;
    VkImage image = VK_NULL_HANDLE;
    VkImageLayout from = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout to = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageAspectFlags aspects = 0;
    VkExtent2D grid{};
    std::vector<VkSampleLocationEXT> locations;
};

std::vector<DepthOrderEvent> depthOrderEvents;

VKAPI_ATTR VkResult VKAPI_CALL depthOrderCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage* image) {
    *image = makeHandle<VkImage>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL depthOrderDestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) { --mock.live; }
VKAPI_ATTR void VKAPI_CALL depthOrderImageRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements) { *requirements = {256, 1, 1}; }
VKAPI_ATTR VkResult VKAPI_CALL depthOrderBindImage(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL depthOrderCreateView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view) {
    *view = makeHandle<VkImageView>();
    ++mock.live;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL depthOrderDestroyView(VkDevice, VkImageView, const VkAllocationCallbacks*) { --mock.live; }

VKAPI_ATTR void VKAPI_CALL depthOrderBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t count, const VkImageMemoryBarrier* barriers) {
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& barrier = barriers[index];
        DepthOrderEvent event{false, barrier.image, barrier.oldLayout, barrier.newLayout, barrier.subresourceRange.aspectMask, {}, {}};
        const auto* locations = static_cast<const VkSampleLocationsInfoEXT*>(barrier.pNext);
        if (locations != nullptr && locations->sType == VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT) {
            event.grid = locations->sampleLocationGridSize;
            event.locations.assign(locations->pSampleLocations, locations->pSampleLocations + locations->sampleLocationsCount);
        }
        depthOrderEvents.push_back(event);
    }
}

VKAPI_ATTR void VKAPI_CALL depthOrderCopy(VkCommandBuffer, VkImage source, VkImageLayout layout, VkImage, VkImageLayout, std::uint32_t, const VkImageCopy*) {
    depthOrderEvents.push_back({true, source, layout, layout, 0, {}, {}});
}

VKAPI_ATTR void VKAPI_CALL depthOrderEndPass(VkCommandBuffer) {}
VKAPI_ATTR void VKAPI_CALL depthOrderDraw(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL depthOrderStencil(VkCommandBuffer, VkStencilFaceFlags, std::uint32_t) {}

PFN_vkVoidFunction VKAPI_CALL depthOrderProc(VkDevice device, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(depthOrderCreateImage)},
        {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(depthOrderDestroyImage)},
        {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(depthOrderImageRequirements)},
        {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(depthOrderBindImage)},
        {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(depthOrderCreateView)},
        {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(depthOrderDestroyView)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(depthOrderBarrier)},
        {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(depthOrderCopy)},
        {"vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(depthOrderEndPass)},
        {"vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(depthOrderDraw)},
        {"vkCmdSetStencilWriteMask", reinterpret_cast<PFN_vkVoidFunction>(depthOrderStencil)},
        {"vkCmdSetStencilReference", reinterpret_cast<PFN_vkVoidFunction>(depthOrderStencil)}
    };
    const auto it = table.find(name);
    return it != table.end() ? it->second : sampleLocationProc(device, name);
}

void sampleOrderTests() {
    using namespace AgcDriver::Graphics;
    auto queue = makeState();
    queue.context[0x2f8] = 0x00100001;
    queue.context[0x292] = 3;
    queue.context[0x3b8] = 0x0dc00000u;
    queue.context[0x31d] = 0x9000;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto decode = [&](std::array<std::uint32_t, 4> words) {
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) queue.context[0x2fe + pixel * 4u] = words[pixel];
        return DecodeState(queue);
    };
    const auto village = decode({0x0c04u, 0x0c04u, 0x040cu, 0x040cu});
    const auto residentEvil4 = decode({0x040cu, 0x040cu, 0x0c04u, 0x0c04u});
    const auto uniform = decode({0x0c04u, 0x0c04u, 0x0c04u, 0x0c04u});
    const auto moved = decode({0x0c04u, 0x0c04u, 0x040bu, 0x040bu});
    const SampleOrder rowSwap{KeptSampleOrder, KeptSampleOrder, 0x76543201u, 0x76543201u};
    Require(SameSetSampleOrder(village) == rowSwap, "the 2x checkerboard (PPSA01556) did not reorder only its odd rows' two samples");
    Require(SameSetSampleOrder(residentEvil4) == rowSwap, "the 2x checkerboard with its rows swapped (PPSA07411) did not reorder only its odd rows' two samples");
    Require(!SameSetSampleOrder(uniform) && !SameSetSampleOrder(moved), "a uniform pattern or one whose odd rows hold other positions was taken as a reordering of one set");
    State four{};
    four.samples = 4;
    four.customSampleLocations = true;
    four.sampleLocationGrid = true;
    const std::array<std::uint8_t, 4> positions{0x2eu, 0xe6u, 0x2au, 0x62u};
    const std::array<std::array<std::uint32_t, 4>, 4> placed{{{0, 1, 2, 3}, {1, 0, 3, 2}, {2, 3, 1, 0}, {3, 2, 0, 1}}};
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        for (std::uint32_t sample = 0; sample < 4; ++sample) four.sampleLocations[pixel * 8u + placed[pixel][sample]] = positions[sample];
    }
    const SampleOrder fourOrder{KeptSampleOrder, 0x76542301u, 0x76540132u, 0x76541023u};
    Require(SameSetSampleOrder(four) == fourOrder, "a 4x pattern whose pixels hold one set of positions in four orders did not give each pixel its order");
    Require(InvertSampleOrder(0x76543201u) == 0x76543201u && InvertSampleOrder(0x76540132u) == 0x76541023u && InvertSampleOrder(KeptSampleOrder) == KeptSampleOrder, "inverting a sample order did not undo it");
    Require(HostSampleBits(0x1u, 0x76543201u) == 0x2u && HostSampleBits(0x1u, 0x76540132u) == 0x8u && HostSampleBits(0xffffffffu, 0x76540132u) == 0xffffffffu, "a guest sample mask did not move to the host samples at its guest samples' locations");
    Require(ReordersSamples(rowSwap) && !ReordersSamples(SampleOrder{}) && !ReordersSamples(SampleOrder{KeptSampleOrder, KeptSampleOrder, KeptSampleOrder, KeptSampleOrder}), "a sample order that keeps every index counted as a reordering");
    Require(!HostsSampleLocationGrid({0, 0}) && !HostsSampleLocationGrid({1, 1}) && !HostsSampleLocationGrid({2, 1}) && !HostsSampleLocationGrid({3, 2}) && HostsSampleLocationGrid({2, 2}) && HostsSampleLocationGrid({4, 2}), "a device's largest sample location grid was taken for one that holds a 2x2 grid, or not, wrongly");
    std::size_t module = 0;
    for (const auto code : SampleOrderShaderModules()) {
        const auto varying = varyingIndex(code);
        Require(varying.empty(), "sample order shader " + std::to_string(module) + " picks a pixel's order word or a held sample through " + varying);
        ++module;
    }

    const auto capable = [] {
        auto context = mockContext();
        context.physical = makeHandle<VkPhysicalDevice>();
        context.device = makeHandle<VkDevice>();
        context.deviceProc = sampleOrderProc;
        context.imageFormatProperties = sampleOrderImageProperties;
        context.formatProperties = sampleOrderFormatProperties;
        context.sampleLocationSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
        context.sampleLocationGridSampleCounts = 0;
        context.storageImageMultisample = true;
        context.sampleRateShading = true;
        context.shaderStencilExport = true;
        context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
        context.limits.framebufferDepthSampleCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
        context.limits.framebufferColorSampleCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
        context.limits.maxColorAttachments = 8;
        context.limits.maxFramebufferWidth = 64;
        context.limits.maxFramebufferHeight = 64;
        context.limits.maxFramebufferLayers = 1;
        return context;
    };
    const auto attached = [](const State& decoded, VkFormat depthFormat, bool stencil) {
        State state = decoded;
        ColorTarget color{};
        color.address = 0x100000u;
        color.extent = {64, 64};
        color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        color.elementBytes = 8;
        color.samples = state.samples;
        state.colors = {color};
        state.color = color;
        state.hasColorTarget = true;
        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask = 0xf;
        state.blends = {blend};
        state.renderExtent = {64, 64};
        if (depthFormat != VK_FORMAT_UNDEFINED) {
            state.depth = DepthTarget{0x200000u, stencil ? 0x300000u : 0u, {64, 64}, depthFormat, 1.0f, 0};
            state.depth->samples = state.samples;
        }
        return state;
    };
    const auto withStencil = attached(village, VK_FORMAT_D32_SFLOAT_S8_UINT, true);
    {
        const auto context = capable();
        const auto host = HostSampleLocationsFor(context, withStencil);
        Require(host.grid.width == 1 && host.grid.height == 1 && host.order == rowSwap, "a 2x checkerboard on a device without a 2x2 grid that can reorder samples did not take the stand-in with its odd rows reordered");
        auto gridded = context;
        gridded.sampleLocationGridSampleCounts = VK_SAMPLE_COUNT_2_BIT;
        const auto native = HostSampleLocationsFor(gridded, withStencil);
        Require(native.grid.width == 2 && native.grid.height == 2 && !ReordersSamples(native.order), "a 2x checkerboard on a device with a 2x2 grid was reordered");
        Require(!ReordersSamples(HostSampleLocationsFor(context, attached(uniform, VK_FORMAT_D32_SFLOAT, false)).order) && !ReordersSamples(HostSampleLocationsFor(context, attached(moved, VK_FORMAT_D32_SFLOAT, false)).order), "a uniform pattern, or one whose pixels hold other positions, was reordered");
        const auto refused = [&](Context device, const State& state, std::string_view reason, const std::string& what) {
            Require(!ReordersSamples(HostSampleLocationsFor(device, state).order), what + " was reordered");
            Require(SampleOrderRefusal(device, state).find(reason) != std::string::npos, what + " was not refused for " + std::string(reason) + ": " + SampleOrderRefusal(device, state));
        };
        auto noStorage = context;
        noStorage.storageImageMultisample = false;
        auto depthOnly = withStencil;
        depthOnly.colors.clear();
        depthOnly.blends.clear();
        depthOnly.hasColorTarget = false;
        Require(ReordersSamples(HostSampleLocationsFor(context, depthOnly).order), "a depth-only pass on a device with multisampled storage images was not reordered");
        refused(noStorage, depthOnly, "lacks shaderStorageImageMultisample", "a depth-only pass on a device without multisampled storage images");
        Require(HostSampleLocationsFor(noStorage, depthOnly).order == HostSampleLocationsFor(noStorage, withStencil).order, "a depth-only pass and a color pass on one depth target took different sample orders on a device without multisampled storage images");
        refused(noStorage, withStencil, "lacks shaderStorageImageMultisample", "a color target on a device without multisampled storage images");
        auto noSampleShading = context;
        noSampleShading.sampleRateShading = false;
        refused(noSampleShading, withStencil, "sampleRateShading", "a device without sample-rate shading");
        orderStorageCounts = VK_SAMPLE_COUNT_4_BIT;
        refused(context, withStencil, "has no storage view at 2 samples", "a color format without 2x storage");
        orderStorageCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
        auto twentyFour = context;
        twentyFour.physical = makeHandle<VkPhysicalDevice>();
        orderSixteenBitStencil = false;
        refused(twentyFour, attached(village, VK_FORMAT_D16_UNORM_S8_UINT, true), "is not written back exactly", "16-bit depth with stencil kept as 24-bit depth");
        orderSixteenBitStencil = true;
        auto sixteen = context;
        sixteen.physical = makeHandle<VkPhysicalDevice>();
        Require(ReordersSamples(HostSampleLocationsFor(sixteen, attached(village, VK_FORMAT_D16_UNORM_S8_UINT, true)).order), "16-bit depth with stencil kept as 16-bit depth was not reordered");

        using Method = SampleOrderMethod;
        const auto steps = [](const std::vector<SampleOrderStep>& plan, std::initializer_list<SampleOrderStep> expected, const std::string& what) {
            Require(plan.size() == expected.size(), what + " planned " + std::to_string(plan.size()) + " attachments");
            std::size_t index = 0;
            for (const auto& step : expected) {
                const auto& planned = plan[index++];
                Require(planned.attachment == step.attachment && planned.method == step.method && planned.toHost == step.toHost && planned.toGuest == step.toGuest, what + " planned attachment " + std::to_string(planned.attachment) + " differently");
            }
        };
        steps(PlanSampleOrder(context, withStencil, rowSwap), {{0, Method::Storage, true, true}, {1, Method::DepthStencilExport, true, true}}, "a color and depth-stencil pass with stencil export");
        auto noExport = context;
        noExport.shaderStencilExport = false;
        steps(PlanSampleOrder(noExport, withStencil, rowSwap), {{0, Method::Storage, true, true}, {1, Method::DepthStencilBits, true, true}}, "a color and depth-stencil pass without stencil export");
        steps(PlanSampleOrder(context, attached(village, VK_FORMAT_D32_SFLOAT, false), rowSwap), {{0, Method::Storage, true, true}, {1, Method::Depth, true, true}}, "a color and depth pass");
        steps(PlanSampleOrder(context, attached(village, VK_FORMAT_D32_SFLOAT_S8_UINT, false), rowSwap), {{0, Method::Storage, true, true}, {1, Method::Depth, true, true}}, "a depth target whose host format has stencil but the guest's has none");
        steps(PlanSampleOrder(context, withStencil, rowSwap, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE), {{0, Method::Storage, false, true}, {1, Method::DepthStencilExport, false, true}}, "a pass that clears its attachments");
        steps(PlanSampleOrder(context, withStencil, rowSwap, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE), {{0, Method::Storage, true, false}, {1, Method::DepthStencilExport, true, false}}, "a pass that discards its attachments");
        steps(PlanSampleOrder(context, withStencil, rowSwap, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE), {}, "a pass that neither loads nor stores");
        steps(PlanSampleOrder(context, withStencil, SampleOrder{}), {}, "a pass that keeps the guest's order");
        auto single = withStencil;
        single.samples = 1;
        single.colors[0].samples = 1;
        single.depth->samples = 1;
        steps(PlanSampleOrder(context, single, rowSwap), {}, "a single-sampled pass");
    }

    auto masked = attached(village, VK_FORMAT_UNDEFINED, false);
    masked.quadSampleMask = {1, 1, 1, 1};
    Require(SampleMaskFollowsOrder(masked, rowSwap) && QuadSampleMasks(masked, rowSwap) == std::array<std::uint16_t, 4>{1, 1, 1, 1} && QuadSampleMasks(masked) == std::array<std::uint16_t, 4>{0xffffu, 0xffffu, 0xffffu, 0xffffu}, "a uniform mask of sample 0 did not move to the pixel shader under the checkerboard's order");
    auto empty = masked;
    empty.quadSampleMask = {0, 0, 0, 0};
    Require(!SampleMaskFollowsOrder(empty, rowSwap) && !SampleMaskFollowsOrder(masked, SampleOrder{}), "an empty mask, or a mask without a reordering, moved to the pixel shader");
    {
        auto earlyTested = attached(village, VK_FORMAT_D32_SFLOAT, false);
        earlyTested.quadSampleMask = {1, 1, 1, 1};
        earlyTested.pixelEarlyZ = true;
        earlyTested.depthTest = true;
        ShaderRecompiler::RecompileResult memoryWriter, memoryKeeper;
        memoryWriter.spirv = makeModule({.fragment = true});
        memoryKeeper.spirv = memoryWriter.spirv;
        ShaderRecompiler::DescriptorBinding written{};
        written.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        written.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        written.count = 1;
        written.bufferWritten = {true};
        memoryWriter.bindings.push_back(written);
        const std::array<CompiledShader, 1> writing{{{ShaderRecompiler::ShaderStage::Fragment, &memoryWriter, 0}}};
        const std::array<CompiledShader, 1> notWriting{{{ShaderRecompiler::ShaderStage::Fragment, &memoryKeeper, 0}}};
        const auto context = capable();
        Require(SampleMaskFollowsOrder(earlyTested, HostSampleLocationsFor(context, earlyTested).order), "a uniform mask of sample 0 with a depth target did not move to the pixel shader under the checkerboard's order");
        expectFailure([&] { ValidateSampleMask(context, earlyTested, writing); }, "from a pixel program with early depth or stencil tests that writes memory, is unsupported");
        ValidateSampleMask(context, earlyTested, notWriting);
        auto gridded = context;
        gridded.sampleLocationGridSampleCounts = VK_SAMPLE_COUNT_2_BIT;
        ValidateSampleMask(gridded, earlyTested, writing);
    }

    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.spirv = makeModule({});
    fragment.spirv = makeModule({.fragment = true});
    vertex.variantId = 1;
    fragment.variantId = 2;
    const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
    const std::array<std::uint32_t, 1> mainKey{41};
    const std::array<VkSampleLocationEXT, 2> pixelZero{{{0.75f, 0.5f}, {0.25f, 0.5f}}};
    for (const bool libraries : {false, true}) {
        for (const bool maskedPass : {false, true}) {
            mock = MockVulkan{};
            pipelineSampleLocations.clear();
            passSampleLocations.clear();
            pipelineSampleMasks.clear();
            auto context = capable();
            context.graphicsPipelineLibrary = libraries;
            context.pipelineLinkTimeOptimization = false;
            auto state = maskedPass ? masked : attached(village, VK_FORMAT_UNDEFINED, false);
            const PipelineSetLayouts resources{makeHandle<VkDescriptorSetLayout>(), VK_NULL_HANDLE, false, mainKey, {}};
            std::size_t reorderedMasks = 0;
            {
                Pipeline pipeline(context, state, VertexInputLayout{}, resources, shaders, VK_IMAGE_LAYOUT_GENERAL);
                Require(pipeline.ReordersSamples() && pipeline.HostSampleOrder() == rowSwap, "a 2x checkerboard pipeline on a device without a 2x2 grid did not reorder its odd rows");
                reorderedMasks = pipelineSampleMasks.size();
                auto gridded = context;
                gridded.sampleLocationGridSampleCounts = VK_SAMPLE_COUNT_2_BIT;
                Pipeline native(gridded, state, VertexInputLayout{}, resources, shaders, VK_IMAGE_LAYOUT_GENERAL);
                Require(!native.ReordersSamples(), "a 2x checkerboard pipeline on a device with a 2x2 grid reordered its samples");
            }
            ClearPipelineLibraries(context.device);
            Require(mock.live == 0, "sample order pipeline tests leaked Vulkan objects");
            Require(!pipelineSampleLocations.empty(), "the reordered pipeline took no sample locations");
            const auto& standIn = pipelineSampleLocations.front();
            Require(standIn.grid.width == 1 && standIn.grid.height == 1 && standIn.locations.size() == 2 && standIn.locations[0].x == pixelZero[0].x && standIn.locations[1].x == pixelZero[1].x, "the reordered pipeline did not draw with pixel (0,0)'s locations");
            const auto keepsMask = [&](std::size_t begin, std::size_t end) { return std::any_of(pipelineSampleMasks.begin() + static_cast<std::ptrdiff_t>(begin), pipelineSampleMasks.begin() + static_cast<std::ptrdiff_t>(end), [](VkSampleMask mask) { return mask == 1u; }); };
            Require(reorderedMasks != 0 && !keepsMask(0, reorderedMasks), "the reordered pipeline kept a static sample mask its odd rows would apply to the other sample");
            Require(keepsMask(reorderedMasks, pipelineSampleMasks.size()) == maskedPass, "the pipeline on a device with a 2x2 grid did not keep the static sample mask exactly when the pass has one");
        }
    }
    mock = MockVulkan{};

    {
        auto context = capable();
        context.deviceProc = depthOrderProc;
        context.memory.memoryTypes[0].propertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const auto commands = makeHandle<VkCommandBuffer>();
        const auto depthImage = makeHandle<VkImage>();
        const SampleOrderTarget target{depthImage, makeHandle<VkImageView>(), VK_FORMAT_D32_SFLOAT_S8_UINT, {16, 8}, 2, SampleOrderMethod::Depth, nullptr};
        const auto onTarget = [&] {
            std::vector<DepthOrderEvent> events;
            for (const auto& event : depthOrderEvents) {
                if (event.image == depthImage) events.push_back(event);
            }
            return events;
        };
        const auto located = [](const DepthOrderEvent& event, VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspects, VkExtent2D grid, std::span<const VkSampleLocationEXT> locations) {
            if (event.copy || event.from != from || event.to != to || event.aspects != aspects || event.grid.width != grid.width || event.grid.height != grid.height || event.locations.size() != locations.size()) return false;
            for (std::size_t index = 0; index < locations.size(); ++index) {
                if (event.locations[index].x != locations[index].x || event.locations[index].y != locations[index].y) return false;
            }
            return true;
        };
        constexpr VkImageAspectFlags depthStencil = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        std::vector<std::shared_ptr<void>> kept;
        for (const bool standIn : {true, false}) {
            depthOrderEvents.clear();
            SampleOrderPass pass{rowSwap, {16, 8}, {}, standIn};
            std::copy(pixelZero.begin(), pixelZero.end(), pass.locations.begin());
            RecordSampleOrder(context, commands, pass, std::span(&target, 1u), false, kept);
            const auto events = onTarget();
            if (standIn) {
                Require(events.size() == 3 && located(events[0], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthStencil, {1, 1}, pixelZero) && events[1].copy && events[1].from == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && located(events[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, depthStencil, {1, 1}, pixelZero), "the depth swap did not copy its target between transitions that name the stand-in sample locations it was rendered with");
            } else {
                Require(events.size() == 1 && events[0].copy && events[0].from == VK_IMAGE_LAYOUT_GENERAL, "the depth swap of a pass without sample locations transitioned its target");
            }
        }
        depthOrderEvents.clear();
        DepthSampleLocations depth;
        depth.image = depthImage;
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = 2;
        depth.grid = {2, 2};
        const std::array<VkSampleLocationEXT, 8> grid{{{0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.5f}, {0.75f, 0.5f}}};
        std::copy(grid.begin(), grid.end(), depth.locations.begin());
        RecordDepthSampleLocations(context, commands, depth);
        const auto events = onTarget();
        Require(events.size() == 2 && located(events[0], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, {2, 2}, grid) && located(events[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT, {2, 2}, grid), "a depth target drawn on a 2x2 sample location grid was not left in the general layout through transitions that name that grid's locations");
        kept.clear();
        ClearSampleOrders(context.device);
        Require(mock.live == 0, "the depth swap tests leaked Vulkan objects");
        depthOrderEvents.clear();
    }
    mock = MockVulkan{};

    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, 0x20000u, {}, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = ShaderRecompiler::ShaderPixelStageInfo{};
    const auto keptHash = ShaderRecompiler::RecompileCacheKey::ContextHash(request);
    request.context.pixel->sampleOrder = rowSwap;
    Require(ShaderRecompiler::RecompileCacheKey::ContextHash(request) != keptHash, "the pixel shader's sample order did not key its variant");
    const ShaderRecompiler::RequestSerializer serializer;
    const auto back = serializer.Deserialize(serializer.Serialize(request));
    Require(back.request.context.pixel.has_value() && back.request.context.pixel->sampleOrder == rowSwap, "the recompile request lost the pixel shader's sample order");
}

void pipelineLibrarySetTests() {
    using namespace AgcDriver::Graphics;
    for (const auto mask : {0u, 1u, 2u, 3u}) {
        mock = MockVulkan{};
        auto context = mockContext();
        context.graphicsPipelineLibrary = true;
        context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT;
        State state{};
        ShaderRecompiler::RecompileResult vertex, fragment;
        vertex.spirv = makeModule({});
        fragment.spirv = makeModule({.fragment = true});
        vertex.variantId = 1;
        fragment.variantId = 2;
        for (std::uint32_t set = 0; set < 2; ++set) {
            ShaderRecompiler::DescriptorBinding binding{};
            binding.descriptorSet = set;
            if ((mask & (1u << set)) != 0u) fragment.bindings.push_back(binding);
            else vertex.bindings.push_back(binding);
        }
        const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
        std::array<std::uint32_t, 1> mainKey{11}, fragmentKey{22};
        PipelineSetLayouts resources{makeHandle<VkDescriptorSetLayout>(), makeHandle<VkDescriptorSetLayout>(), true, mainKey, fragmentKey};
        const auto prebuild = [&] { Require(Pipeline::Prebuild(context, state, VertexInputLayout{}, resources, shaders), "graphics pipeline library prebuild was skipped"); };
        prebuild();
        for (const auto part : {VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT, VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT}) {
            const auto layout = mock.libraryLayouts.at(part);
            const auto expectedMask = part == VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT ? mask : mask ^ 3u;
            const std::vector<VkDescriptorSetLayout> expected{(expectedMask & 1u) != 0u ? resources.main : VK_NULL_HANDLE, (expectedMask & 2u) != 0u ? resources.fragment : VK_NULL_HANDLE};
            Require(mock.pipelineLayoutSets.at(layout) == expected, "shader library lost a used set or retained an unused set");
            Require(mock.pipelineLayoutFlags.at(layout) == VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT, "shader library lost independent sets");
            const auto& pushes = mock.pipelineLayoutPushRanges.at(layout);
            Require(pushes.size() == 2 && pushes[0].offset == 0 && pushes[0].size == PipelinePushSlotBytes && pushes[0].stageFlags == VK_SHADER_STAGE_VERTEX_BIT && pushes[1].offset == PipelinePushSlotBytes && pushes[1].size == PipelinePushSlotBytes && pushes[1].stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT, "shader library changed push constant slots");
        }
        auto before = PipelineLibraryCountersOf(context.device).built;
        prebuild();
        Require(PipelineLibraryCountersOf(context.device).built == before, "identical shader libraries were rebuilt");
        for (std::uint32_t set = 0; set < 2; ++set) {
            if (set == 0) { ++mainKey[0]; resources.main = makeHandle<VkDescriptorSetLayout>(); }
            else { ++fragmentKey[0]; resources.fragment = makeHandle<VkDescriptorSetLayout>(); }
            prebuild();
            const auto after = PipelineLibraryCountersOf(context.device).built;
            Require(after[0] == before[0] && after[3] == before[3], "descriptor layout change rebuilt interface libraries");
            Require(after[1] == before[1] + ((mask & (1u << set)) == 0u ? 1u : 0u), "pre-rasterization library reuse ignored descriptor set usage");
            Require(after[2] == before[2] + ((mask & (1u << set)) != 0u ? 1u : 0u), "fragment library reuse ignored descriptor set usage");
            before = after;
        }
        ShaderRecompiler::DescriptorBinding invalid{};
        invalid.descriptorSet = 32;
        fragment.bindings.push_back(invalid);
        expectFailure(prebuild, "descriptor set exceeds two slots");
        ClearPipelineLibraries(context.device);
        Require(mock.live == 0, "shader library tests leaked Vulkan objects");
    }
}

void pipelineLibraryClearWaitsForBuildTests() {
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    auto context = mockContext();
    context.graphicsPipelineLibrary = true;
    context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    State state{};
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.spirv = makeModule({});
    fragment.spirv = makeModule({.fragment = true});
    vertex.variantId = 101;
    fragment.variantId = 102;
    const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
    std::array<std::uint32_t, 1> mainKey{1}, fragmentKey{2};
    PipelineSetLayouts resources{};
    resources.main = makeHandle<VkDescriptorSetLayout>();
    resources.fragment = makeHandle<VkDescriptorSetLayout>();
    resources.mainKey = mainKey;
    resources.fragmentKey = fragmentKey;
    {
        std::lock_guard lock(pipelineCreateMutex);
        blockLibraryCreate = true;
        libraryCreateEntered = false;
        releaseLibraryCreate = false;
    }
    std::thread build([&] { Require(Pipeline::Prebuild(context, state, VertexInputLayout{}, resources, shaders), "graphics pipeline library prebuild was skipped"); });
    {
        std::unique_lock lock(pipelineCreateMutex);
        pipelineCreateCondition.wait(lock, [] { return libraryCreateEntered; });
    }
    std::atomic<bool> cleared{false};
    std::thread clear([&] {
        ClearCachedPipelines(context.device);
        cleared.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool waited = !cleared.load(std::memory_order_acquire);
    {
        std::lock_guard lock(pipelineCreateMutex);
        releaseLibraryCreate = true;
    }
    pipelineCreateCondition.notify_all();
    build.join();
    clear.join();
    Require(waited, "clearing pipeline libraries did not wait for an active part build");
    Require(cleared.load(std::memory_order_acquire), "clearing pipeline libraries did not complete after the part build");
    Require(mock.live == 0, "clearing during a library build leaked Vulkan objects");
}

void framebufferOwnerTests() {
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    auto context = mockContext();
    context.graphicsPipelineLibrary = true;
    context.pipelineLinkTimeOptimization = false;
    context.limits.maxColorAttachments = 1;
    context.limits.maxFramebufferWidth = 4;
    context.limits.maxFramebufferHeight = 4;
    context.limits.maxFramebufferLayers = 1;
    context.deviceProc = +[](VkDevice device, const char* name) -> PFN_vkVoidFunction {
        if (std::strcmp(name, "vkCreateGraphicsPipelines") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(+[](VkDevice, VkPipelineCache, std::uint32_t count, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline* pipeline) -> VkResult {
                Require(count == 1, "framebuffer owner test expects one graphics pipeline");
                *pipeline = makeHandle<VkPipeline>();
                ++mock.live;
                return VK_SUCCESS;
            });
        }
        return mockProc(device, name);
    };
    {
        State state{};
        state.hasColorTarget = true;
        state.samples = 2;
        state.colors.resize(1);
        state.colors[0].format = VK_FORMAT_R8G8B8A8_UNORM;
        state.colors[0].samples = 2;
        state.blends.resize(1);
        ShaderRecompiler::RecompileResult vertex, fragment;
        vertex.spirv = makeModule({});
        fragment.spirv = makeModule({.fragment = true});
        const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
        Pipeline pipeline(context, state, VertexInputLayout{}, PipelineSetLayouts{}, shaders);
        const std::array<VkImageView, 1> views{makeHandle<VkImageView>()};
        std::array<std::shared_ptr<void>, 1> owners{std::make_shared<int>(1)};
        const std::weak_ptr<void> firstOwner = owners[0];
        auto first = pipeline.AcquireFramebuffer(views, owners, {4, 4});
        auto repeated = pipeline.AcquireFramebuffer(views, owners, {4, 4});
        Require(first == repeated, "a live framebuffer owner did not reuse its framebuffer");
        auto retainedOwner = owners[0];
        owners[0] = std::make_shared<int>(2);
        auto distinct = pipeline.AcquireFramebuffer(views, owners, {4, 4});
        Require(distinct != first, "identical image-view handles reused another live owner's framebuffer");
        retainedOwner.reset();
        Require(firstOwner.expired(), "the framebuffer cache retained its image owner");
        const std::weak_ptr<Framebuffer> retired = first;
        auto recycled = pipeline.AcquireFramebuffer(views, owners, {4, 4});
        Require(recycled == distinct && recycled != first, "an expired owner matched a recycled image-view handle");
        first.reset();
        repeated.reset();
        recycled = pipeline.AcquireFramebuffer(views, owners, {4, 4});
        Require(retired.expired(), "the framebuffer cache retained an unused framebuffer after its owner expired");
        const std::weak_ptr<void> lastOwner = owners[0];
        owners[0].reset();
        Require(lastOwner.expired(), "a cached framebuffer kept the final image owner alive");
    }
    ClearPipelineLibraries(context.device);
    Require(mock.live == 0, "framebuffer owner tests leaked Vulkan objects");
}

void barycentricEmulationTests() {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::Graphics;
    const std::array<FragmentParameter, 3> parameters{{{0u, 0u, false, false}, {1u, 1u, false, true}, {2u, 2u, true, false}}};
    const auto layout = LayoutBarycentricEmulation(parameters, {true, true, true});
    Require(layout.smoothLocation == 1u && layout.linearLocation == 3u && layout.perVertexLocations == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1u, 4u}}, "emulated barycentrics did not take the free locations in order");
    const auto offsetLayout = LayoutBarycentricEmulation(parameters, {true, true, false, true});
    Require(offsetLayout.smoothLocation == 1u && offsetLayout.reciprocalWLocation == 3u && offsetLayout.perVertexLocations == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1u, 4u}}, "perspective offset emulation did not reserve reciprocal-w inputs");
    Require(LayoutBarycentricEmulation(parameters, {true, true, false}).reciprocalWLocation == BarycentricEmulationLayout::NoLocation, "non-offset perspective barycentrics reserved reciprocal-w inputs");
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

    const std::array<std::uint32_t, 2> tessellationCapabilities{spv::CapabilityShader, spv::CapabilityTessellation};
    SpirvTarget tessellationTarget = target;
    tessellationTarget.supportedCapabilities = tessellationCapabilities;
    tessellationTarget.tessellation = TessellationTargetLimits{32u, 128u, 128u, 120u, 4096u, 128u, 128u};
    const auto modes = [](const RecompileResult& result) {
        std::vector<std::uint32_t> values;
        const auto& words = result.spirv.Words();
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpExecutionMode) values.push_back(words[at + 2]);
        }
        return values;
    };
    auto patches = BuildBarycentricTessellationShaders(vertex, fragment, tessellationTarget, 128u, 0u);
    Require(modes(patches.control) == std::vector<std::uint32_t>{spv::ExecutionModeOutputVertices, spv::ExecutionModeTriangles, spv::ExecutionModeSpacingEqual, spv::ExecutionModeVertexOrderCw}, "the generated control stage does not declare three output vertices and the triangle domain");
    Require(modes(patches.evaluation) == std::vector<std::uint32_t>{spv::ExecutionModeTriangles, spv::ExecutionModeSpacingEqual, spv::ExecutionModeVertexOrderCw}, "the generated evaluation stage does not declare the triangle domain");
    const std::array<CompiledShader, 4> patchShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, &patches.control, 0}, {ShaderStage::TessellationEvaluation, &patches.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    Require(BarycentricPatches(state, patchShaders) && !BarycentricPatches(state, shaders), "generated tessellation stages were not recognised as barycentric patches");
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ValidateShaders(patchShaders, state, subgroup, false);
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    ValidateShaders(patchShaders, state, subgroup, false);
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_PRIMITIVE_TOPOLOGY_POINT_LIST}) {
        state.topology = topology;
        expectFailure([&] { ValidateShaders(patchShaders, state, subgroup, false); }, "require a triangle list or strip");
    }
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const std::array<CompiledShader, 4> plainPatchShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, &patches.control, 0}, {ShaderStage::TessellationEvaluation, &patches.evaluation, 0}, {ShaderStage::Fragment, &plain, 0}}};
    expectFailure([&] { ValidateShaders(plainPatchShaders, state, subgroup, false); }, "stage count");
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, plain, tessellationTarget, 128u, 0u)); }, "does not emulate barycentrics");
    auto noTessellation = tessellationTarget;
    noTessellation.tessellation.reset();
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, noTessellation, 128u, 0u)); }, "tessellation shaders are unavailable");
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, target, 128u, 0u)); }, "tessellation shaders are unavailable");
    auto noCapability = tessellationTarget;
    noCapability.supportedCapabilities = std::span<const std::uint32_t>(capabilities).first(1);
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, noCapability, 128u, 0u)); }, "tessellation capability is unavailable");
    auto smallPatches = tessellationTarget;
    smallPatches.tessellation->maxEvaluationOutputComponents = 4u;
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, smallPatches, 128u, 0u)); }, "exceeds device limits");
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, tessellationTarget, 0u, 0u)); }, "exceeds device limits");
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, tessellationTarget, 128u, 1u)); }, "first or the last");
    static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, tessellationTarget, 128u, 2u));
    {
        auto context = mockContext();
        context.provokingVertexLast = false;
        auto lastState = state;
        lastState.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
        ValidateProvokingVertex(context, lastState, patchShaders);
        expectFailure([&] { ValidateProvokingVertex(context, lastState, shaders); }, "last provoking vertex requires VK_EXT_provoking_vertex");
        RecompileResult flatVertices = fragment;
        for (auto& parameter : flatVertices.fragmentParameters) parameter.flat = true;
        const std::array<CompiledShader, 4> flatPatchShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, &patches.control, 0}, {ShaderStage::TessellationEvaluation, &patches.evaluation, 0}, {ShaderStage::Fragment, &flatVertices, 0}}};
        expectFailure([&] { ValidateProvokingVertex(context, lastState, flatPatchShaders); }, "explicit per-vertex flat interpolation");
    }

    RecompileResult clipped;
    clipped.spirv = makeModule({.parameterOutput = true, .clipDistanceLength = 2});
    clipped.parameterExports = {0u};
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(clipped, fragment, tessellationTarget, 128u, 0u)); }, "need shaderClipDistance");
    const std::array<std::uint32_t, 3> clipCapabilities{spv::CapabilityShader, spv::CapabilityTessellation, spv::CapabilityClipDistance};
    auto clipTarget = tessellationTarget;
    clipTarget.supportedCapabilities = clipCapabilities;
    auto clippedPatches = BuildBarycentricTessellationShaders(clipped, fragment, clipTarget, 128u, 0u);
    const auto memberBuiltIn = [](const RecompileResult& result, spv::BuiltIn builtin) {
        const auto& words = result.spirv.Words();
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpMemberDecorate && words[at + 3] == spv::DecorationBuiltIn && words[at + 4] == builtin) return true;
        }
        return false;
    };
    const auto variableBuiltIn = [](const RecompileResult& result, spv::BuiltIn builtin) {
        const auto& words = result.spirv.Words();
        std::set<std::uint32_t> variables;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpVariable) variables.insert(words[at + 2]);
        }
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == builtin && variables.contains(words[at + 1])) return true;
        }
        return false;
    };
    Require(!memberBuiltIn(patches.control, spv::BuiltInClipDistance) && !memberBuiltIn(patches.evaluation, spv::BuiltInClipDistance), "a generated tessellation stage declares clip distances the vertex shader does not write");
    Require(variableBuiltIn(clippedPatches.control, spv::BuiltInClipDistance) && memberBuiltIn(clippedPatches.control, spv::BuiltInClipDistance), "the generated control stage does not read the vertex clip distances as a separate built-in variable and pass them on in its per-vertex block");
    Require(memberBuiltIn(clippedPatches.evaluation, spv::BuiltInClipDistance) && !variableBuiltIn(clippedPatches.evaluation, spv::BuiltInClipDistance), "the generated evaluation stage does not keep the clip distances in its per-vertex blocks");
    const std::array<CompiledShader, 4> clippedShaders{{{ShaderStage::Vertex, &clipped, 0}, {ShaderStage::TessellationControl, &clippedPatches.control, 0}, {ShaderStage::TessellationEvaluation, &clippedPatches.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    ValidateShaders(clippedShaders, state, subgroup, false, false, false, false, false, false, true);
    vertex.parameterExports.clear();
    expectFailure([&] { static_cast<void>(BuildBarycentricTessellationShaders(vertex, fragment, tessellationTarget, 128u, 0u)); }, "not exported by the vertex shader");
    expectFailure([&] { static_cast<void>(BuildBarycentricGeometryShader(vertex, fragment, target, limits)); }, "not exported by the vertex shader");
}

void copiedWriterIndirectOverlapTests() {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::Graphics;

    constexpr std::size_t half = 65536;

    void* directBlock = ::operator new(half, std::align_val_t{half});
    std::memset(directBlock, 0xaa, half);
    const auto directAddress = reinterpret_cast<std::uintptr_t>(directBlock);

    void* copiedBlock = ::operator new(half, std::align_val_t{half});
    std::memset(copiedBlock, 0xbb, half);
    const auto copiedAddress = reinterpret_cast<std::uintptr_t>(copiedBlock);

    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, directBlock, half, true, true, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }

    mock = MockVulkan{};
    auto context = mockContext();
    context.hostImportAlignment = half;
    context.limits.minStorageBufferOffsetAlignment = 16;
    context.limits.maxStorageBufferRange = 1u << 27;

    {
        GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        Require(HostImportCovers(context, directAddress, half), "direct registered range must be covered by host import");
        Require(!HostImportCovers(context, copiedAddress, half), "copied unregistered range must NOT be covered by host import");

        memory.AddWritable(copiedAddress, 1024);
        memory.AddWritable(directAddress, 1024);
        memory.Upload(false);

        Require(memory.HasCopiedWrites(), "writer with one copied region must report HasCopiedWrites() == true");

        Require(memory.WritesOverlap(directAddress, 16), "WritesOverlap must return true for direct written region");
        Require(memory.WritesOverlap(copiedAddress, 16), "WritesOverlap must return true for copied written region");
        Require(!memory.WritesOverlap(directAddress + 2048, 16), "WritesOverlap must return false for unwritten range");

        Require(!memory.CopiedWritesOverlap(directAddress, 16), "CopiedWritesOverlap must return false for direct written region");
        Require(memory.CopiedWritesOverlap(copiedAddress, 16), "CopiedWritesOverlap must return true for copied written region");
        Require(!memory.CopiedWritesOverlap(directAddress + 2048, 16), "CopiedWritesOverlap must return false for unwritten range");

        memory.WriteBack();
    }

    {
        RecompileResult shader;
        shader.runtimeAbiVersion = RuntimeAbi::Version;
        auto copiedBinding = makeBinding(Role::GuestBuffers, 0, 1, vsharp(copiedBlock, 1024));
        copiedBinding.bufferWritten = {true};
        auto directBinding = makeBinding(Role::GuestBuffers, 1, 1, vsharp(directBlock, 1024));
        directBinding.bufferWritten = {true};
        shader.bindings = {copiedBinding, directBinding};
        CompiledShader compiled{ShaderStage::Compute, &shader, 0};
        auto writer = std::make_shared<ShaderResources>(context, compiled);

        Require(writer->HasCopiedWrites(), "ShaderResources with copied region must report HasCopiedWrites() == true");
        Require(writer->WritesOverlap(directAddress, 16), "ShaderResources must report WritesOverlap for direct region");
        Require(!writer->CopiedWritesOverlap(directAddress, 16), "ShaderResources must NOT report CopiedWritesOverlap for direct region");
        Require(writer->CopiedWritesOverlap(copiedAddress, 16), "ShaderResources must report CopiedWritesOverlap for copied region");

        auto copiedWriters = DrawCopiedWriters();
        copiedWriters->push_back(writer);

        const auto overlapsMain = [&](const auto& w) { return w->WritesOverlap(directAddress, 16); };
        const auto overlapsPR = [&](const auto& w) { return w->CopiedWritesOverlap(directAddress, 16); };

        const bool mainOverlaps = std::any_of(copiedWriters->begin(), copiedWriters->end(), overlapsMain);
        const bool prOverlaps = std::any_of(copiedWriters->begin(), copiedWriters->end(), overlapsPR);

        Require(mainOverlaps, "main's check (WritesOverlap) must flag an overlap over direct arguments");
        Require(!prOverlaps, "PR's check (CopiedWritesOverlap) must disambiguate and NOT flag an overlap over direct arguments");

        const auto copiedOverlapsPR = [&](const auto& w) { return w->CopiedWritesOverlap(copiedAddress, 16); };
        Require(std::any_of(copiedWriters->begin(), copiedWriters->end(), copiedOverlapsPR), "PR's check must flag an overlap over copied arguments");

        copiedWriters->erase(std::remove(copiedWriters->begin(), copiedWriters->end(), writer), copiedWriters->end());
        writer->WriteBack();
    }

    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, directBlock);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    ::operator delete(directBlock, std::align_val_t{half});
    ::operator delete(copiedBlock, std::align_val_t{half});
}

void registeredReadableCoverTests() {
    using namespace AgcDriver::Graphics;

    constexpr std::size_t block = 65536;
    auto* memory = static_cast<std::byte*>(::operator new(4 * block, std::align_val_t{block}));
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const auto add = [&](std::size_t index, bool readable) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, memory + index * block, block, readable, readable, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto remove = [&](std::size_t index) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, memory + index * block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };

    add(0, true);
    add(1, true);
    Require(RegisteredReadableCovers(base, block), "a registered allocation must be covered");
    Require(RegisteredReadableCovers(base + block / 2, block), "a range across two adjacent registered allocations must be covered");
    Require(RegisteredReadableCovers(base, 2 * block), "two adjacent registered allocations must be covered together");
    Require(!RegisteredReadableCovers(base + block, 2 * block), "a range past the last registered allocation must not be covered");
    add(3, true);
    Require(!RegisteredReadableCovers(base, 4 * block), "a range over a gap between registered allocations must not be covered");
    add(2, false);
    Require(!RegisteredReadableCovers(base + block, 2 * block), "a range over an unreadable registered allocation must not be covered");
    Require(RegisteredReadableCovers(base + 3 * block, block), "the readable allocation after an unreadable one must be covered");
    Require(!RegisteredReadableCovers(base, 0), "an empty range must not be covered");
    for (std::size_t index = 0; index < 4; ++index) remove(index);
    Require(!RegisteredReadableCovers(base, block), "a removed allocation must not be covered");
    ::operator delete(memory, std::align_val_t{block});
}

std::size_t VariableBuiltIns(const ShaderRecompiler::RecompileResult& result, spv::BuiltIn builtin) {
    std::size_t count = 0;
    const auto& words = result.spirv.Words();
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpDecorate && (words[at] >> 16u) == 4u && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == builtin) ++count;
    }
    return count;
}

bool MemberBuiltIn(const ShaderRecompiler::RecompileResult& result, spv::BuiltIn builtin) {
    const auto& words = result.spirv.Words();
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpMemberDecorate && words[at + 3] == spv::DecorationBuiltIn && words[at + 4] == builtin) return true;
    }
    return false;
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
    {
        RecompileResult clipped;
        clipped.spirv = makeModule({.clipDistanceLength = 2});
        const std::array<std::uint32_t, 3> clipCapabilities{spv::CapabilityShader, spv::CapabilityTessellation, spv::CapabilityClipDistance};
        auto clipTarget = target;
        clipTarget.supportedCapabilities = clipCapabilities;
        auto clippedRect = BuildRectListShaders(clipped, fragment, clipTarget);
        for (const auto* stage : {&clippedRect.control, &clippedRect.lastVertexControl, &clippedRect.evaluation}) {
            Require(!MemberBuiltIn(*stage, spv::BuiltInClipDistance) && VariableBuiltIns(*stage, spv::BuiltInClipDistance) == 2u, "a rect-list stage does not take and forward the vertex clip distances through variables of their own, as the vertex shader writes them");
        }
        const std::array<CompiledShader, 4> clippedShaders{{{ShaderStage::Vertex, &clipped, 0}, {ShaderStage::TessellationControl, &clippedRect.control, 0}, {ShaderStage::TessellationEvaluation, &clippedRect.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
        auto queue = makeState();
        queue.userConfig[0x242] = 17;
        ValidateShaders(clippedShaders, DecodeState(queue), VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false, false, false, false, false, false, true);
    }
    vertex.spirv = makeModule({.parameterOutput = true});
    fragment.spirv = makeModule({.fragment = true, .rectParameters = true});
    vertex.parameterExports = {0};
    fragment.fragmentParameters = {{0, 0, false, false}, {1, 0, true, false}};
    auto auxiliary = BuildRectListShaders(vertex, fragment, target);
    Require(!auxiliary.control.spirv.empty() && !auxiliary.evaluation.spirv.empty(), "rect-list parameter shaders are empty");
    Require(faultAuxiliary.lastVertexControl.Identity() == faultAuxiliary.control.Identity() && auxiliary.lastVertexControl.Identity() != auxiliary.control.Identity() && auxiliary.lastVertexControl.bindings.size() == 1 && auxiliary.lastVertexControl.bdaAbiVersion == auxiliary.control.bdaAbiVersion, "the last-vertex rect-list control shader does not differ from the first-vertex one exactly when a flat parameter is present");
    auto parameterQueue = makeState();
    parameterQueue.userConfig[0x242] = 17;
    const auto parameterState = DecodeState(parameterQueue);
    for (const auto* control : {&auxiliary.control, &auxiliary.lastVertexControl}) {
        const std::array<CompiledShader, 4> parameterShaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::TessellationControl, control, 0}, {ShaderStage::TessellationEvaluation, &auxiliary.evaluation, 0}, {ShaderStage::Fragment, &fragment, 0}}};
        ValidateShaders(parameterShaders, parameterState, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
    }
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
    std::array<std::uint16_t, 8> indices{};
    const auto drawFailure = [&](std::uint32_t count, bool indexed) {
        const AgcDriver::Pm4::DrawParameters draw = indexed ? AgcDriver::Pm4::DrawParameters{reinterpret_cast<std::uint64_t>(indices.data()), count, 2, 1, 0, true} : AgcDriver::Pm4::DrawParameters{0, count, 0, 1, 0, false};
        try {
            Draw(context, state, draw, {});
        } catch (const std::exception& error) {
            return std::string(error.what());
        }
        return std::string{};
    };
    for (const auto indexed : {false, true}) {
        const auto complete = drawFailure(3, indexed);
        for (const auto count : {4u, 5u}) Require(drawFailure(count, indexed) == complete, "a rect-list draw with a partial last rectangle was not drawn like its complete rectangles");
    }
    Require(drawFailure(0, true).empty(), "an indexed draw without indices was not skipped");
    const AgcDriver::Pm4::DrawParameters noInstances{reinterpret_cast<std::uint64_t>(indices.data()), 3, 2, 0, 0, true};
    Draw(context, state, noInstances, {});
}

void quadListTests() {
    using namespace AgcDriver::Graphics;
    auto queue = makeState();
    queue.userConfig[0x242] = 19;
    queue.context[0x1b3] = queue.context[0x1b4] = 2;
    Require(DrawRejection(queue, false).empty(), "valid quad-list draw failed precheck");
    auto state = DecodeState(queue);
    Require(state.quadList, "quad-list state flag was not set");
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "quad-list did not decode to triangle list topology");
    Require(RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Triangle, "quad-list did not produce triangle raster primitive");
    Require(!state.primitiveRestart, "quad-list must not have primitive restart enabled");
    queue.userConfig[0x24b] = 1;
    state = DecodeState(queue);
    Require(!state.primitiveRestart, "quad-list must suppress primitive restart");
    queue.userConfig[0x24b] = 0;
    const Context context{};
    std::array<std::uint16_t, 8> indices{0, 1, 2, 3, 2, 3, 4, 5};
    const auto drawOutcome = [&](std::uint32_t count, bool indexed) {
        const AgcDriver::Pm4::DrawParameters draw = indexed ? AgcDriver::Pm4::DrawParameters{reinterpret_cast<std::uint64_t>(indices.data()), count, 2, 1, 0, true} : AgcDriver::Pm4::DrawParameters{0, count, 0, 1, 0, false};
        try {
            Draw(context, state, draw, {});
        } catch (const std::exception& error) {
            return std::string(error.what());
        }
        return std::string{};
    };
    for (const auto indexed : {false, true}) {
        const auto complete = drawOutcome(4, indexed);
        for (const auto count : {5u, 6u, 7u}) Require(drawOutcome(count, indexed) == complete, "a quad-list draw with partial trailing vertices was not drawn like complete quads");
    }
    Require(drawOutcome(0, true).empty(), "an indexed quad-list draw without indices was not skipped");
    const AgcDriver::Pm4::DrawParameters noInstances{reinterpret_cast<std::uint64_t>(indices.data()), 4, 2, 0, 0, true};
    Draw(context, state, noInstances, {});
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

bool declaresEarlyFragmentTests(const ShaderRecompiler::RecompileRequest& request) {
    const auto result = ShaderRecompiler::Recompile(request);
    const auto& words = result.spirv.Words();
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModeEarlyFragmentTests) return true;
    }
    return false;
}

void earlyFragmentTestsTests() {
    const std::array<std::pair<std::uint32_t, bool>, 9> cases{{
        {0x0000u, false},
        {0x0020u, false},
        {0x0010u, true},
        {0x0030u, true},
        {0x0050u, false},
        {0x0011u, false},
        {0x1000u, true},
        {0x1050u, true},
        {0x1031u, true}
    }};
    const ShaderRecompiler::RequestSerializer serializer;
    for (const auto& [shaderControl, expected] : cases) {
        const auto request = depthExportRequest(shaderControl);
        const auto replayed = serializer.Deserialize(serializer.Serialize(request));
        Require(declaresEarlyFragmentTests(request) == expected && declaresEarlyFragmentTests(replayed.request) == expected, "DB_SHADER_CONTROL " + std::to_string(shaderControl) + (expected ? " did not declare" : " declared") + " early fragment tests");
    }
    for (const auto shaderControl : {0x0010u, 0x1000u}) {
        auto request = depthExportRequest(shaderControl);
        request.context.pixel->alphaToCoverage = true;
        const auto replayed = serializer.Deserialize(serializer.Serialize(request));
        Require(!declaresEarlyFragmentTests(request) && !declaresEarlyFragmentTests(replayed.request), "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " with alpha-to-coverage declared early fragment tests");
        request = depthExportRequest(shaderControl);
        request.context.pixel->quadPixelMask = 0x1u;
        const auto masked = serializer.Deserialize(serializer.Serialize(request));
        Require(!declaresEarlyFragmentTests(request) && !declaresEarlyFragmentTests(masked.request), "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " with a quad pixel mask declared early fragment tests");
        Require(AgcDriver::Graphics::DeclaresEarlyFragmentTests(ShaderRecompiler::Recompile(depthExportRequest(shaderControl))), "the driver did not read early fragment tests from a recompiled pixel shader");
    }
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

std::string interlockExits(std::span<const std::uint32_t> words, std::size_t& kills, std::size_t& terminates) {
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
            if (op == spv::OpBeginInvocationInterlockEXT || op == spv::OpEndInvocationInterlockEXT || op == spv::OpKill || op == spv::OpTerminateInvocation) return "an interlock instruction, OpKill or OpTerminateInvocation outside the entry point";
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
            if (op == spv::OpTerminateInvocation) ++terminates;
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
    std::size_t kills = 0, terminates = 0;
    const auto exits = interlockExits(ShaderRecompiler::Recompile(faulting).spirv.Words(), kills, terminates);
    Require(exits.empty() && kills == 6u && terminates == 0u, "a primitive-ordered pixel shader with BDA reads: " + (exits.empty() ? std::to_string(kills) + " OpKill and " + std::to_string(terminates) + " OpTerminateInvocation, not the valid-mask kill and 5 BDA fault kills" : exits));
    const std::array<std::string_view, 3> terminateExtensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_KHR_terminate_invocation"};
    auto terminating = faulting;
    terminating.target.supportedExtensions = terminateExtensions;
    const auto terminated = ShaderRecompiler::Recompile(terminating).spirv.Copy();
    std::size_t terminatedKills = 0, terminatedStops = 0;
    const auto terminatedExits = interlockExits(terminated, terminatedKills, terminatedStops);
    Require(terminatedExits.empty() && terminatedKills == 1u && terminatedStops == 5u, "a primitive-ordered pixel shader with BDA reads and SPV_KHR_terminate_invocation: " + (terminatedExits.empty() ? std::to_string(terminatedKills) + " OpKill and " + std::to_string(terminatedStops) + " OpTerminateInvocation, not the valid-mask kill and 5 BDA fault terminations" : terminatedExits));
    std::size_t terminateDeclarations = 0;
    for (std::size_t at = 5; at < terminated.size() && (terminated[at] >> 16u) != 0; at += terminated[at] >> 16u) {
        if ((terminated[at] & 0xffffu) == spv::OpExtension && std::string_view(reinterpret_cast<const char*>(&terminated[at + 1])) == "SPV_KHR_terminate_invocation") ++terminateDeclarations;
    }
    Require(terminateDeclarations == 1u, "the module declared SPV_KHR_terminate_invocation " + std::to_string(terminateDeclarations) + " times");
    const auto countOps = [](std::span<const std::uint32_t> words) {
        std::map<spv::Op, std::size_t> counts;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) ++counts[static_cast<spv::Op>(words[at] & 0xffffu)];
        return counts;
    };
    const std::array<std::uint32_t, 4> bdaCapabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    auto unordered = orderedPixelRequest(0x20600u, bdaCapabilities, OrderedBdaPixelCode);
    unordered.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    unordered.target.supportedExtensions = terminateExtensions;
    const auto entryOps = [](std::span<const std::uint32_t> words) {
        std::map<spv::Op, std::size_t> counts;
        std::uint32_t entry = 0, function = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
            if (op == spv::OpEntryPoint) entry = words[at + 2];
            if (op == spv::OpFunction) function = words[at + 2];
            if (function != 0 && function == entry) ++counts[op];
        }
        return counts;
    };
    const auto unorderedWords = ShaderRecompiler::Recompile(unordered).spirv.Copy();
    const auto unorderedOps = countOps(unorderedWords);
    Require(!unorderedOps.contains(spv::OpBeginInvocationInterlockEXT), "a pixel shader without PRIMITIVE_ORDERED_PIXEL_SHADER began the interlock");
    Require(unorderedOps.at(spv::OpTerminateInvocation) == 1u && entryOps(unorderedWords).at(spv::OpTerminateInvocation) == 1u && unorderedOps.at(spv::OpKill) == 1u, "a pixel shader with BDA reads and SPV_KHR_terminate_invocation did not end its BDA faults through one OpTerminateInvocation in its entry point (" + std::to_string(unorderedOps.at(spv::OpTerminateInvocation)) + " in the module)");
    unordered.target.supportedExtensions = bdaExtensions;
    const auto unorderedKillWords = ShaderRecompiler::Recompile(unordered).spirv.Copy();
    const auto unorderedKills = countOps(unorderedKillWords);
    Require(!unorderedKills.contains(spv::OpTerminateInvocation) && unorderedKills.at(spv::OpKill) == 2u && entryOps(unorderedKillWords).at(spv::OpKill) == 1u, "a pixel shader with BDA reads and no SPV_KHR_terminate_invocation did not end its BDA faults through one OpKill in its entry point (" + std::to_string(unorderedKills.at(spv::OpKill)) + " OpKill in the module)");
    auto unreadPixel = orderedPixelRequest(0x20600u, bdaCapabilities);
    unreadPixel.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    for (const auto extensions : {std::span<const std::string_view>(terminateExtensions), std::span<const std::string_view>(bdaExtensions)}) {
        unreadPixel.target.supportedExtensions = extensions;
        const auto unread = ShaderRecompiler::Recompile(unreadPixel);
        const auto unreadOps = countOps(unread.spirv.Words());
        const auto unreadEntry = entryOps(unread.spirv.Words());
        Require(!unreadOps.contains(spv::OpTerminateInvocation) && !unreadEntry.contains(spv::OpKill), "a pixel shader without BDA reads ended its entry point through a BDA fault exit (" + std::to_string(extensions.size()) + " extensions)");
        AgcDriver::Graphics::State state{};
        state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &unread, 0}}};
        AgcDriver::Graphics::ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
    }
    static constexpr std::array<std::uint32_t, 12> vertexBdaCode{0x7e040f00u, 0x7e060280u, 0xdc208001u, 0x047d0002u, 0xdc308008u, 0x057d0002u, 0xbf8c3f70u, 0x4a080b04u, 0x7e0e0d04u, 0xf80008cfu, 0x07070707u, 0xbf810000u};
    const std::array<std::uint32_t, 5> vertexCapabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance};
    const auto vertexWords = [&](std::span<const std::string_view> extensions) {
        ShaderRecompiler::RecompileRequest request{};
        request.shader = {ShaderRecompiler::ShaderStage::Vertex, 0x10000u, vertexBdaCode, 0, {}};
        request.context.waveSize = 64;
        request.context.vertex = ShaderRecompiler::ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.target.supportedCapabilities = vertexCapabilities;
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return ShaderRecompiler::Recompile(request).spirv.Copy();
    };
    const auto vertexStops = vertexWords(bdaExtensions);
    const auto vertexTerminate = vertexWords(terminateExtensions);
    const auto vertexOps = countOps(vertexTerminate);
    Require(std::ranges::equal(vertexStops, vertexTerminate) && !vertexOps.contains(spv::OpTerminateInvocation) && !vertexOps.contains(spv::OpKill) && vertexOps.at(spv::OpReturn) > 1u, "a vertex shader with BDA reads changed its fault exit when SPV_KHR_terminate_invocation was advertised");
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(orderedPixelRequest(0x30600u, plain))); }, "fragmentShaderPixelInterlock");
}

constexpr std::array<std::uint32_t, 4> StencilRefExportCode{0x7e0e02f2u, 0xf8001882u, 0x700u, 0xbf810000u};
constexpr std::array<std::uint32_t, 4> DepthStencilRefExportCode{0x7e0e02f2u, 0xf8001883u, 0x707u, 0xbf810000u};

AgcDriver::QueueState stencilRefExportQueue(std::uint32_t shaderControl, std::uint32_t zFormat) {
    auto queue = makeState();
    queue.context[0x1b3] = 0x300u;
    queue.context[0x1b4] = 0x300u;
    queue.context[0x1c4] = zFormat;
    queue.context[0x203] = shaderControl;
    return queue;
}

ShaderRecompiler::RecompileRequest stencilRefExportRequest(std::uint32_t shaderControl, std::span<const std::uint32_t> capabilities, std::span<const std::uint32_t> code = StencilRefExportCode) {
    const auto queue = stencilRefExportQueue(shaderControl, 2u);
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

std::string stencilRefShape(std::span<const std::uint32_t> words) {
    std::string shape;
    std::uint32_t variable = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpCapability && words[at + 1] == spv::CapabilityStencilExportEXT) shape += "capability ";
        if (op == spv::OpExtension && std::string_view(reinterpret_cast<const char*>(&words[at + 1])) == "SPV_EXT_shader_stencil_export") shape += "extension ";
        if (op == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModeEarlyFragmentTests) shape += "early ";
        if (op == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModeDepthReplacing) shape += "depth ";
        if (op == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModeStencilRefReplacingEXT) shape += "replacing ";
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == spv::BuiltInFragStencilRefEXT) {
            variable = words[at + 1];
            shape += "builtin ";
        }
        if (op == spv::OpStore && variable != 0 && words[at + 1] == variable) shape += "store ";
    }
    return shape;
}

void stencilRefExportTests() {
    for (const auto shaderControl : {0x52u, 0x12u, 0x02u, 0x03u}) {
        for (const auto zFormat : {2u, 9u}) {
            const auto queue = stencilRefExportQueue(shaderControl, zFormat);
            static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
            Require(AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck rejected DB_SHADER_CONTROL " + std::to_string(shaderControl) + " with Z format " + std::to_string(zFormat));
            const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, IdentityExports);
            Require(pixel.stencilRefExportEnable, "STENCIL_TEST_VAL_EXPORT_ENABLE did not reach the pixel stage");
            Require(!pixel.earlyZ, "a pixel shader that exports the stencil reference was given early tests");
        }
        for (const auto zFormat : {0u, 1u, 3u, 7u}) {
            const auto queue = stencilRefExportQueue(shaderControl, zFormat);
            expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(queue)); }, "depth export, shader coverage or ordered fragment execution");
            Require(!AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck accepted a stencil reference export with Z format " + std::to_string(zFormat));
        }
    }
    for (const auto shaderControl : {0x54u, 0x56u, 0x45u}) {
        for (const auto zFormat : {2u, 9u}) {
            const auto unused = stencilRefExportQueue(shaderControl, zFormat);
            static_cast<void>(AgcDriver::Graphics::DecodeState(unused));
            Require(AgcDriver::Graphics::DrawRejection(unused, false).empty(), "the precheck rejected STENCIL_OP_VAL_EXPORT_ENABLE without a stencil test in DB_SHADER_CONTROL " + std::to_string(shaderControl));
            Require(AgcDriver::Graphics::DecodePixelStageInfo(unused.context, IdentityExports).stencilRefExportEnable == ((shaderControl & 2u) != 0), "STENCIL_OP_VAL_EXPORT_ENABLE without a stencil test exported the stencil reference");
            const bool replaces = (shaderControl & 6u) == 4u;
            for (const auto [ops, writeMask, compare, accepted] : {std::tuple{0x000u, 0xffu, 7u, true}, std::tuple{0x737u, 0xffu, 7u, true}, std::tuple{0x110u, 0xffu, 7u, true}, std::tuple{0x050u, 0x00u, 7u, true}, std::tuple{0x040u, 0xffu, 7u, replaces}, std::tuple{0x40040u, 0xffu, 7u, replaces}, std::tuple{0x404u, 0x0fu, 0u, replaces}, std::tuple{0x040u, 0xffu, 2u, false}, std::tuple{0x340u, 0xffu, 7u, false}, std::tuple{0x042u, 0xffu, 7u, false}, std::tuple{0x050u, 0xffu, 7u, false}, std::tuple{0x600u, 0xffu, 7u, false}, std::tuple{0x008u, 0xffu, 7u, false}, std::tuple{0x900u, 0xffu, 7u, false}, std::tuple{0x5000u, 0xffu, 7u, false}}) {
                auto stenciled = stencilRefExportQueue(shaderControl, zFormat);
                for (const auto [offset, word] : {std::pair{0x010u, 0x80000180u}, std::pair{0x011u, 0x20000181u}, std::pair{0x012u, 0x00001000u}, std::pair{0x013u, 0x00002000u}, std::pair{0x015u, 0x00002000u}, std::pair{0x007u, 0x003f003fu}, std::pair{0x000u, 0u}, std::pair{0x002u, 0u}, std::pair{0x00au, 0u}, std::pair{0x00bu, 0x3f800000u}, std::pair{0x200u, 0x007007b3u}}) stenciled.context[offset] = word;
                stenciled.context[0x10b] = ops;
                stenciled.context[0x10c] = 0x0100ff00u | (writeMask << 16u);
                stenciled.context[0x10d] = stenciled.context[0x10c];
                stenciled.context[0x200] = 0x000000b3u | (compare << 8u) | (compare << 20u);
                const auto name = "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " with DB_STENCIL_CONTROL " + std::to_string(ops) + ", write mask " + std::to_string(writeMask) + " and compare " + std::to_string(compare);
                if (accepted) {
                    const auto state = AgcDriver::Graphics::DecodeState(stenciled);
                    Require(state.stencilTest && state.depth.has_value(), "the stencil operation value fixture has no stencil test: " + name);
                    const bool routed = replaces && (ops == 0x040u || ops == 0x40040u || ops == 0x404u);
                    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(stenciled.context, IdentityExports);
                    Require(pixel.stencilRefExportEnable == (routed || (shaderControl & 2u) != 0) && pixel.stencilOperationValueExport == routed, "the stencil reference export does not follow the stencil operation value: " + name);
                    if (routed) {
                        const auto replaced = [&](const VkStencilOpState& face, std::uint32_t faceOps) {
                            const auto expected = [&](std::uint32_t shift) { return ((faceOps >> shift) & 0xfu) == 4u ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP; };
                            return face.failOp == expected(0u) && face.passOp == expected(4u) && face.depthFailOp == expected(8u) && face.writeMask == writeMask;
                        };
                        Require(replaced(state.stencilFront, ops) && replaced(state.stencilBack, ops >> 12u) && !AgcDriver::Graphics::StencilReferenceUnused(state), "REPLACE_OP did not replace with the exported stencil value: " + name);
                    }
                } else {
                    expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(stenciled)); }, "with a pixel shader that exports the stencil");
                }
            }
        }
        for (const auto zFormat : {0u, 1u, 3u, 7u}) {
            const auto queue = stencilRefExportQueue(shaderControl, zFormat);
            expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(queue)); }, "depth export, shader coverage or ordered fragment execution");
            Require(!AgcDriver::Graphics::DrawRejection(queue, false).empty(), "the precheck accepted STENCIL_OP_VAL_EXPORT_ENABLE with Z format " + std::to_string(zFormat));
        }
    }
    Require(!AgcDriver::Graphics::DecodePixelStageInfo(stencilRefExportQueue(0x50u, 2u).context, IdentityExports).stencilRefExportEnable, "a pixel stage without STENCIL_TEST_VAL_EXPORT_ENABLE exports the stencil reference");
    {
        auto early = stencilRefExportQueue(0x1004u, 2u);
        for (const auto [offset, word] : {std::pair{0x011u, 0x20000181u}, std::pair{0x002u, 0u}, std::pair{0x000u, 0u}, std::pair{0x200u, 0x007007b3u}, std::pair{0x10bu, 0x040u}, std::pair{0x10cu, 0x01ffff00u}}) early.context[offset] = word;
        expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodePixelStageInfo(early.context, IdentityExports)); }, "early tests with a pixel shader that exports the stencil operation value");
        Require(!AgcDriver::Graphics::DecodePixelStageInfo(early.context, IdentityExports, true).stencilRefExportEnable, "a skipped pixel program exported the stencil operation value");
    }
    Require(AgcDriver::Graphics::DecodePixelStageInfo(stencilRefExportQueue(0x1012u, 2u).context, IdentityExports).earlyZ, "DEPTH_BEFORE_SHADER with a stencil reference export did not get early tests");
    const std::array<std::uint32_t, 2> stencilExport{spv::CapabilityShader, spv::CapabilityStencilExportEXT};
    const std::array<std::uint32_t, 1> plain{spv::CapabilityShader};
    const ShaderRecompiler::RequestSerializer serializer;
    std::set<std::vector<std::uint64_t>> keys;
    const std::array<std::pair<std::uint32_t, std::string_view>, 4> cases{{{0x52u, "capability extension replacing builtin store "}, {0x12u, "capability extension replacing builtin store "}, {0x50u, ""}, {0x10u, "early "}}};
    for (const auto& [shaderControl, expected] : cases) {
        const auto request = stencilRefExportRequest(shaderControl, stencilExport);
        const auto replayed = serializer.Deserialize(serializer.Serialize(request));
        Require(replayed.request.context.pixel->stencilRefExportEnable == ((shaderControl & 2u) != 0), "the serialized request lost STENCIL_TEST_VAL_EXPORT_ENABLE");
        const auto result = ShaderRecompiler::Recompile(request);
        Require(stencilRefShape(result.spirv.Words()) == expected && stencilRefShape(ShaderRecompiler::Recompile(replayed.request).spirv.Words()) == expected, "DB_SHADER_CONTROL " + std::to_string(shaderControl) + " declared the wrong stencil reference export: " + stencilRefShape(result.spirv.Words()));
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
    Require(keys.size() == cases.size(), "the recompile cache key ignores STENCIL_TEST_VAL_EXPORT_ENABLE");
    const auto both = stencilRefShape(ShaderRecompiler::Recompile(stencilRefExportRequest(0x53u, stencilExport, DepthStencilRefExportCode)).spirv.Words());
    Require(both == "capability extension depth replacing builtin store ", "a depth and stencil reference export declared the wrong outputs: " + both);
    const auto depthOnly = stencilRefShape(ShaderRecompiler::Recompile(stencilRefExportRequest(0x51u, stencilExport, DepthStencilRefExportCode)).spirv.Words());
    Require(depthOnly == "depth ", "an MRTZ stencil channel without STENCIL_TEST_VAL_EXPORT_ENABLE declared the wrong outputs: " + depthOnly);
    const auto emulated = ShaderRecompiler::Recompile(stencilRefExportRequest(0x52u, plain));
    Require(emulated.stencilReferences == std::vector<std::uint32_t>{0u} && emulated.stencilReplaySafe && stencilRefShape(emulated.spirv.Words()).empty(), "a constant stencil reference was not lowered to replay");
    Require(emulated.stencilReferenceDword == 0u && emulated.pushConstants.size() == sizeof(std::uint32_t), "a stencil-only shader did not reserve dynamic reference data");
    {
        const std::array<std::uint32_t, 5> literalStencil{0x7e0e02ffu, 0x00002a05u, 0xf8001882u, 0x700u, 0xbf810000u};
        const auto readsOperationByte = [](std::span<const std::uint32_t> words) {
            for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
                if ((words[at] & 0xffffu) == spv::OpShiftRightLogical) return true;
                if ((words[at] & 0xffffu) == spv::OpConstant && (words[at] >> 16u) == 4u && words[at + 3] == 0x2au) return true;
            }
            return false;
        };
        const auto testValue = stencilRefExportRequest(0x52u, plain, literalStencil);
        auto operationValue = testValue;
        operationValue.context.pixel->stencilOperationValueExport = true;
        Require(ShaderRecompiler::Recompile(testValue).stencilReferences == std::vector<std::uint32_t>{0x05u}, "a stencil test value replay did not take the low byte of the MRTZ stencil channel");
        Require(ShaderRecompiler::Recompile(operationValue).stencilReferences == std::vector<std::uint32_t>{0x2au}, "a stencil operation value replay did not take the second byte of the MRTZ stencil channel");
        const auto exportedTest = stencilRefExportRequest(0x52u, stencilExport, literalStencil);
        auto exportedOperation = exportedTest;
        exportedOperation.context.pixel->stencilOperationValueExport = true;
        Require(!readsOperationByte(ShaderRecompiler::Recompile(exportedTest).spirv.Words()) && readsOperationByte(ShaderRecompiler::Recompile(exportedOperation).spirv.Words()), "the exported stencil operation value was not taken from the second byte of the MRTZ stencil channel");
        Require(serializer.Deserialize(serializer.Serialize(exportedOperation)).request.context.pixel->stencilOperationValueExport, "the serialized request lost the stencil operation value export");
        std::vector<std::uint64_t> testKey;
        std::vector<std::uint64_t> operationKey;
        ShaderRecompiler::RecompileCacheKey::Build(exportedTest, testKey);
        ShaderRecompiler::RecompileCacheKey::Build(exportedOperation, operationKey);
        Require(testKey != operationKey, "the recompile cache key ignores the stencil operation value export");
    }
    auto replayRequest = stencilRefExportRequest(0x52u, plain);
    const auto replayHandle = ShaderRecompiler::PrepareShader(replayRequest);
    const auto replayCapture = ShaderRecompiler::CaptureResources(replayRequest, ShaderRecompiler::SrtRuntime{}, *replayHandle);
    const auto firstReplay = ShaderRecompiler::MaterializeShader(replayRequest, *replayCapture, *replayHandle);
    replayRequest.layout.pushConstantOffsetBytes = 128u;
    Require(ShaderRecompiler::MatchesPreparedShader(replayRequest, *replayHandle), "moving the stencil replay stage slot invalidated the prepared artifact");
    const auto movedReplay = ShaderRecompiler::MaterializeShader(replayRequest, *replayCapture, *replayHandle);
    Require(firstReplay->spirv != movedReplay->spirv && firstReplay->variantId == movedReplay->variantId && firstReplay->pushConstants == movedReplay->pushConstants, "prepared stencil replay did not move only the dynamic reference offset");
    replayRequest.layout.pushConstantSizeBytes = 0u;
    Require(!ShaderRecompiler::MatchesPreparedShader(replayRequest, *replayHandle), "prepared stencil replay accepted an exhausted push range");
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(replayRequest)); }, "unused push constant dword");
    const std::array<std::uint32_t, 5> pushedDepthStencil{0x7e000200u, 0x7e0e02f2u, 0xf8001883u, 0x700u, 0xbf810000u};
    const std::array<std::uint32_t, 1> pushedDepth{0x3f000000u};
    auto pushedRequest = stencilRefExportRequest(0x53u, plain, pushedDepthStencil);
    pushedRequest.context.userData = pushedDepth;
    pushedRequest.layout = {0u, 0u, 96u, 32u};
    const auto pushedHandle = ShaderRecompiler::PrepareShader(pushedRequest);
    ShaderRecompiler::SrtRuntime pushedRuntime{};
    pushedRuntime.userData = pushedDepth;
    const auto pushedCapture = ShaderRecompiler::CaptureResources(pushedRequest, pushedRuntime, *pushedHandle);
    const auto pushedResult = ShaderRecompiler::MaterializeShader(pushedRequest, *pushedCapture, *pushedHandle);
    Require(pushedResult->stencilReferenceDword == 1u && pushedResult->pushConstants.size() == 8u, "a nonaligned replay stage did not reserve its reference after existing push data");
    for (const auto [offset, bytes] : {std::pair{128u, 128u}, std::pair{120u, 8u}}) {
        pushedRequest.layout.pushConstantOffsetBytes = offset;
        pushedRequest.layout.pushConstantSizeBytes = bytes;
        Require(ShaderRecompiler::MatchesPreparedShader(pushedRequest, *pushedHandle), "a replay payload could not move within a valid stage range");
        const auto moved = ShaderRecompiler::MaterializeShader(pushedRequest, *pushedCapture, *pushedHandle);
        Require(moved->pushConstants == pushedResult->pushConstants && moved->spirv != pushedResult->spirv, "moving replay push data changed its payload or retained its old offset");
    }
    pushedRequest.layout = {0u, 0u, 124u, 4u};
    Require(!ShaderRecompiler::MatchesPreparedShader(pushedRequest, *pushedHandle), "a full push budget reused a replay artifact");
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(pushedRequest)); }, "unused push constant dword");
    pushedRequest.layout.pushConstantSizeBytes = 8u;
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(pushedRequest)); }, "crosses a stage slot");
    const std::array<std::uint32_t, 4> noStencilExport{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
    auto noStencilRequest = stencilRefExportRequest(0x52u, plain, noStencilExport);
    noStencilRequest.layout.pushConstantSizeBytes = 0u;
    const auto noStencil = ShaderRecompiler::Recompile(noStencilRequest);
    Require(noStencil.stencilReferences.empty() && noStencil.stencilReferenceDword == 0xffffffffu && noStencil.pushConstants.empty(), "a stencil export flag without a remaining stencil output reserved replay state");
    auto queue = stencilRefExportQueue(0x52u, 2u);
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x007] = (1u << 16u) | 3u;
    queue.context[0x00a] = 7;
    queue.context[0x00b] = 0;
    queue.context[0x010] = 0x22900983;
    queue.context[0x011] = 0x20000181;
    for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
    for (const auto offset : {0x013u, 0x015u}) queue.context[offset] = 0x200;
    queue.context[0x10b] = 0x00030030;
    queue.context[0x10c] = 0x05ffff02;
    queue.context[0x10d] = 0x05ffff02;
    queue.context[0x200] = 0x00200211;
    auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilTest && state.stencilFront.compareOp == VK_COMPARE_OP_EQUAL && state.stencilFront.passOp == VK_STENCIL_OP_REPLACE, "a stencil reference export changed the REPLACE_TEST decode");
    for (const auto ops : {0x00020020u, 0x00040040u, 0x00000200u, 0x00000004u}) {
        queue.context[0x10b] = ops;
        expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(queue)); }, "exports the stencil test value");
    }
    queue.context[0x10b] = 0x00050050;
    queue.context[0x10c] = 0x01ffff02;
    state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.stencilFront.passOp == VK_STENCIL_OP_INCREMENT_AND_CLAMP, "a stencil reference export changed the increment decode");
    queue.context[0x000] = 2;
    expectFailure([&] { static_cast<void>(AgcDriver::Graphics::DecodeState(queue)); }, "stencil clear with a pixel shader that exports the stencil test value");
    queue.context[0x203] = 0x50u;
    static_cast<void>(AgcDriver::Graphics::DecodeState(queue));
}

bool stencilComparison(VkCompareOp op, std::uint32_t reference, std::uint32_t stencil) {
    switch (op) {
        case VK_COMPARE_OP_NEVER: return false;
        case VK_COMPARE_OP_LESS: return reference < stencil;
        case VK_COMPARE_OP_EQUAL: return reference == stencil;
        case VK_COMPARE_OP_LESS_OR_EQUAL: return reference <= stencil;
        case VK_COMPARE_OP_GREATER: return reference > stencil;
        case VK_COMPARE_OP_NOT_EQUAL: return reference != stencil;
        case VK_COMPARE_OP_GREATER_OR_EQUAL: return reference >= stencil;
        case VK_COMPARE_OP_ALWAYS: return true;
        default: throw std::runtime_error("invalid stencil comparison in the reference oracle");
    }
}

std::pair<std::uint32_t, bool> stencilReferenceOutcome(const AgcDriver::Graphics::State& state, const VkStencilOpState& face, std::uint32_t reference, std::uint32_t stencil, bool depthPass, bool boundsPass) {
    if (state.depthBoundsTest && !boundsPass) return {stencil, false};
    const bool stencilPass = stencilComparison(face.compareOp, reference & face.compareMask & 0xffu, stencil & face.compareMask & 0xffu);
    if (!state.depthTest || state.depthCompare == VK_COMPARE_OP_ALWAYS) depthPass = true;
    if (state.depthTest && state.depthCompare == VK_COMPARE_OP_NEVER) depthPass = false;
    const auto op = !stencilPass ? face.failOp : !depthPass ? face.depthFailOp : face.passOp;
    auto updated = stencil;
    switch (op) {
        case VK_STENCIL_OP_KEEP: break;
        case VK_STENCIL_OP_ZERO: updated = 0; break;
        case VK_STENCIL_OP_REPLACE: updated = reference; break;
        case VK_STENCIL_OP_INCREMENT_AND_CLAMP: updated = std::min(stencil + 1u, 255u); break;
        case VK_STENCIL_OP_DECREMENT_AND_CLAMP: updated = stencil == 0 ? 0 : stencil - 1u; break;
        case VK_STENCIL_OP_INVERT: updated = ~stencil; break;
        case VK_STENCIL_OP_INCREMENT_AND_WRAP: updated = stencil + 1u; break;
        case VK_STENCIL_OP_DECREMENT_AND_WRAP: updated = stencil - 1u; break;
        default: throw std::runtime_error("invalid stencil operation in the reference oracle");
    }
    const auto writeMask = face.writeMask & 0xffu;
    return {((stencil & ~writeMask) | (updated & writeMask)) & 0xffu, stencilPass && depthPass};
}

void stencilReferenceUnusedTests() {
    using AgcDriver::Graphics::StencilReferenceUnused;
    AgcDriver::Graphics::State state{};
    state.depth.emplace();
    state.depth->format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    state.stencilTest = true;
    state.stencilFront = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 37u};
    state.stencilBack = state.stencilFront;
    std::size_t accepted = 0;
    std::size_t countermodels = 0;
    const auto verify = [&](const auto& candidate, bool unused, bool witness, std::string_view name) {
        Require(StencilReferenceUnused(candidate) == unused, "wrong unused stencil reference decision for " + std::string(name));
        bool changed = false;
        for (const auto& face : {candidate.stencilFront, candidate.stencilBack}) {
            for (std::uint32_t stencil = 0; stencil < 256u; ++stencil) {
                for (const bool depthPass : {false, true}) {
                    for (const bool boundsPass : {false, true}) {
                        const auto fixed = stencilReferenceOutcome(candidate, face, face.reference, stencil, depthPass, boundsPass);
                        for (std::uint32_t reference = 0; reference < 256u; ++reference) {
                            changed |= stencilReferenceOutcome(candidate, face, reference, stencil, depthPass, boundsPass) != fixed;
                        }
                    }
                }
            }
        }
        Require(!unused || !changed, "dropping the stencil reference changes a sample outcome for " + std::string(name));
        Require(!witness || changed, "the dependent stencil reference has no countermodel for " + std::string(name));
        accepted += unused;
        countermodels += witness;
    };
    verify(state, true, false, "ALWAYS with KEEP");
    for (const auto op : {VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_DECREMENT_AND_CLAMP, VK_STENCIL_OP_INVERT, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_DECREMENT_AND_WRAP}) {
        auto independent = state;
        independent.stencilFront.passOp = op;
        independent.stencilBack = independent.stencilFront;
        verify(independent, true, false, "reference-independent update");
    }
    for (std::uint32_t comparison = 0; comparison < 8u; ++comparison) {
        auto masked = state;
        masked.stencilFront.compareOp = static_cast<VkCompareOp>(comparison);
        masked.stencilFront.compareMask = 0x100u;
        masked.stencilBack = masked.stencilFront;
        verify(masked, true, false, "effective zero compare mask");
        if (comparison != VK_COMPARE_OP_ALWAYS && comparison != VK_COMPARE_OP_NEVER) {
            auto dependent = masked;
            dependent.stencilBack.compareMask = 0x80u;
            dependent.stencilBack.writeMask = 0;
            verify(dependent, false, true, "dependent back-face comparison with no writes");
        }
    }
    auto replace = state;
    replace.stencilFront.passOp = VK_STENCIL_OP_REPLACE;
    verify(replace, false, true, "reachable front pass REPLACE");
    replace.stencilFront.passOp = VK_STENCIL_OP_KEEP;
    replace.stencilBack.failOp = VK_STENCIL_OP_REPLACE;
    verify(replace, true, false, "unreachable back fail REPLACE");
    replace.stencilBack.compareOp = VK_COMPARE_OP_NEVER;
    verify(replace, false, true, "reachable back fail REPLACE");
    replace.stencilBack.failOp = VK_STENCIL_OP_KEEP;
    replace.stencilBack.passOp = VK_STENCIL_OP_REPLACE;
    replace.stencilBack.depthFailOp = VK_STENCIL_OP_REPLACE;
    verify(replace, true, false, "NEVER with unreachable depth operations");
    replace = state;
    replace.stencilBack.passOp = VK_STENCIL_OP_REPLACE;
    replace.stencilBack.writeMask = 0x100u;
    verify(replace, true, false, "effective zero write mask");
    replace.stencilBack.writeMask = 0x80u;
    verify(replace, false, true, "partially writable REPLACE");
    replace = state;
    replace.stencilBack.depthFailOp = VK_STENCIL_OP_REPLACE;
    verify(replace, true, false, "disabled depth test");
    replace.depthTest = true;
    replace.depthCompare = VK_COMPARE_OP_ALWAYS;
    verify(replace, true, false, "always passing depth test");
    replace.depthCompare = VK_COMPARE_OP_LESS;
    verify(replace, false, true, "possibly failing depth test");
    replace.depthCompare = VK_COMPARE_OP_NEVER;
    verify(replace, false, true, "always failing depth test");
    replace.stencilBack.depthFailOp = VK_STENCIL_OP_KEEP;
    replace.stencilBack.passOp = VK_STENCIL_OP_REPLACE;
    verify(replace, true, false, "unreachable depth pass REPLACE");
    replace = state;
    replace.depthBoundsTest = true;
    verify(replace, true, false, "bounds with reference-independent operations");
    replace.stencilBack.depthFailOp = VK_STENCIL_OP_REPLACE;
    Require(!StencilReferenceUnused(replace), "depth bounds admitted an unchecked REPLACE operation");
    auto clear = state;
    clear.stencilFront = {VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 7u};
    clear.stencilBack = clear.stencilFront;
    verify(clear, false, true, "clear replacement state");
    for (const bool front : {false, true}) {
        auto invalid = state;
        (front ? invalid.stencilFront : invalid.stencilBack).compareOp = static_cast<VkCompareOp>(8u);
        Require(!StencilReferenceUnused(invalid), "an unknown stencil comparison lost its export");
        invalid = state;
        (front ? invalid.stencilFront : invalid.stencilBack).passOp = static_cast<VkStencilOp>(8u);
        Require(!StencilReferenceUnused(invalid), "an unknown stencil operation lost its export");
    }
    auto invalidDepth = state;
    invalidDepth.depthTest = true;
    invalidDepth.depthCompare = static_cast<VkCompareOp>(8u);
    Require(!StencilReferenceUnused(invalidDepth), "an unknown depth comparison lost its stencil export");
    auto inactive = clear;
    inactive.stencilTest = false;
    Require(StencilReferenceUnused(inactive), "a disabled stencil test retained the export");
    inactive.stencilTest = true;
    inactive.depth.reset();
    Require(StencilReferenceUnused(inactive), "an absent attachment retained the export");
    std::cout << "Unused stencil reference sample equivalence passed: " << accepted << " accepted states, " << countermodels << " dependent countermodels\n";
}

std::set<std::string> bufferViews(std::span<const std::uint32_t> words) {
    std::set<std::string> views;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpName) continue;
        const std::string name(reinterpret_cast<const char*>(&words[at + 2]));
        if (name.starts_with("buffers")) views.insert(name);
    }
    return views;
}

void aliasedBufferViewTests() {
    constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityInt64Atomics};
    constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    constexpr std::array<std::uint32_t, 12> userData{0u, 0u, 0u, 0u, 0x10000000u, 0u, 0x1000u, 0x31016facu, 0x20000000u, 0u, 0x1000u, 0x31038facu};
    const auto code = [](std::uint32_t store, std::uint32_t resource) {
        return std::array<std::uint32_t, 9>{0xe0000000u, 0x80010a00u, 0xbf8c3f70u, store, resource, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
    };
    const std::array<std::pair<std::array<std::uint32_t, 9>, std::set<std::string>>, 2> cases{{
        {code(0xe0104000u, 0x80010a00u), {"buffers", "buffers_coherent"}},
        {code(0xe1480000u, 0x80010a00u), {"buffers", "buffers_u64"}},
    }};
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    for (const auto& [words, views] : cases) {
        auto request = orderedPixelRequest(0x20600u, capabilities, words);
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        auto pixel = ShaderRecompiler::Recompile(request);
        const auto declared = bufferViews(pixel.spirv.Words());
        std::string names;
        for (const auto& name : declared) names += " " + name;
        Require(declared == views, "the pixel shader declared the guest buffers as" + names + ", not as the expected " + std::to_string(views.size()) + " aliased variables");
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, true);
        auto unaliased = pixel.spirv.Copy();
        for (std::size_t at = 5; at < unaliased.size() && (unaliased[at] >> 16u) != 0; at += unaliased[at] >> 16u) {
            if (static_cast<spv::Op>(unaliased[at] & 0xffffu) == spv::OpDecorate && unaliased[at + 2] == spv::DecorationAliased) unaliased[at + 2] = spv::DecorationRestrict;
        }
        pixel.spirv = std::move(unaliased);
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, true); }, "duplicate SPIR-V resource binding");
    }
}

void LineStripTests() {
    using namespace AgcDriver::Graphics;
    auto queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    queue.userConfig[0x242] = 3;
    auto state = DecodeState(queue);
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP && !state.rectList && !state.primitiveRestart, "primitive type 3 did not decode as a line strip");
    Require(RasterPrimitiveOf(state) == ShaderRecompiler::RasterPrimitive::Line, "a line strip draw did not get the line primitive class");
    queue.userConfig[0x24b] = 1;
    queue.context[0x103] = 0xffffffffu;
    state = DecodeState(queue);
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP && state.primitiveRestart, "primitive restart was lost on a line strip");
    const auto rejection = DrawRejection(queue, true);
    Require(rejection.empty(), "the draw precheck rejected an indexed line strip with primitive restart: " + rejection);
}

std::uint32_t keyedGenerator = 0;
std::size_t keyedWords = 0;

VKAPI_ATTR VkResult VKAPI_CALL keyedCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo* info, const VkAllocationCallbacks*, VkShaderModule* module) {
    keyedWords = info->codeSize / sizeof(std::uint32_t);
    keyedGenerator = info->pCode[2];
    *module = makeHandle<VkShaderModule>();
    return VK_SUCCESS;
}

void ShaderModuleKeyTests() {
    const std::array<std::uint32_t, 5> code{0x07230203u, 0x00010300u, 0x00080001u, 8u, 0u};
    static_cast<void>(AgcDriver::Graphics::CreateShaderModule(keyedCreateShaderModule, VK_NULL_HANDLE, true, code.data(), sizeof(code), "keyed module"));
    Require(keyedWords == code.size() && keyedGenerator == 0x00080001u, "a shader module with graphics pipeline libraries changed its generator word");
    static_cast<void>(AgcDriver::Graphics::CreateShaderModule(keyedCreateShaderModule, VK_NULL_HANDLE, false, code.data(), sizeof(code), "keyed module"));
    Require(keyedWords == code.size() && keyedGenerator == (0x00080001u ^ 0x8000u), "a shader module without graphics pipeline libraries kept the generator word of the other mode");
    Require(code[2] == 0x00080001u, "keying a shader module changed its source words");
}

void ProvokingVertexTests() {
    using namespace AgcDriver::Graphics;
    constexpr std::uint32_t lastVertex = 1u << 19u;
    auto queue = makeState();
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto first = DecodeState(queue);
    Require(first.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, "the default provoking vertex is not first");
    for (const auto primitive : {2u, 3u, 4u, 5u, 6u}) {
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
    ValidateProvokingVertex(unsupported, last);
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
    const std::array<std::uint32_t, 7> flatP10Color{0xc8120000u, 0xc8160100u, 0xc81a0200u, 0xc81e0300u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto flatP10Pixel = recompilePixel({0x400u}, flatP10Color);
    const auto p10Inputs = locatedInputs(flatP10Pixel.spirv.Words());
    Require(p10Inputs.size() == 1 && p10Inputs[0].location == 0 && p10Inputs[0].flat && !p10Inputs[0].perVertex, "flat v_interp_mov P10 did not compile to a flat input");
    Require(flatP10Pixel.fragmentParameters.size() == 1 && flatP10Pixel.fragmentParameters[0].flatP10 && !flatP10Pixel.fragmentParameters[0].perVertex && !flatPixel.fragmentParameters[0].flatP10, "flat v_interp_mov P10 was not recorded apart from P0");
    const std::array<CompiledShader, 1> flatP10Shaders{{{ShaderRecompiler::ShaderStage::Fragment, &flatP10Pixel, 0}}};
    ValidateProvokingVertex(supported, last, flatP10Shaders);
    expectFailure([&] { ValidateProvokingVertex(supported, first, flatP10Shaders); }, "v_interp_mov p10 of a flat input with the first provoking vertex is not measured");
    const std::array<std::uint32_t, 7> flatP20Color{0xc8120001u, 0xc8160101u, 0xc81a0201u, 0xc81e0301u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto flatP20Pixel = recompilePixel({0x400u}, flatP20Color);
    const auto p20Inputs = locatedInputs(flatP20Pixel.spirv.Words());
    Require(p20Inputs.size() == 1 && p20Inputs[0].location == 0 && p20Inputs[0].flat && !p20Inputs[0].perVertex, "flat v_interp_mov P20 did not compile to a flat input");
    Require(flatP20Pixel.fragmentParameters.size() == 1 && flatP20Pixel.fragmentParameters[0].flatP20 && !flatP20Pixel.fragmentParameters[0].flatP10 && !flatP20Pixel.fragmentParameters[0].perVertex && !flatP10Pixel.fragmentParameters[0].flatP20, "flat v_interp_mov P20 was not recorded apart from P10");
    const std::array<CompiledShader, 1> flatP20Shaders{{{ShaderRecompiler::ShaderStage::Fragment, &flatP20Pixel, 0}}};
    ValidateProvokingVertex(supported, first, flatP20Shaders);
    expectFailure([&] { ValidateProvokingVertex(supported, last, flatP20Shaders); }, "v_interp_mov p20 of a flat input with the last provoking vertex is not measured");
    auto lastStrip = last;
    lastStrip.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    auto lastList = last;
    lastList.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const std::array<std::uint32_t, 11> smoothColor{0xc8100000u, 0xc8110001u, 0xc8140100u, 0xc8150101u, 0xc8180200u, 0xc8190201u, 0xc81c0300u, 0xc81d0301u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto smoothPixel = recompilePixel({0x0u}, smoothColor);
    Require(smoothPixel.fragmentParameters.size() == 1 && smoothPixel.fragmentParameters[0].perVertex && !smoothPixel.fragmentParameters[0].vertexOrder, "v_interp_p1/p2 of a smooth input was recorded as reading a vertex slot");
    const std::array<CompiledShader, 1> smoothShaders{{{ShaderRecompiler::ShaderStage::Fragment, &smoothPixel, 0}}};
    ValidateProvokingVertex(supported, lastStrip, smoothShaders);
    const auto slotPixel = recompilePixel({0x0u}, flatP10Color);
    Require(slotPixel.fragmentParameters.size() == 1 && slotPixel.fragmentParameters[0].perVertex && slotPixel.fragmentParameters[0].vertexOrder, "v_interp_mov P10 of a smooth input was not recorded as reading a vertex slot");
    const std::array<CompiledShader, 1> slotShaders{{{ShaderRecompiler::ShaderStage::Fragment, &slotPixel, 0}}};
    expectFailure([&] { ValidateProvokingVertex(supported, lastStrip, slotShaders); }, "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
    ValidateProvokingVertex(supported, lastList, slotShaders);
    ShaderRecompiler::RecompileResult fragment{};
    const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN}) {
        auto state = last;
        state.topology = topology;
        for (const auto flat : {false, true}) {
            for (const auto perVertex : {false, true}) {
                for (const auto vertexOrder : {false, true}) {
                    if (vertexOrder && !perVertex) continue;
                    fragment.fragmentParameters = {{0, 0, flat, perVertex, false, false, vertexOrder}};
                    auto firstState = state;
                    firstState.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
                    ValidateProvokingVertex(unsupported, firstState, shaders);
                    if (flat && perVertex) {
                        expectFailure([&] { ValidateProvokingVertex(supported, state, shaders); }, "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
                    } else if (vertexOrder && topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
                        expectFailure([&] { ValidateProvokingVertex(supported, state, shaders); }, "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
                    } else {
                        ValidateProvokingVertex(supported, state, shaders);
                    }
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
    for (std::uint32_t generated = 0; generated < 2; ++generated) {
        auto state = last;
        if (generated == 0) state.stages.mesh.emplace();
        if (generated == 1) state.stages.tessellation.emplace();
        expectFailure([&] { ValidateProvokingVertex(supported, state); }, "last provoking vertex is unsupported for generated primitive pipelines");
    }
    auto meshDraw = last;
    meshDraw.stages.path = ShaderPath::Geometry;
    meshDraw.stages.mesh.emplace();
    meshDraw.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST}) {
        meshDraw.topology = topology;
        for (const auto& drawn : {std::span<const CompiledShader>{}, std::span<const CompiledShader>(flatShaders), std::span<const CompiledShader>(flatP10Shaders), std::span<const CompiledShader>(smoothShaders), std::span<const CompiledShader>(slotShaders)}) ValidateProvokingVertex(supported, meshDraw, drawn);
    }
    meshDraw.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    for (const auto& drawn : {std::span<const CompiledShader>{}, std::span<const CompiledShader>(flatShaders), std::span<const CompiledShader>(flatP10Shaders), std::span<const CompiledShader>(smoothShaders)}) ValidateProvokingVertex(supported, meshDraw, drawn);
    fragment.fragmentParameters = {{0, 0, false, true, false, false, false}, {1, 1, false, true, false, false, false}, {2, 2, false, true, false, false, false}, {3, 3, false, true, false, false, false}};
    ValidateProvokingVertex(supported, meshDraw, shaders);
    expectFailure([&] { ValidateProvokingVertex(supported, meshDraw, slotShaders); }, "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
    meshDraw.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    expectFailure([&] { ValidateProvokingVertex(supported, meshDraw, flatShaders); }, "last provoking vertex is unsupported for triangle fans assembled for a geometry program");
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN}) {
        auto firstDraw = meshDraw;
        firstDraw.topology = topology;
        firstDraw.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
        ValidateProvokingVertex(supported, firstDraw, flatShaders);
    }
    meshDraw.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    expectFailure([&] { ValidateProvokingVertex(unsupported, meshDraw, flatShaders); }, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
    fragment.fragmentParameters = {{0, 0, true, true, false, false, false}};
    expectFailure([&] { ValidateProvokingVertex(supported, meshDraw, shaders); }, "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
    meshDraw.stages.tessellation.emplace();
    expectFailure([&] { ValidateProvokingVertex(supported, meshDraw, flatShaders); }, "last provoking vertex is unsupported for generated primitive pipelines");
    auto rectangle = last;
    rectangle.rectList = true;
    rectangle.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    ValidateProvokingVertex(supported, rectangle, flatShaders);
    last.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_MAX_ENUM_EXT;
    expectFailure([&] { ValidateProvokingVertex(supported, last); }, "invalid provoking vertex mode");
}

void UnobservedProvokingVertexTests() {
    using namespace AgcDriver::Graphics;
    auto last = DecodeState(makeState());
    last.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
    const Context unsupported{};
    const auto module = [](std::initializer_list<std::uint32_t> instructions) {
        std::vector<std::uint32_t> words{spv::MagicNumber, 0x10300u, 0u, 4u, 0u};
        words.insert(words.end(), instructions);
        return words;
    };
    ShaderRecompiler::RecompileResult fragment{};
    const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    const ShaderRecompiler::FragmentParameter smooth{0, 0, false, false};
    const auto refused = [&](const State& state) {
        expectFailure([&] { ValidateProvokingVertex(unsupported, state, shaders); }, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
    };
    for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN}) {
        auto state = last;
        state.topology = topology;
        ValidateProvokingVertex(unsupported, state);
        fragment.fragmentParameters.clear();
        fragment.spirv = module({});
        ValidateProvokingVertex(unsupported, state, shaders);
        fragment.fragmentParameters = {smooth};
        fragment.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationLocation, 0u});
        ValidateProvokingVertex(unsupported, state, shaders);
        for (std::uint32_t read = 0; read < 4; ++read) {
            const ShaderRecompiler::FragmentParameter observed{1, 1, read == 0, read == 1, read == 2, read == 3, false};
            fragment.fragmentParameters = {smooth, observed};
            refused(state);
        }
        fragment.fragmentParameters = {smooth};
        for (const std::uint32_t decoration : {static_cast<std::uint32_t>(spv::DecorationFlat), static_cast<std::uint32_t>(spv::DecorationPerVertexKHR)}) {
            fragment.spirv = module({(3u << 16u) | spv::OpDecorate, 1u, decoration});
            refused(state);
            fragment.spirv = module({(4u << 16u) | spv::OpMemberDecorate, 1u, 0u, decoration});
            refused(state);
        }
        fragment.spirv = module({(5u << 16u) | spv::OpDecorate, 1u, spv::DecorationLocation});
        refused(state);
        fragment.spirv = std::vector<std::uint32_t>{};
        refused(state);
    }
    fragment.fragmentParameters = {smooth};
    for (const std::uint32_t generated : {static_cast<std::uint32_t>(spv::BuiltInSampleId), static_cast<std::uint32_t>(spv::BuiltInFrontFacing), static_cast<std::uint32_t>(spv::BuiltInSubgroupLocalInvocationId)}) {
        fragment.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, generated, (3u << 16u) | spv::OpDecorate, 1u, spv::DecorationFlat});
        ValidateProvokingVertex(unsupported, last, shaders);
        fragment.spirv = module({(3u << 16u) | spv::OpDecorate, 1u, spv::DecorationFlat, (4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, generated});
        ValidateProvokingVertex(unsupported, last, shaders);
        fragment.spirv = module({(5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, generated, (4u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationFlat});
        ValidateProvokingVertex(unsupported, last, shaders);
        fragment.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, generated, (3u << 16u) | spv::OpDecorate, 2u, spv::DecorationFlat});
        refused(last);
        fragment.spirv = module({(5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, generated, (4u << 16u) | spv::OpMemberDecorate, 1u, 1u, spv::DecorationFlat});
        refused(last);
    }
    for (const std::uint32_t passed : {static_cast<std::uint32_t>(spv::BuiltInLayer), static_cast<std::uint32_t>(spv::BuiltInViewportIndex), static_cast<std::uint32_t>(spv::BuiltInPrimitiveId)}) {
        fragment.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, passed, (3u << 16u) | spv::OpDecorate, 1u, spv::DecorationFlat});
        refused(last);
        fragment.spirv = module({(5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, passed, (4u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationFlat});
        refused(last);
    }
    ShaderRecompiler::RecompileResult vertex{};
    vertex.fragmentParameters = {{0, 0, true, true, true, true, true}};
    vertex.spirv = module({(3u << 16u) | spv::OpDecorate, 1u, spv::DecorationFlat});
    fragment.fragmentParameters = {smooth};
    fragment.spirv = module({});
    const std::array<CompiledShader, 2> stages{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    ValidateProvokingVertex(unsupported, last, stages);
    Context supported{};
    supported.provokingVertexLast = true;
    for (const auto stage : {ShaderRecompiler::ShaderStage::Vertex, ShaderRecompiler::ShaderStage::TessellationEvaluation, ShaderRecompiler::ShaderStage::Geometry, ShaderRecompiler::ShaderStage::Mesh}) {
        ShaderRecompiler::RecompileResult writer{};
        const std::array<CompiledShader, 2> written{{{stage, &writer, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        const auto refusedWrite = [&] {
            expectFailure([&] { ValidateProvokingVertex(unsupported, last, written); }, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
            ValidateProvokingVertex(supported, last, written);
        };
        for (const std::uint32_t selected : {static_cast<std::uint32_t>(spv::BuiltInLayer), static_cast<std::uint32_t>(spv::BuiltInViewportIndex)}) {
            writer.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, selected});
            refusedWrite();
            writer.spirv = module({(5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, selected});
            refusedWrite();
            writer.spirv = module({(5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, selected, (4u << 16u) | spv::OpMemberDecorate, 1u, 1u, spv::DecorationPerPrimitiveEXT});
            refusedWrite();
            writer.spirv = module({(4u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationPerPrimitiveEXT, (5u << 16u) | spv::OpMemberDecorate, 1u, 0u, spv::DecorationBuiltIn, selected});
            ValidateProvokingVertex(unsupported, last, written);
            writer.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, selected, (3u << 16u) | spv::OpDecorate, 1u, spv::DecorationPerPrimitiveEXT});
            ValidateProvokingVertex(unsupported, last, written);
        }
        writer.spirv = module({(4u << 16u) | spv::OpDecorate, 1u, spv::DecorationBuiltIn, spv::BuiltInPosition, (5u << 16u) | spv::OpMemberDecorate, 2u, 0u, spv::DecorationBuiltIn, spv::BuiltInPointSize});
        ValidateProvokingVertex(unsupported, last, written);
        writer.spirv = std::vector<std::uint32_t>{};
        refusedWrite();
    }
    const std::array<CompiledShader, 1> missing{{{ShaderRecompiler::ShaderStage::Fragment, nullptr, 0}}};
    expectFailure([&] { ValidateProvokingVertex(unsupported, last, missing); }, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
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
    for (const auto primitive : {4u, 5u, 6u, 21u}) {
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
    geometry.context[0x280] = 0x00080008u;  // PA_SU_POINT_SIZE and PA_SU_POINT_MINMAX: a point-output geometry program decodes them
    geometry.context[0x281] = 0x00080008u;
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
    tessellation.context[0x2d5] = 0x0200210d;
    tessellation.userConfig[0x242] = 9;
    tessellation.context[0x2d6] = 0xc355;
    tessellation.context[0x2db] = 1u | (2u << 2u) | (2u << 5u);
    tessellation.context[0x286] = 0x42800000;
    tessellation.context[0x287] = 0x3f800000;
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
        Require(AgcDriver::Graphics::DecodeState(queue).conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT, "OVER_RAST_ENABLE rejected centroid inputs " + std::to_string(inputs) + " of a single rasterized sample");
        auto multisampled = queue;
        multisampled.context[0x2f8] = 0x00200002;
        multisampled.context[0x31d] = (1u << 12u) | (1u << 15u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(multisampled); }, "conservative rasterization with centroid interpolation of more than one rasterized sample is unsupported");
        auto raised = queue;
        raised.context[0x31d] = (1u << 12u) | (1u << 15u);
        raised.context[0x292] = 2;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(raised); }, "conservative rasterization with centroid interpolation of a draw raised to a multisampled target is unsupported");
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
    Require(packed.PipelineVariantId() != unpacked.PipelineVariantId() && !(packed.spirv == unpacked.spirv), "the export packing did not specialize the module");
    Require(ShaderRecompiler::Recompile(request).PipelineVariantId() == packed.PipelineVariantId(), "the packed specialization was not reused");
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    const auto validate = [&](const ShaderRecompiler::RecompileResult& pixel, const AgcDriver::Graphics::State& target) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        return AgcDriver::Graphics::ValidateShaders(shaders, target, subgroup, false);
    };
    Require(validate(packed, state) == std::set<std::uint32_t>{0u}, "the packed export did not reach attachment 0");
    expectFailure([&] { validate(unpacked, state); }, "uint4 words to its packed color attachments");
    auto plainState = state;
    plainState.colors[0].packing = ColorExportPacking::None;
    expectFailure([&] { validate(packed, plainState); }, "float4 colors");
    auto targetless = plainState;
    targetless.colors.clear();
    targetless.blends.clear();
    Require(validate(packed, targetless) == std::set<std::uint32_t>{0u}, "a uint4 export from a draw with no color target was refused");
    auto gap = plainState;
    gap.blends.insert(gap.blends.begin(), gap.blends.front());
    gap.colors[0].exportIndex = 1;
    Require(validate(packed, gap) == std::set<std::uint32_t>{0u}, "a uint4 export into the unused slot below a color target was refused");
    auto uintExport = request;
    uintExport.context.pixel->targetOutputMode[0] = 7;
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(uintExport)); }, "fragment output 0 has no color export packing path");
}

void float11_11_10TargetTests() {
    using ShaderRecompiler::ColorExportPacking;
    auto queue = makeState();
    queue.context[0x31c] = (queue.context.at(0x31c) & ~0x1f7cu) | (7u << 2u) | (7u << 8u) | (2u << 11u) | 0x40000u;
    queue.context[0x1b3] = 2;
    queue.context[0x1b4] = 2;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.color.format == VK_FORMAT_R32_UINT && state.color.elementBytes == 4 && state.color.bytes == colorMemory.size() && state.color.componentMapping == 0xc6u && state.color.packing == ColorExportPacking::Float11_11_10, "an 11_11_10 float target did not decode as a packed R32_UINT attachment with blue in the low bits");
    const auto packings = AgcDriver::Graphics::ExportPackings(state);
    Require(packings[0] == ColorExportPacking::Float11_11_10 && std::all_of(packings.begin() + 1, packings.end(), [](ColorExportPacking packing) { return packing == ColorExportPacking::None; }), "export 0 did not carry the 11_11_10 float packing alone");
    Require(AgcDriver::Graphics::ExportMappings(state)[0] == 0xc6u, "export 0 did not reverse its components into the 11_11_10 float target");
    auto changed = queue;
    changed.context[0x31c] &= ~0x40000u;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "rounding (ROUND_MODE 0) 11_11_10 float color targets");
    changed = queue;
    changed.context[0x31c] &= ~0x1800u;
    const auto standard = AgcDriver::Graphics::DecodeState(changed);
    Require(standard.color.format == VK_FORMAT_R32_UINT && standard.color.componentMapping == 0xe4u && standard.color.packing == ColorExportPacking::Float11_11_10, "an 11_11_10 float target with the standard component order did not decode");
    Require(AgcDriver::Graphics::ExportMappings(standard)[0] == 0xe4u, "export 0 reordered its components into the standard-order 11_11_10 float target");
    for (const auto swap : {1u, 3u}) {
        changed = queue;
        changed.context[0x31c] = (changed.context[0x31c] & ~0x1800u) | (swap << 11u);
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "unsupported color format 7 number type 7 component swap " + std::to_string(swap));
    }
    for (const auto number : {0u, 4u}) {
        changed = queue;
        changed.context[0x31c] = (changed.context[0x31c] & ~0x700u) | (number << 8u) | 0x8000u;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "unsupported color format 7 number type " + std::to_string(number) + " component swap 2");
    }
    changed = queue;
    changed.context[0x1e0] = 0x60010101u;
    for (std::uint32_t i = 0; i < 4; ++i) changed.context[0x105 + i] = 0;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "blending into an 11_11_10 float color target");
    changed = queue;
    changed.context[0x8e] = 6;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "partial writes of an 11_11_10 float color target");
    changed.context[0x8e] = 7;
    Require(AgcDriver::Graphics::DecodeState(changed).color.packing == ColorExportPacking::Float11_11_10, "an RGB write of an 11_11_10 float target was refused");
    for (const auto format : {5u, 6u}) {
        changed = queue;
        changed.context[0x1c5] = format;
        expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "color export format " + std::to_string(format) + " into an 11_11_10 float target");
    }
    changed = queue;
    changed.context[0x1c5] = 4;
    Require(AgcDriver::Graphics::DecodeState(changed).color.packing == ColorExportPacking::Float11_11_10, "an FP16_ABGR export into an 11_11_10 float target was refused");
    changed = queue;
    changed.context[0x31c] |= 0x10000000u;
    changed.context[0x325] = 0x1234;
    expectFailure([&] { AgcDriver::Graphics::DecodeState(changed); }, "DCC-compressed 11_11_10 float color targets");

    constexpr std::array<std::uint32_t, 3> code{0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto request = packedExportRequest(queue, state, code);
    auto unorm = request;
    unorm.context.pixel->targetExportPacking[0] = ColorExportPacking::Unorm10_11_11;
    const ShaderRecompiler::RequestSerializer serializer;
    Require(serializer.Deserialize(serializer.Serialize(request)).request.context.pixel->targetExportPacking == request.context.pixel->targetExportPacking, "the 11_11_10 float packing was lost in serialization");
    const auto packed = ShaderRecompiler::Recompile(request);
    const auto unormPacked = ShaderRecompiler::Recompile(unorm);
    Require(packed.variantId == unormPacked.variantId && packed.PipelineVariantId() != unormPacked.PipelineVariantId(), "the 11_11_10 float packing did not key its own specialization of the shared artifact");
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &packed, 0}}};
    Require(AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false) == std::set<std::uint32_t>{0u}, "the packed float export did not reach attachment 0");
    auto invalid = request;
    invalid.context.pixel->targetExportPacking[0] = static_cast<ColorExportPacking>(3);
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(invalid)); }, "invalid prepared fragment export packing");
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

void ReversedBlendShaderTests() {
    const auto queue = reversedBlendQueue(10u, 0u, 2u, 0x65010504u);
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.reversedBlend == 0x570001u && !state.dualSourceBlend && state.blends.size() == 1, "the reversed-order source-alpha blend did not decode to a pixel shader blend code");
    constexpr std::array<std::uint32_t, 3> code{0xf800180fu, 0x07060504u, 0xbf810000u};
    auto request = packedExportRequest(queue, state, code);
    request.context.pixel->reversedBlend = state.reversedBlend;
    auto plain = request;
    plain.context.pixel->reversedBlend = 0u;
    const ShaderRecompiler::RequestSerializer serializer;
    Require(serializer.Deserialize(serializer.Serialize(request)).request.context.pixel->reversedBlend == 0x570001u, "the reversed-order blend code was lost in serialization");
    std::vector<std::uint64_t> key;
    std::vector<std::uint64_t> plainKey;
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    ShaderRecompiler::BuildPreparedShaderKey(plain, plainKey);
    Require(key == plainKey, "the reversed-order blend code entered the static shader ABI");
    const auto prepared = ShaderRecompiler::PrepareShader(plain);
    Require(ShaderRecompiler::MatchesPreparedShader(request, *prepared), "a reversed-order blended draw did not match the registered pixel shader");
    const auto reversed = ShaderRecompiler::Recompile(request);
    const auto single = ShaderRecompiler::Recompile(plain);
    Require(reversed.variantId == single.variantId && reversed.variantId == ShaderRecompiler::GetPreparedArtifact(*prepared).variantId, "the reversed-order blended draw did not reuse the registered artifact");
    Require(reversed.PipelineVariantId() != single.PipelineVariantId() && ShaderRecompiler::Recompile(request).PipelineVariantId() == reversed.PipelineVariantId(), "the reversed-order blend code did not key its own specialization");
    const auto arithmetic = [](std::span<const std::uint32_t> words) {
        std::size_t count = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
            if (op == spv::OpFAdd || op == spv::OpFSub || op == spv::OpFMul || op == spv::OpFDiv || op == spv::OpFNegate || op == spv::OpExtInst || op == spv::OpSelect) ++count;
        }
        return count;
    };
    for (const auto shaderCode : {0x570001u, 0x540001u, 0x560001u, 0x020001u, 0x560301u, 0xffff01u}) {
        auto changed = request;
        changed.context.pixel->reversedBlend = shaderCode;
        const auto specialized = ShaderRecompiler::Recompile(changed);
        const auto words = specialized.spirv.Words();
        const auto outputs = outputDecorations(words);
        Require(outputs.locations.size() == 2 && std::all_of(outputs.locations.begin(), outputs.locations.end(), [](const auto& entry) { return entry.second == 0u; }) && outputs.indices.size() == 1 && outputs.indices.begin()->second == 1u, "blend code " + std::to_string(shaderCode) + " did not add a second color at location 0 index 1");
        Require(arithmetic(words) == arithmetic(single.spirv.Words()), "blend code " + std::to_string(shaderCode) + " computed in the pixel shader instead of copying the export");
    }
    const auto singleOutputs = outputDecorations(single.spirv.Words());
    Require(singleOutputs.locations.size() == 1 && singleOutputs.locations.begin()->second == 0u && singleOutputs.indices.empty(), "a pixel shader without a reversed-order blend code changed its outputs");
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = makeModule({});
    const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    const auto validate = [&](const ShaderRecompiler::RecompileResult& pixel, const AgcDriver::Graphics::State& target) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}}};
        return AgcDriver::Graphics::ValidateShaders(shaders, target, subgroup, false);
    };
    Require(validate(reversed, state) == std::set<std::uint32_t>{0u}, "the reversed-order blended shader did not validate against its draw");
    expectFailure([&] { validate(single, state); }, "needs a float4 second color at location 0 index 1");
    auto plainState = state;
    plainState.reversedBlend = 0u;
    expectFailure([&] { validate(reversed, plainState); }, "Index decoration");
    for (const auto invalid : {0x2u, 0x80u, 0x100u, 0x10000u, 0x1000001u, 0x80000001u}) {
        auto changed = request;
        changed.context.pixel->reversedBlend = invalid;
        expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(changed)); }, "invalid prepared reversed-order blend");
    }
    const auto twoQueue = dualSourceQueue();
    const auto twoState = AgcDriver::Graphics::DecodeState(twoQueue);
    constexpr std::array<std::uint32_t, 5> twoExports{0xf800000fu, 0x07060504u, 0xf800181fu, 0x0b0a0908u, 0xbf810000u};
    auto both = packedExportRequest(twoQueue, twoState, twoExports);
    both.context.pixel->reversedBlend = 0x570001u;
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(both)); }, "reversed-order blending needs MRT0 to be the only color output");
    both.context.pixel->dualSourceBlend = true;
    expectFailure([&] { static_cast<void>(ShaderRecompiler::Recompile(both)); }, "reversed-order blending cannot be combined with dual-source blending");
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
        pixel.spirv = makeModule({.fragment = true});
        vertex.spirv = makeModule({.parameterOutput = true, .clipDistanceLength = 4, .cullDistanceLength = 4});
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, true, true);
        state.paClVsOutCntl = 0x0040000fu;
        vertex.spirv = makeModule({.parameterOutput = true, .clipDistanceLength = 4});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, false, true); }, "PA_CL_VS_OUT_CNTL=0x0040000f");
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, true, false);
        vertex.spirv = makeModule({.parameterOutput = true, .cullDistanceLength = 4});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, true, false); }, "PA_CL_VS_OUT_CNTL=0x0040000f");
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, false, true);
        state.paClVsOutCntl = 0;
        vertex.spirv = makeModule({.parameterOutput = true, .clipDistanceLength = 9});
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, true, true); }, "clip or cull distance");
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
        const auto validateTerminate = [&](bool enabled) {
            return AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, false, false, false, false, false, enabled);
        };
        vertex.spirv = makeModule({});
        pixel.spirv = makeModule({.fragment = true, .terminateInvocation = true});
        validateTerminate(true);
        expectFailure([&] { validateTerminate(false); }, "shaderTerminateInvocation");
        vertex.spirv = makeModule({.terminateInvocation = true});
        pixel.spirv = makeModule({.fragment = true});
        expectFailure([&] { validateTerminate(true); }, "fragment shader");
        vertex.spirv = makeModule({});
        pixel.spirv = makeModule({.fragment = true, .terminateInvocation = true, .omitTerminateExtension = true});
        expectFailure([&] { validateTerminate(true); }, "OpTerminateInvocation requires SPV_KHR_terminate_invocation");
        expectFailure([&] { validateTerminate(false); }, "OpTerminateInvocation requires enabled shaderTerminateInvocation");
        vertex.spirv = makeModule({.terminateInvocation = true, .omitTerminateExtension = true});
        pixel.spirv = makeModule({.fragment = true});
        expectFailure([&] { validateTerminate(true); }, "OpTerminateInvocation requires enabled shaderTerminateInvocation in a fragment shader");
        vertex.spirv = makeModule({});
        pixel.spirv = ShaderRecompiler::SpecializeSpirv(makeModule({.fragment = true, .terminateInvocation = true}));
        std::size_t terminates = 0;
        std::size_t declarations = 0;
        for (std::size_t at = 5; at < pixel.spirv.size(); at += pixel.spirv[at] >> 16u) {
            const auto op = pixel.spirv[at] & 0xffffu;
            if (op == spv::OpTerminateInvocation) ++terminates;
            if (op == spv::OpExtension && std::string_view(reinterpret_cast<const char*>(&pixel.spirv[at + 1])) == "SPV_KHR_terminate_invocation") ++declarations;
        }
        Require(terminates == 1u && declarations == 1u, "specialization dropped OpTerminateInvocation or its extension declaration");
        validateTerminate(true);
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
        Require(AgcDriver::Graphics::VertexBufferReadOffset(attribute, 2, 7) == 64, "vertex range includes records before the first vertex");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1) == 112, "an index past the records still spans its fetch");
        {
            auto wider = attribute;
            wider.resource.fields[2] = 9;
            auto instanced = attribute;
            instanced.fetchIndex = 1;
            instanced.resource.fields[2] = 1000;
            auto constant = attribute;
            constant.resource.fields[1] = 0;
            const std::array attributes{attribute, wider, instanced, constant};
            Require(AgcDriver::Graphics::ZeroFetchVertex(attributes) == 9u, "the first vertex reading zeros is not past the largest per-vertex record count");
            auto clamped = attribute;
            clamped.resource.fields[3] |= 2u << 28u;
            const std::array selective{attribute, clamped};
            Require(!AgcDriver::Graphics::ZeroFetchVertex(selective).has_value(), "an attribute with out-of-bounds select 2 was taken to read zeros past its records");
            std::array<std::uint32_t, 4> indices{0xffffffffu, 2u, 0x12345u, 0xffffffffu};
            Require(AgcDriver::Graphics::ClampDrawIndices(std::as_writable_bytes(std::span(indices)), 4, true, 9u) == 9u && indices == std::array<std::uint32_t, 4>{0xffffffffu, 2u, 9u, 0xffffffffu}, "clamping kept an index past the limit or moved a restart index");
            Require(AgcDriver::Graphics::ClampDrawIndices(std::as_writable_bytes(std::span(indices)), 4, false, 9u) == 9u && indices == std::array<std::uint32_t, 4>{9u, 2u, 9u, 9u}, "clamping without restart kept a 0xffffffff index");
            std::array<std::uint16_t, 2> narrow{0xffffu, 1u};
            Require(AgcDriver::Graphics::ClampDrawIndices(std::as_writable_bytes(std::span(narrow)), 2, false, 4u) == 4u && narrow[0] == 4u && narrow[1] == 1u, "clamping 16-bit indices failed");
        }
        attribute.fetchIndex = 1;
        Require(AgcDriver::Graphics::VertexBufferReadOffset(attribute, 2, 7) == 224, "instance range includes records before the first instance");
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
        const auto zeroStride = [&](std::uint32_t select, std::uint32_t records) {
            attribute.resource.fields[3] = (77u << 12u) | (select << 28u);
            attribute.resource.fields[2] = records;
        };
        for (const auto records : {0u, 1u, 16u, 0xffffffffu}) {
            zeroStride(0, records);
            Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 0 must read every zero-stride fetch as zero");
            Require(AgcDriver::Graphics::ZeroStrideAvailableBytes(attribute) == 0, "OOB_SELECT 0 zero-stride fetch has bytes to read");
            expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1); }, "byte range");
            expectFailure([&] { AgcDriver::Graphics::VertexBufferExtent(attribute); }, "empty or oversized");
        }
        zeroStride(1, 16);
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 16, "zero stride must repeat one value");
        Require(AgcDriver::Graphics::VertexBufferReadOffset(attribute, 100, 7) == 0, "zero-stride input skipped its repeated value");
        Require(AgcDriver::Graphics::VertexBufferExtent(attribute) == 16, "OOB_SELECT 1 extent must cover the whole fetch");
        zeroStride(1, 0);
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 1 without records must read zero");
        zeroStride(1, 8);
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 1 counted NUM_RECORDS in bytes");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 8) == 16, "OOB_SELECT 1 must read the whole element for every index below NUM_RECORDS");
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1, 7) == 16, "OOB_SELECT 1 must read the whole element for the last index below NUM_RECORDS");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 9); }, "record count");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1, 8); }, "record count");
        zeroStride(1, 1);
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1) == 16, "OOB_SELECT 1 with one record must read the element at index 0");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 2); }, "record count");
        attribute.fetchIndex = 0;
        zeroStride(1, 3);
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 2, 1) == 16, "OOB_SELECT 1 must read the element for index 2 of 3 records");
        expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 3, 1); }, "record count");
        attribute.fetchIndex = 1;
        zeroStride(2, 1);
        Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 16, "OOB_SELECT 2 must read a zero-stride fetch whole while NUM_RECORDS is nonzero");
        Require(AgcDriver::Graphics::VertexBufferExtent(attribute) == 16, "OOB_SELECT 2 extent must cover the whole fetch");
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 2 with records was treated as out of range");
        zeroStride(2, 0);
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 2 without records must read zero");
        zeroStride(3, 0);
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 without records must read zero");
        for (const auto records : {1u, 8u, 15u}) {
            zeroStride(3, records);
            Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 must read a fetch that only partly fits as zero");
            expectFailure([&] { AgcDriver::Graphics::VertexBufferReadSize(attribute, 0, 1); }, "byte range");
        }
        for (const auto records : {16u, 17u, 0xffffffffu}) {
            zeroStride(3, records);
            Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 with the whole fetch in range was treated as out of range");
            Require(AgcDriver::Graphics::VertexBufferReadSize(attribute, 100, 2) == 16, "OOB_SELECT 3 must read a fetch inside the range, whatever the index");
            Require(AgcDriver::Graphics::VertexBufferExtent(attribute) == 16, "OOB_SELECT 3 extent must cover only the fetch");
        }
        attribute.resource.fields[3] = (22u << 12u) | (3u << 28u);
        attribute.resource.fields[2] = 4;
        Require(!AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 compared NUM_RECORDS with the wrong fetch size");
        attribute.resource.fields[2] = 3;
        Require(AgcDriver::Graphics::VertexFetchOutOfRange(attribute), "OOB_SELECT 3 read a 4-byte fetch past 3 bytes");
        attribute.resource.fields[3] = (113u << 12u) | (3u << 28u);
        expectFailure([&] { AgcDriver::Graphics::VertexFetchOutOfRange(attribute); }, "unsupported vertex format");
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
    {
        ShaderRecompiler::RecompileResult vertex;
        vertex.spirv = makeModule({.layerOutput = true});
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
        const VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        expectFailure([&] { AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false); }, "VK_EXT_shader_viewport_index_layer");
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, false, false, true);
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
        expectFailure([&] { validate(vertex, 8); }, "descriptor set other than its stage's");
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
    memory.memoryHeapCount = 2;
    memory.memoryHeaps[1].size = VkDeviceSize{16} << 30u;
    memory.memoryTypeCount = 2;
    memory.memoryTypes[0] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0};
    memory.memoryTypes[1] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1};
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{256} << 20u, "the staging pool budget on a 16 GiB heap of system memory is not a sixty-fourth of it");
    memory.memoryHeaps[1].size = VkDeviceSize{4} << 30u;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{128} << 20u, "the staging pool budget on system memory fell below 128 MiB");
    memory.memoryTypes[0] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1};
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory) == VkDeviceSize{512} << 20u, "a heap with a device-only memory type was taken for system memory");
    memory.memoryHeaps[1].size = VkDeviceSize{16} << 30u;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory, &reported) == VkDeviceSize{2} << 30u, "a zero reported budget did not fall back to an eighth of the heap");
    reported.heapBudget[0] = VkDeviceSize{1} << 30u;
    reported.heapBudget[1] = VkDeviceSize{10} << 30u;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory, &reported) == VkDeviceSize{1280} << 20u, "a 10 GiB budget on the 16 GiB heap does not limit the staging pool to an eighth of the budget");
    reported.heapBudget[1] = VkDeviceSize{2} << 30u;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory, &reported) == VkDeviceSize{512} << 20u, "a 2 GiB budget does not keep the 512 MiB floor");
    reported.heapBudget[1] = VkDeviceSize{40} << 30u;
    Require(AgcDriver::Graphics::BufferPool::DeviceBudget(memory, &reported) == VkDeviceSize{2} << 30u, "a budget above the heap size raised the staging pool past an eighth of the heap");
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
    Require(!HighestDrawIndex({}, 2, false).has_value() && !HighestDrawIndex({}, 4, true).has_value(), "an empty index range reached a vertex");
    std::vector<std::uint16_t> long16(1027, 0xffff);
    long16[1026] = 0;
    Require(HighestDrawIndex(std::as_bytes(std::span(long16)), 2, true) == 0, "index 0 after many restart indices was not reached");
    long16[513] = 4000;
    Require(HighestDrawIndex(std::as_bytes(std::span(long16)), 2, true) == 4000, "a 16-bit index inside a long range was missed");
    std::vector<std::uint32_t> long32(1029);
    for (std::size_t i = 0; i < long32.size(); ++i) long32[i] = static_cast<std::uint32_t>(i * 7u % 1000u);
    long32[1028] = 0x80000001u;
    Require(HighestDrawIndex(std::as_bytes(std::span(long32)), 4, true) == 0x80000001u, "a 32-bit index above 2^31 in the tail was missed");
    long32[1028] = 0xffffffffu;
    Require(HighestDrawIndex(std::as_bytes(std::span(long32)), 4, true) == 999u && HighestDrawIndex(std::as_bytes(std::span(long32)), 4, false) == 0xffffffffu, "a 32-bit restart index at the end of a long range");
    std::vector<std::byte> shifted(2 * 1000 + 3);
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto value = static_cast<std::uint16_t>(i == 777 ? 0x8001u : i % 300u);
        std::memcpy(shifted.data() + 1 + 2 * i, &value, sizeof(value));
    }
    Require(HighestDrawIndex(std::span<const std::byte>(shifted).subspan(1, 2 * 1000 + 1), 2, false) == 0x8001u, "an unaligned 16-bit range with a trailing byte");
}

void storeAtFlipTests() {
    using AgcDriver::Graphics::StorageTexture;
    Require(!StorageTexture::StoreAtFlipRequested(nullptr), "an unset APS5_STORE_AT_FLIP stored at each flip");
    Require(StorageTexture::StoreAtFlipRequested("1"), "APS5_STORE_AT_FLIP=1 did not store at each flip");
    for (const char* value : {"0", "", "true", "11"}) expectFailure([&] { StorageTexture::StoreAtFlipRequested(value); }, "expected 1");
}

void storeAtLabelTests() {
    using AgcDriver::Graphics::StorageTexture;
    Require(!StorageTexture::StoreAtLabelRequested(nullptr), "an unset APS5_STORE_AT_LABEL stored at each label");
    Require(StorageTexture::StoreAtLabelRequested("1"), "APS5_STORE_AT_LABEL=1 did not store at each label");
    for (const char* value : {"0", "", "true", "11"}) expectFailure([&] { StorageTexture::StoreAtLabelRequested(value); }, "expected 1");
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

void vertexOverhangTests() {
    using AgcDriver::Graphics::CopyZeroPaddedDrawInput;
    constexpr std::size_t page = 65536;
    constexpr std::size_t stride = 44;
    constexpr std::size_t overhang = 8;
#ifdef _WIN32
    auto* block = static_cast<std::byte*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE, PAGE_NOACCESS));
    Require(block != nullptr && VirtualAlloc(block, page, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot reserve the guest pages of the overhang test");
#else
    void* mapped = mmap(nullptr, page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED && mprotect(mapped, page, PROT_READ | PROT_WRITE) == 0, "cannot reserve the guest pages of the overhang test");
    auto* block = static_cast<std::byte*>(mapped);
#endif
    constexpr std::size_t offset = 0x650;
    const auto readable = page - offset;
    std::fill(block + offset, block + page, std::byte{0x7a});
    const auto address = reinterpret_cast<std::uint64_t>(block) + offset;
    {
        const auto copy = CopyZeroPaddedDrawInput(mockContext(), address, readable + overhang, readable + overhang, stride);
        const auto bytes = copy.buffer->Bytes();
        Require(bytes.size() == readable + overhang, "an overhanging vertex fetch changed its size");
        Require(std::all_of(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(readable), [](std::byte value) { return value == std::byte{0x7a}; }), "an overhanging vertex fetch did not copy the readable bytes");
        Require(std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(readable), bytes.end(), [](std::byte value) { return value == std::byte{0}; }), "an overhanging vertex fetch did not zero its last record");
    }
    expectFailure([&] { CopyZeroPaddedDrawInput(mockContext(), address, readable + stride, readable + stride, stride); }, "is not readable");
    expectFailure([&] { CopyZeroPaddedDrawInput(mockContext(), address, readable + overhang, readable + overhang, 0); }, "is not readable");
#ifdef _WIN32
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, page * 2);
#endif
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

void ResourceContentKeyTests(bool benchmark) {
    using AgcDriver::Graphics::ShaderResources;
    using ShaderRecompiler::DescriptorKind;
    using ShaderRecompiler::DescriptorRole;
    for (const auto count : {0u, 1u, 31u, 32u, 33u, 16384u}) {
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding images{};
        images.kind = DescriptorKind::SampledImage;
        images.role = DescriptorRole::GuestImages;
        images.count = count;
        images.guestDescriptor.resize(static_cast<std::size_t>(count) * 8u);
        for (std::size_t i = 0; i < images.guestDescriptor.size(); ++i) images.guestDescriptor[i] = static_cast<std::uint32_t>(i * 1664525u + 1013904223u);
        for (auto* bits : {&images.imageWritten, &images.samplerDepthCompare, &images.imageDepthCompare, &images.imageAtomic, &images.samplerUnnormalized, &images.samplerIntegerBorder, &images.imageUnnormalized, &images.imageUnnormalizedBaseLevel, &images.bufferWritten}) {
            bits->resize(count);
            for (std::size_t i = 0; i < bits->size(); ++i) (*bits)[i] = i % 3u == 0;
        }
        images.imageSamplers.resize(count, 7u);
        images.imageShapes.resize(count, ShaderRecompiler::DescriptorImageShape::Image2D);
        program.bindings.push_back(std::move(images));
        ShaderRecompiler::DescriptorBinding data{};
        data.kind = DescriptorKind::StorageBuffer;
        data.role = DescriptorRole::ShaderData;
        data.count = 1;
        data.guestDescriptor.resize(257u, 0x12345678u);
        program.bindings.push_back(std::move(data));
        ShaderRecompiler::DescriptorBinding buffers{};
        buffers.kind = DescriptorKind::StorageBuffer;
        buffers.role = DescriptorRole::GuestBuffers;
        buffers.count = 1;
        buffers.guestDescriptor = {0x1000u, 0x10000u, 4u, 0x1234u};
        buffers.bufferWritten = {false};
        program.bindings.push_back(std::move(buffers));
        const AgcDriver::Graphics::CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
        for (const bool dataWords : {false, true}) for (const bool movable : {false, true}) {
            const auto original = ShaderResources::ContentKey(shader, dataWords, movable);
            program.bindings[1].guestDescriptor.back() ^= 1u;
            Require((ShaderResources::ContentKey(shader, dataWords, movable) != original) == (dataWords && !movable), "content key changed its data-word policy");
            program.bindings[1].guestDescriptor.back() ^= 1u;
            program.bindings[2].guestDescriptor[0] += 0x1000u;
            Require((ShaderResources::ContentKey(shader, dataWords, movable) != original) == !movable, "content key changed its movable-buffer policy");
            program.bindings[2].guestDescriptor[0] -= 0x1000u;
            program.bindings[2].bufferWritten[0] = true;
            Require(ShaderResources::ContentKey(shader, dataWords, movable) != original, "content key ignored the written-buffer mask");
            program.bindings[2].bufferWritten[0] = false;
            if (count != 0) {
                for (auto* bits : {&program.bindings[0].imageWritten, &program.bindings[0].samplerDepthCompare, &program.bindings[0].imageDepthCompare, &program.bindings[0].imageAtomic, &program.bindings[0].samplerUnnormalized, &program.bindings[0].samplerIntegerBorder, &program.bindings[0].imageUnnormalized, &program.bindings[0].imageUnnormalizedBaseLevel}) {
                    bits->back().flip();
                    Require(ShaderResources::ContentKey(shader, dataWords, movable) != original, "content key ignored the last packed flag");
                    bits->back().flip();
                }
                ++program.bindings[0].imageSamplers.back();
                Require(ShaderResources::ContentKey(shader, dataWords, movable) != original, "content key ignored the last sampler association");
                --program.bindings[0].imageSamplers.back();
                program.bindings[0].imageShapes.back() = ShaderRecompiler::DescriptorImageShape::Image3D;
                Require(ShaderResources::ContentKey(shader, dataWords, movable) != original, "content key ignored the last image shape");
                program.bindings[0].imageShapes.back() = ShaderRecompiler::DescriptorImageShape::Image2D;
            }
            Require(original.capacity() == original.size(), "content key reserved a different word count than it serialized");
            std::uint64_t hash = 14695981039346656037ull;
            for (const auto word : original) hash = (hash ^ word) * 1099511628211ull;
            std::cout << "content-key count=" << count << " data=" << dataWords << " movable=" << movable << " words=" << original.size() << " capacity=" << original.capacity() << " hash=" << hash;
            if (benchmark) {
                constexpr std::size_t repeats = 256;
                std::uint64_t checksum = 0;
                const auto begin = std::chrono::steady_clock::now();
                for (std::size_t i = 0; i < repeats; ++i) {
                    const auto key = ShaderResources::ContentKey(shader, dataWords, movable);
                    checksum += key.size() + key.back();
                }
                const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count();
                std::cout << " us/key=" << elapsed / repeats << " checksum=" << checksum;
            }
            std::cout << '\n';
        }
    }
}

// The pipeline journal without a GPU: the recipe codec and the store key agree with each other and
// with the canonical state, identities follow the module and the specialization, the journal
// records each pipeline once, and a replay into the recording context (the lavapipe test replays
// into a second device) rebuilds the recorded graphics pipeline so the same configuration hits the
// store, hands the compute result to its hook, recovers a tail torn inside a record's padding and
// stops when cancelled.
void pipelineStoreLazyBufferPoolTests() {
    using namespace AgcDriver::Graphics;
    mock = MockVulkan{};
    auto context = mockContext();
    context.device = makeHandle<VkDevice>();
    context.graphicsPipelineLibrary = true;
    context.pipelineLinkTimeOptimization = false;
    context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.spirv = makeModule({});
    fragment.spirv = makeModule({.fragment = true});
    vertex.variantId = 201;
    fragment.variantId = 202;
    const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}}};
    std::array<std::uint32_t, 1> mainKey{31}, fragmentKey{32};
    const PipelineSetLayouts resources{makeHandle<VkDescriptorSetLayout>(), makeHandle<VkDescriptorSetLayout>(), true, mainKey, fragmentKey};
    Require(context.bufferPool == nullptr, "the lazy buffer pool fixture already owns a buffer pool");
    auto first = CachedPipeline(context, State{}, VertexInputLayout{}, resources, shaders);
    const auto created = mock.pipelineCreateCount;
    static_cast<void>(GetBufferPool(context));
    auto again = CachedPipeline(context, State{}, VertexInputLayout{}, resources, shaders);
    Require(again == first && mock.pipelineCreateCount == created, "creating the buffer pool dropped a cached pipeline of the live device");
    ClearCachedPipelines(context.device);
    again.reset();
    first.reset();
    context.bufferPool.reset();
    Require(mock.live == 0, "lazy buffer pool pipeline tests leaked Vulkan objects");
}

void pipelineJournalTests(bool longPath = false, bool extended = false) {
    using namespace AgcDriver::Graphics;
    using Stage = ShaderRecompiler::ShaderStage;
    const auto root = std::filesystem::temp_directory_path() / ("aps5-pipeline-journal-mock-" + std::to_string(std::random_device{}()));
    auto directory = root;
    if (longPath) {
        Require(root.native().size() < 180, "the temporary root is too long for the journal path fixture");
        directory /= std::filesystem::path(u8"cache-\u00e4\u6f22");
        directory /= std::string(229 - directory.native().size(), 'd');
    }
    std::filesystem::create_directories(directory);
    struct Cleanup {
        std::filesystem::path directory;
        std::filesystem::path journal;
        ~Cleanup() {
            ShaderRecompiler::ShaderDiskCache::Shutdown();
            std::error_code error;
            if (!journal.empty()) {
                std::filesystem::remove(journal, error);
                auto lock = journal;
                lock += ".lock";
                std::filesystem::remove(lock, error);
            }
            std::filesystem::remove_all(directory, error);
        }
    } cleanup{root, {}};
#ifdef _WIN32
    if (extended) directory = std::filesystem::path(L"\\\\?\\" + directory.make_preferred().native());
    _putenv_s("ANYPS5_NO_SHADER_CACHE", "0");
    _wputenv_s(L"ANYPS5_SHADER_CACHE_DIR", directory.c_str());
    _putenv_s("APS5_PIPELINE_JOURNAL_THREADS", "2");
#else
    setenv("ANYPS5_NO_SHADER_CACHE", "0", 1);
    setenv("ANYPS5_SHADER_CACHE_DIR", directory.string().c_str(), 1);
    setenv("APS5_PIPELINE_JOURNAL_THREADS", "2", 1);
#endif
    Require(PipelineJournalEnabled(), "the journal is disabled with a shader cache directory set");
    auto journalFile = PipelineJournalPath();
#ifdef _WIN32
    std::error_code pathError;
    journalFile = ShaderRecompiler::NativeCachePath_nid_no_patch(journalFile, pathError);
    Require(!pathError, "cannot normalize the journal fixture path");
#endif
    cleanup.journal = journalFile;
    if (longPath) Require(journalFile.native().size() > 260, "the journal fixture did not cross MAX_PATH");
    const auto module = [](std::uint32_t salt) {
        ShaderRecompiler::RecompileResult result;
        result.spirv = std::vector<std::uint32_t>{0x07230203u, 0x00010000u, 0u, 8u, 0u, 0x00020011u, 1u, 0x00020011u, 2u + salt};
        result.runtimeAbiVersion = ShaderRecompiler::RuntimeAbi::Version;
        return result;
    };
    {
        auto first = module(1);
        const auto& constFirst = first;
        auto copy = first;
        const auto& constCopy = copy;
        Require(!constFirst.Identity().Empty() && constFirst.Identity() == constCopy.Identity() && constFirst.spirv.data() == constCopy.spirv.data(), "a copied result did not share its module and identity");
        copy.spirv[8] = 99;
        Require(!(constCopy.Identity() == constFirst.Identity()) && constCopy.spirv.data() != constFirst.spirv.data() && constFirst.Identity() == module(1).Identity(), "writing a copied module did not give it an identity of its own");
        auto specialized = module(1);
        specialized.specialization.push_back({3, 4});
        Require(!(specialized.Identity() == constFirst.Identity()), "specialization constants did not enter the identity");
        auto subgroup = module(1);
        subgroup.hostSubgroupSize = 64;
        Require(!(subgroup.Identity() == constFirst.Identity()), "the host subgroup size did not enter the identity");
        Require(ShaderRecompiler::RecompileResult{}.Identity().Empty(), "a result without a module has an identity");
    }
    mock = MockVulkan{};
    auto context = mockContext();
    context.device = makeHandle<VkDevice>();
    context.graphicsPipelineLibrary = true;
    context.tessellationShader = true;
    context.limits.maxTessellationPatchSize = 32;
    context.limits.maxColorAttachments = 8;
    context.limits.maxPushConstantsSize = 256;
    context.limits.maxVertexInputBindings = 16;
    context.limits.maxVertexInputAttributes = 16;
    context.limits.maxFramebufferWidth = 4096;
    context.limits.maxFramebufferHeight = 4096;
    context.limits.maxFramebufferLayers = 1;
    std::uint64_t replayedComputeIdentityLow = 0;
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        auto vertex = module(1);
        vertex.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, std::vector<std::uint32_t>(4)));
        auto fragment = module(2);
        fragment.pushConstants.resize(8);
        fragment.bindings.push_back(makeBinding(Role::ShaderData, 0, 1, std::vector<std::uint32_t>(4)));
        fragment.bindings.back().descriptorSet = ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet;
        const std::array<CompiledShader, 2> shaders{{{Stage::Vertex, &vertex, 0}, {Stage::Fragment, &fragment, PipelinePushSlotBytes}}};
        State state{};
        state.stages = {ShaderPath::Vertex, 7u, 64u, 32u, std::nullopt, std::nullopt};
        state.stages.tessellation = ShaderRecompiler::TessellationConfiguration{3, 3, 2, 1, 4, 128, 16, true};
        ColorTarget color{};
        color.address = 0x1000;
        color.extent = {64, 32};
        color.format = VK_FORMAT_R8G8B8A8_UNORM;
        color.bytes = 64 * 32 * 4;
        color.componentMapping = 0xec;
        state.colors = {color, color};
        state.colors[1].format = VK_FORMAT_B8G8R8A8_UNORM;
        state.colors[1].exportIndex = 1;
        state.color = color;
        state.hasColorTarget = true;
        VkPipelineColorBlendAttachmentState blend{};
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorWriteMask = 0xf;
        state.blends = {blend, blend};
        state.blend = blend;
        state.blendConstants = {0.25f, 0.5f, 0.75f, 1.0f};
        DepthTarget depth{};
        depth.address = 0x2000;
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = 1;
        state.depth = depth;
        state.depthTest = true;
        state.depthWrite = true;
        state.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
        state.stencilTest = true;
        state.stencilFront = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 1};
        state.stencilBack = state.stencilFront;
        state.renderExtent = {64, 32};
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        state.viewport = {0, 32, 64, -32, 0, 1};
        state.scissor = {{0, 0}, {64, 32}};
        state.cullMode = VK_CULL_MODE_BACK_BIT;
        state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        state.samples = 1;
        const VertexInputLayout input{{{0, 16, VK_VERTEX_INPUT_RATE_VERTEX}}, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0}}};
        const auto layouts = DescribeSetLayouts(context, shaders);
        Require(layouts.layouts.split, "a fragment stage with the fragment set did not split the set layouts");
        const auto recipe = MakeGraphicsPipelineRecipe(state, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(recipe.stages.size() == 2 && recipe.stages[0].identity == vertex.Identity() && recipe.stages[1].identity == fragment.Identity() && !recipe.stages[0].generated && recipe.stages[1].pushConstantOffset == PipelinePushSlotBytes && recipe.splitSets, "the recipe does not name its stages");
        Require(recipe.state.colors.size() == 2 && recipe.state.colors[0].address == 0 && recipe.state.colors[0].componentMapping == 0xec && recipe.state.depth && recipe.state.depth->address == 0 && recipe.state.viewport.width == 0 && recipe.state.stages.registerValue == 0 && recipe.state.stages.tessellation && recipe.state.stages.tessellation->breakAtInstance, "the canonical state kept or dropped the wrong fields");
        const auto bytes = EncodeGraphicsPipelineRecipe(recipe);
        GraphicsPipelineRecipe decoded;
        Require(DecodeGraphicsPipelineRecipe(bytes, decoded), "the recipe does not decode");
        Require(EncodeGraphicsPipelineRecipe(decoded) == bytes, "the recipe does not round-trip byte for byte");
        for (std::size_t length = 0; length < bytes.size(); length += 7) {
            GraphicsPipelineRecipe scratch;
            Require(!DecodeGraphicsPipelineRecipe(std::span(bytes).first(length), scratch), "a truncated recipe decoded");
        }
        std::vector<std::byte> original, canonical, replayed, moved, mapped;
        BuildPipelineStoreKey(original, context, state, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(canonical, context, recipe.state, recipe.vertexInput, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        const PipelineSetLayouts decodedLayouts{layouts.layouts.main, layouts.layouts.fragment, decoded.splitSets, decoded.mainLayoutKey, decoded.fragmentLayoutKey};
        BuildPipelineStoreKey(replayed, context, decoded.state, decoded.vertexInput, decodedLayouts, shaders, decoded.attachmentLayout);
        Require(!original.empty() && original == canonical && original == replayed, "the canonical or the decoded state keys another pipeline");
        auto fmaskState = state;
        fmaskState.samples = 8;
        fmaskState.color.samples = 8;
        fmaskState.color.fmaskAddress = 0x30000;
        fmaskState.color.fmaskPipeBankXor = 0x1200;
        for (auto& target : fmaskState.colors) {
            target.samples = 8;
            target.fmaskAddress = 0x40000;
            target.fmaskPipeBankXor = 0x2300;
        }
        const auto fmaskRecipe = MakeGraphicsPipelineRecipe(fmaskState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(fmaskRecipe.state.color.fmaskAddress == 0 && fmaskRecipe.state.color.fmaskPipeBankXor == 0, "the canonical state kept the primary target's FMASK location");
        for (const auto& canonicalColor : fmaskRecipe.state.colors) {
            Require(canonicalColor.fmaskAddress == 0 && canonicalColor.fmaskPipeBankXor == 0, "the canonical state kept an attachment's FMASK location");
        }
        const auto fmaskBytes = EncodeGraphicsPipelineRecipe(fmaskRecipe);
        GraphicsPipelineRecipe fmaskDecoded;
        Require(DecodeGraphicsPipelineRecipe(fmaskBytes, fmaskDecoded) && EncodeGraphicsPipelineRecipe(fmaskDecoded) == fmaskBytes, "the FMASK pipeline recipe did not round-trip byte for byte");
        std::vector<std::byte> fmaskKey, fmaskReplayedKey;
        BuildPipelineStoreKey(fmaskKey, context, fmaskState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(fmaskReplayedKey, context, fmaskDecoded.state, fmaskDecoded.vertexInput, layouts.View(), shaders, fmaskDecoded.attachmentLayout);
        Require(fmaskKey != original && fmaskKey == fmaskReplayedKey, "the decoded FMASK state keys another pipeline");
        auto relocatedFmask = fmaskState;
        relocatedFmask.color.fmaskAddress += 0x10000;
        relocatedFmask.color.fmaskPipeBankXor ^= 0x100;
        for (auto& target : relocatedFmask.colors) {
            target.fmaskAddress += 0x20000;
            target.fmaskPipeBankXor ^= 0x200;
        }
        std::vector<std::byte> relocatedFmaskKey;
        BuildPipelineStoreKey(relocatedFmaskKey, context, relocatedFmask, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(relocatedFmaskKey == fmaskKey, "the FMASK resource location changed the pipeline key");
        Require(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(relocatedFmask, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)) == fmaskBytes, "the FMASK resource location changed the journal recipe");
        auto gridState = state;
        gridState.samples = 2;
        gridState.customSampleLocations = true;
        gridState.sampleLocationGrid = true;
        gridState.perSampleShading = true;
        gridState.sampleLocations[31] = 0x5a;
        GraphicsPipelineRecipe gridDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(gridState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), gridDecoded), "the sample grid recipe does not decode");
        Require(gridDecoded.state.sampleLocationGrid && gridDecoded.state.perSampleShading && gridDecoded.state.sampleLocations[31] == 0x5a, "the recipe dropped the sample grid, per-sample shading or the last sample location");
        std::vector<std::byte> grid, gridReplayed;
        BuildPipelineStoreKey(grid, context, gridState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(gridReplayed, context, gridDecoded.state, gridDecoded.vertexInput, layouts.View(), shaders, gridDecoded.attachmentLayout);
        Require(grid != original && grid == gridReplayed, "the decoded sample grid state keys another pipeline");
        auto lineState = state;
        lineState.stages.mesh = ShaderRecompiler::MeshConfiguration{1, 2, 2, 4, 4, 32, 64, 1, 4, 2};
        GraphicsPipelineRecipe lineDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(lineState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), lineDecoded), "the line strip mesh recipe does not decode");
        Require(lineDecoded.state.stages.mesh && lineDecoded.state.stages.mesh->outputPrimitiveSize == 2u && lineDecoded.state.stages.mesh->esgsItemSize == 4u, "the recipe dropped the mesh output primitive size");
        auto pointState = state;
        pointState.vertexPoints = true;
        pointState.pointSize = 4.0f;
        pointState.pointSizeMin = 0.5f;
        pointState.pointSizeMax = 8.0f;
        GraphicsPipelineRecipe pointDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(pointState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), pointDecoded), "the point list recipe does not decode");
        Require(pointDecoded.state.vertexPoints && pointDecoded.state.pointSize == 4.0f && pointDecoded.state.pointSizeMin == 0.5f && pointDecoded.state.pointSizeMax == 8.0f, "the recipe dropped the vertex point list or its point sizes");
        auto meshPointState = pointState;
        meshPointState.vertexPoints = false;
        meshPointState.stages.mesh = *lineState.stages.mesh;
        meshPointState.stages.mesh->outputPrimitiveSize = 1u;
        meshPointState.stages.mesh->provokingVertex = 0u;
        GraphicsPipelineRecipe meshPointDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(meshPointState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), meshPointDecoded), "the mesh point recipe does not decode");
        Require(!meshPointDecoded.state.vertexPoints && meshPointDecoded.state.stages.mesh && meshPointDecoded.state.stages.mesh->outputPrimitiveSize == 1u && meshPointDecoded.state.pointSize == 4.0f && meshPointDecoded.state.pointSizeMin == 0.5f && meshPointDecoded.state.pointSizeMax == 8.0f, "the recipe dropped mesh points or their sizes without the vertex point flag");
        auto fourSampleState = state;
        fourSampleState.samples = 4;
        auto maskState = fourSampleState;
        maskState.quadSampleMask = {0x3u, 0x3u, 0x3u, 0x3u};
        maskState.stencilReplayReference = 7u;
        maskState.reversedBlend = 1u;
        GraphicsPipelineRecipe maskDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(maskState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), maskDecoded), "the sample mask recipe does not decode");
        Require(maskDecoded.state.quadSampleMask == maskState.quadSampleMask && maskDecoded.state.stencilReplayReference.has_value() && maskDecoded.state.reversedBlend == 1u, "the recipe dropped the quad sample masks, the replayed stencil reference or the reversed blend");
        std::vector<std::byte> fourSamples, masked, maskReplayed;
        BuildPipelineStoreKey(fourSamples, context, fourSampleState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(masked, context, maskState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(maskReplayed, context, maskDecoded.state, maskDecoded.vertexInput, layouts.View(), shaders, maskDecoded.attachmentLayout);
        Require(masked != fourSamples && masked == maskReplayed, "the decoded sample mask state keys another pipeline");
        auto mergedState = fourSampleState;
        mergedState.mergedCoverage = true;
        GraphicsPipelineRecipe mergedDecoded;
        Require(DecodeGraphicsPipelineRecipe(EncodeGraphicsPipelineRecipe(MakeGraphicsPipelineRecipe(mergedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL)), mergedDecoded), "the merged coverage recipe does not decode");
        Require(mergedDecoded.state.mergedCoverage, "the recipe dropped merged coverage");
        std::vector<std::byte> mergedKey, mergedReplayed;
        BuildPipelineStoreKey(mergedKey, context, mergedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        BuildPipelineStoreKey(mergedReplayed, context, mergedDecoded.state, mergedDecoded.vertexInput, layouts.View(), shaders, mergedDecoded.attachmentLayout);
        Require(mergedKey != fourSamples && mergedKey == mergedReplayed, "the decoded merged coverage state keys another pipeline");
        auto movedState = state;
        movedState.viewport.x = 5;
        movedState.colors[0].address = 0x9000;
        movedState.renderLayers = 4;
        BuildPipelineStoreKey(moved, context, movedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(moved == original, "a field the pipeline does not read changed the key");
        auto mappedState = state;
        mappedState.colors[0].componentMapping = 0x24;
        BuildPipelineStoreKey(mapped, context, mappedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(mapped != original, "the component mapping did not enter the key");
        std::vector<std::byte> unclamped, clamped, biased;
        auto clampedState = state;
        clampedState.depthBiasClamp = 1.0f;
        BuildPipelineStoreKey(clamped, context, clampedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(clamped == original, "a depth bias clamp without depth bias changed the key");
        clampedState.depthBias = true;
        BuildPipelineStoreKey(clamped, context, clampedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        clampedState.depthBiasClamp = 0.0f;
        BuildPipelineStoreKey(unclamped, context, clampedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(unclamped != original && clamped != unclamped && CanonicalPipelineState(clampedState).depthBias && CanonicalPipelineState(mappedState).depthBiasClamp == 0.0f, "the depth bias clamp did not enter the key");
        clampedState.depthBiasClamp = 2.0f;
        BuildPipelineStoreKey(biased, context, clampedState, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL);
        Require(biased == clamped, "the depth bias clamp's value entered the key");
        auto compute = module(3);
        compute.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 2, std::vector<std::uint32_t>(8)));
        compute.pushConstants.resize(16);
        compute.hostSubgroupSize = 32;
        constexpr std::uint64_t fingerprint = 0x5eed'0000'0000'0001ull;
        {
            auto bound = StartPipelineJournalReplay(context, fingerprint, {});
            Require(bound != nullptr, "the first replay did not start");
            bound->Wait();
            Require(bound->Done() && PipelineJournalTotals().loadedRecipes == 0, "an absent journal loaded pipelines");
        }
        const auto before = PipelineJournalTotals();
        RecordGraphicsPipeline(recipe, shaders);
        RecordComputePipeline(compute, true);
        FlushPipelineJournal();
        const auto recorded = PipelineJournalTotals();
        Require(recorded.recordedGraphics == before.recordedGraphics + 1 && recorded.recordedCompute == before.recordedCompute + 1 && recorded.recordedModules == before.recordedModules + 3 && recorded.writeFailures == 0, "the journal did not record one graphics and one compute pipeline with three modules");
        RecordGraphicsPipeline(recipe, shaders);
        RecordComputePipeline(compute, true);
        FlushPipelineJournal();
        const auto repeated = PipelineJournalTotals();
        Require(repeated.recordedGraphics == recorded.recordedGraphics && repeated.recordedCompute == recorded.recordedCompute && repeated.recordedModules == recorded.recordedModules, "a repeated pipeline was recorded again");
        Require(std::filesystem::file_size(journalFile) > 0, "the journal file is empty");
        const auto storeBefore = PipelineStoreTotals();
        std::vector<std::byte> receivedWords;
        ShaderRecompiler::RecompileResult received;
        bool receivedPush = false;
        std::uint32_t computeCalls = 0;
        std::mutex computeMutex;
        PipelineJournalHooks hooks{[&](const ShaderRecompiler::RecompileResult& shader, bool pushConstants) {
            std::lock_guard lock(computeMutex);
            ++computeCalls;
            received = shader;
            receivedPush = pushConstants;
            return true;
        }};
        auto replay = StartPipelineJournalReplay(context, fingerprint, hooks);
        Require(replay != nullptr, "the replay did not start");
        replay->Wait();
        Require(replay->Done(), "the replay did not finish");
        const auto totals = PipelineJournalTotals();
        Require(totals.loadedRecipes == 2 && totals.loadedModules == 3 && totals.droppedRecords == 0, "the replay did not load the two recorded pipelines and three modules");
        Require(totals.replayedGraphics == 1 && totals.replayedCompute == 1 && totals.replayRejected == 0 && totals.replayFailed == 0, "the replay did not build one graphics and one compute pipeline");
        Require(computeCalls == 1 && receivedPush && received.Identity() == compute.Identity() && received.spirv == compute.spirv && received.bindings.size() == 1 && received.bindings[0].count == 2 && received.pushConstants.size() == 16 && received.hostSubgroupSize == 32, "the compute hook did not receive the recorded result");
        replayedComputeIdentityLow = received.Identity().low;
        const auto storeReplayed = PipelineStoreTotals();
        Require(storeReplayed.misses == storeBefore.misses + 1 && storeReplayed.cached == storeBefore.cached + 1, "the replayed graphics pipeline did not enter the store");
        const auto created = mock.pipelineCreateCount;
        bool built = true;
        const auto pipeline = CachedPipeline(context, state, input, layouts.View(), shaders, VK_IMAGE_LAYOUT_GENERAL, &built);
        const auto storeHit = PipelineStoreTotals();
        Require(pipeline != nullptr && !built && mock.pipelineCreateCount == created && storeHit.hits == storeReplayed.hits + 1 && storeHit.misses == storeReplayed.misses, "the draw's configuration did not find the replayed pipeline");
        {
            const auto& journalPath = journalFile;
            const auto whole = std::filesystem::file_size(journalPath);
            Require(whole % 8 == 0 && whole > 3, "the journal file is not record aligned");
            std::filesystem::resize_file(journalPath, whole - 3);
            auto later = module(4);
            later.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, std::vector<std::uint32_t>(4)));
            RecordComputePipeline(later, false);
            FlushPipelineJournal();
            Require(std::filesystem::file_size(journalPath) % 8 == 0 && PipelineJournalTotals().writeFailures == 0, "an append after a tail torn inside a record's padding left the file unaligned");
            auto torn = StartPipelineJournalReplay(context, fingerprint, hooks);
            Require(torn != nullptr, "the replay after the torn tail did not start");
            torn->Wait();
            const auto reloaded = PipelineJournalTotals();
            Require(reloaded.droppedRecords == 0 && reloaded.loadedModules == 4 && reloaded.loadedRecipes == 2 && reloaded.replayFailed == 0, "the journal did not recover from a tail torn inside a record's padding");
            const auto writtenBefore = PipelineJournalTotals().writtenBytes;
            RecordComputePipeline(compute, true);
            FlushPipelineJournal();
            Require(PipelineJournalTotals().recordedCompute == reloaded.recordedCompute + 1 && PipelineJournalTotals().recordedModules == reloaded.recordedModules, "the pipeline whose record was torn was not recorded again, or its module was");
            const auto recordBytes = PipelineJournalTotals().writtenBytes - writtenBefore;
            std::vector<std::byte> bytes;
            Require(ShaderRecompiler::ReadWholeFile(journalFile, bytes) && recordBytes > 0 && recordBytes <= bytes.size(), "cannot read the re-recorded journal entry");
            {
                std::ofstream file(journalFile, std::ios::binary | std::ios::app);
                file.write(reinterpret_cast<const char*>(bytes.data() + bytes.size() - recordBytes), static_cast<std::streamsize>(recordBytes));
                const std::array<char, 8> tornTail{};
                file.write(tornTail.data(), tornTail.size());
                file.close();
                Require(file.good(), "cannot append an external complete record and torn tail");
            }
            auto another = module(5);
            another.bindings.push_back(makeBinding(Role::GuestBuffers, 0, 1, std::vector<std::uint32_t>(4)));
            const auto beforeAppend = PipelineJournalTotals();
            RecordComputePipeline(another, false);
            FlushPipelineJournal();
            const auto appended = PipelineJournalTotals();
            Require(appended.writeFailures == 0 && appended.recordedCompute == beforeAppend.recordedCompute + 1, "the append after an external torn tail failed");
            Require(std::filesystem::file_size(journalFile) == bytes.size() + recordBytes + appended.writtenBytes - beforeAppend.writtenBytes, "tail recovery discarded an external complete record or retained torn bytes");
            auto recovered = StartPipelineJournalReplay(context, fingerprint, hooks);
            Require(recovered != nullptr, "the replay after the external append did not start");
            recovered->Wait();
            Require(PipelineJournalTotals().loadedModules == 5 && PipelineJournalTotals().loadedRecipes == 4 && PipelineJournalTotals().droppedRecords == 0, "the external append and torn-tail recovery did not replay cleanly");
        }
        {
            const auto before = PipelineJournalTotals();
            auto cancelled = StartPipelineJournalReplay(context, fingerprint, hooks);
            Require(cancelled != nullptr, "the third replay did not start");
            cancelled->Cancel();
            cancelled->Wait();
            const auto after = PipelineJournalTotals();
            Require(cancelled->Done() && after.replayFailed == 0 && after.replayRejected == 0 && after.replayedGraphics + after.replayPresent <= before.replayedGraphics + before.replayPresent + 1, "a cancelled replay did not stop cleanly");
        }
        {
            const std::uint32_t meshWithoutOutputSize = 2u;
            std::fstream file(journalFile, std::ios::in | std::ios::out | std::ios::binary);
            file.seekp(4);
            file.write(reinterpret_cast<const char*>(&meshWithoutOutputSize), sizeof(meshWithoutOutputSize));
            file.close();
            Require(file.good(), "the journal header could not be rewritten");
            auto stale = StartPipelineJournalReplay(context, fingerprint, hooks);
            Require(stale != nullptr, "the replay of a journal in the previous format did not start");
            stale->Wait();
            const auto discarded = PipelineJournalTotals();
            Require(discarded.loadedRecipes == 0 && discarded.loadedModules == 0, "a journal in format 2, whose mesh records lack the output primitive size, was loaded");
        }
        ClearCachedPipelines(context.device);
    }
    Require(replayedComputeIdentityLow != 0, "the replayed compute identity is empty");
    Require(mock.live == 0, "the pipeline journal test leaked Vulkan objects");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _putenv_s("APS5_PIN_WAIT_MS", "200");
    _putenv_s("APS5_HEAP_MIRROR_MIB", "4");
#else
    setenv("APS5_PIN_WAIT_MS", "200", 1);
    setenv("APS5_HEAP_MIRROR_MIB", "4", 1);
#endif
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--descriptor-cache") {
            descriptorCacheTests();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--unused-viewport-depth") {
            UnusedViewportDepthTests();
            DepthClipTests();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--unused-stencil-reference") {
            stencilReferenceUnusedTests();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--command-batch-fence-failure") {
            commandBatchFenceFailureTests();
            return 0;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--pipeline-journal" || std::string_view(argv[1]) == "--pipeline-journal-long-paths" || std::string_view(argv[1]) == "--pipeline-journal-extended-paths")) {
            pipelineJournalTests(std::string_view(argv[1]) != "--pipeline-journal", std::string_view(argv[1]) == "--pipeline-journal-extended-paths");
            std::cout << "pipeline journal recipe, record and replay tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pipeline-library-clear-waits-for-build") {
            pipelineLibraryClearWaitsForBuildTests();
            std::cout << "pipeline library clear waits for active part builds\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pipeline-store-lazy-buffer-pool") {
            pipelineStoreLazyBufferPoolTests();
            std::cout << "cached pipelines survive lazy buffer pool creation\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--copy-input-kept-bytes") {
            copyInputKeptBytesTests();
            std::cout << "GPU-copy input snapshot accounting and retention passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--depth-copy-kept-bytes") {
            depthCopyKeptBytesTests();
            std::cout << "Depth-copy linear buffer accounting and retention passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sparse-buffer-holes") {
            bool passed = false;
            {
                auto context = mockContext();
                context.limits.maxStorageBufferRange = 1u << 27;
                passed = RunSparseBufferHoleTests(context, {
                    [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); }, {}, {}, {}, {}
                });
            }
            Require(mock.live == 0, "sparse buffer hole tests leaked Vulkan objects");
            std::cout << "sparse buffer hole tests " << (passed ? "passed" : "failed") << '\n';
            return passed ? 0 : 1;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--heap-mirror-unwatch" || std::string_view(argv[1]) == "--heap-mirror-unwatched-import")) {
            {
                mock = MockVulkan{};
                auto context = mockContext();
                context.bufferDeviceAddress = true;
                context.limits.maxStorageBufferRange = 1u << 27;
                RunHeapMirrorUnwatchTests(context, {
                    [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); },
                    {},
                    [](VkDeviceAddress address) {
                        const auto offset = address - 0x100000000000ULL;
                        const auto buffer = reinterpret_cast<VkBuffer>(offset / 0x10000);
                        return mockBufferMemory(buffer).subspan(offset % 0x10000);
                    },
                    {},
                    {}
                }, std::string_view(argv[1]) == "--heap-mirror-unwatched-import");
            }
            Require(mock.live == 0, "the heap mirror unwatch test leaked Vulkan objects");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sparse-heap-mirror-unwatch") {
            {
                mock = MockVulkan{};
                auto context = mockContext();
                context.bufferDeviceAddress = true;
                context.limits.maxStorageBufferRange = 1u << 27;
                RunSparseHeapMirrorUnwatchTests(context, {
                    [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); },
                    {},
                    [](VkDeviceAddress address) {
                        const auto offset = address - 0x100000000000ULL;
                        const auto buffer = reinterpret_cast<VkBuffer>(offset / 0x10000);
                        return mockBufferMemory(buffer).subspan(offset % 0x10000);
                    },
                    {},
                    {}
                });
            }
            Require(mock.live == 0, "the sparse heap mirror unwatch test leaked Vulkan objects");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sparse-heap-mirror-budget") {
            {
                mock = MockVulkan{};
                auto context = mockContext();
                context.bufferDeviceAddress = true;
                context.limits.maxStorageBufferRange = 1u << 27;
                RunSparseHeapMirrorBudgetTests(context, {
                    [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); },
                    {},
                    {},
                    {},
                    {}
                });
            }
            Require(mock.live == 0, "the sparse heap mirror budget test leaked Vulkan objects");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--heap-mirror-run-unwatch") {
            {
                mock = MockVulkan{};
                auto context = mockContext();
                context.bufferDeviceAddress = true;
                context.limits.maxStorageBufferRange = 1u << 27;
                RunHeapMirrorRunUnwatchTests(context, {
                    [](VkBuffer buffer) -> std::span<std::byte> { return mockBufferMemory(buffer); },
                    {},
                    [](VkDeviceAddress address) {
                        const auto offset = address - 0x100000000000ULL;
                        const auto buffer = reinterpret_cast<VkBuffer>(offset / 0x10000);
                        return mockBufferMemory(buffer).subspan(offset % 0x10000);
                    },
                    {},
                    {}
                });
            }
            Require(mock.live == 0, "the heap mirror run unwatch test leaked Vulkan objects");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--address-space-teardown") {
            addressSpaceTeardownTests();
            std::cout << "address-space device teardown releases cached guest allocations\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--buffer-budget-placement") {
            bufferBudgetPlacementTests();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sample-location-grid-fallback") {
            sampleLocationGridFallbackTests();
            std::cout << "Sample location grid fallback checks passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sample-order") {
            sampleOrderTests();
            std::cout << "Sample order checks passed\n";
            return 0;
        }
        {
            AgcDriver::Graphics::PipelineCacheSynchronization synchronization;
            auto pipelineUse = synchronization.LockForPipelineUse();
            bool snapshotAcquired = false;
            std::thread snapshot([&] { snapshotAcquired = synchronization.TryLockForSnapshot().owns_lock(); });
            snapshot.join();
            Require(!snapshotAcquired, "pipeline cache snapshot overlapped pipeline creation");
            pipelineUse.unlock();
            Require(synchronization.TryLockForSnapshot().owns_lock(), "pipeline cache snapshot stayed blocked after pipeline creation finished");
        }
        if (argc == 2 && std::string_view(argv[1]) == "--undithered-alpha-to-coverage-state") {
            UnditheredAlphaToCoverageTests();
            std::cout << "Existing undithered alpha-to-coverage states passed\n";
            return 0;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--resource-content-key" || std::string_view(argv[1]) == "--benchmark-resource-content-key")) {
            ResourceContentKeyTests(std::string_view(argv[1]) == "--benchmark-resource-content-key");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--descriptor-layout-flags") {
            descriptorLayoutFlagTests();
            std::cout << "Descriptor layout native and captured binding flags passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--alpha-to-coverage-state") {
            MsaaEnableSingleSampleTests();
            AlphaToCoverageTests();
            std::cout << "Alpha-to-coverage state and refusal tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--image-table-allocation") {
            imageTableAllocationTests();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--image-table-snapshot") {
            imageTableSnapshotAllocationTests();
            return 0;
        }
        const std::array<std::string_view, 3> samplerCases{"--sampler-table-lod-bias", "--sampler-table-anisotropy", "--sampler-table-reduction-compare"};
        for (std::uint32_t index = 0; index < samplerCases.size(); ++index) {
            if (argc == 2 && std::string_view(argv[1]) == samplerCases[index]) {
                samplerTableDeviceLimitTests(index);
                return 0;
            }
        }
        if (argc == 2 && std::string_view(argv[1]) == "--descriptor-pool-reset-failure") {
            descriptorPoolResetFailureTest();
            std::cerr << "descriptor pool reset failure did not terminate\n";
            return 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--unobserved-provoking-vertex") {
            UnobservedProvokingVertexTests();
            ProvokingVertexTests();
            std::cout << "Unobserved provoking vertex checks passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--covered-centroid-state") {
            multisampleTests();
            std::cout << "Covered-centroid and multisample state checks passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--multisample-dcc-state") {
            multisampleDccStateTests();
            std::cout << "Multisampled DCC state checks passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--depth-bounds-state") {
            DepthBoundsBiasTests();
            std::cout << "Depth bounds state checks passed\n";
            return 0;
        }
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
        ResourceContentKeyTests(false);
        stateTests();
        pipelineStoreLazyBufferPoolTests();
        retainedPixelStageTests();
        pipelineLibrarySetTests();
        pipelineLibraryClearWaitsForBuildTests();
        framebufferOwnerTests();
        hdrComponentSwapTests();
        depthMaintenanceTests();
        hardwareScreenOffsetTests();
        srgb8TargetTests();
        DepthClipTests();
        DepthRemapTests();
        UnusedViewportDepthTests();
        DepthStencilTests();
        OneDimensionalColorTests();
        htileDepthClearTests();
        multisampledSingleClearTests();
        memoryValueFillTests();
        ZExportTests();
        unorm10_11_11TargetTests();
        float11_11_10TargetTests();
        DepthBoundsBiasTests();
        conservativeZExportTests();
        earlyFragmentTestsTests();
        orderedPixelShaderTests();
        stencilRefExportTests();
        stencilReferenceUnusedTests();
        aliasedBufferViewTests();
        LineStripTests();
        ShaderModuleKeyTests();
        ProvokingVertexTests();
        ProvokingVertexRenderPassTests();
        ConservativeRasterizationTests();
        DisabledColorTests();
        OpaqueDestinationAlphaTests();
        Color565TargetTests();
        Color1555TargetTests();
        BlendBypassTests();
        UnclampedNormalizedColorTests();
        CompactedExportTests();
        ReversedComponentOrderTests();
        ReversedComponentOrderBlendTests();
        ReversedBlendRoutingTests();
        ShadersWriteMemoryTests();
        DualSourceBlendTests();
        ReversedBlendShaderTests();
        metadataPassTests();
        depthCopyPassTests();
        cmaskTests();
        uint16ExportTests();
        uint8x2TargetTests();
        uint8x4TargetTests();
        msaaEnableClearDepthTests();
        multisampleTests();
        MsaaEnableSingleSampleTests();
        multisampledDepthTests();
        clipDistanceTests();
        framebufferLayerTests();
        msaaEnableClearOneSampleTargetTests();
        UnwrittenSampleLocationTests();
        sampleLocationGridFallbackTests();
        sampleOrderTests();
        UnwrittenDepthPlaneTests();
        depthCopyFormatTests();
        quadPixelMaskTests();
        ShaderStageTests();
        UnsupportedTessellationFieldTests();
        NggVertexWorkgroupTests();
        inactiveVertexPointSizeTests();
        PolygonPrimitiveTests();
        vertexPointListTests();
        TuningFieldTests();
        PixelInputLayoutTests();
        UnexportedPixelInputTests();
        ColorPipeBankXorTests();
        HeaderlessVertexStageTests();
        HiddenUserWordVertexTablesTests();
        ComputeScratchTests();
        shaderUserDataTailPaddingTests();
        InitialContextTests();
        AlphaToCoverageTests();
        descriptorPoolTests();
        pushConstantTests();
        resourceTests();
        oversizedDescriptorTests();
        guestBufferViewBytesTests();
        descriptorSnapshotTests();
        dispatchMoveTests();
        descriptorCacheTests();
        bufferBudgetPlacementTests();
        imageTableAllocationTests();
        imageTableSnapshotAllocationTests();
        samplerTableNonSeamlessCubeTests();
        samplerTableDeviceLimitTests();
        tableUpdateAfterBindTests();
        misalignedShaderDataTests();
        debugBranchTests();
        hostImportBudgetTests();
        textureCacheBudgetTests();
        sampledTextureBudgetTests();
        storageTextureBudgetTests();
        sampledBudgetReportTests();
        meshArgumentTests();
        meshIndexBufferTests();
        stagingPoolBudgetTests();
        validationTests();
        vertexCopyTests();
        highestDrawIndexTests();
        storeAtFlipTests();
        storeAtLabelTests();
        nullVertexDescriptorTests();
        vertexZeroFillTests();
        vertexOverhangTests();
        vertexZeroFillMergeTests();
        parallelCompareTests();
        pixelParameterSlotTests();
        rectListTests();
        quadListTests();
        floatControlsModeTests();
        barycentricEmulationTests();
        copiedWriterIndirectOverlapTests();
        registeredReadableCoverTests();
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
            [] { return mock.allocationAttempts; },
            [](std::optional<VkDeviceSize> limit) { mock.importLimit = limit; }
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
        RunLastRecordIndicesTests();
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
            [] { return mock.pipelineSpecializations.back(); },
            [] { return mock.descriptorPools.size(); },
            [] {
                std::uint32_t resets = 0;
                for (const auto& [handle, pool] : mock.descriptorPools) resets += pool.resets;
                return resets;
            }
        });
        std::cout << "Graphics validation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
