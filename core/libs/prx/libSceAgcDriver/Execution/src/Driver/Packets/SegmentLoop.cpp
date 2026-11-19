#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SegmentLoop.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/DriverTimed.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/PacketRules.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct LeanRun {
    PacketProfile& packetProfile;
    PacketCounters& packetCounters;
    FlushGate gate;
    bool packetEpoch = false;
    bool report = false;
    std::uint64_t packets = 0;
};

namespace {

using LeanFront::Counter;
constexpr std::uint32_t NoTag = 0xffffffffu;

bool orderedWaits() {
    static const bool ordered = std::getenv("APS5_PIPELINE_DRAIN_LABELS") == nullptr;
    return ordered;
}

bool orderedCompletion() {
    static const bool ordered = std::getenv("APS5_PIPELINE_DRAIN_SUBMISSIONS") == nullptr && orderedWaits();
    return ordered;
}

bool epochEach() {
    static const bool each = std::getenv("APS5_SUBMISSION_EPOCH_EACH") != nullptr;
    return each;
}

bool traceGpu() {
    static const bool trace = std::getenv("APS5_TRACE_GPU") != nullptr;
    return trace;
}

bool profileDraws() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

bool traceDraws() {
    static const bool trace = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    return trace;
}

bool drainAll() {
    static const bool all = std::getenv("APS5_DRAIN_ALL") != nullptr;
    return all;
}

bool drawDrain() {
    static const bool drain = std::getenv("APS5_DRAW_DRAIN") != nullptr;
    return drain;
}

bool skipInvalidLists() {
    static const bool skip = std::getenv("APS5_SKIP_INVALID_INDIRECT_REGISTERS") != nullptr;
    return skip;
}

bool everyPacketFlush() {
    static const bool every = !LabelBatchSubmit() || std::getenv("APS5_LABEL_OWN_LOCK") != nullptr || std::getenv("APS5_REAP_EACH_PACKET") != nullptr || std::getenv("APS5_LABEL_TRY_EACH_PACKET") != nullptr;
    return every;
}

bool timersWanted() {
    return APS5_ENABLE_TIMING_LOG || PerformanceTrace::Recording();
}

bool reportWanted() {
    return profileDraws() || CaptureTrace::Enabled() || traceGpu() || traceDraws();
}

std::string where(const Submission& submission, std::size_t cursor) {
    return "serial " + std::to_string(submission.serial) + " " + Pm4::Name(submission.commands[cursor]) + " at DWORD " + std::to_string(cursor);
}

LeanPacket::Pipeline pipelineOf(std::span<const std::uint32_t> packet, std::uint32_t header, std::uint32_t opcode) {
    using Pipeline = LeanPacket::Pipeline;
    if (header == FlipPacketHeader || header == RenderingWaitPacketHeader) return Pipeline::Drains;
    if (Pm4::DrawOpcode(opcode)) return Pipeline::Keeps;
    switch (opcode) {
        case 0x63: case 0x64: case 0x9f: return packet.size() >= 5 ? Pipeline::AsksLists : Pipeline::Drains;
        case 0x50:
            if (packet.size() < 7) return Pipeline::Drains;
            if (Pm4::DmaDestination(packet) == Pm4::DmaDestinationNowhere || Pm4::DmaBytes(packet) == 0) return Pipeline::Keeps;
            return Pipeline::Asks;
        case 0x3c: case 0x93: return packet.size() >= 5 ? Pipeline::Asks : Pipeline::Drains;
        case 0x40: return packet.size() >= 6 && !Pm4::CopyDataToRegister(packet) && Pm4::CopyDataSource(packet) == Pm4::CopyDataGpuClockSource ? Pipeline::Asks : Pipeline::Drains;
        default: return PacketRules::keepsDrawPipeline(packet, header, opcode) ? Pipeline::Keeps : Pipeline::Drains;
    }
}

LeanPacket classify(std::span<const std::uint32_t> packet, std::size_t cursor) {
    const auto header = packet[0];
    const auto opcode = (header >> 8u) & 0xffu;
    LeanPacket entry;
    entry.cursor = static_cast<std::uint32_t>(cursor);
    entry.words = static_cast<std::uint32_t>(packet.size());
    entry.header = header;
    entry.opcode = static_cast<std::uint8_t>(opcode);
    std::uint32_t flags = 0;
    if (Pm4::Predicated(header)) flags |= LeanPacket::Predicated;
    if (Pm4::DrawOpcode(opcode)) flags |= LeanPacket::Draw;
    if (Pm4::IndirectDrawOpcode(opcode)) flags |= LeanPacket::IndirectDraw;
    if (header == FlipPacketHeader) flags |= LeanPacket::FlipHeader;
    if (header == RenderingWaitPacketHeader) flags |= LeanPacket::RenderingWait;
    if (PacketRules::keepsDrawFinisher(header, opcode)) flags |= LeanPacket::KeepsFinisher;
    if (PacketRules::sealsWriteOnly(packet, opcode)) flags |= LeanPacket::Seal;
    if (opcode == 0x3c || opcode == 0x93) flags |= LeanPacket::Wait;
    if (Pm4::IndirectRegisterOpcode(opcode)) flags |= LeanPacket::IndirectRegisters;
    switch (opcode) {
        case 0x37: case 0x49: case 0x40: case 0x50: case 0x83: flags |= LeanPacket::LabelIssue; break;
        default: break;
    }
    switch (opcode) {
        case 0x37: case 0x49: case 0x50: case 0x83: case 0x46: flags |= LeanPacket::Enqueues; break;
        default: break;
    }
    switch (opcode) {
        case 0x37: case 0x49: case 0x3c: case 0x93: case 0x40: case 0x50: flags |= LeanPacket::Traced; break;
        default: break;
    }
    bool memory = drainAll() || header == FlipPacketHeader || Pm4::SampleDump(packet) || (Pm4::DrawOpcode(opcode) && drawDrain());
    switch (opcode) {
        case 0x37: case 0x49: case 0x40: case 0x45: case 0x50: case 0x83: memory = true; break;
        default: break;
    }
    if (memory) flags |= LeanPacket::Memory;
    constexpr std::uint32_t busy = LeanPacket::Predicated | LeanPacket::Draw | LeanPacket::FlipHeader | LeanPacket::RenderingWait | LeanPacket::Seal | LeanPacket::Wait | LeanPacket::LabelIssue | LeanPacket::Enqueues | LeanPacket::Memory | LeanPacket::Traced;
    bool plain = (flags & busy) == 0;
    switch (opcode) {
        case 0x15: case 0x16: case 0x22: case 0x91: case 0x3f: case 0x46: case 0x42: case 0x58: plain = false; break;
        default: break;
    }
    if (plain) flags |= LeanPacket::Plain;
    entry.flags = flags;
    entry.pipeline = pipelineOf(packet, header, opcode);
    return entry;
}

std::vector<std::uint64_t> labelWritesAt(const Submission& submission, std::span<const std::uint32_t> cursors) {
    std::vector<std::uint64_t> writes;
    for (const auto cursor : cursors) {
        const auto packet = std::span<const std::uint32_t>(submission.commands).subspan(cursor, std::min<std::size_t>(Pm4::PacketWords(submission.commands[cursor]), submission.commands.size() - cursor));
        const auto opcode = (packet[0] >> 8u) & 0xffu;
        if ((packet[0] >> 30u) != 3u || (opcode != 0x49 && opcode != 0x37)) continue;
        const auto label = Pm4::DecodeLabelWrite(packet);
        if (!label.has_value()) continue;
        const auto bytes = label->Bytes();
        if (label->address % 4 != 0 || bytes.size() > 64) continue;
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4) writes.push_back(label->address + offset);
    }
    return writes;
}

void mapLeanLists(Submission& submission) {
    if (!submission.leanLists) return;
    for (const auto& ref : submission.lists.refs) {
        const auto view = submission.lists.View(ref);
        submission.registerLists.insert_or_assign(ref.cursor, std::vector<std::uint32_t>(view.pairs.begin(), view.pairs.end()));
    }
    submission.lists.Clear();
    submission.leanLists = false;
}

}

std::size_t PacketTable::IndexAt(std::size_t cursor) const {
    const auto found = std::lower_bound(packets.begin(), packets.end(), cursor, [](const LeanPacket& entry, std::size_t at) { return entry.cursor < at; });
    return static_cast<std::size_t>(found - packets.begin());
}

SegmentSlot& SegmentSlots::Take() {
    for (auto& slot : slots) {
        if (slot->busy) continue;
        slot->busy = true;
        ++slot->generation;
        return *slot;
    }
    slots.push_back(std::make_unique<SegmentSlot>());
    LeanFront::AddOnWorker(Counter::SlotsCreated);
    auto& slot = *slots.back();
    slot.busy = true;
    slot.generation = 1;
    return slot;
}

void Driver::scanSegment(const Submission& submission, PacketTable& table, bool validating, const std::uint32_t* guest) {
    table.Clear();
    const auto& commands = submission.commands;
    std::vector<std::size_t> guarded;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        if (validating) std::erase_if(guarded, [cursor](std::size_t end) { return end <= cursor; });
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) {
            ++cursor;
            continue;
        }
        const auto count = Pm4::PacketWords(header);
        const auto opcode = (header >> 8u) & 0xffu;
        if (validating) {
            bool valid = (header & 0xc0000000u) == 0xc0000000u && count <= commands.size() - cursor;
            if (valid) {
                try {
                    Pm4::Validate(std::span(commands).subspan(cursor, count), submission.queue);
                    if (header == FlipPacketHeader && !guarded.empty()) valid = false;
                    if (opcode == 0x22u) {
                        const auto end = submission.conditionalEnds.find(cursor);
                        if (end == submission.conditionalEnds.end()) valid = false;
                        else guarded.push_back(end->second);
                    }
                } catch (const std::exception&) {
                    valid = false;
                }
            }
            if (!valid) {
                validate(submission, guest);
                throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": the lean packet scan refused a packet today's validation accepts");
            }
        }
        require(count <= commands.size() - cursor, "truncated PM4 packet");
        table.packets.push_back(classify(std::span(commands).subspan(cursor, count), cursor));
        const bool type3 = (header >> 30u) == 3u;
        if (header == FlipPacketHeader || header == RenderingWaitPacketHeader || (type3 && (opcode == 0x49 || opcode == 0x37 || opcode == 0x3c || opcode == 0x93))) table.outputs.push_back(static_cast<std::uint32_t>(cursor));
        if (header == FlipPacketHeader) ++table.flipHeaders;
        cursor += count;
    }
    for (auto& entry : table.packets) {
        if (entry.opcode != 0x22 && !(entry.opcode == 0x3f && entry.Has(LeanPacket::Predicated))) continue;
        const auto end = submission.conditionalEnds.find(entry.cursor);
        if (end != submission.conditionalEnds.end()) entry.skip = static_cast<std::uint32_t>(table.IndexAt(end->second));
    }
}

void Driver::markOutputs(const Submission& submission, PacketTable& table) {
    const auto mark = [&](std::size_t cursor, std::uint32_t flag) {
        const auto index = table.IndexAt(cursor);
        if (index < table.packets.size() && table.packets[index].cursor == cursor) {
            table.packets[index].flags |= flag;
            table.packets[index].flags &= ~LeanPacket::Plain;
        }
    };
    for (const auto& [cursor, request] : submission.flips) mark(cursor, LeanPacket::Flip);
    for (const auto cursor : submission.heldAtSubmit) mark(cursor, LeanPacket::Held);
    table.built = true;
}

void Driver::verifyOutputs(const Submission& submission, const PacketTable& table) {
    const auto listed = [&](std::size_t cursor) { return std::binary_search(table.outputs.begin(), table.outputs.end(), static_cast<std::uint32_t>(cursor)); };
    for (const auto& [cursor, request] : submission.flips) {
        if (!listed(cursor)) LeanFront::NoteDifference(LeanFront::Field::Flips, where(submission, cursor) + ": today's walk reserved a flip the packet table does not list");
    }
    for (const auto& [cursor, wait] : submission.renderingWaits) {
        if (!listed(cursor)) LeanFront::NoteDifference(LeanFront::Field::RenderingWaits, where(submission, cursor) + ": today's walk captured a rendering wait the packet table does not list");
    }
    for (const auto cursor : submission.heldAtSubmit) {
        if (!listed(cursor)) LeanFront::NoteDifference(LeanFront::Field::HeldWaits, where(submission, cursor) + ": today's walk held a wait at submission the packet table does not list");
    }
    const auto writes = labelWritesAt(submission, table.outputs);
    if (writes != submission.labelWrites) {
        LeanFront::NoteDifference(LeanFront::Field::LabelWrites, "serial " + std::to_string(submission.serial) + ": today's walk noted " + std::to_string(submission.labelWrites.size()) + " label write dwords, the packet table's positions " + std::to_string(writes.size()));
    }
}

void Driver::verifySegmentTable(const Submission& submission, const PacketTable& table) {
    PacketTable fresh;
    scanSegment(submission, fresh);
    markOutputs(submission, fresh);
    std::size_t index = 0;
    const auto& commands = submission.commands;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) {
            ++cursor;
            continue;
        }
        const auto count = Pm4::PacketWords(header);
        const auto packet = std::span(commands).subspan(cursor, count);
        const auto opcode = (header >> 8u) & 0xffu;
        if (index >= table.packets.size()) {
            LeanFront::NoteDifference(LeanFront::Field::PacketTable, where(submission, cursor) + ": the packet table ends before this packet");
            return;
        }
        const auto& entry = table.packets[index];
        const auto expect = [&](bool same, const char* what) {
            if (!same) LeanFront::NoteDifference(LeanFront::Field::PacketTable, where(submission, cursor) + ": the packet table's " + what + " differs from today's reading");
        };
        expect(entry.cursor == cursor && entry.words == count && entry.header == header && entry.opcode == opcode, "position");
        expect(entry.Has(LeanPacket::Predicated) == Pm4::Predicated(header), "predication");
        expect(entry.Has(LeanPacket::Draw) == Pm4::DrawOpcode(opcode) && entry.Has(LeanPacket::IndirectDraw) == Pm4::IndirectDrawOpcode(opcode), "draw kind");
        expect(entry.Has(LeanPacket::FlipHeader) == (header == FlipPacketHeader) && entry.Has(LeanPacket::RenderingWait) == (header == RenderingWaitPacketHeader), "flip header");
        expect(entry.Has(LeanPacket::Flip) == submission.flips.contains(cursor), "flip reservation");
        expect(entry.Has(LeanPacket::Held) == submission.heldAtSubmit.contains(cursor), "held wait");
        expect(entry.Has(LeanPacket::KeepsFinisher) == PacketRules::keepsDrawFinisher(header, opcode), "finisher verdict");
        expect(entry.Has(LeanPacket::Seal) == PacketRules::sealsWriteOnly(packet, opcode), "seal");
        expect(entry.Has(LeanPacket::IndirectRegisters) == Pm4::IndirectRegisterOpcode(opcode), "register list kind");
        if (entry.pipeline == LeanPacket::Pipeline::Keeps || entry.pipeline == LeanPacket::Pipeline::Drains) {
            expect((entry.pipeline == LeanPacket::Pipeline::Keeps) == PacketRules::keepsDrawPipeline(packet, header, opcode), "committer verdict");
        }
        if (opcode == 0x22 || (opcode == 0x3f && Pm4::Predicated(header))) {
            const auto end = submission.conditionalEnds.find(cursor);
            const auto skip = end == submission.conditionalEnds.end() ? LeanPacket::NoSkip : static_cast<std::uint32_t>(table.IndexAt(end->second));
            expect(entry.skip == skip && (skip == LeanPacket::NoSkip || skip >= table.packets.size() || table.packets[skip].cursor >= end->second), "conditional end");
        }
        if (index < fresh.packets.size()) expect(entry.flags == fresh.packets[index].flags && entry.pipeline == fresh.packets[index].pipeline, "classes");
        ++index;
        cursor += count;
    }
    if (index != table.packets.size()) LeanFront::NoteDifference(LeanFront::Field::PacketTable, "serial " + std::to_string(submission.serial) + ": the packet table lists " + std::to_string(table.packets.size()) + " packets, today's walk " + std::to_string(index));
    if (table.outputs != fresh.outputs) LeanFront::NoteDifference(LeanFront::Field::PacketTable, "serial " + std::to_string(submission.serial) + ": the packet table's output positions differ from a fresh scan");
}

bool Driver::flushDue(FlushGate& gate, FlushInputs& inputs) {
    inputs.given = false;
    if (gate.everyPacket || !deferredLabels().labels.empty()) return true;
    inputs.pending = Graphics::Recorder::PendingLabelSince();
    inputs.completions = completionsPending();
    inputs.work = Graphics::Recorder::RecordedWorkSinceSubmit();
    inputs.reap = lastReapTries[0];
    inputs.given = true;
    if (gate.cap != 0 && inputs.work >= gate.cap) return true;
    const auto& pending = inputs.pending;
    const bool completions = inputs.completions;
    if (!pending.has_value() && !completions) return false;
    if (pending.has_value() && (!gate.labelArmed || gate.label != *pending)) {
        gate.label = *pending;
        gate.labelArmed = true;
        gate.labelDue = FlushTicksAt(*pending + labelFlushDeadline());
    }
    if (completions && (!gate.reapArmed || gate.reap != inputs.reap)) {
        gate.reap = inputs.reap;
        gate.reapArmed = true;
        gate.reapDue = FlushTicksAt(gate.reap + ReapInterval);
    }
    const auto now = FlushTicks();
    return (pending.has_value() && now >= gate.labelDue) || (completions && now >= gate.reapDue);
}

void Driver::checkFlush(const Submission& submission, std::size_t cursor, const FlushGate& gate, bool gateDue, std::uint32_t decision, const FlushInputs& inputs) {
    using LeanFront::Allowed;
    using LeanFront::Field;
    const bool acts = (decision & FlushDecision::Acts) != 0;
    if (!acts) {
        if (gateDue && decision == 0) LeanFront::NoteAllowed(Allowed::EarlyFlushChecks);
        return;
    }
    if (gateDue) return;
    const auto late = [&](std::chrono::steady_clock::time_point due) { return std::chrono::duration_cast<std::chrono::nanoseconds>(inputs.now - due).count(); };
    const auto detail = [&](const char* what, std::int64_t lateNs) { return where(submission, cursor) + ": today's flush " + what + " where the lean deadline gate skipped the check" + (lateNs >= 0 ? " (" + std::to_string(lateNs) + " ns past the deadline)" : std::string()); };
    if ((decision & (FlushDecision::Record | FlushDecision::RecordAtLock)) != 0) {
        LeanFront::NoteDifference(Field::FlushRecords, detail("records queued labels", -1));
    } else if ((decision & FlushDecision::Capped) != 0) {
        if (Graphics::RuntimeSwitches::Generation() != gate.capGeneration) LeanFront::NoteAllowed(Allowed::CapMoved);
        else LeanFront::NoteDifference(Field::FlushCaps, detail("submits at the batch cap", -1));
    } else if ((decision & FlushDecision::Submit) != 0) {
        LeanFront::NoteDifference(Field::FlushSubmits, detail("submits a pending label", inputs.pending.has_value() ? late(*inputs.pending + labelFlushDeadline()) : -1));
    } else {
        LeanFront::NoteDifference(Field::FlushReaps, detail("reaps completions", late(inputs.reap + ReapInterval)));
    }
}

void Driver::releaseSegmentSlot(SegmentSlot& slot) {
    if (slot.prepared) {
        recycleLeanTail(slot.own);
        auto labelWrites = std::move(slot.own.labelWrites);
        labelWrites.clear();
        slot.own = Submission{};
        slot.own.labelWrites = std::move(labelWrites);
        slot.prepared = false;
    }
    slot.table.Clear();
    slot.mark = 0;
    slot.busy = false;
}

bool Driver::endLeanSegment(const Submission& submission, FlipHold& flipHold) {
    DrawFinisher::Get().Drain();
    PerformanceContext endContext(frameTiming());
    PerformanceTimer endTiming("Driver.EndSubmission");
    static const bool submitAtEnd = std::getenv("APS5_SUBMIT_AT_END") != nullptr;
    if (orderedCompletion() && deferredLabels().labels.empty()) {
        if (submission.rewindTail == nullptr) {
            flipHold.working = false;
            deferCompletion(submission);
            return false;
        }
        if ((std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(submission.rewindTail - 1)).load(std::memory_order_acquire) & 0x80000000u) != 0) {
            const bool holdsFlip = std::exchange(flipHold.working, false);
            DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::SubmitEnd, [this, holdsFlip, flipWork = submission.flipWork, progress = submission.progress] {
                if (holdsFlip) finishFlipWork(flipWork, submitFlipWork(0));
                else submitOpenWork(progress);
                wakeQueueZeroForCompletions();
            }, {});
            endTiming.Finish();
            return true;
        }
    }
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    if (!deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
        auto& costs = submissionCosts(submission.queue);
        if (!submitAtEnd && submission.rewindTail == nullptr && workerQueued() != nullptr && workerQueued()->load(std::memory_order_acquire) != 0) {
            ++costs.endSkipped;
            flipHold.Finish();
            return false;
        }
        ++costs.endSubmits;
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::End);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto localDevice = device.Load();

        recordDeferredLabels(localDevice.get(), submission.queue);
        if (localDevice != nullptr) localDevice->SubmitRecorded(true);
    }
    flipHold.Finish();
    endTiming.Finish();
    if (submission.rewindTail == nullptr) return false;
    if (submission.progress) submitOpenWork(submission.progress);
    return true;
}

void Driver::executeLean(Submission& first) {
    FlipHold flipHold{*this, first.queue, first.holdsFlip, first.holdsFlip, first.flipWork};
    const LeanFront::SegmentNesting nesting(&frontLatch);
    DrawPipeline::Active() = true;
    struct PipelineDrain {
        ~PipelineDrain() {
            if (std::uncaught_exceptions() == 0) return;
            try {
                DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
            } catch (...) {
            }
        }
    } pipelineDrain;
    QueueState* state = nullptr;
    {
        std::lock_guard lock(mutex);
        if (resetGraphics) {
            queues.erase(0);
            resetGraphics = false;
        }
        state = &queues[0];
    }
    struct ClearAhead {
        ~ClearAhead() { clearResolvedAhead(); }
    } clearAhead;
    struct HeldSlots {
        Driver& driver;
        std::vector<SegmentSlot*> slots;
        ~HeldSlots() {
            for (auto* slot : slots) driver.releaseSegmentSlot(*slot);
        }
    } held{*this, {}};
    struct SettleFinisher {
        ~SettleFinisher() {
            if (DrawFinisher::Busy()) LeanFront::AddOnWorker(Counter::SettlesWaited);
            DrawFinisher::Get().Settle();
        }
    } settleFinisher;
    struct PacketProfileStorage {};
    LeanRun run{HostThreadLocal<PacketProfile, PacketProfileStorage>(), PacketCounters::ThisThread(), {}, packetEpoch(), false, 0};
    run.gate.everyPacket = everyPacketFlush();
    held.slots.reserve(8);
    auto* slot = &segmentSlots.Take();
    held.slots.push_back(slot);
    Submission* segment = &first;
    bool verify = first.front == FrontMode::Lean ? first.verifySegment : LeanFront::Due(frontLatch, LeanFront::Item::Segment);
    bool head = true;
    for (;;) {
        FlipHold quiet{*this, segment->queue, false, false, nullptr};
        auto& hold = head ? flipHold : quiet;
        runLeanSegment(*segment, *state, *slot, hold, run, verify);
        const bool follows = endLeanSegment(*segment, hold);
        slot->mark = DrawFinisher::Get().Issued();
        if (!follows) break;
        const Submission& stalled = *segment;
        std::optional<PerformanceTimer> rewindTiming;
        if (timersWanted()) rewindTiming.emplace("Driver.Rewind");
        static const bool ring = ReadSubmissionRingSettings();
        if (ring) PrepareSubmissionRing(stalled.queue);
        const auto front = latchFront(stalled.queue, false);
        awaitRewindTail(stalled, front);
        if (front != FrontMode::Lean) {
            Submission tail{};
            const auto retiredTakes = prepareTail(stalled, tail, front, ring, nullptr, false);
            if (rewindTiming) rewindTiming->Finish();
            retireLabelTakes(tail.queue, retiredTakes);
            execute(tail);
            break;
        }
        auto& next = segmentSlots.Take();
        held.slots.push_back(&next);
        next.prepared = true;
        const bool verifyNext = LeanFront::Due(frontLatch, LeanFront::Item::Segment);
        const auto retiredTakes = prepareTail(stalled, next.own, front, ring, &next.table, verifyNext);
        next.own.slotGeneration = next.generation;
        if (rewindTiming) rewindTiming->Finish();
        retireLabelTakes(next.own.queue, retiredTakes);
        LeanFront::AddOnWorker(Counter::SegmentsLooped);
        const auto finished = DrawFinisher::Get().Front();
        std::size_t kept = 0;
        for (auto* heldSlot : held.slots) {
            if (heldSlot != &next && (!heldSlot->prepared || heldSlot->mark <= finished)) {
                releaseSegmentSlot(*heldSlot);
                continue;
            }
            held.slots[kept++] = heldSlot;
        }
        held.slots.resize(kept);
        if (kept > 1) LeanFront::AddOnWorker(Counter::SlotsHeld, kept - 1);
        segment = &next.own;
        slot = &next;
        verify = verifyNext;
        head = false;
    }
}

void Driver::runLeanSegment(Submission& submission, QueueState& queue, SegmentSlot& slot, FlipHold& flipHold, LeanRun& run, bool verify) {
    includeTimingSubmission(submission, true);
    DrawPipeline::Active() = true;
    if (!orderedCompletion()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    if (traceGpu()) std::fprintf(stderr, "[gpu] %.1f execute serial=%llu queue=0x%x dwords=%zu\n", TraceMs(), static_cast<unsigned long long>(submission.serial), submission.queue, submission.commands.size());
    PacketRules::dumpQueue(submission);
    ++run.packetProfile.submissions;
    const bool orderedAtSubmit = submission.rewindTail == nullptr && submission.enqueuedAt != std::chrono::steady_clock::time_point{};
    const auto submittedAt = orderedAtSubmit ? static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(submission.enqueuedAt.time_since_epoch()).count()) : 0;
    if (epochEach() || submission.enqueuedAt == std::chrono::steady_clock::time_point{} || submission.enqueuedAt > lastEpochBump()) {
        if (!epochEach()) noteReleasedTails(submission, FrontMode::Lean);
        bumpEpochSince(&EpochBumps::submissions, submittedAt);
    } else {
        raiseEpochSince(submittedAt);
    }
    clearResolvedAhead();
    gateOpened() = true;
    prepareLeanDraws();
    if (!slot.table.built) {
        scanSegment(submission, slot.table);
        markOutputs(submission, slot.table);
        LeanFront::AddOnWorker(Counter::PacketTables);
    }
    if (verify) {
        verifySegmentTable(submission, slot.table);
        if (submission.front != FrontMode::Lean) LeanFront::NoteChecked(LeanFront::Item::Segment);
    }
    run.gate.capGeneration = Graphics::RuntimeSwitches::Generation();
    run.gate.cap = batchCap();
    const auto skipped = run.gate.skipped;
    const auto calls = run.gate.calls;
    run.report = reportWanted();
    if (timersWanted()) runLeanPackets<true>(submission, queue, slot.table, flipHold, run);
    else runLeanPackets<false>(submission, queue, slot.table, flipHold, run);
    LeanFront::AddOnWorker(Counter::FlushChecksSkipped, run.gate.skipped - skipped);
    LeanFront::AddOnWorker(Counter::FlushCalls, run.gate.calls - calls);
}

template <bool Timers>
void Driver::runLeanPackets(Submission& submission, QueueState& queue, PacketTable& table, FlipHold& flipHold, LeanRun& run) {
    struct NoContext {
        explicit NoContext(FrameTiming*) {}
    };
    struct NoTimer {
        explicit NoTimer(const char*) {}
        void Mark(const char*, std::uint64_t = 0) {}
        void Finish() {}
        void FinishPacket(std::uint64_t, std::size_t, std::uint32_t, std::size_t) {}
    };
    using Context = std::conditional_t<Timers, PerformanceContext, NoContext>;
    using Timer = std::conditional_t<Timers, PerformanceTimer, NoTimer>;
    const bool report = run.report;
    const bool profilePackets = report && profileDraws();
    PacketHistory recent{submission.commands};
    auto& packetProfile = run.packetProfile;
    auto& packetCounters = run.packetCounters;
    struct SealState {
        bool drawn = PacketRules::drawnSinceSeal();
        ~SealState() { PacketRules::drawnSinceSeal() = drawn; }
    } seal;
    std::size_t listHint = 0;
    std::uint32_t tagged = NoTag;
    std::size_t nextSegment = 0;
    std::size_t visitFrom = 0;
    std::size_t bumpFrom = 0;
    auto* const executingPacket = workerPacket();
    for (std::size_t index = 0; index < table.packets.size();) {
        const std::size_t arrived = table.packets[index].cursor;
        if (run.packetEpoch) {
            for (std::size_t at = bumpFrom; at <= arrived; ++at) bumpEpoch(&EpochBumps::packets);
        }
        bumpFrom = arrived + 1;
        if (failed.load(std::memory_order_acquire) || stopping.load(std::memory_order_acquire) || (++run.packets & 63u) == 0) CheckFailure();
        bool refreshed = false;
        while (nextSegment < submission.indirectSegments.size() && submission.indirectSegments[nextSegment].start <= arrived) {
            const auto start = submission.indirectSegments[nextSegment].start;
            if (start >= visitFrom) {
                mapLeanLists(submission);
                if (refreshIndirectSegment(submission, nextSegment)) {
                    recent.commands = submission.commands;
                    scanSegment(submission, table);
                    markOutputs(submission, table);
                    LeanFront::AddOnWorker(Counter::PacketTables);
                    listHint = 0;
                    visitFrom = start;
                    bumpFrom = start + 1;
                    index = table.IndexAt(start);
                    refreshed = true;
                    break;
                }
            }
            ++nextSegment;
        }
        if (refreshed) continue;
        const LeanPacket entry = table.packets[index];
        const std::size_t cursor = entry.cursor;
        const std::uint32_t header = entry.header;
        const std::size_t count = entry.words;
        const auto& commands = submission.commands;
        const auto packet = std::span<const std::uint32_t>(commands).subspan(cursor, count);
        const std::uint32_t opcode = entry.opcode;
        if (executingPacket != nullptr) executingPacket->store((static_cast<std::uint64_t>(cursor + 1) << 8u) | opcode, std::memory_order_relaxed);
        auto next = index + 1;
        std::size_t nextCursor = cursor + count;
        const auto skipTo = [&](std::size_t& target) {
            target = submission.conditionalEnds.at(cursor);
            return entry.skip != LeanPacket::NoSkip ? static_cast<std::size_t>(entry.skip) : table.IndexAt(target);
        };
        struct Advance {
            std::size_t& index;
            std::size_t& visitFrom;
            std::size_t& bumpFrom;
            const std::size_t& next;
            const std::size_t& nextCursor;
            ~Advance() {
                index = next;
                visitFrom = nextCursor;
                bumpFrom = nextCursor;
            }
        } advance{index, visitFrom, bumpFrom, next, nextCursor};
        const bool predicated = entry.Has(LeanPacket::Predicated) && queue.predication.operation != 0;
        const bool flip = entry.Has(LeanPacket::Flip);
        struct TagReset {
            std::uint32_t& tagged;
            bool plain;
            ~TagReset() {
                if (!plain) tagged = NoTag;
            }
        } tagReset{tagged, entry.Has(LeanPacket::Plain) && !predicated};
        const bool verifyPacket = LeanFront::Due(frontLatch, LeanFront::Item::Packet);
        const auto pipelineVerdict = [&] {
            switch (entry.pipeline) {
                case LeanPacket::Pipeline::Keeps: return true;
                case LeanPacket::Pipeline::Drains: return false;
                default: return PacketRules::keepsDrawPipeline(packet, header, opcode);
            }
        };
        if (DrawFinisher::Busy() && !entry.Has(LeanPacket::KeepsFinisher)) {
            const bool queuesLabel = !predicated && !entry.Has(LeanPacket::FlipHeader) && !entry.Has(LeanPacket::RenderingWait) && !flip && (opcode == 0x49 || opcode == 0x37) && labelPacketQueues(packet, opcode);
            const bool heldLabelWait = !predicated && orderedWaits() && entry.Has(LeanPacket::Wait) && !entry.Has(LeanPacket::RenderingWait) && PacketRules::waitTakesPendingLabel(packet);
            const bool beside = DrawFinisher::Overlap() && opcode != 0x15 && (queuesLabel || heldLabelWait || pipelineVerdict());
            if (!beside) DrawFinisher::Get().Drain();
            if (DrawFinisher::Overlap()) {
                DrawFinisher::NoteDrain(DrawFinisher::DrainSite::Packet, !beside);
                if (!beside) DrawFinisher::NoteDrainedOpcode(entry.Has(LeanPacket::FlipHeader) ? 0xffu : opcode, pipelineVerdict());
            }
        }
        if constexpr (Timers) {
            if (APS5_ENABLE_TIMING_LOG && pendingFrameTiming == nullptr) includeTimingSubmission(submission, false);
        }
        Context timingContext(pendingFrameTiming.get());
        Timer timing("Driver.Packet");
        if (!predicated && entry.Has(LeanPacket::LabelIssue)) noteLabelIssue(packet, submission.queue);
        if (!predicated && !entry.Has(LeanPacket::FlipHeader) && !entry.Has(LeanPacket::RenderingWait) && !flip && (entry.Has(LeanPacket::Enqueues) || verifyPacket)) {
            if (enqueueLabelPacket(packet, opcode, submission.queue, releaseInterrupt(packet, opcode, false)) || enqueueDmaPacket(packet, opcode, submission.queue, queue) || enqueueConstantRamDump(packet, opcode, submission.queue, queue) || enqueueSampleDumpPacket(packet, opcode, submission.queue)) {
                if (!entry.Has(LeanPacket::Enqueues)) LeanFront::NoteDifference(LeanFront::Field::PacketTable, where(submission, cursor) + ": today's code queued the packet beside the pipeline, the packet table skips that check");
                if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
                traceLabel(packet, submission.queue);
                recent.Record(cursor);
                continue;
            }
        }
        if (!predicated && entry.Has(LeanPacket::Wait) && !entry.Has(LeanPacket::RenderingWait) && orderedWaits() && DrawPipeline::Queue0().Busy()) {
            const auto awaitedBytes = Pm4::WaitAwaitedBytes(packet);
            const std::uint64_t awaited = (packet[2] & (awaitedBytes == 8 ? ~7u : ~3u)) | (static_cast<std::uint64_t>(packet[3]) << 32u);
            const auto value = DrawPipeline::Queue0().PendingLabel(awaited, awaitedBytes);
            if (value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                ++waitOutcomes().fromRecorderSameQueue;
                gateOpened() = true;
                noteLabelPass(packet, submission.queue);
                if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
                recent.Record(cursor);
                continue;
            }
            if (!DrawPipeline::Queue0().Overlaps(awaited, awaitedBytes) && Pm4::WaitSatisfiedUnchecked(packet) && !OwnStoreRejects(packet, awaited, awaitedBytes, submission.queue)) {
                bumpWaitEpoch(awaited, awaitedBytes, storeOrderingSince(packet, awaited, awaitedBytes, submission.received, submission.queue));
                ++waitOutcomes().atEntry;
                gateOpened() = true;
                noteLabelPass(packet, submission.queue);
                if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
                recent.Record(cursor);
                continue;
            }
        }
        timing.Mark("pipeline_prepare");
        if (DrawPipeline::Queue0().Busy()) {
            bool keeps = false;
            if (verifyPacket) {
                keeps = PacketRules::keepsDrawPipeline(packet, header, opcode);
                if ((entry.pipeline == LeanPacket::Pipeline::Keeps || entry.pipeline == LeanPacket::Pipeline::Drains) && keeps != (entry.pipeline == LeanPacket::Pipeline::Keeps)) {
                    LeanFront::NoteDifference(LeanFront::Field::PipelineVerdicts, where(submission, cursor) + ": today's committer verdict " + (keeps ? "keeps" : "drains") + " the pipeline, the packet table's static verdict does not");
                }
            } else {
                keeps = pipelineVerdict();
            }
            if (!keeps) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, entry.Has(LeanPacket::FlipHeader) ? 0xffu : opcode);
        }
        if (predicated && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().Overlaps(queue.predication.address, Pm4::PredicationBytes(queue.predication))) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, opcode);
        timing.Mark("pipeline_drain");
        std::shared_lock deviceUse(deviceReplacement, std::defer_lock);
        if (!entry.Has(LeanPacket::Wait) && !entry.Has(LeanPacket::RenderingWait) && !entry.Has(LeanPacket::FlipHeader)) deviceUse.lock();
        timing.Mark("device_lock");
        const std::uint32_t tag = entry.Has(LeanPacket::FlipHeader) ? 0xffffu : opcode;
        if (tag != tagged) {
            GuestMemory::SetCurrentPacket(tag, submission.queue);
            tagged = tag;
        }
        if (report) CaptureTrace::Log("packet submission=%llu queue=%x offset=%zu header=%08x words=%zu", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, header, packet.size());
        if (predicated) {
            landPendingWaits(submission.queue);
            recordQueuedLabelsBeforeRead(submission.queue);
            const bool passes = Pm4::PredicationPasses(queue);
            publishReadPoint();
            if (!passes) {
                if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
                if (opcode == 0x3f) next = skipTo(nextCursor);
                continue;
            }
            if (entry.Has(LeanPacket::LabelIssue)) noteLabelIssue(packet, submission.queue);
        }
        if (opcode == 0x3f) {
            if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
            continue;
        }

        const auto flushStart = profilePackets ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        const bool labelPacket = opcode == 0x49 || opcode == 0x37;
        FlushInputs flushInputs;
        if (verifyPacket) {
            const auto before = std::chrono::steady_clock::now();
            const bool gateDue = flushDue(run.gate, flushInputs);
            flushInputs.now = before;
            const auto decision = flushBetweenPackets(submission.queue, header, labelPacket, flushInputs.given ? &flushInputs : nullptr);
            tagged = NoTag;
            ++run.gate.calls;
            checkFlush(submission, cursor, run.gate, gateDue, decision, flushInputs);
        } else if (flushDue(run.gate, flushInputs)) {
            if (flushInputs.given && (flushInputs.pending.has_value() || flushInputs.completions)) flushInputs.now = std::chrono::steady_clock::now();
            flushBetweenPackets(submission.queue, header, labelPacket, flushInputs.given ? &flushInputs : nullptr);
            tagged = NoTag;
            ++run.gate.calls;
        } else {
            ++run.gate.skipped;
        }
        timing.Mark("flush_between_packets");
        std::optional<PacketTimer> packetTimer;
        if (profilePackets) {
            packetTimer.emplace(true, entry.Has(LeanPacket::FlipHeader) ? 0xffffu : opcode, submission.queue, packetProfile, std::chrono::steady_clock::now());
            packetStartedAt() = packetTimer->start;
            pendingDispatchPhases() = {};
            pendingDrawPhases() = {};
            packetProfile.flushMs += std::chrono::duration<double, std::milli>(packetTimer->start - flushStart).count();
        }
        const auto finishDispatchPacket = [&](bool indirect) {
            if (!profilePackets) return;
            const auto now = std::chrono::steady_clock::now();
            auto& pending = pendingDispatchPhases();
            if (pending.phases) {
                pending.ms[PhaseEpilogue] = std::chrono::duration<double, std::milli>(now - pending.tailAt).count();
                addDriverPhases(indirect ? Queue0Indirect : Queue0Direct, pending.ms, pending.hit, pending.validated);
            }
            if (indirect) return;
            auto& row = packetProfile.dispatchOutcomes[static_cast<std::size_t>(pending.outcome)];
            ++row.first;
            row.second += std::chrono::duration<double, std::milli>(now - packetTimer->start).count();
        };
        const auto finishDrawPacket = [&](bool drawn) {
            if (!profilePackets) return;
            const auto now = std::chrono::steady_clock::now();
            auto& pending = pendingDrawPhases();
            std::array<double, DrawDriverPhaseCount> ms{};
            if (drawn && pending.phases) {
                ms = pending.ms;
                ms[DrawRowEpilogue] = std::chrono::duration<double, std::milli>(now - pending.tailAt).count();
            } else {
                ms[DrawRowSkipped] = std::chrono::duration<double, std::milli>(now - packetTimer->start).count();
            }
            addDrawPhases(ms, drawn && pending.phases, pending.captures);
        };

        bool wroteOnGpu = false, endOfPipeInterrupt = false, interruptDeferred = false, drawPacket = false, sampleDump = false, drains = false;
        if (entry.Has(LeanPacket::Memory) || verifyPacket) {
            drains = preparePacketMemory(submission, queue, packet, header, opcode, releaseInterrupt(packet, opcode, flip), wroteOnGpu, endOfPipeInterrupt, interruptDeferred, drawPacket, sampleDump);
            if (!entry.Has(LeanPacket::Memory) && (drains || wroteOnGpu || endOfPipeInterrupt || interruptDeferred || sampleDump || drawPacket != entry.Has(LeanPacket::Draw))) {
                LeanFront::NoteDifference(LeanFront::Field::PacketTable, where(submission, cursor) + ": today's packet memory step acted on a packet the packet table skips it for");
            }
        } else {
            drawPacket = entry.Has(LeanPacket::Draw);
        }
        if (entry.Has(LeanPacket::Traced)) traceLabel(packet, submission.queue);
        timing.Mark("prepare_memory");

        const bool waitPacket = entry.Has(LeanPacket::Wait) || entry.Has(LeanPacket::RenderingWait);
        struct Progress {
            Driver& driver;
            bool counted;
            ~Progress() {
                if (!counted) return;
                --driver.packetsInFlight;
                ++driver.packetsDone;
            }
        } progress{*this, !waitPacket};
        if (!waitPacket) ++packetsInFlight;
        recent.Record(cursor);
        if (entry.Has(LeanPacket::RenderingWait)) {
            timed(&WorkerProfile::waitMs, [&] { submission.renderingWaits.at(cursor)->Wait(); });
        } else if (flip) {
            if (opcode == 0x49) {
                if (!wroteOnGpu) {
                    Pm4::Execute(packet, queue);
                    if (const auto label = Pm4::DecodeLabelWrite(packet)) noteLabelStore(label->address, label->Bytes(), ++eventSerial, submission.queue);
                }
            }
            if (deviceUse.owns_lock()) deviceUse.unlock();
            CheckFailure();
            std::uint64_t batchesAtFlip = 0, unsignaledAtFlip = 0;
            Graphics::StorageTexture::NoteFlip();
            if (!drains) {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                const auto localDevice = device.Load();
                if (localDevice != nullptr) static_cast<void>(Graphics::StorageTexture::StoreDrawnLinear("flip"));
                if (!recordLabelsForPacket(localDevice.get(), submission.queue) && localDevice != nullptr) localDevice->SubmitRecorded(true);
                if (localDevice != nullptr) localDevice->FlipBatches(batchesAtFlip, unsignaledAtFlip);
            }
            ++flipsCounted;
            Graphics::NoteHostImportFrame();
            if (batchesAtFlip != 0) flipSerial = batchesAtFlip;
            flipBatchesUnsignaled += unsignaledAtFlip;
            if (Graphics::AbConfigured()) {
                abAtFlip();
                run.gate.capGeneration = Graphics::RuntimeSwitches::Generation();
                run.gate.cap = batchCap();
            }
            timing.Mark("flip_flush");
            timing.FinishPacket(submission.serial, cursor, 0xffffu, count);
            const bool queueTiming = APS5_ENABLE_TIMING_LOG;
            auto frame = queueTiming ? std::move(pendingFrameTiming) : std::make_shared<FrameTiming>(++frameSerial);
            require(frame != nullptr, "flip has no frame timing");
            const auto now = FrameTiming::Clock::now();
            if (!queueTiming) frame->IncludeSubmission(submission.serial, now, now, now, true);
            frame->SetFlip(submission.serial, cursor, queueTiming ? submission.receivedAt : now, now);
            if (queueTiming) frame->CollectBackground();
            frame->NoteFlipBatches(batchesAtFlip, unsignaledAtFlip);
            if (report) CaptureTrace::Log("flip frame=%llu submission=%llu offset=%zu batch=%llu unsignaled=%llu", static_cast<unsigned long long>(frameSerial.load()), static_cast<unsigned long long>(submission.serial), cursor, static_cast<unsigned long long>(batchesAtFlip), static_cast<unsigned long long>(unsignaledAtFlip));
            struct FlipReadying {
                Driver& driver;
                std::uint32_t handle;
                FlipReadying(Driver& owner, std::uint32_t flipHandle) : driver(owner), handle(flipHandle) { driver.beginFlipReady(handle); }
                FlipReadying(const FlipReadying&) = delete;
                FlipReadying& operator=(const FlipReadying&) = delete;
                ~FlipReadying() { driver.endFlipReady(handle); }
            } readying(*this, submission.flipHandles.at(cursor));
            flipHold.Release();
            submission.flips.at(cursor)->GpuReady(frame);
        } else if (opcode == 0x15 || opcode == 0x16) {
            packetCounters.CountDispatch();
            landPendingWaits(submission.queue);
            currentPacketOffset() = cursor;
            if (gateOpened()) {
                gateOpened() = false;
                if (!pipelineDispatch()) timed(&WorkerProfile::dispatchMs, [&] { resolveGroupAhead(submission, queue, cursor); });
            }
            if (opcode == 0x15) timed(&WorkerProfile::dispatchMs, [&] { dispatch(queue, packet, submission); });
            else timed(&WorkerProfile::dispatchMs, [&] { dispatchIndirect(queue, packet, submission); });
            Pm4::NoteDispatchRegisters(packet, queue);
            Graphics::Recorder::CountRecordedWork();
            finishDispatchPacket(opcode == 0x16);
        } else if (entry.Has(LeanPacket::Wait)) {
            const auto waitStart = traceGpu() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            timed(&WorkerProfile::waitMs, [&] { waitMemory(packet, submission.queue, recent, submission.received, entry.Has(LeanPacket::Held)); });
            noteLabelPass(packet, submission.queue);
            gateOpened() = true;
            if (traceGpu() && std::chrono::steady_clock::now() - waitStart > std::chrono::milliseconds(200)) {
                for (std::size_t following = cursor + count, shown = 0; following < commands.size() && shown < 48; ++shown) {
                    const auto followingHeader = commands[following];
                    const auto followingCount = Pm4::PacketWords(followingHeader);
                    const auto followingPacket = std::span(commands).subspan(following, followingCount);
                    std::fprintf(stderr, "[gpu]   then %s", Pm4::Name(followingHeader).c_str());
                    for (std::size_t i = 1; i < followingPacket.size() && i < 7; ++i) std::fprintf(stderr, " %08x", followingPacket[i]);
                    std::fprintf(stderr, "\n");
                    following += followingCount;
                }
            }
        } else if (drawPacket) {
            landPendingWaits(submission.queue);
            packetCounters.CountDraw();
            seal.drawn = true;
            bool drawn = false;
            timed(&WorkerProfile::drawMs, [&] {
                const bool profile = profileDraws();
                const auto color = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                const auto countSkip = [&](Graphics::DrawSkip kind) {
                    if (profile) Graphics::CountDrawSkip(kind, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count());
                };
                try {
                    std::string rejected;
                    const auto verdict = drawLean(queue, packet, submission, cursor, rejected);
                    drawn = verdict == DrawVerdict::Drawn;
                    CaptureTrace::Log("draw submission=%llu queue=%x offset=%zu target=%llx mask=%x verdict=%d reason=%.256s", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e), static_cast<int>(verdict), rejected.c_str());
                    if (verdict == DrawVerdict::Rejected) {
                        countSkip(Graphics::DrawSkip::Prechecked);
                        throw std::runtime_error(rejected);
                    } else if (verdict == DrawVerdict::Nothing) {
                        countSkip(Graphics::DrawSkip::Nothing);
                    } else if (traceDraws()) {
                        std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x ok\n", static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e));
                    }
                } catch (const DrawCompilePending&) {
                    countSkip(Graphics::DrawSkip::Thrown);
                } catch (const RefusedDraw& refused) {
                    CaptureTrace::Log("draw-refused submission=%llu offset=%zu reason=%.256s", static_cast<unsigned long long>(submission.serial), cursor, refused.what());
                    countSkip(Graphics::DrawSkip::Thrown);
                    ReportRefusedDraw(refused);
                } catch (const std::exception& error) {
                    CaptureTrace::Log("draw-error submission=%llu offset=%zu reason=%.256s", static_cast<unsigned long long>(submission.serial), cursor, error.what());
                    countSkip(Graphics::DrawSkip::Thrown);
                    throw;
                }
            });
            finishDrawPacket(drawn);
        } else if (opcode == 0x22) {
            landPendingWaits(submission.queue);
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto condition = Pm4::ReadCondition(packet);
            if (condition == 0) next = skipTo(nextCursor);
            if (traceGpu()) std::fprintf(stderr, "[gpu] %.1f queue 0x%x COND_EXEC at DWORD %zu reads 0x%x at 0x%llx: %s %zu dwords\n", TraceMs(), submission.queue, cursor, condition, static_cast<unsigned long long>(packet[1] | (static_cast<std::uint64_t>(packet[2]) << 32u)), condition == 0 ? "skips" : "executes", submission.conditionalEnds.at(cursor) - cursor - count);
        } else if (opcode == 0x91) {
            landPendingWaits(submission.queue);
            recordQueuedLabelsBeforeRead(submission.queue);
            Pm4::Execute(packet, queue);
        } else if (sampleDump && !wroteOnGpu) {
            dumpSampleCounters(Pm4::SampleDumpAddress(packet));
        } else if (entry.Has(LeanPacket::Seal)) {
            if (std::exchange(seal.drawn, false)) {
                const auto sealPass = [this] {
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    if (const auto localDevice = device.Load()) localDevice->SealPassWriteOnly();
                };
                if (DrawPipeline::Queue0().Busy()) DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::Seal, sealPass, {});
                else sealPass();
            }
        } else if (opcode != 0x42 && opcode != 0x46 && opcode != 0x58) {
            if (!wroteOnGpu) {
                if (entry.Has(LeanPacket::IndirectRegisters)) {
                    const auto list = RegisterListAt(submission, cursor, listHint);
                    const auto listed = list.pairs;
                    if (list.valid) {
                        Pm4::ExecuteIndirectRegisters(packet, listed, queue);
                    } else {
                        landPendingWaits(submission.queue);
                        recordQueuedLabelsBeforeRead(submission.queue);
                        const auto current = Pm4::ReadIndirectRegisters(packet);
                        static std::atomic<bool> reported{false};
                        if (!reported.exchange(true)) {
                            std::uint32_t invalid = 0;
                            for (std::size_t i = 0; i < listed.size(); i += 2) {
                                if (!Pm4::IndirectRegistersValid(std::span(&listed[i], 1))) {
                                    invalid = listed[i];
                                    break;
                                }
                            }
                            std::fprintf(stderr, "[gpu] queue 0x%x %s at DWORD %zu: register list entry with offset dword 0x%08x read again where the packet executes%s\n", submission.queue, Pm4::Name(packet[0]).c_str(), cursor, invalid, skipInvalidLists() ? "; invalid entries are skipped (APS5_SKIP_INVALID_INDIRECT_REGISTERS)" : "");
                        }
                        Pm4::ExecuteIndirectRegisters(packet, current, queue, skipInvalidLists());
                    }
                } else {
                    Pm4::Execute(packet, queue);
                }
                if (labelPacket) {
                    if (const auto label = Pm4::DecodeLabelWrite(packet)) noteLabelStore(label->address, label->Bytes(), ++eventSerial, submission.queue);
                }
            }
            if (endOfPipeInterrupt && !interruptDeferred) deliverGuestEopInterrupt(submission.queue, packet[7] & 0x0fffffffu);
        }
        if (drawPacket || (sampleDump && wroteOnGpu)) Graphics::Recorder::CountRecordedWork();
        timing.Mark(entry.Has(LeanPacket::RenderingWait) ? "rendering_wait" : entry.Has(LeanPacket::Wait) ? "memory_wait" : opcode == 0x15 || opcode == 0x16 ? "dispatch" : drawPacket ? "draw" : "execute");
        timing.FinishPacket(submission.serial, cursor, opcode, count);
        if (verifyPacket) LeanFront::NoteChecked(LeanFront::Item::Packet);
    }
}

template void Driver::runLeanPackets<true>(Submission&, QueueState&, PacketTable&, FlipHold&, LeanRun&);
template void Driver::runLeanPackets<false>(Submission&, QueueState&, PacketTable&, FlipHold&, LeanRun&);

}
