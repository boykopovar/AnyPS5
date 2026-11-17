#pragma once

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace CommandStateLog {

using namespace AgcDriver::Graphics;

inline std::vector<std::string> stateLog;
inline std::vector<VkPipeline> statePipelines;

inline std::uint64_t statePipelineId(VkPipeline pipeline) {
    const auto found = std::find(statePipelines.begin(), statePipelines.end(), pipeline);
    if (found != statePipelines.end()) return static_cast<std::uint64_t>(found - statePipelines.begin());
    statePipelines.push_back(pipeline);
    return statePipelines.size() - 1u;
}

inline std::uint64_t stateWord(float value) { return std::bit_cast<std::uint32_t>(value); }
inline std::uint64_t stateWord(std::uint32_t value) { return value; }
inline std::uint64_t stateWord(std::int32_t value) { return static_cast<std::uint32_t>(value); }
inline std::uint64_t stateWord(VkPipeline value) { return statePipelineId(value); }

template<typename... TArguments>
std::string stateLine(const char* name, TArguments... arguments) {
    std::string line = name;
    ((line += ' ' + std::to_string(stateWord(arguments))), ...);
    return line;
}

template<typename... TArguments>
void stateNote(const char* name, TArguments... arguments) {
    stateLog.push_back(stateLine(name, arguments...));
}

inline VKAPI_ATTR void VKAPI_CALL stateBind(VkCommandBuffer, VkPipelineBindPoint point, VkPipeline pipeline) { stateNote("bind", static_cast<std::uint32_t>(point), pipeline); }
inline VKAPI_ATTR void VKAPI_CALL stateViewport(VkCommandBuffer, std::uint32_t, std::uint32_t, const VkViewport* viewport) { stateNote("viewport", viewport->x, viewport->y, viewport->width, viewport->height, viewport->minDepth, viewport->maxDepth); }
inline VKAPI_ATTR void VKAPI_CALL stateScissor(VkCommandBuffer, std::uint32_t, std::uint32_t, const VkRect2D* scissor) { stateNote("scissor", scissor->offset.x, scissor->offset.y, scissor->extent.width, scissor->extent.height); }
inline VKAPI_ATTR void VKAPI_CALL stateDepthBounds(VkCommandBuffer, float minimum, float maximum) { stateNote("bounds", minimum, maximum); }
inline VKAPI_ATTR void VKAPI_CALL stateDepthBias(VkCommandBuffer, float constant, float clamp, float slope) { stateNote("bias", constant, clamp, slope); }
inline VKAPI_ATTR void VKAPI_CALL stateCullMode(VkCommandBuffer, VkCullModeFlags mode) { stateNote("cull", static_cast<std::uint32_t>(mode)); }
inline VKAPI_ATTR void VKAPI_CALL stateFrontFace(VkCommandBuffer, VkFrontFace face) { stateNote("front", static_cast<std::uint32_t>(face)); }
inline VKAPI_ATTR void VKAPI_CALL stateDepthTest(VkCommandBuffer, VkBool32 enable) { stateNote("depthtest", static_cast<std::uint32_t>(enable)); }
inline VKAPI_ATTR void VKAPI_CALL stateDepthWrite(VkCommandBuffer, VkBool32 enable) { stateNote("depthwrite", static_cast<std::uint32_t>(enable)); }
inline VKAPI_ATTR void VKAPI_CALL stateDepthCompare(VkCommandBuffer, VkCompareOp compare) { stateNote("depthcompare", static_cast<std::uint32_t>(compare)); }
inline VKAPI_ATTR void VKAPI_CALL stateBoundsTest(VkCommandBuffer, VkBool32 enable) { stateNote("boundstest", static_cast<std::uint32_t>(enable)); }
inline VKAPI_ATTR void VKAPI_CALL stateStencilTest(VkCommandBuffer, VkBool32 enable) { stateNote("stenciltest", static_cast<std::uint32_t>(enable)); }
inline VKAPI_ATTR void VKAPI_CALL stateStencilOp(VkCommandBuffer, VkStencilFaceFlags faces, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) { stateNote("stencilop", static_cast<std::uint32_t>(faces), static_cast<std::uint32_t>(failOp), static_cast<std::uint32_t>(passOp), static_cast<std::uint32_t>(depthFailOp), static_cast<std::uint32_t>(compareOp)); }
inline VKAPI_ATTR void VKAPI_CALL stateCompareMask(VkCommandBuffer, VkStencilFaceFlags faces, std::uint32_t mask) { stateNote("comparemask", static_cast<std::uint32_t>(faces), mask); }
inline VKAPI_ATTR void VKAPI_CALL stateWriteMask(VkCommandBuffer, VkStencilFaceFlags faces, std::uint32_t mask) { stateNote("writemask", static_cast<std::uint32_t>(faces), mask); }
inline VKAPI_ATTR void VKAPI_CALL stateReference(VkCommandBuffer, VkStencilFaceFlags faces, std::uint32_t reference) { stateNote("reference", static_cast<std::uint32_t>(faces), reference); }

inline VKAPI_ATTR void VKAPI_CALL stateColorWriteMask(VkCommandBuffer, std::uint32_t first, std::uint32_t count, const VkColorComponentFlags* masks) {
    auto line = stateLine("colormask", first, count);
    for (std::uint32_t index = 0; index < count; ++index) line += ' ' + std::to_string(masks[index]);
    stateLog.push_back(line);
}

inline void expectStateLog(const std::vector<std::string>& expected, const std::string& what) {
    if (stateLog != expected) {
        std::string actual;
        for (const auto& line : stateLog) actual += "[" + line + "]";
        std::string wanted;
        for (const auto& line : expected) wanted += "[" + line + "]";
        stateLog.clear();
        throw std::runtime_error(what + ": recorded " + actual + " but expected " + wanted);
    }
    stateLog.clear();
}

inline VkPipeline stateFakePipeline(std::size_t index) {
    while (statePipelines.size() <= index) statePipelines.push_back(reinterpret_cast<VkPipeline>(static_cast<std::uintptr_t>(0x5a000 + 16 * (statePipelines.size() + 1))));
    return statePipelines[index];
}

inline DeviceFunctions stateFunctions() {
    DeviceFunctions functions;
    functions.cmdBindPipeline = stateBind;
    functions.cmdSetViewport = stateViewport;
    functions.cmdSetScissor = stateScissor;
    functions.cmdSetDepthBounds = stateDepthBounds;
    functions.cmdSetDepthBias = stateDepthBias;
    functions.cmdSetCullMode = stateCullMode;
    functions.cmdSetFrontFace = stateFrontFace;
    functions.cmdSetDepthTestEnable = stateDepthTest;
    functions.cmdSetDepthWriteEnable = stateDepthWrite;
    functions.cmdSetDepthCompareOp = stateDepthCompare;
    functions.cmdSetDepthBoundsTestEnable = stateBoundsTest;
    functions.cmdSetStencilTestEnable = stateStencilTest;
    functions.cmdSetStencilOp = stateStencilOp;
    functions.cmdSetStencilCompareMask = stateCompareMask;
    functions.cmdSetStencilWriteMask = stateWriteMask;
    functions.cmdSetStencilReference = stateReference;
    functions.cmdSetColorWriteMask = stateColorWriteMask;
    return functions;
}

inline void stateTrackedDraw(CommandState& recording, const Context& context, VkPipeline pipeline) {
    recording.BindPipeline(context, pipeline, CommandState::DynamicSlots(PipelineLibraryDynamicStates()));
    recording.SetViewport(context, VkViewport{0.0f, 0.0f, 64.0f, 32.0f, 0.0f, 1.0f});
    recording.SetScissor(context, VkRect2D{{1, 2}, {30, 20}});
    recording.SetDepthBias(context, 2.0f, 0.0f, 1.0f);
    recording.SetDepthBounds(context, 0.25f, 0.75f);
    recording.SetCullMode(context, VK_CULL_MODE_BACK_BIT);
    recording.SetFrontFace(context, VK_FRONT_FACE_CLOCKWISE);
    recording.SetDepthTestEnable(context, VK_TRUE);
    recording.SetDepthWriteEnable(context, VK_TRUE);
    recording.SetDepthCompareOp(context, VK_COMPARE_OP_LESS);
    recording.SetDepthBoundsTestEnable(context, VK_TRUE);
    recording.SetStencilTestEnable(context, VK_TRUE);
    for (const VkStencilFaceFlags face : {VK_STENCIL_FACE_FRONT_BIT, VK_STENCIL_FACE_BACK_BIT}) {
        recording.SetStencilOp(context, face, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
        recording.SetStencilCompareMask(context, face, 0xffu);
        recording.SetStencilWriteMask(context, face, 0x0fu);
        recording.SetStencilReference(context, face, 3u);
    }
}

inline std::vector<std::string> stateTrackedDrawLines(VkPipeline pipeline) {
    std::vector<std::string> lines{
        stateLine("bind", 0u, pipeline),
        stateLine("viewport", 0.0f, 0.0f, 64.0f, 32.0f, 0.0f, 1.0f),
        stateLine("scissor", 1, 2, 30u, 20u),
        stateLine("bias", 2.0f, 0.0f, 1.0f),
        stateLine("bounds", 0.25f, 0.75f),
        stateLine("cull", static_cast<std::uint32_t>(VK_CULL_MODE_BACK_BIT)),
        stateLine("front", static_cast<std::uint32_t>(VK_FRONT_FACE_CLOCKWISE)),
        stateLine("depthtest", 1u),
        stateLine("depthwrite", 1u),
        stateLine("depthcompare", static_cast<std::uint32_t>(VK_COMPARE_OP_LESS)),
        stateLine("boundstest", 1u),
        stateLine("stenciltest", 1u)};
    for (const VkStencilFaceFlags face : {VK_STENCIL_FACE_FRONT_BIT, VK_STENCIL_FACE_BACK_BIT}) {
        const auto bits = static_cast<std::uint32_t>(face);
        lines.push_back(stateLine("stencilop", bits, static_cast<std::uint32_t>(VK_STENCIL_OP_KEEP), static_cast<std::uint32_t>(VK_STENCIL_OP_REPLACE), static_cast<std::uint32_t>(VK_STENCIL_OP_KEEP), static_cast<std::uint32_t>(VK_COMPARE_OP_ALWAYS)));
        lines.push_back(stateLine("comparemask", bits, 0xffu));
        lines.push_back(stateLine("writemask", bits, 0x0fu));
        lines.push_back(stateLine("reference", bits, 3u));
    }
    return lines;
}

}
