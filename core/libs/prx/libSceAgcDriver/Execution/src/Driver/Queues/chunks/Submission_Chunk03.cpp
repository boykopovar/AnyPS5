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
namespace {
void Driver::SuspendPoint() {
    const auto receivedAt = APS5_ENABLE_TIMING_LOG ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    require(!onWorkerThread(), "worker cannot suspend itself");
    submitsQueue0() = true;
    auto* const capture = SubmissionCapture::Active();
    auto captured = capture != nullptr ? SubmissionCapture::Prepare(0, SubmissionKind::Suspend, {}, {}) : CapturedSubmission{};
    std::unique_lock lock(mutex);
    if (captureHold.load(std::memory_order_relaxed)) changed.wait(lock, [&] { return !captureHold.load(std::memory_order_relaxed); });
    rethrowFailure();
    checkStopping();
    require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
    Submission boundary{};
    boundary.serial = accepted + 1;
    boundary.suspend = true;
    static const bool trace = std::getenv("APS5_TRACE_GPU") != nullptr;
    if (trace) std::fprintf(stderr, "[gpu] %.1f accepted serial=%llu suspend point\n", TraceMs(), static_cast<unsigned long long>(boundary.serial));

    boundary.queue = 0;
    boundary.receivedAt = receivedAt;
    boundary.enqueuedAt = std::chrono::steady_clock::now();
    if (capture != nullptr) capture->Take(captured, boundary.serial, captureTime(), boundary.heldAtSubmit);
    enqueue(std::move(boundary));
    ++accepted;

    changed.notify_all();
    if (capture == nullptr) return;
    lock.unlock();
    writeCaptured(*this, *capture, captured);
}

bool Driver::waitFree(const Submission& submission) {
    if (submission.queue == 0 || submission.suspend || !submission.flips.empty() || !submission.renderingWaits.empty() || submission.rewindTail != nullptr) return false;
    for (std::size_t at = 0; at < submission.commands.size(); at += std::max<std::size_t>(1, Pm4::PacketWords(submission.commands[at]))) {
        const auto header = submission.commands[at];
        const auto opcode = (header >> 8u) & 0xffu;
        if ((header >> 30u) == 3u && (opcode == 0x3c || opcode == 0x93)) return false;
    }
    return true;
}

bool Driver::queue0Before(std::uint64_t received) const {
    if (queue0Executing != 0 && queue0Executing < received) return true;
    if (!queue0Uncommitted.empty() && queue0Uncommitted.front() < received) return true;
    const auto worker = workers.find(0);
    if (worker == workers.end()) return false;
    for (const auto& pending : worker->second.pending) {
        if (pending.suspend) continue;
        return pending.received < received;
    }
    return false;
}

bool Driver::waitFreeBefore(std::uint64_t received) const {
    for (const auto& [queue, worker] : workers) {
        if (queue == 0 || worker.executingWaits) continue;
        if (worker.executingWaitFree != 0 && worker.executingWaitFree < received) return true;
        for (const auto& pending : worker.pending) {
            if (!pending.waitFree || pending.received >= received) break;
            return true;
        }
    }
    return false;
}

bool Driver::orderReleased(std::uint32_t queue, std::uint64_t received) const {
    if (!queue0Before(received)) return true;
    const auto awaited = queue0Awaited.load(std::memory_order_acquire);
    if (awaited == 0) return false;
    if (workers.at(queue).unfinishedWrites.contains(awaited & ~std::uint64_t{3})) return true;
    return runningWorkers.load(std::memory_order_acquire) == 0 && !completionsPending() && !Graphics::Recorder::SnapshotWriteOverlaps(awaited, 4);
}

void Driver::TrackRewindTails() {
    tracksTails.store(true, std::memory_order_relaxed);
}

void Driver::noteTailRead(const std::uint32_t* read, std::uint32_t queue, const std::uint32_t* next) {
    if (!tracksTails.load(std::memory_order_relaxed)) return;
    std::lock_guard lock(tailsMutex);
    if (read != nullptr) {
        if (const auto copied = std::ranges::find(unreadTails, std::pair{queue, read}); copied != unreadTails.end()) unreadTails.erase(copied);
    }
    if (next != nullptr) unreadTails.emplace_back(queue, next);
}

bool Driver::RewindTailsUnread() {
    CheckFailure();
    std::lock_guard lock(tailsMutex);
    return std::ranges::any_of(unreadTails, [&](const auto& tail) {
        const auto [queue, words] = tail;
        if ((std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(words - 1)).load(std::memory_order_acquire) & Pm4::RewindValid) == 0) return false;
        const auto& slot = awaitedSlots[queue == 0 ? 0 : queue - 0x20 + 1];
        const auto awaited = slot.address.load(std::memory_order_acquire);
        if (awaited == 0 || awaited == reinterpret_cast<std::uint64_t>(words - 1) || !awaitsTitle(awaited)) return true;
        const auto shape = slot.shape.load(std::memory_order_relaxed);
        const auto reference = slot.reference.load(std::memory_order_relaxed);
        const auto mask = slot.mask.load(std::memory_order_relaxed);
        if (slot.address.load(std::memory_order_acquire) != awaited) return true;
        const auto bytes = static_cast<std::uint32_t>(shape >> 32u);
        if (bytes == 0) return (std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(awaited)).load(std::memory_order_acquire) & Pm4::RewindValid) != 0;
        std::uint64_t value = 0;
        std::memcpy(&value, reinterpret_cast<const void*>(awaited), bytes == 8 ? 8 : 4);
        return Pm4::WaitCompares(static_cast<std::uint32_t>(shape), reference, mask, value);
    });
}

void Driver::noteWaitFields(std::uint32_t queue, std::span<const std::uint32_t> packet) {
    const auto fields = Pm4::DecodeWait(packet);
    auto& slot = awaitedSlots[queue == 0 ? 0 : queue - 0x20 + 1];
    slot.reference.store(fields.reference, std::memory_order_relaxed);
    slot.mask.store(fields.mask, std::memory_order_relaxed);
    slot.shape.store(static_cast<std::uint64_t>(fields.function) | static_cast<std::uint64_t>(fields.bytes) << 32u, std::memory_order_relaxed);
}

void Driver::noteWaitBlocked(std::uint32_t queue, std::uint64_t awaited, bool blocked) {
    auto& slot = awaitedSlots[queue == 0 ? 0 : queue - 0x20 + 1];
    if (!blocked) {
        runningWorkers.fetch_add(1, std::memory_order_seq_cst);
        while (captureHold.load(std::memory_order_seq_cst)) PollSleep();
        slot.shape.store(0, std::memory_order_relaxed);
    }
    slot.address.store(blocked ? awaited : 0, std::memory_order_release);
    if (blocked) {
        if (runningWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            idleEpoch.fetch_add(1, std::memory_order_release);
            changed.notify_all();
        }
    }
    if (queue == 0) queue0Awaited.store(blocked ? awaited : 0, std::memory_order_release);
    if (queue == 0 && !blocked) queue0AwaitsTitle.store(false, std::memory_order_release);
    if (blocked && orderHolders.load(std::memory_order_acquire) != 0) {
        std::lock_guard lock(mutex);
        changed.notify_all();
    }
}

void Driver::enqueue(Submission submission) {
    const auto queue = submission.queue;
    submission.waitFree = waitFree(submission);
    auto& worker = workers[queue];
    for (const auto dword : submission.labelWrites) ++worker.unfinishedWrites[dword];
    worker.pending.push_back(std::move(submission));
    worker.queued.fetch_add(1, std::memory_order_acq_rel);
    if (!worker.thread.joinable()) worker.thread = Libc::StartHostThread([this, queue] {
        char role[32];
        std::snprintf(role, sizeof(role), "queue 0x%x worker", queue);
        try {
            RaiseWorkerThreadPriority(role);
        } catch (...) {
            ReportFailure(std::current_exception());
            return;
        }
        run(queue);
    });
}

void Driver::noteHeldAtSubmit(Submission& submission, std::size_t cursor) {
    const auto packet = std::span<const std::uint32_t>(submission.commands).subspan(cursor, std::min<std::size_t>(Pm4::PacketWords(submission.commands[cursor]), submission.commands.size() - cursor));
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    if ((packet[0] >> 30u) != 3u) return;
    if (opcode == 0x49 || opcode == 0x37) {
        if (const auto label = Pm4::DecodeLabelWrite(packet)) {
            const auto bytes = label->Bytes();
            if (label->address % 4 == 0 && bytes.size() <= 64) {
                for (std::size_t offset = 0; offset < bytes.size(); offset += 4) submission.labelWrites.push_back(label->address + offset);
            }
        }
        return;
    }
    if ((opcode != 0x3c && opcode != 0x93) || packet.size() < 7 || ((packet[1] >> 4u) & 3u) != 1u) return;
    const auto wait = Pm4::DecodeWait(packet);
    const auto address = wait.address;
    const std::size_t bytes = wait.bytes;
    if (address % 4 != 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) return;
    const auto worker = workers.find(submission.queue);
    for (std::size_t offset = 0; offset < bytes; offset += 4) {
        const auto dword = address + offset;
        if (std::find(submission.labelWrites.begin(), submission.labelWrites.end(), dword) != submission.labelWrites.end()) return;
        if (worker != workers.end() && worker->second.unfinishedWrites.contains(dword)) return;
    }
    std::uint64_t value = *reinterpret_cast<const volatile std::uint32_t*>(address);
    if (bytes == 8) value |= static_cast<std::uint64_t>(*reinterpret_cast<const volatile std::uint32_t*>(address + 4)) << 32u;
    if (Pm4::WaitComparesValue(packet, value)) submission.heldAtSubmit.insert(cursor);
}

void Driver::forgetUnfinishedWrites(QueueWorker& worker, const Submission& submission) {
    forgetUnfinishedWrites(worker, submission.labelWrites);
}

void Driver::forgetUnfinishedWrites(QueueWorker& worker, std::span<const std::uint64_t> labelWrites) {
    for (const auto dword : labelWrites) {
        const auto found = worker.unfinishedWrites.find(dword);
        if (found == worker.unfinishedWrites.end()) continue;
        if (--found->second == 0) worker.unfinishedWrites.erase(found);
    }
}

}
}
}
