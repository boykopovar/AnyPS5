#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>

namespace AgcDriver::DriverDetail {
namespace {
void Driver::readRegisterLists(Submission& submission) {
    submission.registerLists.clear();
    for (std::size_t cursor = 0; cursor < submission.commands.size(); cursor += Pm4::PacketWords(submission.commands[cursor])) {
        const auto header = submission.commands[cursor];
        if ((header >> 30u) != 3u || !Pm4::IndirectRegisterOpcode((header >> 8u) & 0xffu)) continue;
        try {
            submission.registerLists.emplace(cursor, Pm4::ReadIndirectRegisters(std::span<const std::uint32_t>(submission.commands).subspan(cursor, Pm4::PacketWords(header))));
        } catch (const std::exception& error) {
            throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what());
        }
    }
}

void Driver::waitForFlipRoom(const Submission& submission) {
    for (std::size_t cursor = 0; cursor < submission.commands.size(); cursor += Pm4::PacketWords(submission.commands[cursor])) {
        if (submission.commands[cursor] != FlipPacketHeader) continue;
        std::shared_ptr<IVideoOutput> output;
        {
            std::lock_guard lock(mutex);
            const auto found = outputs.find(submission.commands[cursor + 1]);
            require(found != outputs.end(), "flip references an unregistered video output");
            output = found->second;
        }
        output->WaitForFlipRoom();
    }
}

bool Driver::awaitsTitle(std::uint64_t awaited) const {
    return runningWorkers.load(std::memory_order_acquire) == 0 && orderHolders.load(std::memory_order_acquire) == 0 && !completionsPending() && !Graphics::Recorder::SnapshotWriteOverlaps(awaited, 4);
}

bool Driver::stillAwaited(const AwaitedSlot& slot, std::uint64_t awaited) {
    const auto shape = slot.shape.load(std::memory_order_relaxed);
    if (shape == 0) return (*reinterpret_cast<const volatile std::uint32_t*>(awaited) & Pm4::RewindValid) == 0;
    const std::uint64_t value = shape >> 32u == 8 ? *reinterpret_cast<const volatile std::uint64_t*>(awaited) : *reinterpret_cast<const volatile std::uint32_t*>(awaited);
    return !Pm4::WaitCompares(static_cast<std::uint32_t>(shape), slot.reference.load(std::memory_order_relaxed), slot.mask.load(std::memory_order_relaxed), value);
}

bool Driver::parkedOnTitle() const {
    if (!queue0Uncommitted.empty() || DrawPipeline::Queue0().Busy() || DrawFinisher::Busy() || Graphics::Recorder::QueuedLabelsPending()) return false;
    bool parked = false;
    for (const auto& [queue, worker] : workers) {
        const auto& slot = awaitedSlots[queue == 0 ? 0 : queue - 0x20 + 1];
        const auto awaited = slot.address.load(std::memory_order_acquire);
        if (awaited == 0 ? !worker.pending.empty() : !awaitsTitle(awaited) || !stillAwaited(slot, awaited)) return false;
        parked = parked || awaited != 0;
    }
    return parked;
}

void Driver::noteAwaitingTitle(std::uint32_t queue, std::uint64_t awaited) {
    if (queue != 0 || flipHolders.load(std::memory_order_acquire) == 0 || queue0AwaitsTitle.load(std::memory_order_acquire) || !awaitsTitle(awaited)) return;
    std::lock_guard lock(mutex);
    queue0AwaitsTitle.store(true, std::memory_order_release);
    changed.notify_all();
}

bool Driver::holdFlipBehindWorker(const Submission& submission, bool cpuFlip) {
    if (submission.queue != 0 || onWorkerThread()) return false;
    bool candidate = false;
    for (std::size_t cursor = 0; cursor < submission.commands.size() && !candidate; cursor += Pm4::PacketWords(submission.commands[cursor])) {
        const auto words = std::span(submission.commands).subspan(cursor);
        candidate = words[0] == FlipPacketHeader || EopRelease(words);
    }
    if (!candidate) return false;
    std::unique_lock lock(mutex);
    bool flips = false;
    for (std::size_t cursor = 0; cursor < submission.commands.size() && !flips; cursor += Pm4::PacketWords(submission.commands[cursor])) {
        const auto words = std::span(submission.commands).subspan(cursor);
        flips = words[0] == FlipPacketHeader || (EopRelease(words) && eopFlips.contains(words[7]));
    }
    if (!flips) return false;
    flipHolders.fetch_add(1, std::memory_order_acq_rel);
    changed.wait(lock, [&] { return failure != nullptr || stopping.load(std::memory_order_acquire) || shutdownToken.stop_requested() || flipsAhead.load(std::memory_order_acquire) == 0 || queue0AwaitsTitle.load(std::memory_order_acquire); });
    flipHolders.fetch_sub(1, std::memory_order_acq_rel);
    return !cpuFlip && !(failure != nullptr || stopping.load(std::memory_order_acquire) || shutdownToken.stop_requested()) && flipsAhead.load(std::memory_order_acquire) == 0;
}

bool Driver::flipWorkDone(const std::shared_ptr<FlipWork>& work) {
    if (work == nullptr) return true;
    if (!work->finished || !flipWorkDone(work->previous)) return false;
    if (work->previous != nullptr) {
        work->serial = std::max(work->serial, work->previous->serial);
        work->previous.reset();
    }
    return true;
}

void Driver::holdFlipBehindWork(const std::shared_ptr<FlipWork>& earlier) {
    if (earlier == nullptr) return;
    std::unique_lock lock(mutex);
    flipHolders.fetch_add(1, std::memory_order_acq_rel);
    changed.wait(lock, [&] { return failure != nullptr || stopping.load(std::memory_order_acquire) || shutdownToken.stop_requested() || flipWorkDone(earlier) || queue0AwaitsTitle.load(std::memory_order_acquire); });
    flipHolders.fetch_sub(1, std::memory_order_acq_rel);
    if (failure != nullptr || stopping.load(std::memory_order_acquire) || shutdownToken.stop_requested() || !flipWorkDone(earlier)) return;
    const auto workSerial = earlier->serial;
    lock.unlock();
    waitFlipWork(workSerial);
}

void Driver::waitFlipWork(std::uint64_t workSerial) {
    if (workSerial == 0) return;
    auto localDevice = device.Load();
    if (localDevice == nullptr) return;
    if (localDevice->CanWaitUnlocked()) {
        try {
            localDevice->WaitRecorded(workSerial);
        } catch (...) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Wait);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            localDevice.reset();
            throw;
        }
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Wait);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        localDevice->ReapRecorded(workSerial);
        localDevice.reset();
        return;
    }
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Wait);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    localDevice->WaitIdle();
    localDevice.reset();
}

void Driver::releaseFlipHold() {
    std::lock_guard lock(mutex);
    flipsAhead.fetch_sub(1, std::memory_order_acq_rel);
    if (flipHolders.load(std::memory_order_acquire) != 0) changed.notify_all();
}

void Driver::beginFlipReady(std::uint32_t handle) {
    std::lock_guard lock(mutex);
    ++flipsReadying[handle];
}

void Driver::endFlipReady(std::uint32_t handle) {
    std::lock_guard lock(mutex);
    const auto found = flipsReadying.find(handle);
    if (found != flipsReadying.end() && --found->second == 0) flipsReadying.erase(found);
}

void Driver::finishFlipWork(const std::shared_ptr<FlipWork>& work, std::uint64_t workSerial) {
    std::lock_guard lock(mutex);
    if (work != nullptr) {
        work->serial = std::max(work->serial, workSerial);
        work->finished = true;
        flipWorkDone(work);
    }
    if (flipHolders.load(std::memory_order_acquire) != 0) changed.notify_all();
}

void Driver::reserveOutputs(Submission& submission) {
    std::set<std::uint32_t> eopTokens;
    const auto prepare = [&](Submission::IndirectSegment& segment) {
        auto waits = std::make_shared<Submission::PreparedWaits>();
        for (const auto& target : renderingWaitTargets) {
            const auto output = outputs.find(target.first);
            if (output == outputs.end()) continue;
            try {
                if (auto wait = output->second->CaptureRenderingWait(target.second)) waits->emplace(target, std::move(wait));
            } catch (const std::exception&) {
            }
        }
        segment.waits = std::move(waits);
    };
    std::size_t nextSegment = 0;
    for (std::size_t cursor = 0; cursor < submission.commands.size();) {
        while (nextSegment < submission.indirectSegments.size() && submission.indirectSegments[nextSegment].start <= cursor) prepare(submission.indirectSegments[nextSegment++]);
        const auto* words = submission.commands.data() + cursor;
        if (words[0] == RenderingWaitPacketHeader) {
            const auto output = outputs.find(words[1]);
            require(output != outputs.end(), "rendering wait references an unregistered video output");
            auto wait = output->second->CaptureRenderingWait(words[2]);
            require(wait != nullptr, "video output returned a null rendering wait");
            submission.renderingWaits.emplace(cursor, std::move(wait));
            renderingWaitTargets.emplace(words[1], words[2]);
        }
        if (EopRelease(std::span(submission.commands).subspan(cursor))) {
            const auto token = words[7];
            const auto flip = eopFlips.find(token);
            if (flip != eopFlips.end()) {
                require(submission.queue == 0, "EOP flips on async queues are not implemented");
                require(eopTokens.insert(token).second, "EOP flip token repeated in a submission");
                submission.flips.emplace(cursor, flip->second.request);
                submission.flipHandles.emplace(cursor, flip->second.info.handle);
            }
        }
        noteHeldAtSubmit(submission, cursor);
        if (words[0] == FlipPacketHeader) {
            const auto output = outputs.find(words[1]);
            require(output != outputs.end(), "flip references an unregistered video output");
            const FlipInfo info{words[1], std::bit_cast<std::int32_t>(words[2]), words[3], std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32u))};
            auto request = output->second->Reserve(info);
            require(request != nullptr, "video output returned a null flip reservation");
            submission.flips.emplace(cursor, std::move(request));
            submission.flipHandles.emplace(cursor, words[1]);
        }
        cursor += Pm4::PacketWords(words[0]);
    }
    for (const auto token : eopTokens) eopFlips.erase(token);
}

namespace {

std::vector<std::uint64_t> BoundGraphicsPrograms(const Submission& submission) {
    constexpr std::array<std::uint32_t, 6> programRegisters{0x008u, 0x048u, 0x088u, 0x0c8u, 0x108u, 0x148u};
    static std::array<std::atomic<std::uint32_t>, programRegisters.size()> lastHigh{};
    std::array<std::optional<std::uint32_t>, programRegisters.size()> low{};
    std::array<bool, programRegisters.size()> written{};
    std::vector<std::uint64_t> programs;
    const auto note = [&](std::uint32_t offset, std::uint32_t value) {
        for (std::size_t program = 0; program < programRegisters.size(); ++program) {
            if (offset == programRegisters[program] + 1u) lastHigh[program].store(value & 0xffu, std::memory_order_relaxed);
            else if (offset == programRegisters[program]) low[program] = value;
            else continue;
            written[program] = true;
        }
    };
    const auto flush = [&] {
        for (std::size_t program = 0; program < programRegisters.size(); ++program) {
            if (!written[program] || !low[program]) continue;
            written[program] = false;
            programs.push_back((static_cast<std::uint64_t>(*low[program]) << 8u) | (static_cast<std::uint64_t>(lastHigh[program].load(std::memory_order_relaxed)) << 40u));
        }
    };
    const auto& commands = submission.commands;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        const auto header = commands[cursor];
        const auto count = Pm4::PacketWords(header);
        if (cursor + count > commands.size()) break;
        const auto packet = std::span(commands).subspan(cursor, count);
        const auto opcode = (header >> 8u) & 0xffu;
        if (!Pm4::FillerPacket(header) && opcode == 0x76 && count > 2 && !Pm4::IsTagMarker(packet)) {
            for (std::size_t index = 2; index < count; ++index) note((packet[1] & ~0x70000000u) + static_cast<std::uint32_t>(index - 2), packet[index]);
        } else if (!Pm4::FillerPacket(header) && opcode == 0x63) {
            if (const auto found = submission.registerLists.find(cursor); found != submission.registerLists.end()) {
                for (std::size_t index = 0; index + 1 < found->second.size(); index += 2) note(found->second[index] & ~0x70000000u, found->second[index + 1]);
            }
        }
        flush();
        cursor += count;
    }
    std::ranges::sort(programs);
    programs.erase(std::ranges::unique(programs).begin(), programs.end());
    return programs;
}

}

void Driver::executeRewindTail(const Submission& stalled) {
    PerformanceTimer timing("Driver.Rewind");
    static const bool ring = ReadSubmissionRingSettings();
    if (ring) PrepareSubmissionRing(stalled.queue);
    std::atomic_ref<std::uint32_t> control(*const_cast<std::uint32_t*>(stalled.rewindTail - 1));
    if ((control.load(std::memory_order_acquire) & Pm4::RewindValid) == 0) {
        noteWaitBlocked(stalled.queue, reinterpret_cast<std::uint64_t>(stalled.rewindTail - 1), true);
        struct BlockedWait {
            Driver& driver;
            std::uint32_t queue;
            ~BlockedWait() { driver.noteWaitBlocked(queue, 0, false); }
        } blocked{*this, stalled.queue};
        StallWatch stall(stalled.queue);
        while ((control.load(std::memory_order_acquire) & Pm4::RewindValid) == 0) {
            CheckFailure();
            checkStopping();
            noteAwaitingTitle(stalled.queue, reinterpret_cast<std::uint64_t>(stalled.rewindTail - 1));
            stall.Check("REWIND at", reinterpret_cast<std::uint64_t>(stalled.rewindTail - 1));
            if (stalled.queue == 0 && completionsPending()) reapCompletionLabels();
            PollSleep();
        }
    }
    Submission tail{};
    tail.queue = stalled.queue;
    if (APS5_ENABLE_TIMING_LOG) tail.receivedAt = std::chrono::steady_clock::now();
    auto& released = releasedTails();
    if (!released.empty() && released.front() == stalled.rewindTail - 1) {
        released.pop_front();
        tail.enqueuedAt = lastEpochBump();
    } else {
        released.clear();
        tail.enqueuedAt = std::chrono::steady_clock::now();
    }
    copyCommands(tail, stalled.rewindTail, stalled.rewindWords);
    noteTailRead(stalled.rewindTail, tail.queue, tail.rewindTail);
    if (APS5_ENABLE_TIMING_LOG) tail.copiedAt = std::chrono::steady_clock::now();
    validate(tail, stalled.rewindTail);
    readRegisterLists(tail);
    if (LibraryPrebuildsWaiting()) PrioritizeLibraryPrebuilds(BoundGraphicsPrograms(tail));
    if (APS5_ENABLE_TIMING_LOG) tail.validatedAt = std::chrono::steady_clock::now();
    waitForFlipRoom(tail);
    if (APS5_ENABLE_TIMING_LOG) tail.roomReadyAt = std::chrono::steady_clock::now();
    auto* const capture = SubmissionCapture::Active();
    auto captured = capture != nullptr ? SubmissionCapture::Prepare(tail.queue, SubmissionKind::Rewind, tail.commands, tail.conditionalEnds) : CapturedSubmission{};
    std::uint64_t retiredTakes = 0;
    {
        std::lock_guard lock(mutex);
        rethrowFailure();
        checkStopping();
        reserveOutputs(tail);
        if (!tail.flips.empty()) rewindTailFlips.fetch_add(1, std::memory_order_acq_rel);
        tail.shaders = shaders;
        tail.serial = stalled.serial;
        tail.received = ++eventSerial;
        const auto& waiting = workers[tail.queue].pending;
        retiredTakes = waiting.empty() ? tail.received : std::min(tail.received, waiting.front().received);
        if (capture != nullptr) capture->Take(captured, tail.serial, captureTime(), tail.heldAtSubmit);
        if (ring) RecordSubmission(tail.queue, tail.serial, tail.commands, stalled.rewindTail, true);
    }
    if (capture != nullptr) writeCaptured(*this, *capture, captured);
    if (APS5_ENABLE_TIMING_LOG) tail.enqueuedAt = tail.dequeuedAt = tail.orderedAt = std::chrono::steady_clock::now();
    timing.Finish();
    retireLabelTakes(tail.queue, retiredTakes);
    execute(tail);
}

void Driver::SubmitFlip(const FlipInfo& info) {
    const auto argument = static_cast<std::uint64_t>(info.argument);
    std::array<std::uint32_t, FlipPacketWords> words{FlipPacketHeader, info.handle, static_cast<std::uint32_t>(info.index), info.mode, static_cast<std::uint32_t>(argument), static_cast<std::uint32_t>(argument >> 32u)};
    const Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Submit(&packet, 0, true);
}

void Driver::Submit(const Packet* packet, std::uint32_t queue, bool cpuFlip) {
    const auto receivedAt = APS5_ENABLE_TIMING_LOG ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    CheckFailure();
    require(queue == 0 || (queue >= 0x20 && queue < SubmissionQueueEnd), "unsupported compute queue");
    static const bool ring = ReadSubmissionRingSettings();
    if (ring) PrepareSubmissionRing(queue);
    static const bool flipsInOrder = std::getenv("APS5_CPU_FLIP_IN_ORDER") != nullptr || std::getenv("APS5_SYNC_FLIP") != nullptr;
    const bool unorderedFlip = cpuFlip && !flipsInOrder && !APS5_ENABLE_TIMING_LOG && !submitsQueue0();
    if (queue == 0 && !cpuFlip) submitsQueue0() = true;
    GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
    const auto descriptor = *packet;
    require(descriptor.flags == 0, "nonzero submission flags are not implemented");
    Submission submission{};
    submission.queue = queue;
    submission.receivedAt = receivedAt;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& costs = submissionCosts(queue);
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (descriptor.dw_num != 0) {
        require(descriptor.dw_num <= std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t), "command size overflow");
        GuestMemory::CheckRange(descriptor.addr, static_cast<std::size_t>(descriptor.dw_num) * sizeof(std::uint32_t), alignof(std::uint32_t));
        copyCommands(submission, descriptor.addr, descriptor.dw_num);
    }
    const auto copied = profile ? std::chrono::steady_clock::now() : start;
    if (APS5_ENABLE_TIMING_LOG) submission.copiedAt = std::chrono::steady_clock::now();
    validate(submission, descriptor.addr);
    readRegisterLists(submission);
    if (APS5_ENABLE_TIMING_LOG) submission.validatedAt = std::chrono::steady_clock::now();
    waitForFlipRoom(submission);
    const bool holdsWork = holdFlipBehindWorker(submission, cpuFlip);
    std::shared_ptr<FlipWork> earlierFlipWork;
    const auto scan = ReferencedPrograms(submission.commands, submission.registerLists);
    if (!scan.complete || submission.rewindTail != nullptr) {
        AwaitRegistrationPreparation();
    } else {
        std::shared_ptr<const ShaderRegistry> registry;
        {
            std::lock_guard lock(mutex);
            registry = shaders;
        }
        if (registry != nullptr) AwaitReferencedPreparations(*registry, scan.addresses);
    }
    if (LibraryPrebuildsWaiting()) PrioritizeLibraryPrebuilds(BoundGraphicsPrograms(submission));
    if (APS5_ENABLE_TIMING_LOG) submission.roomReadyAt = std::chrono::steady_clock::now();
    static const bool trace = std::getenv("APS5_TRACE_GPU") != nullptr;
    if (trace) std::fprintf(stderr, "[gpu] %.1f submit queue=0x%x dwords=%zu at %p\n", TraceMs(), queue, submission.commands.size(), static_cast<const void*>(descriptor.addr));
    const auto validated = profile ? std::chrono::steady_clock::now() : start;
    auto* const capture = SubmissionCapture::Active();
    auto captured = capture != nullptr ? SubmissionCapture::Prepare(queue, SubmissionKind::Submit, submission.commands, submission.conditionalEnds) : CapturedSubmission{};
    static const bool progressEnabled = std::getenv("APS5_SUBMIT_PROGRESS") != nullptr;
    const auto progress = progressEnabled && progressEligible(submission) ? std::make_shared<SubmissionProgress>() : nullptr;
    submission.progress = progress;
    const auto progressDeadline = progress ? std::chrono::steady_clock::now() + std::chrono::milliseconds(2) : std::chrono::steady_clock::time_point{};
    {
        std::unique_lock lock(mutex);
        if (captureHold.load(std::memory_order_relaxed)) changed.wait(lock, [&] { return !captureHold.load(std::memory_order_relaxed); });
        rethrowFailure();
        checkStopping();
        require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
        reserveOutputs(submission);
        if (unorderedFlip && capture == nullptr && accepted != 0 && flipsAhead.load(std::memory_order_acquire) == 0 && !flipsReadying.contains(submission.flipHandles.begin()->second)) {
            const auto now = FrameTiming::Clock::now();
            auto frame = std::make_shared<FrameTiming>(++frameSerial);
            frame->IncludeSubmission(accepted, now, now, now, true);
            frame->SetFlip(accepted, 0, now, now);
            ++flipsCounted;
            submission.flips.begin()->second->GpuReady(frame);
            lock.unlock();
            Graphics::NoteHostImportFrame();
            return;
        }
        submission.shaders = shaders;
        submission.serial = accepted + 1;
        if (trace) std::fprintf(stderr, "[gpu] %.1f accepted serial=%llu queue=0x%x dwords=%zu at %p%s\n", TraceMs(), static_cast<unsigned long long>(submission.serial), queue, submission.commands.size(),
                                static_cast<const void*>(descriptor.addr), cpuFlip ? " (cpu flip)" : "");

        submission.received = ++eventSerial;
        const auto now = std::chrono::steady_clock::now();
        submission.enqueuedAt = now;
        if (capture != nullptr) capture->Take(captured, submission.serial, captureTime(), submission.heldAtSubmit);
        if (ring) RecordSubmission(queue, submission.serial, submission.commands, descriptor.addr, false);
        if (profile) {
            ++costs.submissions;
            costs.validateNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(validated - copied).count());
            costs.copyNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>((copied - start) + (now - validated)).count());
        }
        if (APS5_ENABLE_TIMING_LOG) submission.enqueuedAt = std::chrono::steady_clock::now();
        if (queue == 0 && !submission.flips.empty()) {
            submission.holdsFlip = true;
            flipsAhead.fetch_add(1, std::memory_order_acq_rel);
            submission.flipWork = std::make_shared<FlipWork>();
            submission.flipWork->previous = lastFlipWork;
            if (holdsWork) earlierFlipWork = lastFlipWork;
            lastFlipWork = submission.flipWork;
        }
        if (!submission.flips.empty()) ++flipSubmissionsOfThread();
        if (submission.rewindTail != nullptr) noteTailRead(nullptr, queue, submission.rewindTail);
        enqueue(std::move(submission));
        ++accepted;
    }
    changed.notify_all();
    if (capture != nullptr) writeCaptured(*this, *capture, captured);
    if (progress) {
        PerformanceTimer timing("Driver.SubmitProgress");
        const auto [localDevice, epoch] = progress->WaitUntil(progressDeadline);
        const auto now = std::chrono::steady_clock::now();
        bool gpuReady = false;
        if (localDevice && epoch != 0 && now < progressDeadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(progressDeadline - now).count();
            gpuReady = localDevice->WaitRecordedFor(epoch, static_cast<std::uint64_t>(remaining));
        }
        if (profile) {
            static std::atomic<std::uint64_t> eligible{0}, ready{0}, expired{0};
            if (gpuReady) ready.fetch_add(1, std::memory_order_relaxed);
            else expired.fetch_add(1, std::memory_order_relaxed);
            const auto count = eligible.fetch_add(1, std::memory_order_relaxed) + 1;
            if (count % 1000 == 0) AgcDriver::ProfilePrint_nid_no_patch("[submit-progress] %llu eligible terminal releases, %llu GPU-ready returns, %llu expired or unsupported\n",
                static_cast<unsigned long long>(count), static_cast<unsigned long long>(ready.load()), static_cast<unsigned long long>(expired.load()));
        }
        CheckFailure();
    }
    holdFlipBehindWork(earlierFlipWork);
}

}
}
