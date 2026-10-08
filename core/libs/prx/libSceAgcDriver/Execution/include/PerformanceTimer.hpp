#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETIMER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETIMER_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace AgcDriver {

class FrameTiming {
public:
    using Clock = std::chrono::steady_clock;

    struct Metric {
        Clock::duration total{};
        Clock::duration maximum{};
        std::uint64_t count = 0;
        std::uint64_t bytes = 0;
    };

    explicit FrameTiming(std::uint64_t id, bool localMetrics = false) : localMetrics(localMetrics), id(id), workerStart(Clock::now()) {}

    static FrameTiming* Preparation() {
        if (!APS5_ENABLE_TIMING_LOG) return nullptr;
        static FrameTiming timing(0);
        return &timing;
    }

    static FrameTiming* Async() {
        if (!APS5_ENABLE_TIMING_LOG) return nullptr;
        static FrameTiming timing(0);
        return &timing;
    }

    void CollectBackground() {
        const auto collect = [&](FrameTiming* source, auto& destination) {
            if (source == nullptr) return;
            std::scoped_lock lock(mutex, source->mutex);
            for (auto& [key, metric] : source->metrics) {
                if (metric.count == 0) continue;
                auto& target = destination[key];
                target.total += metric.total;
                target.maximum = std::max(target.maximum, metric.maximum);
                target.count += metric.count;
                target.bytes += metric.bytes;
                metric = {};
            }
        };
        collect(Preparation(), metrics);
        collect(Async(), asyncMetrics);
    }

    Metric* Get(const char* scope, const char* stage) {
        std::unique_lock lock(mutex, std::defer_lock);
        if (!localMetrics) lock.lock();
        return &metrics[{scope, stage}];
    }

    void Add(Metric* metric, Clock::duration elapsed, std::uint64_t bytes = 0) {
        std::unique_lock lock(mutex, std::defer_lock);
        if (!localMetrics) lock.lock();
        metric->total += elapsed;
        metric->maximum = std::max(metric->maximum, elapsed);
        ++metric->count;
        metric->bytes += bytes;
    }

    void NotePacket(std::uint64_t serial, std::size_t offset, std::uint32_t opcode, std::size_t words, Clock::duration elapsed) {
        std::unique_lock lock(mutex, std::defer_lock);
        if (!localMetrics) lock.lock();
        ++packetCount;
        packetWords += words;
        if (elapsed > slowPacketTime) {
            slowPacketTime = elapsed;
            slowPacketSerial = serial;
            slowPacketOffset = offset;
            slowPacketOpcode = opcode;
        }
    }

    void IncludeSubmission(std::uint64_t serial, Clock::time_point received, Clock::time_point enqueued, Clock::time_point dequeued, bool firstSegment) {
        if (serial == 0 || received == Clock::time_point{} || received > enqueued || enqueued > dequeued) throw std::runtime_error("Frame timing: invalid submission timestamps");
        if (firstSerial == 0) {
            firstSerial = serial;
            start = received;
            executionStart = dequeued;
        }
        start = std::min(start, received);
        lastSerial = serial;
        if (firstSegment) {
            Add(Get("Submission", "accept"), enqueued - received);
            Add(Get("Submission", "queue"), dequeued - enqueued);
        }
    }

    void SetFlip(std::uint64_t serial, std::size_t offset, Clock::time_point received, Clock::time_point reached) {
        if (serial != lastSerial || received > reached || executionStart > reached) throw std::runtime_error("Frame timing: invalid flip lineage");
        flipSerial = serial;
        flipOffset = offset;
        flipReceived = received;
        flipReached = reached;
        localMetrics = false;
    }

    Clock::time_point FlipReached() {
        std::lock_guard lock(mutex);
        return flipReached;
    }

    // Recorder batches at the flip packet (after its submit) and how many had not signaled.
    void NoteFlipBatches(std::uint64_t atFlip, std::uint64_t unsignaled) {
        std::lock_guard lock(mutex);
        batchesAtFlip = atFlip;
        unsignaledAtFlip = unsignaled;
    }

    // 0 until the flip packet sampled the recorder (the drain paths never do).
    std::uint64_t BatchesAtFlip() {
        std::lock_guard lock(mutex);
        return batchesAtFlip;
    }

    // At the presentation's blit: the batches ahead of it, the newest batch's vkQueueSubmit time
    // and the blit's own, both measured from the flip packet.
    void NoteBlit(std::uint64_t atBlit, Clock::time_point lastSubmitted, Clock::time_point blitSubmitted) {
        std::lock_guard lock(mutex);
        batchesAtBlit = atBlit;
        if (flipReached == Clock::time_point{}) return;
        blitSubmitAfterFlip = blitSubmitted - flipReached;
        lastSubmitAfterFlip = std::max(lastSubmitted, blitSubmitted) - flipReached;
    }

    // A presentation retired during this present (with APS5_FLIP_INFLIGHT=1 the previous frame's):
    // the batches submitted behind its blit; and, for a retired presentation whose batches all
    // have their completion record by this present (VulkanDevice's SettleRetired: usually one
    // present later than the retire), the GPU's busy time and idle gaps over those batches.
    void NoteRetired(std::uint64_t afterFlip, double busyMs, double gapMs) {
        std::lock_guard lock(mutex);
        batchesAfterFlip += afterFlip;
        gpuBusyMs += busyMs;
        gpuGapMs += gapMs;
    }

    std::function<void(std::ostream&)> Capture(std::uint32_t outputHandle, std::int32_t buffer, std::int64_t argument, Clock::time_point finished, Clock::duration interval) {
        std::lock_guard lock(mutex);
        if (firstSerial == 0 || flipSerial == 0) throw std::runtime_error("Frame timing: incomplete submission lineage");
        return [asyncMetrics = asyncMetrics, metrics = metrics, id = id, firstSerial = firstSerial, lastSerial = lastSerial, flipSerial = flipSerial, flipOffset = flipOffset, workerStart = workerStart, start = start, executionStart = executionStart, flipReceived = flipReceived, flipReached = flipReached, batchesAtFlip = batchesAtFlip, unsignaledAtFlip = unsignaledAtFlip, batchesAtBlit = batchesAtBlit, batchesAfterFlip = batchesAfterFlip, gpuBusyMs = gpuBusyMs, gpuGapMs = gpuGapMs, lastSubmitAfterFlip = lastSubmitAfterFlip, blitSubmitAfterFlip = blitSubmitAfterFlip, packetCount = packetCount, packetWords = packetWords, slowPacketTime = slowPacketTime, slowPacketSerial = slowPacketSerial, slowPacketOffset = slowPacketOffset, slowPacketOpcode = slowPacketOpcode, outputHandle, buffer, argument, finished, interval](std::ostream& output) {
            output << std::fixed << std::setprecision(3);
            output << "[FrameTiming] frame=" << id << " submissions=" << firstSerial << ':' << lastSerial;
            output << " output=" << outputHandle;
            output << " flip=" << flipSerial << ':' << flipOffset << " buffer=" << buffer << " argument=" << argument;
            output << " endpoint=flip_complete display_confirmed=0 worker_queue=0 background_metrics=completed_since_previous_flip";
            output << " first_submit_at_ms=" << milliseconds(start.time_since_epoch());
            output << " first_execute_at_ms=" << milliseconds(executionStart.time_since_epoch());
            output << " flip_packet_at_ms=" << milliseconds(flipReached.time_since_epoch());
            output << " flip_complete_at_ms=" << milliseconds(finished.time_since_epoch());
            output << " first_submit_to_flip_ms=" << milliseconds(finished - start);
            output << " flip_submit_to_complete_ms=" << milliseconds(finished - flipReceived);
            output << " first_submit_to_execute_ms=" << milliseconds(executionStart - start);
            output << " execute_to_flip_packet_ms=" << milliseconds(flipReached - executionStart);
            output << " flip_packet_to_complete_ms=" << milliseconds(finished - flipReached);
            auto accounted = Clock::duration::zero();
            for (const auto scope : {"Driver.Packet", "Driver.Suspend", "Driver.WorkerWait", "Driver.Completion", "Driver.EndSubmission", "Driver.Rewind"}) {
                const auto it = metrics.find({scope, "total"});
                if (it != metrics.end()) accounted += it->second.total;
            }
            output << " worker_begin_at_ms=" << milliseconds(workerStart.time_since_epoch());
            output << " worker_span_ms=" << milliseconds(flipReached - workerStart);
            output << " worker_accounted_ms=" << milliseconds(accounted);
            output << " worker_unattributed_ms=" << milliseconds(flipReached - workerStart - accounted);
            if (interval != Clock::duration::zero()) output << " flip_interval_ms=" << milliseconds(interval);
            output << " batches_at_flip=" << batchesAtFlip << " unsignaled_at_flip=" << unsignaledAtFlip << " batches_at_blit=" << batchesAtBlit << " batches_after_flip=" << batchesAfterFlip;
            output << " gpu_busy_ms=" << gpuBusyMs << " gpu_gap_ms=" << gpuGapMs;
            output << " last_submit_after_flip_ms=" << milliseconds(lastSubmitAfterFlip) << " blit_submit_after_flip_ms=" << milliseconds(blitSubmitAfterFlip);
            output << " packet_count=" << packetCount << " packet_dwords=" << packetWords;
            output << " slow_packet_submission=" << slowPacketSerial << " slow_packet_offset=" << slowPacketOffset << " slow_packet_opcode=" << slowPacketOpcode << " slow_packet_ms=" << milliseconds(slowPacketTime);
            output << " metrics=inclusive(count,sum_ms,max_ms[,bytes])";
            const auto writeMetrics = [&](const auto& entries, const char* prefix) {
                for (const auto& [key, metric] : entries) {
                    if (metric.count == 0) continue;
                    output << ' ' << prefix << key.first << '.' << key.second << "=(" << metric.count << ',' << milliseconds(metric.total) << ',' << milliseconds(metric.maximum);
                    if (metric.bytes != 0) output << ',' << metric.bytes;
                    output << ')';
                }
            };
            writeMetrics(metrics, "");
            writeMetrics(asyncMetrics, "Async.");
            output << '\n';
        };
    }

private:
    static double milliseconds(Clock::duration elapsed) {
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    std::mutex mutex;
    bool localMetrics = false;
    std::map<std::pair<std::string_view, std::string_view>, Metric> metrics;
    std::map<std::pair<std::string_view, std::string_view>, Metric> asyncMetrics;
    std::uint64_t packetCount = 0;
    std::uint64_t packetWords = 0;
    Clock::duration slowPacketTime{};
    std::uint64_t slowPacketSerial = 0;
    std::size_t slowPacketOffset = 0;
    std::uint32_t slowPacketOpcode = 0;
    std::uint64_t id;
    std::uint64_t firstSerial = 0;
    std::uint64_t lastSerial = 0;
    std::uint64_t flipSerial = 0;
    std::size_t flipOffset = 0;
    Clock::time_point workerStart{};
    Clock::time_point start{};
    Clock::time_point executionStart{};
    Clock::time_point flipReceived{};
    Clock::time_point flipReached{};
    std::uint64_t batchesAtFlip = 0;
    std::uint64_t unsignaledAtFlip = 0;
    std::uint64_t batchesAtBlit = 0;
    std::uint64_t batchesAfterFlip = 0;
    double gpuBusyMs = 0;
    double gpuGapMs = 0;
    Clock::duration lastSubmitAfterFlip{};
    Clock::duration blitSubmitAfterFlip{};
};

class PerformanceContext {
public:
    explicit PerformanceContext(FrameTiming* frame) : previous(current) {
        current = frame;
    }

    PerformanceContext(const PerformanceContext&) = delete;
    PerformanceContext& operator=(const PerformanceContext&) = delete;

    ~PerformanceContext() {
        current = previous;
    }

    static FrameTiming* Current() {
        return current;
    }

private:
    inline static thread_local FrameTiming* current = nullptr;
    FrameTiming* previous;
};

class PerformanceTimer {
public:
    explicit PerformanceTimer(const char* scope) : frame(PerformanceContext::Current()), scope(scope) {
        if (frame != nullptr) {
            total = frame->Get(scope, "total");
        }
        if (frame != nullptr) start = Clock::now();
        previous = start;
    }

    PerformanceTimer(const PerformanceTimer&) = delete;
    PerformanceTimer& operator=(const PerformanceTimer&) = delete;

    ~PerformanceTimer() {
        Finish();
    }

    void Finish() {
        if (frame == nullptr) return;
        const auto end = Clock::now();
        if (tail != nullptr) frame->Add(tail, end - previous);
        frame->Add(total, end - start);
        frame = nullptr;
    }

    void FinishPacket(std::uint64_t serial, std::size_t offset, std::uint32_t opcode, std::size_t words) {
        if (frame != nullptr) frame->NotePacket(serial, offset, opcode, words, Clock::now() - start);
        Finish();
    }

    void Mark(const char* stage, std::uint64_t bytes = 0) {
        if (frame == nullptr) return;
        const auto now = Clock::now();
        if (tail == nullptr) tail = frame->Get(scope, "tail");
        frame->Add(frame->Get(scope, stage), now - previous, bytes);
        previous = now;
    }

private:
    using Clock = FrameTiming::Clock;

    FrameTiming* frame;
    const char* scope;
    FrameTiming::Metric* total = nullptr;
    FrameTiming::Metric* tail = nullptr;
    Clock::time_point start{};
    Clock::time_point previous{};
};

}

#endif
