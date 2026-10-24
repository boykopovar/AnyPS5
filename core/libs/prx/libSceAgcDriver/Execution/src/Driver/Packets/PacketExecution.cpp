#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <shared_mutex>
#include <utility>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::uint32_t eopSelfSignalFlipLimit = 4;

bool orderedFills() {
    static const bool ordered = std::getenv("APS5_PIPELINE_DRAIN_FILLS") == nullptr;
    return ordered;
}

bool pipelineDma() {
    static const bool enabled = std::getenv("APS5_PIPELINE_DMA") != nullptr;
    return enabled;
}

bool keepsDrawFinisher(std::uint32_t header, std::uint32_t opcode) {
    if (header == FlipPacketHeader || header == RenderingWaitPacketHeader) return false;
    if (Pm4::DrawOpcode(opcode)) return true;
    switch (opcode) {
        case 0x13: case 0x26: case 0x2a: case 0x2f: case 0x69: case 0x76: case 0x79: return true;
        case 0x10: return ((header >> 2u) & 0x3fu) == 0;
        default: return false;
    }
}

bool waitTakesPendingLabel(std::span<const std::uint32_t> packet) {
    if (packet.size() < 5) return false;
    const auto awaitedBytes = Pm4::WaitAwaitedBytes(packet);
    const std::uint64_t awaited = (packet[2] & (awaitedBytes == 8 ? ~7u : ~3u)) | (static_cast<std::uint64_t>(packet[3]) << 32u);
    const auto value = DrawPipeline::Queue0().PendingLabel(awaited, awaitedBytes);
    return value.has_value() && Pm4::WaitComparesValue(packet, *value);
}

bool keepsDrawPipeline(std::span<const std::uint32_t> packet, std::uint32_t header, std::uint32_t opcode) {
    if (header == FlipPacketHeader || header == RenderingWaitPacketHeader) return false;
    if (Pm4::DrawOpcode(opcode)) return true;
    switch (opcode) {
        case 0x11: case 0x12: case 0x13: case 0x20: case 0x26: case 0x2a: case 0x2f: case 0x42: case 0x58: case 0x59: case 0x69: case 0x76: case 0x79: case 0x7a: case 0x81: return true;
        case 0x46: return packet.size() >= 2 && (packet[1] & 0x3fu) != 0x39u;
        case 0x15: return orderedFills();
        case 0x63: case 0x64: case 0x9f:
            return packet.size() >= 5 && !DrawPipeline::Queue0().Overlaps(packet[1] | (static_cast<std::uint64_t>(packet[2]) << 32u), static_cast<std::size_t>(packet[4]) * 8);
        case 0x50: {
            if (packet.size() < 7) return false;
            const auto destination = Pm4::DmaDestination(packet);
            if (destination == Pm4::DmaDestinationNowhere) return true;
            if (!pipelineDma()) return false;
            if (destination == Pm4::DmaSelectGds) return true;
            const auto bytes = Pm4::DmaBytes(packet);
            if (bytes == 0) return true;
            const auto source = Pm4::DmaSource(packet);
            if ((source == 0 || source == 3) && DrawPipeline::Queue0().Overlaps(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u), bytes)) return false;
            const auto address = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u);
            return !DrawPipeline::Queue0().Overlaps(address, bytes);
        }
        case 0x3c: case 0x93: {
            if (packet.size() < 5) return false;
            const std::uint64_t awaited = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
            const auto bytes = Pm4::WaitAwaitedBytes(packet);
            if (DrawPipeline::Queue0().Overlaps(awaited, bytes)) return false;
            GuestMemory::CheckRange(reinterpret_cast<const void*>(awaited), bytes, bytes);
            return Pm4::WaitSatisfiedUnchecked(packet);
        }
        case 0x10: {
            const auto operation = (header >> 2u) & 0x3fu;
            return operation == 0 || operation == 0x09 || operation == 0x0b || operation == 0x0c || operation == 0x1a;
        }
        default: return false;
    }
}

bool sealsWriteOnly(std::span<const std::uint32_t> packet, std::uint32_t opcode) {
    if (opcode == 0x58) return true;
    if (opcode != 0x46 || packet.size() < 2) return false;
    switch (packet[1] & 0x3fu) {
        case 0x07: case 0x0f: case 0x10: case 0x16: case 0x2a: case 0x2c: case 0x2e: case 0x31: return true;
        default: return false;
    }
}

bool& drawnSinceSeal() {
    static thread_local bool drawn = false;
    return drawn;
}

}

void Driver::deliverGuestEopInterrupt(std::uint32_t queue) {
    if (!AgcDriverDeliverEopInterrupt(queue)) return;
    std::lock_guard<std::mutex> lock(eopSelfSignalMutex);
    eopSelfSignalFlips[queue] = 0;
}

void Driver::deliverFlipEopInterrupt(std::uint32_t queue) {
    {
        std::lock_guard<std::mutex> lock(eopSelfSignalMutex);
        const auto latched = eopSelfSignalFlips.find(queue);
        if (latched != eopSelfSignalFlips.end()) {
            if (++latched->second <= eopSelfSignalFlipLimit) return;
            eopSelfSignalFlips.erase(latched);
        }
    }
    AgcDriverDeliverEopInterrupt(queue);
}

template <typename TWork>
void Driver::timed(double WorkerProfile::*bucket, TWork&& work) {
    static const bool report = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!report) {
        work();
        return;
    }
    static thread_local WorkerProfile profile;
    const auto begin = std::chrono::steady_clock::now();
    work();
    const auto end = std::chrono::steady_clock::now();
    profile.*bucket += std::chrono::duration<double, std::milli>(end - begin).count();
    if (end - profile.reported > std::chrono::seconds(10)) {
        profile.reported = end;
        AgcDriver::ProfilePrint_nid_no_patch( "[gpu] worker at %.0f s: dispatch %.1f s, draw %.1f s, wait %.1f s\n", std::chrono::duration<double>(end - profile.start).count(), profile.dispatchMs / 1000, profile.drawMs / 1000, profile.waitMs / 1000);
    }
    if (report && bucket != &WorkerProfile::waitMs) PerformanceCounters::AddDriverNanoseconds(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
}

void Driver::execute(const Submission& submission) {
    auto* submissionTiming = includeTimingSubmission(submission, true);
    struct FlipHold {
        Driver& driver;
        bool held;
        void Release() {
            if (!held) return;
            held = false;
            driver.releaseFlipHold();
        }
        ~FlipHold() { Release(); }
    } flipHold{*this, submission.holdsFlip};
    const bool pipelined = submission.queue == 0 && DrawPipeline::Depth() != 0;
    static const bool orderedWaits = std::getenv("APS5_PIPELINE_DRAIN_LABELS") == nullptr;
    DrawPipeline::Active() = pipelined;
    struct PipelineDrain {
        bool pipelined;
        ~PipelineDrain() {
            if (!pipelined || std::uncaught_exceptions() == 0) return;
            try {
                DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
            } catch (...) {
            }
        }
    } pipelineDrain{pipelined};
    static const bool orderedCompletion = std::getenv("APS5_PIPELINE_DRAIN_SUBMISSIONS") == nullptr && orderedWaits;
    if (pipelined && (submission.suspend || !orderedCompletion)) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    if (submission.suspend) {
        PerformanceContext timingContext(submissionTiming);
        PerformanceTimer timing("Driver.Suspend");

        static const bool suspendDrain = std::getenv("APS5_SUSPEND_DRAIN") != nullptr;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        ++suspendPoints;
        auto& costs = submissionCosts(submission.queue);

        if (suspendDrain || !deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
            const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            const auto localDevice = device.Load();
            recordDeferredLabels(localDevice.get(), submission.queue);
            if (localDevice != nullptr) {
                if (suspendDrain) localDevice->WaitIdle();
                else localDevice->SubmitRecorded(submission.queue == 0);
            }
            ++costs.suspends;
            if (profile) costs.suspendNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
        } else {
            ++costs.suspendsSkipped;
        }
        resetGraphics = true;
        return;
    }
    QueueState* state = nullptr;
    {
        std::lock_guard lock(mutex);
        if (submission.queue == 0 && resetGraphics) {
            queues.erase(0);
            resetGraphics = false;
        }
        state = &queues[submission.queue];
    }
    auto& queue = *state;
    static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
    if (traceGpu) std::fprintf(stderr, "[gpu] %.1f execute serial=%llu queue=0x%x dwords=%zu\n", TraceMs(), static_cast<unsigned long long>(submission.serial), submission.queue, submission.commands.size());

    static const long dumpQueue = [] { const char* text = std::getenv("APS5_DUMP_QUEUE"); return text ? std::strtol(text, nullptr, 16) : -1L; }();
    if (static_cast<long>(submission.queue) == dumpQueue) {

        static int dumped = 0;
        if (dumped++ < 40) {
            std::string text = "[queue] submission " + std::to_string(submission.serial) + ":\n";
            for (std::size_t cursor = 0; cursor < submission.commands.size();) {
                const auto header = submission.commands[cursor];
                const auto count = Pm4::PacketWords(header);
                char line[200];
                int length = std::snprintf(line, sizeof(line), "[queue]   %s", Pm4::Name(header).c_str());
                for (std::size_t i = 1; i < count && i < 10 && length < 180; ++i) length += std::snprintf(line + length, sizeof(line) - length, " %08x", submission.commands[cursor + i]);
                text += line;
                text += "\n";
                cursor += count;
            }
            std::fputs(text.c_str(), stderr);
        }
    }
    PacketHistory recent{submission.commands};

    static const bool profilePackets = std::getenv("APS5_PROFILE_DRAW") != nullptr;

    thread_local PacketProfile* packetProfileSlot = nullptr;
    auto& packetProfile = ShaderRecompiler::ThreadOwned(packetProfileSlot);
    ++packetProfile.submissions;
    auto& packetCounters = PacketCounters::ThisThread();

    static const bool epochEach = std::getenv("APS5_SUBMISSION_EPOCH_EACH") != nullptr;
    if (epochEach || submission.enqueuedAt == std::chrono::steady_clock::time_point{} || submission.enqueuedAt > lastEpochBump()) {
        if (!epochEach) noteReleasedTails(submission);
        bumpEpoch(&EpochBumps::submissions);
    }
    clearResolvedAhead();
    gateOpened() = true;
    struct ClearAhead {
        ~ClearAhead() { clearResolvedAhead(); }
    } clearAhead;
    struct SettleFinisher {
        bool queue0;
        ~SettleFinisher() {
            if (queue0) DrawFinisher::Get().Settle();
        }
    } settleFinisher{submission.queue == 0};
    for (std::size_t cursor = 0; cursor < submission.commands.size();) {
        if (packetEpoch()) bumpEpoch(&EpochBumps::packets);
        CheckFailure();
        const auto header = submission.commands[cursor];
        if (Pm4::FillerPacket(header)) { ++cursor; continue; }
        const auto count = Pm4::PacketWords(header);
        const auto packet = std::span(submission.commands).subspan(cursor, count);
        const auto opcode = (header >> 8u) & 0xffu;
        auto nextCursor = cursor + count;
        const bool predicated = Pm4::Predicated(header) && queue.predication.operation != 0;
        if (submission.queue == 0 && DrawFinisher::Busy() && !keepsDrawFinisher(header, opcode)) {
            const bool queuesLabel = pipelined && !predicated && header != FlipPacketHeader && header != RenderingWaitPacketHeader && !submission.flips.contains(cursor) && labelPacketQueues(packet, opcode);
            const bool heldLabelWait = pipelined && !predicated && orderedWaits && (opcode == 0x3c || opcode == 0x93) && header != RenderingWaitPacketHeader && waitTakesPendingLabel(packet);
            const bool beside = DrawFinisher::Overlap() && opcode != 0x15 && (queuesLabel || heldLabelWait || keepsDrawPipeline(packet, header, opcode));
            if (!beside) DrawFinisher::Get().Drain();
            if (DrawFinisher::Overlap()) {
                DrawFinisher::NoteDrain(DrawFinisher::DrainSite::Packet, !beside);
                if (!beside) DrawFinisher::NoteDrainedOpcode(header == FlipPacketHeader ? 0xffu : opcode, keepsDrawPipeline(packet, header, opcode));
            }
        }
        if (APS5_ENABLE_TIMING_LOG && submission.queue == 0 && pendingFrameTiming == nullptr) includeTimingSubmission(submission, false);
        PerformanceContext timingContext(submission.queue == 0 ? pendingFrameTiming.get() : FrameTiming::Async());
        PerformanceTimer timing("Driver.Packet");
        if (pipelined && !predicated && header != FlipPacketHeader && header != RenderingWaitPacketHeader && !submission.flips.contains(cursor) && (enqueueLabelPacket(packet, opcode, submission.queue) || enqueueDmaPacket(packet, opcode, submission.queue, queue))) {
            traceLabel(packet, submission.queue);
            recent.Record(cursor);
            cursor = nextCursor;
            continue;
        }
        if (pipelined && !predicated && (opcode == 0x3c || opcode == 0x93) && header != RenderingWaitPacketHeader && orderedWaits && DrawPipeline::Queue0().Busy()) {
            const auto awaitedBytes = Pm4::WaitAwaitedBytes(packet);
            const std::uint64_t awaited = (packet[2] & (awaitedBytes == 8 ? ~7u : ~3u)) | (static_cast<std::uint64_t>(packet[3]) << 32u);
            const auto value = DrawPipeline::Queue0().PendingLabel(awaited, awaitedBytes);
            if (value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                ++waitOutcomes().fromRecorderSameQueue;
                gateOpened() = true;
                recent.Record(cursor);
                cursor = nextCursor;
                continue;
            }
            if (!DrawPipeline::Queue0().Overlaps(awaited, awaitedBytes) && Pm4::WaitSatisfiedUnchecked(packet)) {
                bumpEpoch(&EpochBumps::waits);
                ++waitOutcomes().atEntry;
                gateOpened() = true;
                recent.Record(cursor);
                cursor = nextCursor;
                continue;
            }
        }
        timing.Mark("pipeline_prepare");
        if (pipelined && DrawPipeline::Queue0().Busy() && !keepsDrawPipeline(packet, header, opcode)) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, header == FlipPacketHeader ? 0xffu : opcode);
        if (pipelined && predicated && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().Overlaps(queue.predication.address, queue.predication.operation == 3 ? 8 : 4)) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, opcode);
        timing.Mark("pipeline_drain");
        std::shared_lock deviceUse(deviceReplacement, std::defer_lock);
        if (opcode != 0x3c && opcode != 0x93 && header != RenderingWaitPacketHeader && header != FlipPacketHeader) deviceUse.lock();

        timing.Mark("device_lock");
        GuestMemory::SetCurrentPacket(header == FlipPacketHeader ? 0xffffu : opcode, submission.queue);
        CaptureTrace::Log("packet submission=%llu queue=%x offset=%zu header=%08x words=%zu", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, header, packet.size());
        if (predicated) {
            landPendingWaits(submission.queue);
            recordQueuedLabelsBeforeRead(submission.queue);
            if (!Pm4::PredicationPasses(queue)) {
                cursor = opcode == 0x3f ? submission.conditionalEnds.at(cursor) : nextCursor;
                continue;
            }
        }
        if (opcode == 0x3f) {
            cursor = nextCursor;
            continue;
        }

        const auto flushStart = profilePackets ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        flushBetweenPackets(submission.queue, header, opcode == 0x49 || opcode == 0x37);
        timing.Mark("flush_between_packets");
        PacketTimer packetTimer{profilePackets, header == FlipPacketHeader ? 0xffffu : opcode, submission.queue, packetProfile, profilePackets ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}};

        if (profilePackets) {
            packetStartedAt() = packetTimer.start;
            pendingDispatchPhases() = {};
            pendingDrawPhases() = {};
        }
        const auto finishDispatchPacket = [&](bool indirect) {
            if (!profilePackets) return;
            const auto now = std::chrono::steady_clock::now();
            auto& pending = pendingDispatchPhases();
            if (pending.phases) {
                pending.ms[PhaseEpilogue] = std::chrono::duration<double, std::milli>(now - pending.tailAt).count();
                addDriverPhases(submission.queue != 0 ? OtherQueues : indirect ? Queue0Indirect : Queue0Direct, pending.ms, pending.hit, pending.validated);
            }
            if (indirect) return;
            auto& row = packetProfile.dispatchOutcomes[static_cast<std::size_t>(pending.outcome)];
            ++row.first;
            row.second += std::chrono::duration<double, std::milli>(now - packetTimer.start).count();
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
                ms[DrawRowSkipped] = std::chrono::duration<double, std::milli>(now - packetTimer.start).count();
            }
            addDrawPhases(ms, drawn && pending.phases, pending.captures);
        };
        if (profilePackets) packetProfile.flushMs += std::chrono::duration<double, std::milli>(packetTimer.start - flushStart).count();

        bool wroteOnGpu = false, endOfPipeInterrupt = false, interruptDeferred = false, drawPacket = false, sampleDump = false;
        const bool drains = preparePacketMemory(submission, queue, packet, header, opcode, wroteOnGpu, endOfPipeInterrupt, interruptDeferred, drawPacket, sampleDump);
        traceLabel(packet, submission.queue);
        timing.Mark("prepare_memory");

        const bool waitPacket = opcode == 0x3c || opcode == 0x93 || header == RenderingWaitPacketHeader;
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
        if (header == RenderingWaitPacketHeader) {
            timed(&WorkerProfile::waitMs, [&] { submission.renderingWaits.at(cursor)->Wait(); });
        } else if (submission.flips.contains(cursor)) {
            if (opcode == 0x49) {
                if (!wroteOnGpu) {
                    Pm4::Execute(packet, queue);
                    if (const auto label = Pm4::DecodeLabelWrite(packet)) noteLabelStore(label->address, label->Bytes(), ++eventSerial, submission.queue);
                }
                if (endOfPipeInterrupt && !interruptDeferred) deliverGuestEopInterrupt(submission.queue);
            }
            if (deviceUse.owns_lock()) deviceUse.unlock();
            CheckFailure();
            std::uint64_t batchesAtFlip = 0, unsignaledAtFlip = 0;
            bool flipInterruptDeferred = false;
            if (!drains) {

                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                const auto localDevice = device.Load();

                if (!recordLabelsForPacket(localDevice.get(), submission.queue) && localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
                if (localDevice != nullptr) {
                    localDevice->FlipBatches(batchesAtFlip, unsignaledAtFlip);
                    const auto queueId = submission.queue;
                    flipInterruptDeferred = localDevice->AfterRecordedWork([this, queueId] { deliverFlipEopInterrupt(queueId); }, false);
                }
            }
            if (!flipInterruptDeferred) deliverFlipEopInterrupt(submission.queue);
            ++flipsCounted;
            if (batchesAtFlip != 0) flipSerial = batchesAtFlip;
            flipBatchesUnsignaled += unsignaledAtFlip;

            timing.Mark("flip_flush");
            timing.FinishPacket(submission.serial, cursor, 0xffffu, count);
            const bool queueTiming = APS5_ENABLE_TIMING_LOG && submission.queue == 0;
            auto frame = queueTiming ? std::move(pendingFrameTiming) : std::make_shared<FrameTiming>(++frameSerial);
            require(frame != nullptr, "flip has no frame timing");
            const auto now = FrameTiming::Clock::now();
            if (!queueTiming) frame->IncludeSubmission(submission.serial, now, now, now, true);
            frame->SetFlip(submission.serial, cursor, queueTiming ? submission.receivedAt : now, now);
            if (queueTiming) frame->CollectBackground();
            frame->NoteFlipBatches(batchesAtFlip, unsignaledAtFlip);
            CaptureTrace::Log("flip frame=%llu submission=%llu offset=%zu batch=%llu unsignaled=%llu", static_cast<unsigned long long>(frameSerial.load()), static_cast<unsigned long long>(submission.serial), cursor, static_cast<unsigned long long>(batchesAtFlip), static_cast<unsigned long long>(unsignaledAtFlip));
            flipHold.Release();
            submission.flips.at(cursor)->GpuReady(frame);
        } else if (opcode == 0x15 || opcode == 0x16) {
            packetCounters.CountDispatch();
            landPendingWaits(submission.queue);
            currentPacketOffset() = cursor;
            if (gateOpened()) {
                gateOpened() = false;
                if (!pipelineDispatch() || submission.queue != 0) {
                    if (pipelined && DrawPipeline::Queue0().Busy()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, opcode);
                    timed(&WorkerProfile::dispatchMs, [&] { resolveGroupAhead(submission, queue, cursor); });
                }
            }
            if (opcode == 0x15) timed(&WorkerProfile::dispatchMs, [&] { dispatch(queue, packet, submission); });
            else timed(&WorkerProfile::dispatchMs, [&] { dispatchIndirect(queue, packet, submission); });
            Pm4::NoteDispatchRegisters(packet, queue);
            Graphics::Recorder::CountRecordedWork();
            finishDispatchPacket(opcode == 0x16);
        } else if (opcode == 0x3c || opcode == 0x93) {
            static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
            const auto waitStart = traceGpu ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            timed(&WorkerProfile::waitMs, [&] { waitMemory(packet, submission.queue, recent, submission.received, submission.heldAtSubmit.contains(cursor)); });
            gateOpened() = true;
            if (traceGpu && std::chrono::steady_clock::now() - waitStart > std::chrono::milliseconds(200)) {

                for (std::size_t next = cursor + count, shown = 0; next < submission.commands.size() && shown < 48; ++shown) {
                    const auto nextHeader = submission.commands[next];
                    const auto nextCount = Pm4::PacketWords(nextHeader);
                    const auto nextPacket = std::span(submission.commands).subspan(next, nextCount);
                    std::fprintf(stderr, "[gpu]   then %s", Pm4::Name(nextHeader).c_str());
                    for (std::size_t i = 1; i < nextPacket.size() && i < 7; ++i) std::fprintf(stderr, " %08x", nextPacket[i]);
                    std::fprintf(stderr, "\n");
                    next += nextCount;
                }
            }
        } else if (drawPacket) {
            landPendingWaits(submission.queue);
            packetCounters.CountDraw();
            drawnSinceSeal() = true;
            bool drawn = false;
            timed(&WorkerProfile::drawMs, [&] {
                static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
                static const bool profileDraws = std::getenv("APS5_PROFILE_DRAW") != nullptr;
                const auto color = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                const auto started = profileDraws ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                const auto countSkip = [&](Graphics::DrawSkip kind) {
                    if (profileDraws) Graphics::CountDrawSkip(kind, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count());
                };

                try {
                    std::string rejected;
                    const auto verdict = draw(queue, packet, submission, cursor, rejected);
                    drawn = verdict == DrawVerdict::Drawn;
                    CaptureTrace::Log("draw submission=%llu queue=%x offset=%zu target=%llx mask=%x verdict=%d reason=%.256s", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e), static_cast<int>(verdict), rejected.c_str());
                    if (verdict == DrawVerdict::Rejected) {
                        countSkip(Graphics::DrawSkip::Prechecked);
                        throw std::runtime_error(rejected);
                    } else if (verdict == DrawVerdict::Nothing) {
                        countSkip(Graphics::DrawSkip::Nothing);
                    } else if (traceDraws) {
                        std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x ok\n",static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e));
                    }
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
            if (condition == 0) nextCursor = submission.conditionalEnds.at(cursor);
            if (traceGpu) std::fprintf(stderr, "[gpu] %.1f queue 0x%x COND_EXEC at DWORD %zu reads 0x%x at 0x%llx: %s %zu dwords\n", TraceMs(), submission.queue, cursor, condition, static_cast<unsigned long long>(packet[1] | (static_cast<std::uint64_t>(packet[2]) << 32u)), condition == 0 ? "skips" : "executes", submission.conditionalEnds.at(cursor) - cursor - count);
        } else if (opcode == 0x91) {
            landPendingWaits(submission.queue);
            recordQueuedLabelsBeforeRead(submission.queue);
            Pm4::Execute(packet, queue);
        } else if (sampleDump && !wroteOnGpu) {
            dumpSampleCounters(Pm4::SampleDumpAddress(packet));
        } else if (submission.queue == 0 && sealsWriteOnly(packet, opcode)) {
            if (std::exchange(drawnSinceSeal(), false)) {
                const auto seal = [this] {
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    if (const auto localDevice = device.Load()) localDevice->SealPassWriteOnly();
                };
                if (pipelined && DrawPipeline::Queue0().Busy()) DrawPipeline::Queue0().Enqueue(seal, {});
                else seal();
            }
        } else if (opcode != 0x42 && opcode != 0x46 && opcode != 0x58) {
            if (!wroteOnGpu) {
                if (Pm4::IndirectRegisterOpcode(opcode)) {
                    const auto& listed = submission.registerLists.at(cursor);
                    if (Pm4::IndirectRegistersValid(listed)) {
                        Pm4::ExecuteIndirectRegisters(packet, listed, queue);
                    } else {
                        landPendingWaits(submission.queue);
                        recordQueuedLabelsBeforeRead(submission.queue);
                        const auto current = Pm4::ReadIndirectRegisters(packet);
                        static const bool skipInvalid = std::getenv("APS5_SKIP_INVALID_INDIRECT_REGISTERS") != nullptr;
                        static std::atomic<bool> reported{false};
                        if (!reported.exchange(true)) {
                            std::uint32_t invalid = 0;
                            for (std::size_t i = 0; i < listed.size(); i += 2) {
                                if (!Pm4::IndirectRegistersValid(std::span(&listed[i], 1))) {
                                    invalid = listed[i];
                                    break;
                                }
                            }
                            std::fprintf(stderr, "[gpu] queue 0x%x %s at DWORD %zu: register list entry with offset dword 0x%08x read again where the packet executes%s\n", submission.queue, Pm4::Name(packet[0]).c_str(), cursor, invalid, skipInvalid ? "; invalid entries are skipped (APS5_SKIP_INVALID_INDIRECT_REGISTERS)" : "");
                        }
                        Pm4::ExecuteIndirectRegisters(packet, current, queue, skipInvalid);
                    }
                } else {
                    Pm4::Execute(packet, queue);
                }
                if (opcode == 0x49 || opcode == 0x37) {
                    if (const auto label = Pm4::DecodeLabelWrite(packet)) noteLabelStore(label->address, label->Bytes(), ++eventSerial, submission.queue);
                }
            }
            if (endOfPipeInterrupt && !interruptDeferred) deliverGuestEopInterrupt(submission.queue);
        }
        if (drawPacket || (sampleDump && wroteOnGpu)) Graphics::Recorder::CountRecordedWork();
        timing.Mark(header == RenderingWaitPacketHeader ? "rendering_wait" : opcode == 0x3c || opcode == 0x93 ? "memory_wait" : opcode == 0x15 || opcode == 0x16 ? "dispatch" : drawPacket ? "draw" : "execute");
        if (submission.queue == 0) timing.FinishPacket(submission.serial, cursor, opcode, count);
        else timing.Finish();
        cursor = nextCursor;
    }

    if (submission.queue == 0) DrawFinisher::Get().Drain();
    PerformanceContext endContext(submission.queue == 0 ? frameTiming() : FrameTiming::Async());
    PerformanceTimer endTiming("Driver.EndSubmission");
    static const bool submitAtEnd = std::getenv("APS5_SUBMIT_AT_END") != nullptr;
    if (pipelined && orderedCompletion && deferredLabels().labels.empty()) {
        if (submission.rewindTail == nullptr) {
            deferCompletion(submission);
            return;
        }
        if ((std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(submission.rewindTail - 1)).load(std::memory_order_acquire) & 0x80000000u) != 0) {
            DrawPipeline::Queue0().Enqueue([this] {
                submitOpenWork();
                wakeQueueZeroForCompletions();
            }, {});
            endTiming.Finish();
            executeRewindTail(submission);
            return;
        }
    }
    if (pipelined) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    if (!deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
        auto& costs = submissionCosts(submission.queue);
        if (!submitAtEnd && submission.rewindTail == nullptr && submission.queue == 0 && workerQueued() != nullptr && workerQueued()->load(std::memory_order_acquire) != 0) {
            ++costs.endSkipped;
            return;
        }
        ++costs.endSubmits;
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::End);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto localDevice = device.Load();

        recordDeferredLabels(localDevice.get(), submission.queue);
        if (localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
    }
    endTiming.Finish();
    if (submission.rewindTail != nullptr) executeRewindTail(submission);
}

}
