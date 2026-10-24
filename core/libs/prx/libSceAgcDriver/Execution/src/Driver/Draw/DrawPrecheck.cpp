#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

bool Driver::drawPrecheck() {
    static const bool precheck = std::getenv("APS5_NO_DRAW_PRECHECK") == nullptr;
    return precheck;
}

std::optional<DrawVerdict> Driver::precheckDraw(const QueueState& queue, const Submission& submission, std::span<const std::uint32_t> packet, const Pm4::DrawParameters& drawParameters, std::string& rejected, bool& traceIndirect) {
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
        if (DrawPipeline::Active()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, (packet[0] >> 8u) & 0xffu);
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        const std::shared_ptr<VulkanDevice> localDevice = device;
        recordLabelsForPacket(localDevice.get(), submission.queue);
        localDevice->DepthCopyPass(*copy);
        return DrawVerdict::Drawn;
    }
    if (const auto control = Graphics::DecodeDepthRenderControl(queue); control && ((control->control & 0x60u) != 0u || Graphics::DepthSurfaceAt(control->address))) {
        if (DrawPipeline::Active()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, (packet[0] >> 8u) & 0xffu);
        Graphics::NoteDepthRenderControl(*control);
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
            const auto restartDevice = device.Load();
            if (!strip && !(list && restartDevice != nullptr && restartDevice->PrimitiveListRestart())) {
                rejected = "AGC graphics: primitive restart is only supported for strips, and for lists with VK_EXT_primitive_topology_list_restart";
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
