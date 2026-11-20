#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MeshArguments_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBackBound_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleCounter_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleDumps_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DispatchArguments_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include "prx/libc/include/general/AtomicSharedPtr.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
std::optional<std::chrono::steady_clock::time_point> Recorder::PendingLabelSince() {
    const auto since = pendingLabelSince.load(std::memory_order_acquire);
    if (since == NoPendingLabel) return std::nullopt;
    return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(since));
}

std::uint64_t Recorder::WriteGeneration() {
    return writeGeneration.load(std::memory_order_acquire);
}

std::uint64_t Recorder::PublishGeneration() {
    return publishGeneration.load(std::memory_order_acquire);
}

std::uint64_t Recorder::PendingCompletionLabels() {
    return completionLabels.load(std::memory_order_acquire);
}

std::uint64_t Recorder::PendingCompletionStores() {
    return completionStores.load(std::memory_order_acquire);
}

std::uint64_t Recorder::PendingWriteBackCompletions() {
    return writeBackCompletions.load(std::memory_order_acquire);
}

std::uint64_t Recorder::RecordedWorkSinceSubmit() {
    return workSinceSubmit.load(std::memory_order_relaxed);
}

bool Recorder::NoteGuestWork(VkCommandBuffer commands) {
    if (open == nullptr || open->commands != commands) return false;
    open->guestWork = true;
    return true;
}

void Recorder::CountRecordedWork() {
    workSinceSubmit.fetch_add(1, std::memory_order_relaxed);
}

namespace {

bool sameBits(float left, float right) {
    return std::bit_cast<std::uint32_t>(left) == std::bit_cast<std::uint32_t>(right);
}

template<std::size_t Count>
bool sameBits(const std::array<float, Count>& left, const std::array<float, Count>& right) {
    for (std::size_t index = 0; index < Count; ++index) {
        if (!sameBits(left[index], right[index])) return false;
    }
    return true;
}

bool sameBits(const VkViewport& left, const VkViewport& right) {
    return sameBits(left.x, right.x) && sameBits(left.y, right.y) && sameBits(left.width, right.width) && sameBits(left.height, right.height) && sameBits(left.minDepth, right.minDepth) && sameBits(left.maxDepth, right.maxDepth);
}

bool sameBits(const VkRect2D& left, const VkRect2D& right) {
    return left.offset.x == right.offset.x && left.offset.y == right.offset.y && left.extent.width == right.extent.width && left.extent.height == right.extent.height;
}

}

std::uint32_t CommandState::DynamicSlots(std::span<const VkDynamicState> states) {
    std::uint32_t slots = 0;
    for (const auto state : states) {
        switch (state) {
        case VK_DYNAMIC_STATE_VIEWPORT: slots |= Mask(Viewport); break;
        case VK_DYNAMIC_STATE_SCISSOR: slots |= Mask(Scissor); break;
        case VK_DYNAMIC_STATE_DEPTH_BIAS: slots |= Mask(DepthBias); break;
        case VK_DYNAMIC_STATE_DEPTH_BOUNDS: slots |= Mask(DepthBounds); break;
        case VK_DYNAMIC_STATE_CULL_MODE: slots |= Mask(CullMode); break;
        case VK_DYNAMIC_STATE_FRONT_FACE: slots |= Mask(FrontFace); break;
        case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE: slots |= Mask(DepthTest); break;
        case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: slots |= Mask(DepthWrite); break;
        case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP: slots |= Mask(DepthCompare); break;
        case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE: slots |= Mask(DepthBoundsTest); break;
        case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE: slots |= Mask(StencilTest); break;
        case VK_DYNAMIC_STATE_STENCIL_OP: slots |= Mask(StencilOpFront) | Mask(StencilOpBack); break;
        case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: slots |= Mask(CompareMaskFront) | Mask(CompareMaskBack); break;
        case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: slots |= Mask(WriteMaskFront) | Mask(WriteMaskBack); break;
        case VK_DYNAMIC_STATE_STENCIL_REFERENCE: slots |= Mask(ReferenceFront) | Mask(ReferenceBack); break;
        case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT: slots |= Mask(ColorWriteMask); break;
        default: throw std::runtime_error("AGC graphics: a pipeline declares a dynamic state the command tracker does not follow");
        }
    }
    return slots;
}

void CommandState::Begin(VkCommandBuffer recording, bool suppress) {
    commands = recording;
    trackable = suppress ? ~0u : 0u;
    Invalidate();
}

void CommandState::Invalidate() noexcept {
    valid = 0;
    overwritten = 0;
    dynamicSlots = 0;
    pipeline = VK_NULL_HANDLE;
    pipelineKnown = false;
}

template<typename TValue, typename TEqual, typename TEmit>
void CommandState::apply(Slot slot, TValue CommandState::*field, const TValue& value, TEqual equal, TEmit emit) {
    if (Known(slot) && equal(this->*field, value)) return;
    emit();
    this->*field = value;
    wrote(Mask(slot));
}

template<typename TValue, typename TEmit>
void CommandState::applyFaces(Slot first, VkStencilFaceFlags faces, std::array<TValue, 2> CommandState::*field, const TValue& value, TEmit emit) {
    VkStencilFaceFlags needed = 0;
    for (std::uint32_t face = 0; face < 2; ++face) {
        const VkStencilFaceFlags bit = 1u << face;
        if ((faces & bit) == 0) continue;
        if (!Known(static_cast<Slot>(first + face)) || !((this->*field)[face] == value)) needed |= bit;
    }
    if (needed == 0) return;
    emit(needed);
    for (std::uint32_t face = 0; face < 2; ++face) {
        if ((needed & (1u << face)) == 0) continue;
        (this->*field)[face] = value;
        wrote(Mask(static_cast<Slot>(first + face)));
    }
}

void CommandState::BindPipeline(const Context& context, VkPipeline bound, std::uint32_t slots) {
    if (pipelineKnown && pipeline == bound && dynamicSlots == slots && (overwritten & ~slots) == 0) return;
    context.tracked(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, bound);
    pipeline = bound;
    pipelineKnown = trackable != 0 && bound != VK_NULL_HANDLE;
    dynamicSlots = slots;
    valid &= slots;
    overwritten = 0;
}

void CommandState::SetViewport(const Context& context, const VkViewport& value) {
    apply(Viewport, &CommandState::viewport, value, [](const auto& left, const auto& right) { return sameBits(left, right); }, [&] { context.tracked(&DeviceFunctions::cmdSetViewport, "vkCmdSetViewport")(commands, 0, 1, &value); });
}

void CommandState::SetScissor(const Context& context, const VkRect2D& value) {
    apply(Scissor, &CommandState::scissor, value, [](const auto& left, const auto& right) { return sameBits(left, right); }, [&] { context.tracked(&DeviceFunctions::cmdSetScissor, "vkCmdSetScissor")(commands, 0, 1, &value); });
}

void CommandState::SetViewports(const Context& context, std::span<const VkViewport> viewports, std::span<const VkRect2D> scissors) {
    Require(!viewports.empty() && viewports.size() == scissors.size(), "every viewport needs a scissor");
    const auto count = static_cast<std::uint32_t>(viewports.size());
    context.tracked(&DeviceFunctions::cmdSetViewport, "vkCmdSetViewport")(commands, 0, count, viewports.data());
    context.tracked(&DeviceFunctions::cmdSetScissor, "vkCmdSetScissor")(commands, 0, count, scissors.data());
    viewport = viewports.front();
    scissor = scissors.front();
    wrote(Mask(Viewport) | Mask(Scissor));
}

void CommandState::SetDepthBias(const Context& context, float constant, float clamp, float slope) {
    const std::array<float, 3> bias{constant, clamp, slope};
    apply(DepthBias, &CommandState::depthBias, bias, [](const auto& left, const auto& right) { return sameBits(left, right); }, [&] { context.tracked(&DeviceFunctions::cmdSetDepthBias, "vkCmdSetDepthBias")(commands, constant, clamp, slope); });
}

void CommandState::SetDepthBounds(const Context& context, float minimum, float maximum) {
    const std::array<float, 2> bounds{minimum, maximum};
    apply(DepthBounds, &CommandState::depthBounds, bounds, [](const auto& left, const auto& right) { return sameBits(left, right); }, [&] { context.tracked(&DeviceFunctions::cmdSetDepthBounds, "vkCmdSetDepthBounds")(commands, minimum, maximum); });
}

void CommandState::SetCullMode(const Context& context, VkCullModeFlags mode) {
    apply(CullMode, &CommandState::cullMode, mode, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetCullMode, "vkCmdSetCullModeEXT")(commands, mode); });
}

void CommandState::SetFrontFace(const Context& context, VkFrontFace face) {
    apply(FrontFace, &CommandState::frontFace, face, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetFrontFace, "vkCmdSetFrontFaceEXT")(commands, face); });
}

void CommandState::SetDepthTestEnable(const Context& context, VkBool32 enable) {
    apply(DepthTest, &CommandState::depthTest, enable, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetDepthTestEnable, "vkCmdSetDepthTestEnableEXT")(commands, enable); });
}

void CommandState::SetDepthWriteEnable(const Context& context, VkBool32 enable) {
    apply(DepthWrite, &CommandState::depthWrite, enable, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetDepthWriteEnable, "vkCmdSetDepthWriteEnableEXT")(commands, enable); });
}

void CommandState::SetDepthCompareOp(const Context& context, VkCompareOp compare) {
    apply(DepthCompare, &CommandState::depthCompare, compare, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetDepthCompareOp, "vkCmdSetDepthCompareOpEXT")(commands, compare); });
}

void CommandState::SetDepthBoundsTestEnable(const Context& context, VkBool32 enable) {
    apply(DepthBoundsTest, &CommandState::depthBoundsTest, enable, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetDepthBoundsTestEnable, "vkCmdSetDepthBoundsTestEnableEXT")(commands, enable); });
}

void CommandState::SetStencilTestEnable(const Context& context, VkBool32 enable) {
    apply(StencilTest, &CommandState::stencilTest, enable, std::equal_to<>{}, [&] { context.tracked(&DeviceFunctions::cmdSetStencilTestEnable, "vkCmdSetStencilTestEnableEXT")(commands, enable); });
}

void CommandState::SetStencilOp(const Context& context, VkStencilFaceFlags faces, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    applyFaces(StencilOpFront, faces, &CommandState::stencilOps, StencilOps{failOp, passOp, depthFailOp, compareOp}, [&](VkStencilFaceFlags needed) { context.tracked(&DeviceFunctions::cmdSetStencilOp, "vkCmdSetStencilOpEXT")(commands, needed, failOp, passOp, depthFailOp, compareOp); });
}

void CommandState::SetStencilCompareMask(const Context& context, VkStencilFaceFlags faces, std::uint32_t mask) {
    applyFaces(CompareMaskFront, faces, &CommandState::compareMasks, mask, [&](VkStencilFaceFlags needed) { context.tracked(&DeviceFunctions::cmdSetStencilCompareMask, "vkCmdSetStencilCompareMask")(commands, needed, mask); });
}

void CommandState::SetStencilWriteMask(const Context& context, VkStencilFaceFlags faces, std::uint32_t mask) {
    applyFaces(WriteMaskFront, faces, &CommandState::writeMasks, mask, [&](VkStencilFaceFlags needed) { context.tracked(&DeviceFunctions::cmdSetStencilWriteMask, "vkCmdSetStencilWriteMask")(commands, needed, mask); });
}

void CommandState::SetStencilReference(const Context& context, VkStencilFaceFlags faces, std::uint32_t reference) {
    applyFaces(ReferenceFront, faces, &CommandState::references, reference, [&](VkStencilFaceFlags needed) { context.tracked(&DeviceFunctions::cmdSetStencilReference, "vkCmdSetStencilReference")(commands, needed, reference); });
}

void CommandState::SetColorWriteMasks(const Context& context, std::span<const VkColorComponentFlags> masks) {
    ColorWriteMasks value{};
    Require(!masks.empty() && masks.size() <= value.masks.size(), "color write masks exceed the color attachments");
    value.count = static_cast<std::uint32_t>(masks.size());
    std::ranges::copy(masks, value.masks.begin());
    const auto covered = [](const ColorWriteMasks& known, const ColorWriteMasks& wanted) { return wanted.count <= known.count && std::equal(wanted.masks.begin(), wanted.masks.begin() + wanted.count, known.masks.begin()); };
    apply(ColorWriteMask, &CommandState::colorWriteMasks, value, covered, [&] { context.tracked(&DeviceFunctions::cmdSetColorWriteMask, "vkCmdSetColorWriteMaskEXT")(commands, 0, value.count, masks.data()); });
}

CommandState& Recorder::GraphicsState() {
    Require(open != nullptr, "no batch is open for graphics state");
    return open->graphicsState;
}

Recorder* Recorder::Active() {
    return activeRecorder;
}

void Recorder::Activate() {
    activeRecorder = this;
    {
        std::lock_guard tableLock(labelTableMutex);
        labelTableOwner = this;
    }
    GuestMemory::SetFlushHook(&FlushForAccess);
}

std::optional<Recorder::LabelHit> Recorder::LookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, LabelRefusal* refusal) {
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return std::nullopt;
    return labelTableOwner->lookupLabel(address, bytes, afterStamp, refusal);
}

bool Recorder::WideLabelIn(std::uint64_t address, std::size_t bytes) {
    std::lock_guard tableLock(labelTableMutex);
    return labelTableOwner != nullptr && labelTableOwner->wideLabelInLocked(address, bytes);
}

bool Recorder::wideLabelInLocked(std::uint64_t address, std::size_t bytes) const {
    if (wideLabels.empty() || bytes == 0) return false;
    const auto end = address + bytes;
    for (auto it = wideLabels.lower_bound(address >= wideLabelBytes ? address - wideLabelBytes + 1 : 0); it != wideLabels.end() && it->first < end; ++it) {
        if (it->second.end > address) return true;
    }
    return false;
}

bool Recorder::LookupStoredBytes(std::uint64_t address, std::span<std::byte> bytes) {
    std::lock_guard tableLock(labelTableMutex);
    return labelTableOwner != nullptr && labelTableOwner->storedBytesLocked(address, bytes);
}

bool Recorder::storedBytesLocked(std::uint64_t address, std::span<std::byte> bytes) const {
    if (bytes.empty() || bytes.size() % 4 != 0 || address % 4 != 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes.size()) return false;
    const auto end = address + bytes.size();
    if (const auto queued = queuedLabels.lower_bound(address); queued != queuedLabels.end() && queued->first < end) return false;
    const auto dwords = bytes.size() / 4;
    std::vector<const std::pair<const std::uint64_t, WideLabel>*> newest(dwords, nullptr);
    for (auto it = wideLabels.lower_bound(address >= wideLabelBytes ? address - wideLabelBytes + 1 : 0); it != wideLabels.end() && it->first < end; ++it) {
        if (it->second.end <= address) continue;
        const auto from = std::max(it->first, address);
        const auto to = std::min(it->second.end, end);
        for (auto dword = (from - address) / 4; dword < (to - address + 3) / 4; ++dword) {
            if (newest[dword] == nullptr || newest[dword]->second.sequence < it->second.sequence) newest[dword] = &*it;
        }
    }
    std::vector<std::byte> composed(bytes.size());
    for (std::size_t dword = 0; dword < dwords; ++dword) {
        const auto at = address + dword * 4;
        if (const auto found = labels.find(at); found != labels.end()) {
            const auto& entry = found->second;
            if (entry.batch == nullptr || entry.batch->submitStarted.load(std::memory_order_acquire) || entry.overwritten || entry.behindCompletion) return false;
            std::memcpy(composed.data() + dword * 4, &entry.value, 4);
            continue;
        }
        const auto* wide = newest[dword];
        if (wide == nullptr || at < wide->first || wide->second.end - at < 4 || wide->second.overwritten || wide->second.batch == nullptr || wide->second.batch->submitStarted.load(std::memory_order_acquire)) return false;
        std::memcpy(composed.data() + dword * 4, wide->second.bytes.data() + (at - wide->first), 4);
    }
    std::copy(composed.begin(), composed.end(), bytes.begin());
    return true;
}

Recorder::OwnLabel Recorder::UnlandedOwnLabel(std::uint64_t address, std::size_t bytes, std::uint32_t queue) {
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return {};
    return labelTableOwner->unlandedOwnLabel(address, bytes, queue);
}

Recorder::OwnLabel Recorder::unlandedOwnLabel(std::uint64_t address, std::size_t bytes, std::uint32_t queue) const {
    OwnLabel own;
    if ((labels.empty() && queuedLabels.empty()) || (bytes != 4 && bytes != 8) || address % 4 != 0) return own;
    for (std::size_t offset = 0; offset < bytes; offset += 4) {
        const auto dword = address + offset;
        const LabelEntry* entry = nullptr;
        if (const auto found = labels.find(dword); found != labels.end()) entry = &found->second;
        if (const auto queued = queuedLabels.find(dword); queued != queuedLabels.end() && (entry == nullptr || queued->second.stamp > entry->stamp)) entry = &queued->second;
        if (entry == nullptr || entry->queue != queue || entry->overwritten) continue;
        own.value |= static_cast<std::uint64_t>(entry->value) << (offset * 8u);
        own.mask |= 0xffffffffull << (offset * 8u);
    }
    return own;
}

std::optional<std::uint64_t> Recorder::LookupLabelValue(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp) {
    const auto hit = LookupLabel(address, bytes, afterStamp);
    if (!hit.has_value()) return std::nullopt;
    return hit->value;
}

bool Recorder::LateTrust() {
    return LateTrustEnabled();
}

Recorder::LateStatistics Recorder::LateCounts() {
    return lateCounts;
}

void Recorder::CloseLabelGroup(std::uint64_t trackerGeneration) {
    if (LabelGroupDwords().empty()) return;
    auto group = std::move(LabelGroupDwords());
    LabelGroupDwords().clear();
    if (trackerGeneration == 0) return;
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return;
    auto& recorded = labelTableOwner->labels;
    for (const auto& [dword, stamp] : group) {
        // Only an entry still ours (stamps are unique): a later note of the dword has its own group.
        // And only while its batch is unsubmitted: a submitted one (a drain or APS5_LABEL_SUBMIT_NOW
        // inside the group) may have landed the value already, and a CPU store between that and
        // this close is not newer than the generation.
        const auto found = recorded.find(dword);
        if (found != recorded.end() && found->second.stamp == stamp && found->second.batch != nullptr && !found->second.batch->submitted) found->second.generation = trackerGeneration;
    }
    // A poller that refused the group as unclosed re-consults the table on the next generation.
    writeGeneration.fetch_add(1, std::memory_order_release);
}

void Recorder::NoteQueuedLabel(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue) {
    if (bytes.size() < 4 || address % 4 != 0) return;
    // The range before the table: the hook covers the group whether or not a table exists to
    // enter it in (no owner: the callback records it outright, by a CPU store when need be).
    QueuedLabelRanges().emplace_back(address, address + bytes.size());
    QueuedLabelsOnThread::Set(true);
    queuedLabelsNoted.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return;
    auto& recorded = labelTableOwner->labels;
    auto& table = SeparateQueuedLabels() ? labelTableOwner->queuedLabels : recorded;
    for (std::size_t offset = 0; offset + 4 <= bytes.size(); offset += 4) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, 4);
        if (const auto found = recorded.find(address + offset); found != recorded.end() && found->second.batch != nullptr) queuedLabelsOverRecorded.fetch_add(1, std::memory_order_relaxed);
        table.insert_or_assign(address + offset, LabelEntry{value, queue, stamp, nullptr});
    }
    labelTableOwner->recordedLabels.store(recorded.size(), std::memory_order_relaxed);
}

void Recorder::ForgetQueuedLabels() {
    if (!QueuedLabelsOnThread::Get()) return;
    auto ranges = std::move(QueuedLabelRanges());
    QueuedLabelRanges().clear();
    QueuedLabelsOnThread::Set(false);
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return;
    auto& recorded = labelTableOwner->labels;
    auto& queued = labelTableOwner->queuedLabels;
    const auto tag = GuestMemory::GpuLockThreadTag();
    for (const auto& [begin, end] : ranges) {
        for (auto dword = begin; dword + 4 <= end; dword += 4) {
            if (const auto own = queued.find(dword); own != queued.end() && own->second.queue == tag) queued.erase(own);
            const auto found = recorded.find(dword);
            if (found != recorded.end() && found->second.batch == nullptr) recorded.erase(found);
        }
    }
    labelTableOwner->recordedLabels.store(recorded.size(), std::memory_order_relaxed);
}

std::uint64_t Recorder::ThreadClearAccesses() {
    return ClearAccesses::Get();
}

bool Recorder::QueuedLabelsPending() {
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return false;
    const auto& recorded = labelTableOwner->labels;
    return !labelTableOwner->queuedLabels.empty() || std::any_of(recorded.begin(), recorded.end(), [](const auto& entry) { return entry.second.batch == nullptr; });
}

void Recorder::SetQueuedLabelRecorder(void (*recorder)()) {
    queuedLabelRecorder.store(recorder, std::memory_order_release);
}

Recorder::StoreStatistics Recorder::StoreCounts() {
    return StoreStatistics{storeCount.load(std::memory_order_relaxed), storeRuns.load(std::memory_order_relaxed), storesJoined.load(std::memory_order_relaxed), storesReplaced.load(std::memory_order_relaxed), storeWawBarriers.load(std::memory_order_relaxed), storeJoinsRefused.load(std::memory_order_relaxed), queuedLabelsNoted.load(std::memory_order_relaxed), queuedLabelsOverRecorded.load(std::memory_order_relaxed), queuedLabelHits.load(std::memory_order_relaxed), queuedLabelHookRecords.load(std::memory_order_relaxed), queuedLabelHookInCompletion.load(std::memory_order_relaxed), keyStoreCount.load(std::memory_order_relaxed), keyStoreRuns.load(std::memory_order_relaxed), keyStoreRunsForWriter.load(std::memory_order_relaxed), keyStoresJoined.load(std::memory_order_relaxed), storeRunsAtSubmit.load(std::memory_order_relaxed), storeRunsForced.load(std::memory_order_relaxed)};
}

std::uint64_t Recorder::ThreadHookWaits() {
    return hookRealWaits;
}

namespace {

struct DispatchOrder {
    std::uint32_t queueKey = 0;
    bool unordered = false;
};

thread_local DispatchOrder nextDispatchOrder;
std::atomic<std::uint64_t> overlapDeferred{0}, overlapJoined{0}, overlapFlushed{0};

}

bool Recorder::DispatchOverlapEnabled() {
    static const bool enabled = std::getenv("APS5_NO_DISPATCH_OVERLAP") == nullptr;
    return enabled;
}

void Recorder::SetDispatchOrder(std::uint32_t queueKey, bool unordered) {
    nextDispatchOrder = {queueKey, unordered};
}

std::uint32_t Recorder::PeekUnorderedDispatch() {
    return nextDispatchOrder.unordered ? nextDispatchOrder.queueKey : 0u;
}

}
}
}
