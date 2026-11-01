#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

namespace {

QueueState DepthCopyDraw(const QueueState& queue, const Graphics::DepthCopyPass& copy) {
    const bool multisampled = copy.source.samples != 1u;
    const auto address = DepthCopyPixelProgramAddress(multisampled);
    const auto words = Graphics::DepthCopySourceWords(copy.source);
    QueueState draw = queue;
    draw.context.insert_or_assign(0x000u, 0u);
    draw.shader.insert_or_assign(0x008u, static_cast<std::uint32_t>(address >> 8u));
    draw.shader.insert_or_assign(0x009u, static_cast<std::uint32_t>(address >> 40u));
    draw.shader.insert_or_assign(0x00au, 0u);
    draw.shader.insert_or_assign(0x00bu, DepthCopyPixelUserWords(multisampled) << 1u);
    for (std::uint32_t index = 0; index < words.size(); ++index) draw.shader.insert_or_assign(0x00cu + index, words[index]);
    if (multisampled) draw.shader.insert_or_assign(0x014u, copy.sample);
    draw.context.insert_or_assign(0x1b3u, 0x8002u);
    draw.context.insert_or_assign(0x1b4u, 0x8002u);
    draw.context.insert_or_assign(0x1b6u, 0u);
    draw.context.insert_or_assign(0x1c4u, 0u);
    draw.context.insert_or_assign(0x1c5u, 1u);
    draw.context.insert_or_assign(0x08fu, 1u);
    draw.context.insert_or_assign(0x203u, 0u);
    if (const auto alpha = draw.context.find(0x2dcu); alpha != draw.context.end()) draw.context.insert_or_assign(0x2dcu, alpha->second & ~1u);
    return draw;
}

}

bool Driver::drawPrecheck() {
    static const bool precheck = std::getenv("APS5_NO_DRAW_PRECHECK") == nullptr;
    return precheck;
}

std::optional<DrawVerdict> Driver::precheckDraw(const QueueState& queue, const Submission& submission, std::size_t packetOffset, std::span<const std::uint32_t> packet, const Pm4::DrawParameters& drawParameters, std::string& rejected, bool& traceIndirect) {
    if (!drawParameters.indirect && !drawParameters.indexed && (drawParameters.indexCount == 0 || drawParameters.instanceCount == 0)) return DrawVerdict::Nothing;
    if (const auto pass = Graphics::DecodeColorMetadataPass(queue)) {
        require(!drawParameters.indirect, "indirect CB metadata passes are unsupported");
        if (DrawPipeline::Active() && DrawPipeline::MetadataPassesPipelined() && deferredLabels().labels.empty()) {
            auto writes = Graphics::PipelinedColorMetadataPassWrites(*pass);
            const auto localDevice = device.Load();
            if (!writes.empty() && localDevice != nullptr) {
                const auto queueId = submission.queue;
                const auto opcode = (packet[0] >> 8u) & 0xffu;
                DrawPipeline::Queue0().Enqueue([this, localDevice, queueId, opcode, pass = *pass] {
                    GuestMemory::SetCurrentPacket(opcode, queueId);
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    auto current = device.Load();
                    const auto& target = current != nullptr ? current : localDevice;
                    recordLabelsForPacket(target.get(), queueId);
                    target->ColorMetadataPass(pass);
                }, std::move(writes));
                DrawPipeline::CountPipelinedMetadataPass();
                return DrawVerdict::Drawn;
            }
        }
        if (DrawPipeline::Active()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, (packet[0] >> 8u) & 0xffu);
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        const std::shared_ptr<VulkanDevice> localDevice = device;
        recordLabelsForPacket(localDevice.get(), submission.queue);
        localDevice->ColorMetadataPass(*pass);
        return DrawVerdict::Drawn;
    }
    if (const auto copy = Graphics::DecodeDepthCopyPass(queue)) {
        require(!drawParameters.indirect, "indirect DB depth copies are unsupported");
        auto copyDraw = DepthCopyDraw(queue, *copy);
        return draw(copyDraw, packet, submission, packetOffset, rejected);
    }
    if (Graphics::DepthResummarizeDrawsNothing(queue)) return DrawVerdict::Nothing;
    rejected = Graphics::DepthMaintenanceRejection(queue);
    if (!rejected.empty()) return DrawVerdict::Rejected;
    static const bool traceIndirectEnabled = std::getenv("APS5_TRACE_INDIRECT_DRAWS") != nullptr;
    traceIndirect = traceIndirectEnabled;
    if (traceIndirect && drawParameters.indirect) std::fprintf(stderr, "[draw] indirect packet %s args 0x%llx count %u reached\n", Pm4::Name(packet[0]).c_str(), static_cast<unsigned long long>(drawParameters.indirect->arguments), drawParameters.indirect->count);
    {
        const auto targetMask = queue.context.find(0x8e);
        const auto shaderMask = queue.context.find(0x8f);
        const bool colorWrites = targetMask != queue.context.end() && shaderMask != queue.context.end() && Graphics::ColorWriteMask(queue.context) != 0;
        if (!colorWrites && !queue.shader.contains(0x8)) {
            const auto word = [&](std::uint32_t offset) { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
            if ((word(0x200) & 3u) == 0 || ((word(0x010) & 3u) == 0 && (word(0x011) & 1u) == 0)) return DrawVerdict::Nothing;
        }
    }
    if (drawParameters.indexed) {
        const auto restart = queue.userConfig.find(0x24b);
        if (restart != queue.userConfig.end() && restart->second != 0) {
            const auto resetIndex = queue.context.find(0x103);
            const auto primitive = queue.userConfig.find(0x242);
            const std::uint32_t allOnes = drawParameters.indexSize == 2 ? 0xffffu : 0xffffffffu;
            const auto type = primitive == queue.userConfig.end() ? 0u : primitive->second & 0x3fu;
            const bool strip = type == 3 || type == 5 || type == 6 || type == 21;
            const bool list = type == 1 || type == 2 || type == 4;
            if (!strip && !list) {
                rejected = "AGC graphics: primitive restart is only supported for strips and lists";
                return DrawVerdict::Rejected;
            }
            if (resetIndex == queue.context.end() || (resetIndex->second & allOnes) != allOnes) {
                rejected = "AGC graphics: primitive restart index other than all ones is unsupported";
                return DrawVerdict::Rejected;
            }
        }
    }
    if (drawPrecheck()) {
        rejected = Graphics::DrawRejection(queue, drawParameters.indexed);
        if (!rejected.empty()) return DrawVerdict::Rejected;
    }
    return std::nullopt;
}

}
