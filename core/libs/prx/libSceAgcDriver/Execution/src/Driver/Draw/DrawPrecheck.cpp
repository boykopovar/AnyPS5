#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawAhead.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

namespace {

bool precheckRejections() {
    static const bool precheck = std::getenv("APS5_NO_DRAW_PRECHECK") == nullptr;
    return precheck;
}

template <typename TRestart, typename TReached>
DrawPrecheckOutcome precheckState(const QueueState& queue, const Pm4::DrawParameters& drawParameters, std::string& rejected, std::optional<Graphics::ColorMetadataPass>& metadata, TRestart&& primitiveListRestart, TReached&& reached) {
    if (!drawParameters.indirect && !drawParameters.indexed && (drawParameters.indexCount == 0 || drawParameters.instanceCount == 0)) return DrawPrecheckOutcome::Nothing;
    if (auto pass = Graphics::DecodeColorMetadataPass(queue)) {
        require(!drawParameters.indirect, "indirect CB metadata passes are unsupported");
        metadata = std::move(pass);
        return DrawPrecheckOutcome::MetadataPass;
    }
    if (Graphics::DepthResummarizeDrawsNothing(queue)) return DrawPrecheckOutcome::Nothing;
    rejected = Graphics::DepthMaintenanceRejection(queue);
    if (!rejected.empty()) return DrawPrecheckOutcome::Rejected;
    reached();
    {
        const auto targetMask = queue.context.find(0x8e);
        const auto shaderMask = queue.context.find(0x8f);
        const bool colorWrites = targetMask != queue.context.end() && shaderMask != queue.context.end() && Graphics::ColorWriteMask(queue.context) != 0;
        if (!colorWrites && !queue.shader.contains(0x8)) {
            const auto word = [&](std::uint32_t offset) { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
            if ((word(0x200) & 3u) == 0 || ((word(0x010) & 3u) == 0 && (word(0x011) & 1u) == 0)) return DrawPrecheckOutcome::Nothing;
        }
    }
    if (drawParameters.indexed) {
        const auto restart = queue.userConfig.find(0x24b);
        if (restart != queue.userConfig.end() && restart->second != 0) {
            const auto resetIndex = queue.context.find(0x103);
            const auto primitive = queue.userConfig.find(0x242);
            const std::uint32_t allOnes = drawParameters.indexSize == 2 ? 0xffffu : 0xffffffffu;
            const auto type = primitive == queue.userConfig.end() ? 0u : primitive->second & 0x3fu;
            const bool strip = type == 3 || type == 5 || type == 6;
            const bool list = type == 1 || type == 2 || type == 4;
            if (!strip && !(list && primitiveListRestart())) {
                rejected = "AGC graphics: primitive restart is only supported for strips, and for lists with VK_EXT_primitive_topology_list_restart";
                return DrawPrecheckOutcome::Rejected;
            }
            if (resetIndex == queue.context.end() || (resetIndex->second & allOnes) != allOnes) {
                rejected = "AGC graphics: primitive restart index other than all ones is unsupported";
                return DrawPrecheckOutcome::Rejected;
            }
        }
    }
    if (precheckRejections()) {
        rejected = Graphics::DrawRejection(queue, drawParameters.indexed);
        if (!rejected.empty()) return DrawPrecheckOutcome::Rejected;
    }
    return DrawPrecheckOutcome::Proceed;
}

}

bool TraceIndirectDraws() {
    static const bool trace = std::getenv("APS5_TRACE_INDIRECT_DRAWS") != nullptr;
    return trace;
}

DrawPrecheckOutcome PrecheckDrawState(const QueueState& queue, const Pm4::DrawParameters& drawParameters, bool primitiveListRestart, std::string& rejected) {
    std::optional<Graphics::ColorMetadataPass> metadata;
    return precheckState(queue, drawParameters, rejected, metadata, [&] { return primitiveListRestart; }, [] {});
}

std::optional<DrawVerdict> Driver::precheckDraw(const QueueState& queue, const Submission& submission, std::span<const std::uint32_t> packet, const Pm4::DrawParameters& drawParameters, std::string& rejected, bool& traceIndirect) {
    std::optional<Graphics::ColorMetadataPass> metadata;
    const auto outcome = precheckState(queue, drawParameters, rejected, metadata, [&] {
        const auto restartDevice = device.Load();
        return restartDevice != nullptr && restartDevice->PrimitiveListRestart();
    }, [&] {
        traceIndirect = TraceIndirectDraws();
        if (traceIndirect && drawParameters.indirect) std::fprintf(stderr, "[draw] indirect packet %s args 0x%llx count %u reached\n", Pm4::Name(packet[0]).c_str(), static_cast<unsigned long long>(drawParameters.indirect->arguments), drawParameters.indirect->count);
    });
    switch (outcome) {
        case DrawPrecheckOutcome::MetadataPass: {
            if (DrawPipeline::Active()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, (packet[0] >> 8u) & 0xffu);
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            const std::shared_ptr<VulkanDevice> localDevice = device;
            recordLabelsForPacket(localDevice.get(), submission.queue);
            localDevice->ColorMetadataPass(*metadata);
            return DrawVerdict::Drawn;
        }
        case DrawPrecheckOutcome::Nothing: return DrawVerdict::Nothing;
        case DrawPrecheckOutcome::Rejected: return DrawVerdict::Rejected;
        case DrawPrecheckOutcome::Proceed: return std::nullopt;
    }
    throw std::runtime_error("unknown draw precheck outcome");
}

}
