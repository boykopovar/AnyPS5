#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <immintrin.h>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace AgcDriver {
namespace {

// Milliseconds since the first trace line, for the APS5_TRACE_GPU timeline.
double TraceMs() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - origin).count();
}

// A short sleep between polls of a label another queue or the CPU writes. std::this_thread::sleep_for
// goes through winpthreads' nanosleep and rounds up to the scheduler tick (15.6 ms unless raised),
// which multiplied by the dozens of queue-to-queue hand-offs in a frame; a high-resolution waitable
// timer sleeps for the 200 us asked.
void PollSleep() {
#ifdef _WIN32
    thread_local HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer != nullptr) {
        LARGE_INTEGER due{};
        due.QuadPart = -2000;  // 200 us in 100 ns units
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
#endif
    std::this_thread::sleep_for(std::chrono::microseconds(200));
}

void require(bool condition, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string("AGC driver: ") + reason);
    }
}

struct ShaderSnapshot {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;
};

struct Submission {
    std::uint64_t serial;
    std::uint32_t queue;
    std::vector<std::uint32_t> commands;
    std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>> shaders;
    std::map<std::size_t, std::shared_ptr<IFlipRequest>> flips;
    bool suspend = false;
    // Record-order stamp (Driver::eventSerial) taken when the game submitted: a WAIT_REG_MEM of this
    // submission trusts only labels the recorder noted with a newer stamp (see Recorder::NoteLabel).
    std::uint64_t received = 0;
};

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    require(it != registers.end(), "required shader register has not been written");
    return it->second;
}

// The last packets of a submission, for the WAIT_REG_MEM timeout report and the draw-failure dump.
// Only offsets are kept per packet: formatting every packet's line cost more than most packets
// themselves, and the reports are rare. The commands outlive the history (both live in execute).
struct PacketHistory {
    std::span<const std::uint32_t> commands;
    std::array<std::size_t, 64> offsets{};
    std::size_t count = 0;

    void Record(std::size_t offset) {
        offsets[count % offsets.size()] = offset;
        ++count;
    }

    std::string Format(std::size_t offset) const {
        const auto header = commands[offset];
        const auto words = std::min(Pm4::PacketWords(header), commands.size() - offset);
        char line[160];
        int length = std::snprintf(line, sizeof(line), "%s", Pm4::Name(header).c_str());
        for (std::size_t i = 1; i < words && i < 9 && length < 140; ++i) length += std::snprintf(line + length, sizeof(line) - length, " %08x", commands[offset + i]);
        return line;
    }

    // Visits the recorded packets' lines oldest to newest.
    template <typename Visit>
    void Each(Visit&& visit) const {
        const auto shown = std::min(count, offsets.size());
        const auto first = count > shown ? count % offsets.size() : 0;
        for (std::size_t i = 0; i < shown; ++i) visit(Format(offsets[(first + i) % offsets.size()]));
    }
};

// The driver's device. It is created, replaced and reset under GuestMemory::GpuMutex only; the
// atomic lets a dispatch's prologue (capture, cache validation, recompile), which runs without that
// lock, fetch it without waiting behind another queue's device phase. Arrow access is for holders of
// the GpuMutex, under which the pointee cannot be released.
class DevicePointer {
public:
    std::shared_ptr<VulkanDevice> load() const { return pointer.load(std::memory_order_acquire); }
    operator std::shared_ptr<VulkanDevice>() const { return load(); }
    DevicePointer& operator=(std::shared_ptr<VulkanDevice> value) {
        pointer.store(std::move(value), std::memory_order_release);
        return *this;
    }
    void reset() { *this = nullptr; }
    VulkanDevice* operator->() const { return load().get(); }
    explicit operator bool() const { return load() != nullptr; }
    bool operator==(std::nullptr_t) const { return load() == nullptr; }

private:
    std::atomic<std::shared_ptr<VulkanDevice>> pointer;
};

// [sync] statistics (APS5_PROFILE_DRAW): device drains by packet, and the label outcomes of
// VulkanDevice::WriteLabelOnGpu (reason 0 GPU, 5 GPU store plus a completion action behind
// write-backs, 6 completion action because the memory is not host-imported, 1-4 CPU fallbacks by
// reason). Not-imported labels
// are counted apart: a title whose CPU-side label polling stalls looks there first.
// Counted from the label path and the drain sites of Driver::execute; the report itself is printed
// under GuestMemory::GpuMutex only (lastSyncReport is plain).
std::atomic<std::uint64_t> gpuLabels{0}, completionLabels{0}, notImportedLabels{0}, noOpLabels{0}, unlockedDrains{0};
std::atomic<std::uint64_t> labelFallbacks[5] = {};
// Labels queued in a worker's deferred list, the groups they were recorded in, and labels that
// took the locked path at once (too large for the list, or APS5_LABEL_LOCK_EACH=1).
std::atomic<std::uint64_t> queuedLabels{0}, labelGroups{0}, immediateLabels{0};
// Label groups recorded and label batches submitted at the following packet's own GPU mutex
// acquisition (recordLabelsForPacket), groups and submits a failed try in flushBetweenPackets left
// to that acquisition (counted apart: a compute worker's try is often submit-only), groups a
// self-locking packet recorded right after its capture (a try that succeeded there, or a queued
// label overlapping the capture, which the packet then redoes), and suspend points (all in the
// [labels] line).
std::atomic<std::uint64_t> packetLockRecords{0}, packetLockSubmits{0}, packetLockDeferred{0}, packetSubmitDeferred{0}, captureTryRecords{0}, captureRetries{0}, suspendPoints{0};
// APS5_PROFILE_DRAW: the time (microseconds) the record and the submit take at the packet's lock.
std::atomic<std::uint64_t> packetLockRecordUs{0}, packetLockSubmitUs{0};
// Non-label stores the driver makes for the title (COPY_DATA, DMA_DATA, DUMP_CONST_RAM; see the
// store path of Driver::execute): recorded on the GPU, recorded on the GPU and stored again by a
// completion action (behind a pending write-back, or memory the GPU has no view of: reasons 5 and
// 6, kept apart so a stale GPU read of a store is looked for there first), stored by the CPU under
// the mutex because the recorder was idle (nothing to order after), or stored by the CPU behind a
// device drain as before (too large, misaligned, not decodable, no device, or APS5_CPU_STORES=1).
std::atomic<std::uint64_t> storesOnGpu{0}, storesBehindCompletions{0}, storesOnCpu{0}, storesDrained{0};
std::map<std::uint32_t, std::uint64_t> drainCounts;
std::uint64_t drainTotal = 0;
std::chrono::steady_clock::time_point lastSyncReport = std::chrono::steady_clock::now();
void reportSync() {
    std::string report;
    for (const auto& [code, count] : drainCounts) report += " " + (code == 0xffffu ? std::string("flip") : Pm4::Name(code << 8u)) + "=" + std::to_string(count);
    std::fprintf(stderr, "[sync] %llu device drains by packet:%s (%llu waited without the GPU mutex); %llu labels written on the GPU, %llu deferred behind completions, %llu deferred not imported, %llu no-op, fallbacks: idle %llu, completions %llu, not imported %llu, undecodable %llu; %llu labels queued per worker, recorded in %llu groups, %llu recorded at once; stores: %llu recorded on the GPU, %llu behind completions, %llu on the CPU (idle), %llu synced (drained)\n", static_cast<unsigned long long>(drainTotal), report.c_str(), static_cast<unsigned long long>(unlockedDrains.load()), static_cast<unsigned long long>(gpuLabels.load()), static_cast<unsigned long long>(completionLabels.load()), static_cast<unsigned long long>(notImportedLabels.load()), static_cast<unsigned long long>(noOpLabels.load()), static_cast<unsigned long long>(labelFallbacks[1].load()), static_cast<unsigned long long>(labelFallbacks[2].load()), static_cast<unsigned long long>(labelFallbacks[3].load()), static_cast<unsigned long long>(labelFallbacks[4].load()), static_cast<unsigned long long>(queuedLabels.load()), static_cast<unsigned long long>(labelGroups.load()), static_cast<unsigned long long>(immediateLabels.load()), static_cast<unsigned long long>(storesOnGpu.load()), static_cast<unsigned long long>(storesBehindCompletions.load()), static_cast<unsigned long long>(storesOnCpu.load()), static_cast<unsigned long long>(storesDrained.load()));
}
void countLabelOutcome(int reason) {
    if (reason == 0) ++gpuLabels;
    else if (reason == 5) ++completionLabels;
    else if (reason == 6) ++notImportedLabels;
    else ++labelFallbacks[std::min(reason, 4)];
}

// A worker's deferred labels. A label packet (RELEASE_MEM, WRITE_DATA) no longer takes the GPU
// mutex to record its store: the label is queued here, on the worker's own thread, and the whole
// group is recorded under the mutex the next time this worker needs it anyway (before any packet
// of its queue that touches memory or the device: the label still precedes that work in queue
// order), or at the label flush deadline (APS5_LABEL_FLUSH_US) checked between packets. Other
// queues and game threads poll memory for the value, so their latency stays bounded by the
// deadline. Debug aid: APS5_LABEL_LOCK_EACH=1 records every label under the mutex as before.
struct DeferredLabel {
    static constexpr std::size_t Capacity = 32;
    std::uint64_t address;
    std::size_t size;
    std::array<std::byte, Capacity> bytes;
};
struct DeferredLabels {
    std::vector<DeferredLabel> labels;
    // When the first label of the group was queued (the deadline counts from here).
    std::chrono::steady_clock::time_point since;
};
DeferredLabels& deferredLabels() {
    static thread_local DeferredLabels deferred;
    return deferred;
}
bool DeferLabels() {
    // APS5_DRAIN_COMPLETION_LABELS=1 implies the locked path too: a label that must drain then
    // keeps the packet path's unlocked timeline wait instead of a WaitIdle under the mutex, so
    // that switch bisects the same way it did before labels were deferred.
    static const bool defer = std::getenv("APS5_LABEL_LOCK_EACH") == nullptr && std::getenv("APS5_DRAIN_COMPLETION_LABELS") == nullptr;
    return defer;
}
// Whether a packet needs this worker's deferred labels recorded before it runs: everything except
// pure register and state writes, NOP, and the events the driver does not execute. A wait, dispatch,
// draw, copy, indirect register load, constant RAM dump or flip either reads memory a label may
// write (through the flush hook, which only knows recorded stores) or records device work that must
// follow the label in queue order.
bool NeedsRecordedLabels(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x26: case 0x28: case 0x2a: case 0x2f:
        case 0x42: case 0x46: case 0x58: case 0x68: case 0x69: case 0x76: case 0x78: case 0x79: case 0x7a: case 0x81:
            return false;
        default: return true;
    }
}
// Packets whose own execution takes GuestMemory::GpuMutex and records this worker's deferred labels
// first thing inside it (Driver::recordLabelsForPacket): dispatches (direct and indirect, including
// the fill HLE), draws and the flip. Their unlocked prologue (capture, resource stage A) reads guest
// memory through a flush hook that does not know a still-queued label, so a capture is checked
// against the queued labels once it is known what it read (recordQueuedLabelsAfterCapture: an
// overlap records them and the packet starts over), and a CPU read of dispatch arguments records
// the queue's labels first (recordQueuedLabelsBeforeRead). Stores (COPY_DATA, DMA_DATA,
// DUMP_CONST_RAM) lock too but resolve their source before that lock, so they keep the record
// ahead of them: a copy of a label value this queue just wrote must see it.
bool PacketLocksItself(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x15: case 0x16: case 0x2d: case 0x35: return true;
        default: return false;
    }
}

class Driver {
public:
    static Driver& Get() {
        static Driver driver;
        return driver;
    }

    ~Driver() {
        stop();
    }

    void Shutdown() {
        stop();
        CheckFailure();
    }

private:
    void stop() {
        require(!OnWorkerThread(), "worker cannot stop itself");
        std::lock_guard shutdownLock(shutdownMutex);
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        // No worker is added once `stopping` is set, so the map is stable while the threads finish.
        for (auto& [queue, worker] : workers) {
            if (worker.thread.joinable()) worker.thread.join();
        }
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        device.reset();
    }

public:

    void Submit(const Packet* packet, std::uint32_t queue) {
        CheckFailure();
        require(queue == 0 || (queue >= 0x20 && queue < 0x58), "unsupported compute queue");
        GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
        const auto descriptor = *packet;
        require(descriptor.flags == 0, "nonzero submission flags are not implemented");
        Submission submission{};
        submission.queue = queue;
        if (descriptor.dw_num != 0) {
            require(descriptor.dw_num <= std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t), "command size overflow");
            GuestMemory::CheckRange(descriptor.addr, static_cast<std::size_t>(descriptor.dw_num) * sizeof(std::uint32_t), alignof(std::uint32_t));
            submission.commands.assign(descriptor.addr, descriptor.addr + descriptor.dw_num);
        }
        validate(submission.commands, queue, descriptor.addr);
        static const bool trace = std::getenv("APS5_TRACE_GPU") != nullptr;
        if (trace) std::fprintf(stderr, "[gpu] %.1f submit queue=0x%x dwords=%zu at %p\n", TraceMs(), queue, submission.commands.size(), static_cast<const void*>(descriptor.addr));
        {
            std::lock_guard lock(mutex);
            rethrowFailure();
            require(!stopping, "submission during shutdown");
            require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
            for (std::size_t cursor = 0; cursor < submission.commands.size();) {
                const auto* words = submission.commands.data() + cursor;
                if (words[0] == FlipPacketHeader) {
                    const auto output = outputs.find(words[1]);
                    require(output != outputs.end(), "flip references an unregistered video output");
                    const FlipInfo info{words[1], std::bit_cast<std::int32_t>(words[2]), words[3], std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32u))};
                    auto request = output->second->Reserve(info);
                    require(request != nullptr, "video output returned a null flip reservation");
                    submission.flips.emplace(cursor, std::move(request));
                }
                cursor += Pm4::PacketWords(words[0]);
            }
            submission.shaders = shaders;
            submission.serial = accepted + 1;
            // Every CPU store the game made before this call precedes the stamp; labels recorded
            // later carry a larger one.
            submission.received = ++eventSerial;
            enqueue(std::move(submission));
            ++accepted;
        }
        changed.notify_all();
    }

    void WaitIdle() {
        require(!OnWorkerThread(), "worker cannot wait for itself");
        std::unique_lock lock(mutex);
        const auto target = accepted;
        changed.wait(lock, [&] { return failure != nullptr || completed >= target; });
        rethrowFailure();
    }

    void SuspendPoint() {
        require(!OnWorkerThread(), "worker cannot suspend itself");
        std::unique_lock lock(mutex);
        rethrowFailure();
        require(!stopping, "suspend during shutdown");
        require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
        Submission boundary{};
        boundary.serial = accepted + 1;
        boundary.suspend = true;
        // The boundary resets graphics state, so it runs in order with the graphics queue.
        boundary.queue = 0;
        enqueue(std::move(boundary));
        ++accepted;
        // The suspend point only marks where the system may suspend the title; it does not wait for
        // the GPU. Blocking here deadlocks frames whose GPU work waits on labels the CPU writes later.
        changed.notify_all();
    }

    void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        require(output != nullptr, "null video output");
        std::lock_guard lock(mutex);
        rethrowFailure();
        require(!stopping, "video output registration during shutdown");
        require(outputs.emplace(handle, output).second, "video output already registered");
    }

    void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        std::lock_guard lock(mutex);
        const auto it = outputs.find(handle);
        require(it != outputs.end() && it->second == output, "video output registration mismatch");
        outputs.erase(it);
    }

    // Called per packet and per poll of a wait: the flag keeps the driver mutex out of those loops
    // until a failure exists, and `failure` itself is still read under the mutex.
    void CheckFailure() {
        if (!failed.load(std::memory_order_acquire)) return;
        std::lock_guard lock(mutex);
        rethrowFailure();
    }

    void ReportFailure(std::exception_ptr error) {
        require(error != nullptr, "null asynchronous failure");
        {
            std::lock_guard lock(mutex);
            if (!failure) failure = error;
            failed.store(true, std::memory_order_release);
            for (const auto& [handle, output] : outputs) output->Fail(failure);
            for (auto& [queue, worker] : workers) {
                for (const auto& item : worker.pending) {
                    for (const auto& [offset, flip] : item.flips) flip->Fail(failure);
                }
                worker.pending.clear();
            }
        }
        changed.notify_all();
    }

    void Present(const PresentationWindow& window, const DisplayBuffer* buffer, bool opaque, void (*gpuReady)(void*), void* context) {
        // The presenter's frame record travels in the window descriptor: PerformanceContext is a
        // thread_local per DLL, so the one the video-out library set is invisible here.
        PerformanceContext timingContext(window.timing.get());
        PerformanceTimer timing("Driver.Present");
        CheckFailure();
        require(gpuReady != nullptr && context != nullptr, "missing GPU completion callback");
        require(window.getDrawableSize != nullptr, "missing window drawable size query");
        // The device is held only while the presentation is recorded and submitted; the swapchain
        // acquire (where FIFO mode waits for the vblank) and the render fence wait happen outside
        // GpuMutex so the queue workers are not blocked for the present's GPU or display time (the
        // presenter submits on the same VkQueue as the recorder, so queue order makes the frame's
        // batches complete first). Debug aid: APS5_SYNC_FLIP=1 drains the device and presents
        // synchronously under one mutex hold, as before.
        static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
        std::shared_ptr<VulkanDevice> presenting;
        timing.Mark("validate");
        try {
            bool submitted = false;
            bool presentable = false;
            {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                timing.Mark("gpu_mutex_wait");
                if (device == nullptr || device->Window() == nullptr) {
                    if (device) device->WaitIdle();
                    device = std::make_shared<VulkanDevice>(&window);
                }
                require(device->Window() == window.context, "presentation window does not match device surface");
                presenting = device;
                timing.Mark("device_setup");
                std::uint32_t drawableWidth = 0;
                std::uint32_t drawableHeight = 0;
                window.getDrawableSize(window.context, &drawableWidth, &drawableHeight);
                presenting->Resize(drawableWidth, drawableHeight);
                timing.Mark("resize");
                presentable = presenting->Presentable();
                if (buffer != nullptr) require(buffer->width == window.width && buffer->height == window.height, "display buffer extent differs from output");
            }
            // The swapchain, its fences and semaphores are the presenter's own: only the VkQueue needs
            // the mutex, so the acquire (and its vblank wait) is taken first without it.
            if (presentable && !syncFlip) {
                presentable = presenting->AcquireImage();
                timing.Mark("acquire_image");
            }
            if (presentable) {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                timing.Mark("gpu_mutex_wait");
                if (buffer != nullptr) {
                    if (syncFlip) {
                        presenting->WaitIdle();
                        timing.Mark("device_idle_wait");
                    }
                    submitted = presenting->PresentDisplayBuffer(*buffer);
                    timing.Mark("present_display_buffer");
                } else {
                    submitted = presenting->PresentClear(window.width, window.height, opaque);
                    timing.Mark("present_clear");
                }
                if (submitted && syncFlip) {
                    presenting->FinishPresent();
                    timing.Mark("render_fence_wait");
                    presenting->QueuePresent();
                    timing.Mark("queue_present");
                    submitted = false;
                }
            }
            if (submitted) {
                presenting->FinishPresent();
                timing.Mark("render_fence_wait");
                // The single VkQueue is shared with the recorder: presenting on it needs the mutex.
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                presenting->QueuePresent();
                timing.Mark("queue_present");
            }
            gpuReady(context);
            timing.Mark("release_and_callback");
            CheckFailure();
        } catch (...) {
            ReportFailure(std::current_exception());
            throw;
        }
    }

    void ReleaseWindow(void* window) {
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (device && device->Window() == window) device.reset();
    }

    void RegisterShader(const Shader* shader) {
        CheckFailure();
        GuestMemory::CheckRange(shader, sizeof(Shader), alignof(Shader));
        require(shader->file_header == 0x34333231u && shader->version == 0x18u, "invalid shader header");
        require(shader->header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
        require(shader->shader_size != 0 && (shader->shader_size & 3u) == 0, "invalid shader size");
        GuestMemory::CheckRange(shader, shader->header_size, alignof(Shader));
        const auto* code = const_cast<const void*>(shader->code);
        GuestMemory::CheckRange(code, shader->shader_size, 256);
        ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), shader->type, {}, {}};
        snapshot.code.resize(shader->shader_size / sizeof(std::uint32_t));
        std::memcpy(snapshot.code.data(), code, shader->shader_size);
        snapshot.header.resize(shader->header_size);
        std::memcpy(snapshot.header.data(), shader, shader->header_size);
        // Debug aid: APS5_TRACE_SHADER_REGS=<hex code address> (or "all") prints a shader's register lists.
        static const char* traceRegs = std::getenv("APS5_TRACE_SHADER_REGS");
        if (traceRegs != nullptr && (std::string(traceRegs) == "all" || std::strtoull(traceRegs, nullptr, 16) == snapshot.codeAddress)) {
            std::fprintf(stderr, "[shader] 0x%llx type %u cx", static_cast<unsigned long long>(snapshot.codeAddress), shader->type);
            for (std::uint32_t i = 0; i < shader->num_cx_registers && shader->cx_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->cx_registers[i].offset, shader->cx_registers[i].value);
            std::fprintf(stderr, " sh");
            for (std::uint32_t i = 0; i < shader->num_sh_registers && shader->sh_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->sh_registers[i].offset, shader->sh_registers[i].value);
            std::fprintf(stderr, "\n");
        }
        std::lock_guard lock(mutex);
        rethrowFailure();
        const auto address = snapshot.codeAddress;
        shaders.insert_or_assign(address, std::make_shared<const ShaderSnapshot>(std::move(snapshot)));
    }

private:
    std::mutex mutex;
    std::mutex shutdownMutex;
    std::condition_variable changed;
    // Each queue runs on its own thread, so a WAIT_REG_MEM blocks only its own queue as on the GPU.
    // Draws and dispatches take GuestMemory::GpuMutex, so device work stays serialized.
    struct QueueWorker {
        std::deque<Submission> pending;
        std::thread thread;
    };
    std::map<std::uint32_t, QueueWorker> workers;
    std::uint64_t frameSerial = 0;
    // Packets executing now and packets finished, across all queues; waits time out only when neither
    // moves. On a cache line of their own: the waiting queues poll them while the others update them.
    alignas(64) std::atomic<int> packetsInFlight{0};
    std::atomic<std::uint64_t> packetsDone{0};
    // Orders the game's submit calls against the labels the workers record (Submission::received).
    std::atomic<std::uint64_t> eventSerial{0};
    alignas(64) std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>> shaders;
    // Dispatch cache: a program with the same user data and shader registers whose captured memory
    // (SRT chains, descriptors) is unchanged reuses its capture and recompile result. Guarded by
    // dispatchCacheMutex; entries are evicted least recently used.
    struct DispatchEntry {
        std::shared_ptr<ShaderMemory> memory;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        std::shared_ptr<const ShaderRecompiler::RecompileResult> compiled;
        std::vector<std::pair<std::uint64_t, std::size_t>> spans;
        std::uint64_t generation = 0;
        // The registered shader whose code and header the capture references.
        std::shared_ptr<const ShaderSnapshot> shader;
        // This entry's position in dispatchOrder.
        std::list<std::uint64_t>::iterator order;
    };
    static constexpr std::size_t DispatchCacheEntries = 4096;
    std::unordered_map<std::uint64_t, DispatchEntry> dispatchCache;
    // Keys most recently used first.
    std::list<std::uint64_t> dispatchOrder;
    std::mutex dispatchCacheMutex;
    std::uint64_t dispatchCacheHits = 0;
    std::uint64_t dispatchCacheEvictions = 0;
    std::map<std::uint32_t, QueueState> queues;
    std::map<std::uint32_t, std::shared_ptr<IVideoOutput>> outputs;
    DevicePointer device;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    std::set<std::uint64_t> completedOutOfOrder;
    std::exception_ptr failure;
    // Set once `failure` is, so hot loops can check for one without the mutex.
    std::atomic<bool> failed{false};
    bool stopping = false;
    bool resetGraphics = false;

    static bool& OnWorkerThread() {
        static thread_local bool worker = false;
        return worker;
    }

    // Caller holds `mutex`.
    void enqueue(Submission submission) {
        const auto queue = submission.queue;
        auto& worker = workers[queue];
        worker.pending.push_back(std::move(submission));
        if (!worker.thread.joinable()) worker.thread = std::thread([this, queue] { run(queue); });
    }

    Driver() {
        try {
            LibcRegisterShutdown_nid_postfix([] { Driver::Get().Shutdown(); });
        } catch (...) {
            stop();
            throw;
        }
    }

    void rethrowFailure() const {
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    // Summarizes a rejected submission (packet names with counts and the first rejection reason per name).
    static void dumpPackets(std::span<const std::uint32_t> commands, const std::uint32_t* guest = nullptr) {
        std::map<std::string, std::pair<std::size_t, std::string>> summary;
        std::vector<std::pair<std::size_t, std::uint32_t>> walk;
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            if ((header & 0xc0000000u) != 0xc0000000u) break;
            const auto count = Pm4::PacketWords(header);
            if (count > commands.size() - cursor) break;
            walk.emplace_back(cursor, header);
            auto& entry = summary[Pm4::Name(header)];
            if (entry.first++ == 0) {
                try {
                    Pm4::Validate(commands.subspan(cursor, count), 0);
                } catch (const std::exception& error) {
                    entry.second = error.what();
                }
            }
            cursor += count;
        }
        std::fprintf(stderr, "[gpu] rejected submission of %zu dwords at guest %p:\n", commands.size(), static_cast<const void*>(guest));
        for (const auto& [name, entry] : summary) std::fprintf(stderr, "[gpu]   %-28s x%-5zu %s\n", name.c_str(), entry.first, entry.second.c_str());
        // The last packets walked before the parse broke and, with APS5_DUMP_REJECTED=1, the whole
        // stream for offline analysis.
        for (std::size_t i = walk.size() > 24 ? walk.size() - 24 : 0; i < walk.size(); ++i) {
            const auto [offset, header] = walk[i];
            std::fprintf(stderr, "[gpu]   at DWORD %-6zu header 0x%08x %-24s %zu dwords:", offset, header, Pm4::Name(header).c_str(), Pm4::PacketWords(header));
            for (std::size_t j = offset + 1; j < std::min(commands.size(), offset + std::min<std::size_t>(Pm4::PacketWords(header), 12)); ++j) std::fprintf(stderr, " %08x", commands[j]);
            std::fprintf(stderr, "\n");
        }
        static const bool dumpRejected = std::getenv("APS5_DUMP_REJECTED") != nullptr;
        if (!dumpRejected) return;
        static int dumps = 0;
        char fileName[64];
        std::snprintf(fileName, sizeof(fileName), "rejected_submission_%d.bin", dumps++);
        if (FILE* file = std::fopen(fileName, "wb")) {
            std::fwrite(commands.data(), sizeof(std::uint32_t), commands.size(), file);
            std::fclose(file);
            std::fprintf(stderr, "[gpu] rejected submission written to %s\n", fileName);
        }
    }

    static void validate(std::span<const std::uint32_t> commands, std::uint32_t queue, const std::uint32_t* guest = nullptr) {
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            if ((header & 0xc0000000u) != 0xc0000000u) {
                char what[96];
                std::snprintf(what, sizeof(what), "unsupported PM4 packet type: header 0x%08x at DWORD %zu of %zu", header, cursor, commands.size());
                // The packets before it (and the dwords around it) show which packet was mis-sized.
                dumpPackets(commands, guest);
                std::fprintf(stderr, "[gpu] dwords %zu..%zu:", cursor >= 8 ? cursor - 8 : 0, std::min(commands.size(), cursor + 8));
                for (std::size_t i = cursor >= 8 ? cursor - 8 : 0; i < std::min(commands.size(), cursor + 8); ++i) std::fprintf(stderr, " %08x", commands[i]);
                std::fprintf(stderr, "\n");
                throw std::runtime_error(std::string("AGC driver: ") + what);
            }
            const auto count = Pm4::PacketWords(header);
            require(count <= commands.size() - cursor, "truncated PM4 packet");
            try {
                Pm4::Validate(commands.subspan(cursor, count), queue);
            } catch (const std::exception& error) {
                dumpPackets(commands, guest);
                throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what());
            }
            cursor += count;
        }
    }

    // Worker time per packet class, reported every 10 s (APS5_PROFILE_DRAW) so throughput problems
    // show where time goes.
    struct WorkerProfile {
        double dispatchMs = 0;
        double drawMs = 0;
        double waitMs = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point reported = start;
    };
    template <typename Work>
    static void timed(double WorkerProfile::*bucket, Work&& work) {
        static thread_local WorkerProfile profile;
        const auto begin = std::chrono::steady_clock::now();
        work();
        const auto end = std::chrono::steady_clock::now();
        profile.*bucket += std::chrono::duration<double, std::milli>(end - begin).count();
        static const bool report = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (report && end - profile.reported > std::chrono::seconds(10)) {
            profile.reported = end;
            std::fprintf(stderr, "[gpu] worker at %.0f s: dispatch %.1f s, draw %.1f s, wait %.1f s\n", std::chrono::duration<double>(end - profile.start).count(), profile.dispatchMs / 1000, profile.drawMs / 1000, profile.waitMs / 1000);
        }
    }

    // A draw or dispatch the translator cannot handle is skipped so the rest of the frame still runs;
    // each distinct failure is reported once.
    template <typename Work>
    void tolerate(const char* kind, Work&& work) {
        try {
            work();
        } catch (const std::exception& error) {
            static std::mutex reportedMutex;
            static std::set<std::string> reported;
            std::lock_guard lock(reportedMutex);
            // APS5_TRACE_SKIPS reports every skip (shortened), not only the first per reason.
            static const bool traceSkips = std::getenv("APS5_TRACE_SKIPS") != nullptr;
            if (reported.insert(error.what()).second) std::fprintf(stderr, "[gpu] skipped %s: %s\n", kind, error.what());
            else if (traceSkips) std::fprintf(stderr, "[gpu] skipped %s again: %.100s\n", kind, error.what());
        }
    }

    // Saves a request once per shader address so it can be replayed with agc_shader_replay.
    static std::string dumpRequest(std::uint64_t address, const ShaderRecompiler::RecompileRequest& request) {
        static std::mutex dumpMutex;
        static std::set<std::uint64_t> dumped;
        char name[64];
        std::snprintf(name, sizeof(name), "shader_%llx.req", static_cast<unsigned long long>(address));
        std::lock_guard lock(dumpMutex);
        if (!dumped.insert(address).second) return name;
        try {
            const auto text = ShaderRecompiler::RequestSerializer{}.Serialize(request);
            if (std::FILE* file = std::fopen(name, "wb")) {
                std::fwrite(text.data(), 1, text.size(), file);
                std::fclose(file);
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] could not serialize request for 0x%llx: %s\n", static_cast<unsigned long long>(address), error.what());
        }
        return name;
    }

    // Capture and recompile run without the GPU lock, so the compute queues prepare their dispatches
    // in parallel; only resource building and recording are serialized.
    // The engine clears and initializes buffers with a nine-dword compute kernel (every thread stores one
    // 16-byte record of a 32_32_32_32_UINT typed buffer: v_lshl_add_u32 v4, s8, 6, v0; four v_mov from
    // the pattern in user data; buffer_store_format_xyzw v[0:3], v4, s[0:3] idxen). Demon's Souls runs
    // it ~13 times per frame over ~80 MB, and typed 16-byte stores into host-imported system memory took
    // ~22 ms each on the GPU while every drain waited for them. The fill is done as a transfer instead
    // (or a CPU store when the memory is not imported). Debug aid: APS5_NO_FILL_HLE=1 runs the kernel.
    // Whether `code` is that kernel with a V# the transfer can reproduce: everything fillBuffer
    // decides on except the group count, so an indirect dispatch can tell before reading its count.
    static bool matchesFillKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
        static const bool enabled = std::getenv("APS5_NO_FILL_HLE") == nullptr;
        if (!enabled || userData.size() < 8 || compute.numThreads[0] != 64 || compute.numThreads[1] != 1 || compute.numThreads[2] != 1) return false;
        static constexpr std::array<std::uint32_t, 9> fillKernel{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};
        if (code.size() < fillKernel.size() || !std::equal(fillKernel.begin(), fillKernel.end(), code.begin())) return false;
        // V# in user[0..3]: base, stride 16, record count, 32_32_32_32_UINT with an identity swizzle.
        const auto stride = (userData[1] >> 16u) & 0x3fffu;
        const bool swizzled = ((userData[1] >> 31u) & 1u) != 0;
        const auto dstSel = userData[3] & 0xfffu;
        const bool addTid = ((userData[3] >> 23u) & 1u) != 0;
        const auto type = userData[3] >> 30u;
        const auto format = (userData[3] >> 12u) & 0x7fu;
        return type == 0 && stride == 16 && !swizzled && !addTid && dstSel == 0xfacu && format == 0x4bu;
    }

    bool fillBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice) {
        if (!matchesFillKernel(code, userData, compute)) return false;
        const auto numRecords = userData[2];
        std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
        if ((packet[4] & 0x20u) != 0) {
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
                groups[axis] = (groups[axis] + threads - 1) / threads;
            }
        }
        if (groups[1] != 1 || groups[2] != 1) return false;
        const auto records = std::min<std::uint64_t>(static_cast<std::uint64_t>(groups[0]) * 64u, numRecords);
        const auto base = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
        const auto bytes = static_cast<std::size_t>(records * 16u);
        const std::array<std::uint32_t, 4> pattern{userData[4], userData[5], userData[6], userData[7]};
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static std::atomic<std::uint64_t> fills{0}, filledBytes{0}, cpuFills{0};
        ++fills;
        filledBytes += bytes;
        if (bytes != 0) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Fill);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            // This queue's labels first (queue order), as at every dispatch's lock.
            recordLabelsForPacket(localDevice.get(), queueId);
            GuestMemory::CheckRange(reinterpret_cast<const void*>(base), bytes, 16, true);
            // Results still on the GPU for the range would be stored over the fill later.
            Graphics::StorageTexture::FlushPending(base, bytes, nullptr, "buffer fill");
            if (!localDevice->FillBuffer(base, bytes, pattern)) {
                ++cpuFills;
                // Ordered after recorded work like any CPU store from the queue.
                localDevice->WaitIdle();
                static thread_local std::vector<std::byte> block;
                const auto chunk = std::min<std::size_t>(bytes, 1u << 20u);
                block.resize(chunk);
                for (std::size_t at = 0; at < chunk; at += 16) std::memcpy(block.data() + at, pattern.data(), 16);
                for (std::size_t done = 0; done < bytes; done += chunk) GuestMemory::Write(base + done, std::span<const std::byte>(block).first(std::min(chunk, bytes - done)), 16);
            }
        }
        if (profile && fills % 500 == 0) std::fprintf(stderr, "[fill] %llu buffer fills (%.0f MiB), %llu stored by the CPU\n", static_cast<unsigned long long>(fills.load()), filledBytes.load() / 1048576.0, static_cast<unsigned long long>(cpuFills.load()));
        return true;
    }

    // How DISPATCH_INDIRECT group counts were resolved, for the [indirect] line (APS5_PROFILE_DRAW,
    // every 10 s). 0..3 are VulkanDevice::DispatchIndirect's outcomes (0 recorded GPU-side), the rest
    // are decided here before the dispatch is prepared. `readMs` is the CPU read's time (its sync).
    enum IndirectPath { IndirectGpu = 0, IndirectPendingImage = 1, IndirectCopiedWrite = 2, IndirectNotImported = 3, IndirectThreadDimensions = 4, IndirectFillKernel = 5, IndirectMisaligned = 6, IndirectDisabled = 7, IndirectPaths = 8 };
    static void countIndirect(int path, double readMs) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile || path < 0 || path >= IndirectPaths) return;
        static std::mutex countsMutex;
        static std::uint64_t counts[IndirectPaths] = {};
        static double readWaitedMs = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        std::lock_guard lock(countsMutex);
        ++counts[path];
        readWaitedMs += readMs;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport < std::chrono::seconds(10)) return;
        lastReport = now;
        std::uint64_t cpu = 0;
        for (int i = 1; i < IndirectPaths; ++i) cpu += counts[i];
        std::fprintf(stderr, "[indirect] gpu-side %llu, cpu-side %llu (thread dimensions %llu, fill kernel %llu, pending image results %llu, pending label or copied write %llu, not imported %llu, misaligned %llu, disabled %llu); CPU argument reads took %.1f s\n", static_cast<unsigned long long>(counts[IndirectGpu]), static_cast<unsigned long long>(cpu), static_cast<unsigned long long>(counts[IndirectThreadDimensions]), static_cast<unsigned long long>(counts[IndirectFillKernel]), static_cast<unsigned long long>(counts[IndirectPendingImage]), static_cast<unsigned long long>(counts[IndirectCopiedWrite]), static_cast<unsigned long long>(counts[IndirectNotImported]), static_cast<unsigned long long>(counts[IndirectMisaligned]), static_cast<unsigned long long>(counts[IndirectDisabled]), readWaitedMs / 1000);
    }

    // `indirectArguments` (non-zero): a DISPATCH_INDIRECT whose group counts, at that guest address,
    // are not read here; `packet` then carries only the initiator (see dispatchIndirect).
    void dispatch(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::uint64_t indirectArguments = 0) {
        const auto address = (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20c)) << 8u) | (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20d) & 0xffu) << 40u);
        auto it = submission.shaders.upper_bound(address);
        require(it != submission.shaders.begin(), "compute program does not belong to a registered shader");
        --it;
        const auto& snapshot = *it->second;
        require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "compute program is outside registered shader code");
        require(snapshot.type == 0, "compute program refers to a non-compute shader");
        const auto userCount = (readRegister(queue.shader, 0x213) >> 1u) & 0x1fu;
        std::vector<std::uint32_t> userData;
        for (std::uint32_t i = 0; i < userCount; ++i) {
            userData.push_back(readRegister(queue.shader, 0x240 + i));
        }
        const auto compute = Graphics::DecodeComputeStageInfo(queue.shader);
        const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
        // The GpuMutex is only taken to create the device: taking it just to copy the pointer made
        // every dispatch wait behind another queue's whole device phase before its lock-free prologue.
        // Debug aid: APS5_NO_UNLOCKED_DEVICE=1 takes the lock to read it as before.
        static const bool unlockedDevice = std::getenv("APS5_NO_UNLOCKED_DEVICE") == nullptr;
        std::shared_ptr<VulkanDevice> localDevice = unlockedDevice ? device.load() : nullptr;
        if (localDevice == nullptr) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Dispatch);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
        }
        const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
        std::array<std::uint32_t, 5> resolved{};
        if (indirectArguments != 0 && matchesFillKernel(std::span(snapshot.code).subspan(codeOffset), userData, compute)) {
            // The fill HLE consumes the group count on the CPU: read it as before (waiting for the
            // producer through the flush hook) and continue as a direct dispatch.
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            resolved = Pm4::ReadDispatchArguments(indirectArguments, packet[4]);
            countIndirect(IndirectFillKernel, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
            packet = resolved;
            indirectArguments = 0;
        }
        if (fillBuffer(queue, submission.queue, packet, std::span(snapshot.code).subspan(codeOffset), userData, compute, localDevice)) return;
        ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
            {(packet[4] & 0x8000u) != 0 ? 32u : 64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
            localDevice->Target(),
            {0, 0, 0, 128}
        };
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static double captureMs = 0, keyMs = 0, recompileMs = 0, deviceMs = 0;
        static std::uint64_t cacheHits = 0;
        static std::uint64_t dispatches = 0;
        auto lap = std::chrono::steady_clock::now();
        const auto elapsed = [&] {
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - lap).count();
            lap = now;
            return ms;
        };
        // Debug aid: APS5_NO_DISPATCH_CACHE=1 captures and recompiles every dispatch.
        static const bool noDispatchCacheEnv = std::getenv("APS5_NO_DISPATCH_CACHE") != nullptr;
        // Debug aid: APS5_PROBE_DISPATCH=<hex code address>:<n> applies the recompiler's APS5_PROBE
        // register probe to the n-th (0-based) dispatch of that program only; that dispatch is
        // recompiled outside the dispatch cache.
        static const std::pair<std::uint64_t, std::uint64_t> probeDispatch = [] {
            const char* text = std::getenv("APS5_PROBE_DISPATCH");
            if (text == nullptr) return std::pair<std::uint64_t, std::uint64_t>{0, 0};
            char* end = nullptr;
            const auto probeAddress = std::strtoull(text, &end, 16);
            const auto index = end != nullptr && *end == ':' ? std::strtoull(end + 1, nullptr, 10) : 0ull;
            return std::pair<std::uint64_t, std::uint64_t>{probeAddress, index};
        }();
        bool probeThis = false;
        // The heap base differs between runs (0x10…, 0x20…, 0x30…), so only the low 36 bits are compared.
        if (probeDispatch.first != 0 && (address & 0xfffffffffull) == (probeDispatch.first & 0xfffffffffull)) {
            static std::atomic<std::uint64_t> dispatchesSeen{0};
            probeThis = dispatchesSeen.fetch_add(1) == probeDispatch.second;
            if (probeThis) std::fprintf(stderr, "[gpu] probing dispatch %llu of 0x%llx\n", static_cast<unsigned long long>(probeDispatch.second), static_cast<unsigned long long>(address));
        }
        const bool noDispatchCache = noDispatchCacheEnv || probeThis;
        std::uint64_t key = 0xcbf29ce484222325ull;
        const auto mix = [&](std::uint64_t value) {
            key ^= value;
            key *= 0x100000001b3ull;
        };
        mix(address);
        mix(packet[4] & 0x8000u);
        for (const auto word : userData) mix(word);
        // Only the shader registers the request reads (thread counts and RSRC1/2; the program
        // address is `address`): on queue 0 the bank also holds the graphics stages' registers,
        // which every draw rewrites. Debug aid: APS5_NO_DISPATCH_KEY_HYGIENE=1 mixes the whole bank.
        static const bool keyHygiene = std::getenv("APS5_NO_DISPATCH_KEY_HYGIENE") == nullptr;
        if (keyHygiene) {
            for (const auto offset : {0x207u, 0x208u, 0x209u, 0x212u, 0x213u}) {
                const auto found = queue.shader.find(offset);
                // An absent register mixes a value no 32-bit register can hold.
                mix(found == queue.shader.end() ? (1ull << 32u) : found->second);
            }
        } else {
            for (const auto& [offset, value] : queue.shader) {
                mix(offset);
                mix(value);
            }
        }
        std::shared_ptr<const ShaderRecompiler::RecompileResult> compiledResult;
        std::shared_ptr<ShaderMemory> keepMemory;
        // The captured code and header spans point into the entry's snapshot, which may differ
        // from this submission's (re-registered, identical bytes); a hit keeps it past eviction.
        std::shared_ptr<const ShaderSnapshot> keepShader;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        bool cached = false;
        // Debug aid: APS5_TRACE_DISPATCH_CACHE reports, per program, what changed between dispatches.
        static const bool traceCache = std::getenv("APS5_TRACE_DISPATCH_CACHE") != nullptr;
        if (traceCache) {
            std::lock_guard traceLock(dispatchCacheMutex);
            struct Last { std::vector<std::uint32_t> userData; std::map<std::uint32_t, std::uint32_t> shader; std::uint64_t key; };
            static std::map<std::uint64_t, Last> last;
            static int reports = 0;
            auto& previous = last[address];
            if (previous.key != 0 && previous.key != key && reports < 200) {
                std::string what;
                for (std::size_t i = 0; i < userData.size(); ++i) {
                    if (i >= previous.userData.size() || previous.userData[i] != userData[i]) {
                        char text[48];
                        std::snprintf(text, sizeof(text), " user[%zu] %08x->%08x", i, i < previous.userData.size() ? previous.userData[i] : 0u, userData[i]);
                        what += text;
                    }
                }
                for (const auto& [offset, value] : queue.shader) {
                    const auto old = previous.shader.find(offset);
                    if (old == previous.shader.end() || old->second != value) {
                        char text[48];
                        std::snprintf(text, sizeof(text), " sh[%x] %08x->%08x", offset, old == previous.shader.end() ? 0u : old->second, value);
                        what += text;
                    }
                }
                ++reports;
                std::fprintf(stderr, "[dispatch-cache] 0x%llx key changed:%s\n", static_cast<unsigned long long>(address), what.c_str());
            }
            previous.userData = userData;
            previous.shader = std::map<std::uint32_t, std::uint32_t>(queue.shader.begin(), queue.shader.end());
            previous.key = key;
        }
        if (!noDispatchCache) {
            // The entry is copied out and validated without the cache lock: CollectWrites and
            // EqualsCommitted (which can wait for the GpuMutex through the flush hook) would
            // otherwise stall every other queue's lookup behind this one. The write watch keeps
            // concurrent validations of one entry correct, so the re-lock only has to skip an entry
            // another worker changed meanwhile. Debug aid: APS5_NO_UNLOCKED_VALIDATE=1 holds the lock.
            static const bool validateUnlocked = std::getenv("APS5_NO_UNLOCKED_VALIDATE") == nullptr;
            std::unique_lock cacheLock(dispatchCacheMutex);
            if (const auto found = dispatchCache.find(key); found != dispatchCache.end()) {
                auto entry = found->second;
                if (validateUnlocked) cacheLock.unlock();
                bool current = true;
                for (const auto& [begin, bytes] : entry.spans) {
                    GuestMemory::CollectWrites(begin, bytes);
                    if (!GuestMemory::UnchangedSince(begin, bytes, entry.generation)) {
                        current = false;
                        break;
                    }
                }
                std::uint64_t restamped = 0;
                if (!current) {
                    // A write landed in a watched span (spans merge regions up to 64 KiB apart, so it
                    // is often unrelated): the captured bytes themselves decide, and an unchanged
                    // capture is re-stamped at a fresh generation. The generation is collected
                    // BEFORE the compare, as on insert: a write landing after the collect dirties
                    // the pages again and fails the next validation, whereas one landing between a
                    // compare and a later collect would be stamped into the entry and never seen.
                    std::uint64_t generation = 0;
                    for (const auto& [begin, bytes] : entry.spans) generation = std::max(generation, GuestMemory::CollectWrites(begin, bytes));
                    bool same = generation != 0;
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                    for (const auto& region : entry.captured) {
                        if (!same) break;
                        same = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                    }
                    if (same) {
                        restamped = generation;
                        current = true;
                    }
                }
                if (!cacheLock.owns_lock()) cacheLock.lock();
                // Only an entry still at the copied generation is re-stamped or erased; the validated
                // copy is used either way.
                const auto again = dispatchCache.find(key);
                const bool untouched = again != dispatchCache.end() && again->second.generation == entry.generation;
                if (current) {
                    compiledResult = std::move(entry.compiled);
                    keepMemory = std::move(entry.memory);
                    keepShader = std::move(entry.shader);
                    captured = std::move(entry.captured);
                    cached = true;
                    ++dispatchCacheHits;
                    if (untouched) {
                        if (restamped != 0) again->second.generation = restamped;
                        dispatchOrder.splice(dispatchOrder.begin(), dispatchOrder, again->second.order);
                    }
                } else {
                    if (traceCache) std::fprintf(stderr, "[dispatch-cache] 0x%llx captured memory changed\n", static_cast<unsigned long long>(address));
                    if (untouched) {
                        dispatchOrder.erase(again->second.order);
                        dispatchCache.erase(again);
                    }
                }
            }
        }
        if (cached) {
            captureMs += elapsed();
        } else {
            auto shaderMemory = std::make_shared<ShaderMemory>(memory);
            // Debug aid: APS5_DUMP_SHADERS=1 saves every request, failing ones included, for agc_shader_replay.
            static const bool dumpShaders = std::getenv("APS5_DUMP_SHADERS") != nullptr;
            try {
                // The probe is part of the recompiler's cache key, so it covers the capture's plan
                // lookup as well as the recompile that reuses it.
                struct ProbeScope {
                    bool active;
                    explicit ProbeScope(bool active) : active(active) { if (active) ShaderRecompiler::SetDebugProbeActive(true); }
                    ~ProbeScope() { if (active) ShaderRecompiler::SetDebugProbeActive(false); }
                } probeScope{probeThis};
                const auto capture = shaderMemory->Capture(request);
                captured = shaderMemory->Regions();
                request.context.memory = captured;
                captureMs += elapsed();
                if (dumpShaders) static_cast<void>(dumpRequest(address, request));
                const auto started = std::chrono::steady_clock::now();
                // The recompile reuses the capture's plan and materialization rather than walking
                // the captured regions again. Debug aid: APS5_NO_CAPTURE_REUSE=1 walks them again.
                static const bool reuseCapture = std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
                compiledResult = std::make_shared<const ShaderRecompiler::RecompileResult>(reuseCapture ? ShaderRecompiler::Recompile(request, *capture) : ShaderRecompiler::Recompile(request));
                if (compiledResult->cacheHit) ++cacheHits;
                const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                static double totalMs = 0;
                totalMs += elapsed;
                if (profile && elapsed > 200) std::fprintf(stderr, "[gpu] compute shader 0x%llx recompile took %.0f ms (%zu SPIR-V words, %zu captured regions, total %.1f s)\n", static_cast<unsigned long long>(address), elapsed, compiledResult->spirv.size(), captured.size(), totalMs / 1000);
            } catch (const std::exception& error) {
                const auto dump = dumpShaders ? dumpRequest(address, request) : std::string{};
                // Recompile errors append the serialized request; keep only the first line in the report.
                std::string reason = error.what();
                if (const auto newline = reason.find('\n'); newline != std::string::npos) reason.resize(newline);
                char where[96];
                if (dump.empty()) std::snprintf(where, sizeof(where), "compute shader 0x%llx: ", static_cast<unsigned long long>(address));
                else std::snprintf(where, sizeof(where), "compute shader 0x%llx (%s): ", static_cast<unsigned long long>(address), dump.c_str());
                throw std::runtime_error(where + reason);
            }
            recompileMs += elapsed();
            if (!noDispatchCache) {
                // The captured regions, merged into spans when close, are watched for writes from now
                // on; bytes that already differ show a write racing the capture, and nothing is kept.
                DispatchEntry fresh{shaderMemory, captured, compiledResult, {}, 0, it->second};
                for (const auto& region : captured) {
                    const auto begin = region.guestAddress;
                    const auto end = begin + region.bytes.size();
                    if (!fresh.spans.empty() && begin <= fresh.spans.back().first + fresh.spans.back().second + 65536) {
                        auto& last = fresh.spans.back();
                        last.second = static_cast<std::size_t>(std::max(last.first + last.second, end) - last.first);
                    } else {
                        fresh.spans.emplace_back(begin, static_cast<std::size_t>(end - begin));
                    }
                }
                std::uint64_t generation = 0;
                for (const auto& [begin, bytes] : fresh.spans) generation = std::max(generation, GuestMemory::CollectWrites(begin, bytes));
                bool stable = generation != 0;
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                for (const auto& region : captured) {
                    if (!stable) break;
                    stable = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                }
                if (stable) {
                    fresh.generation = generation;
                    std::lock_guard cacheLock(dispatchCacheMutex);
                    // Another worker may have inserted the key meanwhile; its entry is as good.
                    const auto [inserted, isNew] = dispatchCache.try_emplace(key, std::move(fresh));
                    if (isNew) {
                        dispatchOrder.push_front(key);
                        inserted->second.order = dispatchOrder.begin();
                    }
                    // Debug aid: APS5_NO_DISPATCH_LRU=1 clears the cache when it fills, as before.
                    static const bool lru = std::getenv("APS5_NO_DISPATCH_LRU") == nullptr;
                    if (dispatchCache.size() > DispatchCacheEntries) {
                        if (lru) {
                            dispatchCache.erase(dispatchOrder.back());
                            dispatchOrder.pop_back();
                            ++dispatchCacheEvictions;
                        } else {
                            dispatchCacheEvictions += dispatchCache.size();
                            dispatchCache.clear();
                            dispatchOrder.clear();
                        }
                    }
                }
            }
        }
        // A label this queue still has queued over a captured region: recorded now, and the dispatch
        // starts over (the cache entry just validated or inserted fails its next validation, the
        // label store marks the tracker). The queue is empty after, so this happens once.
        if (recordQueuedLabelsAfterCapture(submission.queue, captured)) {
            dispatch(queue, packet, submission, indirectArguments);
            return;
        }
        const auto& compiled = *compiledResult;
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        for (const auto& region : captured) snapshots.push_back({region.guestAddress, region.bytes});
        std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
        if (indirectArguments == 0 && (packet[4] & 0x20u) != 0) {
            // USE_THREAD_DIMENSIONS: the packet counts threads; launch enough whole groups to cover them.
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
                groups[axis] = (groups[axis] + threads - 1) / threads;
            }
        }
        static const bool traceIo = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
        if (traceIo) {
            // APS5_TRACE_DISPATCH_IO=2 also lists the user data.
            std::string words;
            if (std::getenv("APS5_TRACE_DISPATCH_IO")[0] == '2') {
                for (const auto word : userData) {
                    char text[12];
                    std::snprintf(text, sizeof(text), " %08x", word);
                    words += text;
                }
            }
            std::fprintf(stderr, "[dispatch-io] shader 0x%llx%s\n", static_cast<unsigned long long>(address), words.c_str());
        }
        // A failure of the build or the device work is reported against the shader.
        const auto rethrow = [&](const std::exception& error) {
            char where[64];
            std::snprintf(where, sizeof(where), "compute shader 0x%llx: ", static_cast<unsigned long long>(address));
            throw std::runtime_error(where + std::string(error.what()));
        };
        // Stage A of the resource build (copies, buffers, the descriptor set) runs before the
        // lock, which then covers only stage B (texture lookups, imports, descriptor writes) and
        // the record: a compute queue's build no longer stalls the graphics queue for its whole
        // resources phase. Null when the resource cache may serve this dispatch, or with
        // APS5_LOCKED_BUILD=1 (the whole build under the lock as before).
        std::shared_ptr<PreparedDispatch> prepared;
        try {
            prepared = localDevice->PrepareDispatch(compiled, snapshots);
        } catch (const std::exception& error) {
            rethrow(error);
        }
        GuestMemory::TagGpuLockSite(indirectArguments != 0 ? GuestMemory::GpuLockSite::Indirect : GuestMemory::GpuLockSite::Dispatch);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        // This queue's labels first (queue order), and their batch out before the dispatch. Not
        // under the shader's name: a label that fails to record is not a shader failure.
        recordLabelsForPacket(localDevice.get(), submission.queue);
        try {
            if (indirectArguments != 0) {
                const auto outcome = localDevice->DispatchIndirect(compiled, indirectArguments, snapshots, address, std::move(prepared));
                countIndirect(outcome.cpuReason, outcome.argumentReadMs);
            } else {
                localDevice->Dispatch(compiled, groups[0], groups[1], groups[2], snapshots, address, std::move(prepared));
            }
        } catch (const std::exception& error) {
            rethrow(error);
        }
        deviceMs += elapsed();
        if (profile && ++dispatches % 100 == 0) std::fprintf(stderr, "[gpu] %llu dispatches (%llu dispatch cache hits, %llu evictions, %llu recompile cache hits): capture %.1f s, cache key %.1f s, recompile %.1f s, device %.1f s\n", static_cast<unsigned long long>(dispatches), static_cast<unsigned long long>(dispatchCacheHits), static_cast<unsigned long long>(dispatchCacheEvictions), static_cast<unsigned long long>(cacheHits), captureMs / 1000, keyMs / 1000, recompileMs / 1000, deviceMs / 1000);
    }

    // DISPATCH_INDIRECT: the group counts are three dwords a shader usually wrote, and reading them
    // on the CPU waited for that shader (the flush hook syncs on its batch) before this dispatch could
    // even be prepared. The GPU reads them in place instead (VulkanDevice::DispatchIndirect, which
    // still falls back to a CPU read when it cannot see the current bytes); the CPU path stays for
    // thread-dimension initiators (the counts are rounded to groups here) and the fill HLE (needs the
    // count, decided in dispatch once the code is known). Debug aid: APS5_NO_GPU_INDIRECT=1 reads
    // every count on the CPU as before.
    void dispatchIndirect(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
        static const bool gpuIndirect = std::getenv("APS5_NO_GPU_INDIRECT") == nullptr;
        const auto arguments = Pm4::DispatchArgumentAddress(packet, queue);
        const auto initiator = packet.back();
        int path = IndirectGpu;
        if (!gpuIndirect) path = IndirectDisabled;
        else if ((initiator & 0x20u) != 0) path = IndirectThreadDimensions;
        else if (arguments % 4 != 0) path = IndirectMisaligned;
        if (path != IndirectGpu) {
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            const auto direct = Pm4::ReadDispatchArguments(arguments, initiator);
            countIndirect(path, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
            dispatch(queue, direct, submission);
            return;
        }
        // A DISPATCH_DIRECT shape with the counts unknown: dispatch reads only the initiator from it.
        const std::array<std::uint32_t, 5> unresolved{0xc0031500u, 0, 0, 0, initiator};
        dispatch(queue, unresolved, submission, arguments);
    }

    void draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
        PerformanceTimer timing("Driver.Draw");
        auto drawParameters = Pm4::ResolveDraw(packet, queue);
        if (!drawParameters.indexed && (drawParameters.indexCount == 0 || drawParameters.instanceCount == 0)) return;
        // Depth/stencil-only passes (no color writes, no pixel shader) have no effect without depth
        // targets, which are not emulated.
        {
            const auto targetMask = queue.context.find(0x8e);
            const auto shaderMask = queue.context.find(0x8f);
            const bool colorWrites = targetMask != queue.context.end() && shaderMask != queue.context.end() && (targetMask->second & shaderMask->second) != 0;
            if (!colorWrites && !queue.shader.contains(0x8)) return;
        }
        const auto graphics = Graphics::DecodeState(queue);
        struct Program {
            ShaderRecompiler::ShaderBinary binary;
            std::uint32_t userDataBase;
            std::uint32_t firstUserSgpr = 8;
            std::vector<std::uint32_t> userData;
            std::array<ShaderRecompiler::MemoryRegion, 2> memory;
        };
        const auto programAddress = [&](std::uint32_t base) {
            const auto high = readRegister(queue.shader, base + 1);
            require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
            return (static_cast<std::uint64_t>(readRegister(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
        };
        const auto prepare = [&](std::uint64_t address, std::uint8_t type, ShaderRecompiler::ShaderStage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
            auto it = submission.shaders.upper_bound(address);
            require(it != submission.shaders.begin(), "graphics program does not belong to a registered shader");
            --it;
            const auto& snapshot = *it->second;
            require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "graphics program is outside registered shader code");
            require(snapshot.type == type, "graphics program refers to an incompatible shader binary type");
            const auto resources = readRegister(queue.shader, rsrc2);
            const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
            require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
            const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
            Program result{
                {stage, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
                userDataBase,
                8,
                {},
                {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}}
            };
            for (std::uint32_t i = 0; i < userCount; ++i) result.userData.push_back(readRegister(queue.shader, userDataBase + i));
            return result;
        };
        using Stage = ShaderRecompiler::ShaderStage;
        using Role = ShaderRecompiler::ProgramRole;
        std::vector<Program> programs;
        std::vector<Role> roles;
        programs.reserve(5);
        roles.reserve(5);
        const auto append = [&](std::uint32_t base, std::uint8_t type, Stage stage, std::uint32_t resources, std::uint32_t users, Role role) {
            programs.push_back(prepare(programAddress(base), type, stage, resources, users));
            roles.push_back(role);
        };
        const auto initializeMerged = [&](Program& program, std::uint32_t pointerBase, bool pointerRequired) {
            program.firstUserSgpr = 0;
            program.userData.insert(program.userData.begin(), 8, 0);
            if (pointerRequired) {
                const auto low = readRegister(queue.shader, pointerBase);
                const auto high = readRegister(queue.shader, pointerBase + 1);
                const auto address = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
                require(address != 0, "merged shader user-data address is null");
                GuestMemory::CheckRange(reinterpret_cast<const void*>(address), 8, 4);
                program.userData[0] = low;
                program.userData[1] = high;
            }
        };
        if (graphics.stages.path == Graphics::ShaderPath::Tessellation) {
            append(0x148, 5, Stage::Local, 0x10b, 0x10c, Role::Local);
            append(0x108, 7, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
            initializeMerged(programs.back(), 0x102, true);
            append(0x0c8, 2, Stage::TessellationEvaluation, 0x08b, 0x08c, Role::Domain);
        } else if (graphics.stages.path == Graphics::ShaderPath::Geometry) {
            const auto frontAddress = programAddress(0xc8);
            auto snapshot = submission.shaders.upper_bound(frontAddress);
            require(snapshot != submission.shaders.begin(), "geometry front program is not registered");
            --snapshot;
            const auto type = snapshot->second->type;
            require(type == 2 || type == 4, "invalid geometry front binary type");
            append(0xc8, type, Stage::Mesh, 0x8b, 0x8c, Role::Main);
            initializeMerged(programs.back(), 0x82, type == 4);
            if (type == 4) append(0x88, 6, Stage::Mesh, 0x8b, 0x8c, Role::GeometryBack);
        } else {
            append(0xc8, 2, Stage::Vertex, 0x8b, 0x8c, Role::Main);
        }
        append(0x008, 1, Stage::Fragment, 0x00b, 0x00c, Role::Fragment);
        programs.back().firstUserSgpr = 0;
        const auto pixel = Graphics::DecodePixelStageInfo(queue.context, graphics.hasColorTarget, graphics.color.componentMapping);
        std::vector<ShaderRecompiler::MemoryRegion> memory;
        std::vector<ShaderRecompiler::LinkedProgram> linked;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& program = programs[i];
            memory.insert(memory.end(), program.memory.begin(), program.memory.end());
            linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        }
        timing.Mark("prepare");
        // As in dispatch, the capture, the recompiles and the rect-list shaders run without the
        // GpuMutex (guest memory is read through the self-locking flush hook, the recompiler has its
        // own locks); only the device creation and the device Draw take it, so the compute workers
        // do not pile up behind a draw's whole preparation. The device is held by this shared_ptr
        // meanwhile (DevicePointer's arrow form is for holders of the mutex only). Debug aid:
        // APS5_LOCKED_DRAW_PREPARE=1 holds the mutex across the preparation as before.
        static const bool lockedPrepare = std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr;
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        std::shared_ptr<VulkanDevice> localDevice;
        if (lockedPrepare) {
            // The device is created under the lock already held: one "draw" acquisition per draw,
            // as in the baseline, so the [lock] site counts stay comparable.
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            gpuLock.lock();
            timing.Mark("gpu_mutex_wait");
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
            // This queue's labels first (queue order), as at the unlocked path's acquisition below.
            recordLabelsForPacket(localDevice.get(), submission.queue);
        } else if ((localDevice = device.load()) == nullptr) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            std::lock_guard createLock(GuestMemory::GpuMutex());
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
        }
        timing.Mark("device_setup");
        ShaderMemory shaderMemory(memory);
        std::vector<ShaderRecompiler::RecompileResult> results;
        std::vector<Graphics::CompiledShader> stages;
        results.reserve(programs.size() + (graphics.rectList ? 2u : 0u));
        stages.reserve(programs.size());
        std::uint32_t pushCursorBytes = 0;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::GeometryBack) continue;
            const auto& program = programs[i];
            const auto waveSize = program.binary.stage == Stage::Fragment ? graphics.stages.fragmentWaveSize : graphics.stages.vertexWaveSize;
            ShaderRecompiler::RecompileRequest request{
                program.binary,
                {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(pixel) : std::nullopt, program.binary.stage == Stage::Fragment ? std::nullopt : std::optional(Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData)), memory},
                localDevice->Target(),
                {0, 0, pushCursorBytes, Graphics::PipelinePushConstantBytes - pushCursorBytes},
                ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, linked, graphics.stages.mesh, graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}}
            };
            const auto capture = shaderMemory.Capture(request);
            memory = shaderMemory.Regions();
            request.context.memory = memory;
            // Debug aid: APS5_DUMP_DRAW_SHADERS=<hex color target> saves the requests of draws into that
            // target as shader_<address>.req for agc_shader_replay.
            static const std::uint64_t dumpTarget = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SHADERS"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
            if (dumpTarget != 0) {
                // Match the first enabled target or the raw slot-0 base, which skip reports name.
                const auto slot0 = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                if ((graphics.hasColorTarget && graphics.color.address == dumpTarget) || slot0 == dumpTarget) static_cast<void>(dumpRequest(program.binary.codeAddress, request));
            }
            // Debug aid: APS5_DUMP_DRAW_SLOT1=<hex address> saves the requests and registers of draws whose
            // second color target (slot 1) is at that address, as shader_<address>.req and draw_slot1.regs.
            static const std::uint64_t dumpSlot1 = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SLOT1"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
            if (dumpSlot1 != 0) {
                const auto value = [&](std::uint32_t offset) -> std::uint64_t { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
                const auto slot1 = (value(0x391) << 40u) | (value(0x327) << 8u);
                if (slot1 == dumpSlot1) {
                    static_cast<void>(dumpRequest(program.binary.codeAddress, request));
                    if (std::FILE* file = std::fopen("draw_slot1.regs", "w")) {
                        for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                        for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                        for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                        std::fclose(file);
                    }
                }
            }
            // As in dispatch: the recompile reuses the capture (APS5_NO_CAPTURE_REUSE=1 restores).
            static const bool reuseCapture = std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
            results.push_back(reuseCapture ? ShaderRecompiler::Recompile(request, *capture) : ShaderRecompiler::Recompile(request));
            const auto& result = results.back();
            if (!drawParameters.indexed && i == 0) {
                const auto offsetValue = [&](std::int32_t sgpr) {
                    require(sgpr >= 0 && static_cast<std::uint32_t>(sgpr) >= program.firstUserSgpr, "invalid draw offset SGPR");
                    const auto index = static_cast<std::uint32_t>(sgpr) - program.firstUserSgpr;
                    require(index < program.userData.size(), "draw offset SGPR exceeds user data");
                    return program.userData[index];
                };
                if (drawParameters.firstVertex == 0 && result.vertexOffsetSgpr >= 0) drawParameters.firstVertex = offsetValue(result.vertexOffsetSgpr);
                if (result.instanceOffsetSgpr >= 0) drawParameters.firstInstance = offsetValue(result.instanceOffsetSgpr);
            }
            require(result.pushConstants.size() <= Graphics::PipelinePushConstantBytes - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
            stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
            pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
        }
        timing.Mark("shader_compile_and_link");
        // As in dispatch: a label this queue still has queued over a captured region is recorded
        // now and the draw starts over (nothing queued on the locked-prepare path, recorded above).
        if (recordQueuedLabelsAfterCapture(submission.queue, memory)) {
            draw(queue, packet, submission);
            return;
        }
        if (graphics.rectList) {
            require(stages.size() == 2, "rect-list requires vertex and fragment programs");
            auto rectangle = ShaderRecompiler::BuildRectListShaders(results[0], results[1], localDevice->Target());
            results.push_back(std::move(rectangle.control));
            results.push_back(std::move(rectangle.evaluation));
            stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[2], 0}, {Stage::TessellationEvaluation, &results[3], 0}});
        }
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
        timing.Mark("post_compile_prepare");
        if (!gpuLock.owns_lock()) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            gpuLock.lock();
            timing.Mark("gpu_mutex_wait");
            // Present replaces a windowless device by a windowed one (after WaitIdle) while draws
            // may be preparing unlocked: the draw then records on the current device, whose
            // recorder Recorder::Active() already is, rather than mixing the old device's images
            // and pipelines into the new recorder's batch. The preparation is device-independent
            // (SPIR-V for the same physical device; pipelines are created inside Draw).
            if (auto current = device.load(); current != nullptr && current != localDevice) {
                static std::atomic<std::uint64_t> replaced{0};
                std::fprintf(stderr, "[draw] device replaced during unlocked preparation (%llu)\n", static_cast<unsigned long long>(++replaced));
                localDevice = std::move(current);
            }
            // This queue's labels first (queue order), and their batch out before the draw.
            recordLabelsForPacket(localDevice.get(), submission.queue);
        }
        localDevice->Draw(graphics, drawParameters, stages, snapshots);
        timing.Mark("draw_and_resource_release");
    }

    // Serials complete out of order when a wait runs other queues' work; `completed` stays the
    // highest serial below which everything has finished. Caller holds `mutex`.
    void markCompleted(std::uint64_t serial) {
        completedOutOfOrder.insert(serial);
        while (!completedOutOfOrder.empty() && *completedOutOfOrder.begin() == completed + 1) {
            completed = *completedOutOfOrder.begin();
            completedOutOfOrder.erase(completedOutOfOrder.begin());
        }
    }

    // Debug aid: APS5_TRACE_LABEL=<hex address> logs every GPU write to or wait on memory within 256
    // bytes of that address.
    static void traceLabel(std::span<const std::uint32_t> packet, std::uint32_t queue) {
        static const std::uint64_t watched = [] {
            const char* value = std::getenv("APS5_TRACE_LABEL");
            return value ? std::strtoull(value, nullptr, 16) : 0ull;
        }();
        const auto opcode = (packet[0] >> 8u) & 0xffu;
        std::uint64_t target = 0;
        const char* kind = nullptr;
        if (opcode == 0x49 && packet.size() >= 7) { target = packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u); kind = "RELEASE_MEM"; }
        else if (opcode == 0x37 && packet.size() >= 5) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WRITE_DATA"; }
        else if ((opcode == 0x3c || opcode == 0x93) && packet.size() >= 7) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WAIT_REG_MEM"; }
        else if (opcode == 0x40 && packet.size() >= 6) { target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u); kind = "COPY_DATA"; }
        std::uint64_t length = 4;
        if (opcode == 0x50 && packet.size() >= 7) {
            target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u);
            length = packet[6] & 0x3ffffffu;
            kind = "DMA_DATA";
        }
        if (kind == nullptr) return;
        if (std::string_view(kind) != "WAIT_REG_MEM") {
            static std::mutex historyMutex;
            std::lock_guard lock(historyMutex);
            auto& entry = writeHistory()[writeCursor()++ % writeHistory().size()];
            entry = {target, length, queue, opcode};
        }
        if (watched == 0 || target + length + 0x100 < watched || target > watched + 0x100) return;
        std::fprintf(stderr, "[label] %lld ms queue 0x%x %s 0x%llx:", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), queue, kind, static_cast<unsigned long long>(target));
        for (std::size_t i = 1; i < packet.size() && i < 9; ++i) std::fprintf(stderr, " %08x", packet[i]);
        std::fprintf(stderr, "\n");
    }

    struct WriteRecord {
        std::uint64_t target;
        std::uint64_t length;
        std::uint32_t queue;
        std::uint32_t opcode;
    };
    static std::array<WriteRecord, 16384>& writeHistory() {
        static std::array<WriteRecord, 16384> history{};
        return history;
    }
    static std::size_t& writeCursor() {
        static std::size_t cursor = 0;
        return cursor;
    }

    static int WaitTimeoutMs() {
        static const int value = [] { const char* text = std::getenv("APS5_GPU_WAIT_TIMEOUT_MS"); return text ? std::atoi(text) : 1000; }();
        return value;
    }

    // How WAIT_REG_MEM packets were satisfied, per queue worker (reported in its [packets] line):
    // already true when reached; from the recorder's pending-label table (and how many of those by
    // a label of this same queue); by polling memory (the GPU, another queue or the CPU wrote it);
    // timed out. Plus the submits and reaps the poll loop made on behalf of the producer.
    // fromRecorderUnlocked: table hits (at entry or polling) that took no GPU mutex; entriesUnlocked:
    // waits that went to polling without the GPU mutex (nothing recorded writes the range);
    // entryTriesFailed: overlapping entries whose try of the mutex failed and went to polling
    // instead of waiting for it (see waitMemory).
    struct WaitOutcomes {
        std::uint64_t atEntry = 0, fromRecorder = 0, fromRecorderSameQueue = 0, fromRecorderPolling = 0, polled = 0, timedOut = 0, pollSubmits = 0, pollReaps = 0;
        std::uint64_t fromRecorderUnlocked = 0, entriesUnlocked = 0, entryTriesFailed = 0;
    };
    static WaitOutcomes& waitOutcomes() {
        static thread_local WaitOutcomes outcomes;
        return outcomes;
    }

    // Collect-epoch bumps by ordering point, per queue worker (reported in its [packets] line, see
    // GuestMemory::BumpCollectEpoch): the start of a submission, a wait the CPU may have satisfied,
    // a drain, a reap outside a submission, and (APS5_PACKET_EPOCH=1 only) every packet.
    struct EpochBumps {
        std::uint64_t submissions = 0, waits = 0, drains = 0, reaps = 0, packets = 0;
    };
    static EpochBumps& epochBumps() {
        static thread_local EpochBumps bumps;
        return bumps;
    }
    static void bumpEpoch(std::uint64_t EpochBumps::*counter) {
        GuestMemory::BumpCollectEpoch();
        ++(epochBumps().*counter);
    }
    static bool PacketEpoch() {
        static const bool packet = std::getenv("APS5_PACKET_EPOCH") != nullptr;
        return packet;
    }

    // Labels recorded on the GPU are not submitted one by one (see VulkanDevice::WriteLabelOnGpu):
    // the open batch goes out at the next non-label packet of the recording worker, and at the
    // latest APS5_LABEL_FLUSH_US (default 250) after its first label, checked by every worker
    // between packets and by waiting workers inside their poll loop, so a game thread polling the
    // label is never left waiting on an unsubmitted batch for long. The deadline is read lock-free.
    static std::chrono::microseconds LabelFlushDeadline() {
        static const std::chrono::microseconds value = [] {
            const char* text = std::getenv("APS5_LABEL_FLUSH_US");
            return std::chrono::microseconds(text ? std::atoi(text) : 250);
        }();
        return value;
    }
    static bool LabelFlushDue() {
        const auto since = Graphics::Recorder::PendingLabelSince();
        return since.has_value() && std::chrono::steady_clock::now() - *since >= LabelFlushDeadline();
    }
    // Submits the open batch after this many dispatches/draws even without a label, so the GPU
    // starts on them while the CPU records the rest (APS5_BATCH_CAP, default 16; 0 disables).
    static std::uint64_t BatchCap() {
        static const std::uint64_t value = [] {
            const char* text = std::getenv("APS5_BATCH_CAP");
            return text ? std::strtoull(text, nullptr, 10) : 16ull;
        }();
        return value;
    }

    // Records this worker's deferred labels (see DeferredLabels) under GuestMemory::GpuMutex, in
    // queue order, each as VulkanDevice::WriteLabelOnGpu would have at its packet: on the GPU, as a
    // completion action, or by a CPU store when nothing is recorded (the store is ordered already).
    // The stamp is taken here, right before the label is noted, so it orders after every submission
    // the game made before this point (a wait received later never trusts the label). Only an
    // unusable recorder (reason 4) can still ask for a drain here (APS5_DRAIN_COMPLETION_LABELS=1
    // disables deferral, see DeferLabels); it waits idle under the mutex, which is cheap then.
    // A throw (lost device) leaves nothing queued: the worker fails with the packet, or, when the
    // record ran inside a tolerated dispatch or draw, that packet is skipped; the dropped group is
    // reported either way, so a WAIT_REG_MEM timeout later is not the first sign of it.
    void recordDeferredLabels(VulkanDevice* localDevice, std::uint32_t queue) {
        auto& deferred = deferredLabels();
        if (deferred.labels.empty()) return;
        struct Clear {
            std::vector<DeferredLabel>& labels;
            std::uint32_t queue;
            ~Clear() {
                if (std::uncaught_exceptions() != 0) std::fprintf(stderr, "[gpu] queue 0x%x dropped %zu queued labels: their record failed\n", queue, labels.size());
                labels.clear();
            }
        } clear{deferred.labels, queue};
        bool first = true;
        for (const auto& label : deferred.labels) {
            const auto bytes = std::span<const std::byte>(label.bytes).first(label.size);
            const int reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label.address, bytes, ++eventSerial, queue, first) : 4;
            first = false;
            countLabelOutcome(reason);
            if (reason == 0 || reason == 5 || reason == 6) continue;
            if (reason != 1 && localDevice != nullptr) localDevice->WaitIdle();
            GuestMemory::Write(label.address, bytes, 4);
        }
        ++labelGroups;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        // The [sync] line every 10 s (checked every 64 groups; drains alone are too rare now).
        if (profile && (labelGroups.load(std::memory_order_relaxed) & 63u) == 0 && std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
            lastSyncReport = std::chrono::steady_clock::now();
            reportSync();
        }
    }

    // The first thing a packet does under its own GpuMutex hold (dispatch, draw, flip: see
    // PacketLocksItself), before its work is recorded: this worker's deferred labels go in, in
    // queue order ahead of that work, and the open batch is submitted when it carries labels,
    // exactly what flushBetweenPackets did under an acquisition of its own before the packet
    // (APS5_LABEL_OWN_LOCK=1 restores that). One acquisition instead of two: the label one was
    // queue 0's first after a run of unlocked packets and absorbed the compute queues' whole
    // critical sections ([lock] site 'label'), after which the packet's own came uncontended.
    // Returns whether the batch was submitted (the flip then needs no submit of its own).
    bool recordLabelsForPacket(VulkanDevice* localDevice, std::uint32_t queue) {
        static const bool packetFlush = std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;
        // APS5_PROFILE_DRAW: what this costs inside the packet's own hold ([labels] line): the
        // record (with queue 0's reap of finished batches before the first label) and the submit
        // (with another reap on queue 0), both of which run completions, so a dispatch's or
        // indirect dispatch's hold ([lock] line) is partly this.
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (!deferredLabels().labels.empty()) {
            recordDeferredLabels(localDevice, queue);
            ++packetLockRecords;
            if (profile) packetLockRecordUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
        }
        if (localDevice == nullptr || !packetFlush || !Graphics::Recorder::PendingLabelSince().has_value()) return false;
        // As in flushBetweenPackets: a compute worker submits without reaping.
        const auto submitStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        localDevice->SubmitRecorded(queue == 0);
        ++packetLockSubmits;
        if (profile) packetLockSubmitUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - submitStart).count());
        return true;
    }

    // Before a packet's CPU read of guest memory outside any lock (the group counts of a
    // DISPATCH_INDIRECT resolved on the CPU): a label this queue still has queued may write those
    // bytes and the flush hook only knows recorded stores, so the queued labels are recorded first.
    // Rare (a label is normally followed by a wait or a dispatch that records it) and cheap.
    void recordQueuedLabelsBeforeRead(std::uint32_t queue) {
        if (deferredLabels().labels.empty()) return;
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto localDevice = device.load();
        recordDeferredLabels(localDevice.get(), queue);
    }

    // After a self-locking packet's unlocked capture (dispatch, draw), when this worker still has
    // labels queued (the try in flushBetweenPackets failed): a queued label whose bytes lie in a
    // captured region (a fence value in SRT-chased data, a patched constant) was read before its
    // write, and the hook could not sync on it, so the labels are recorded outright now and the
    // caller redoes the packet from the top (returns true; once, the queue is empty after). With no
    // overlap the labels are recorded if the mutex happens to be free, so a poller of the label
    // does not wait for the rest of this prologue (stage A takes milliseconds for a BDA build);
    // otherwise the packet's own acquisition records them as planned. The check itself is a few
    // dozen compares: a group holds a handful of labels, a capture a few dozen regions.
    bool recordQueuedLabelsAfterCapture(std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions) {
        const auto& labels = deferredLabels().labels;
        if (labels.empty()) return false;
        for (const auto& label : labels) {
            for (const auto& region : regions) {
                if (label.address < region.guestAddress + region.bytes.size() && region.guestAddress < label.address + label.size) {
                    recordQueuedLabelsBeforeRead(queue);
                    ++captureRetries;
                    return true;
                }
            }
        }
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        if (!gpuLock.try_lock()) return false;
        const auto localDevice = device.load();
        recordDeferredLabels(localDevice.get(), queue);
        ++captureTryRecords;
        return false;
    }

    // Between packets: records this worker's deferred labels before a packet that needs them (or
    // once their deadline passed), submits the open batch when a label is pending (at once on a
    // non-label packet, else once the deadline passed) or the batch holds enough work, and reaps
    // finished batches whose completion actions still hold labels. The mutex is taken outright when
    // the packet needs the labels or a deadline passed, and by the graphics worker for a pending
    // submit; otherwise it is only tried (a register-only packet must not queue behind a 2-4 ms
    // dispatch for this) and the next packet retries. A packet that takes the mutex itself
    // (PacketLocksItself) gets both the record and the submit at its own acquisition
    // (recordLabelsForPacket), so for it the mutex is only tried here as well: a try that succeeds
    // records at once (the packet's unlocked prologue then reads through a hook that knows the
    // labels, as before), one that fails leaves the group to the packet. Only a deadline still
    // locks outright before such a packet: its prologue may take milliseconds, and pollers of the
    // label are promised the deadline. APS5_NO_LABEL_PACKET_FLUSH=1 leaves recorded labels pending
    // until the deadline (the batch then absorbs the following work); APS5_LABEL_OWN_LOCK=1 takes
    // the mutex outright before every packet that needs the labels, as before.
    void flushBetweenPackets(std::uint32_t queue, std::uint32_t header, bool labelPacket) {
        static const bool packetFlush = std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;
        static const bool ownLock = std::getenv("APS5_LABEL_OWN_LOCK") != nullptr;
        static std::atomic<std::uint64_t> packetFlushes{0}, deadlineFlushes{0}, capFlushes{0}, boundaryReaps{0};
        auto& deferred = deferredLabels();
        const bool queued = !deferred.labels.empty();
        const bool selfLocking = !ownLock && PacketLocksItself(header);
        bool submit = false, record = false, mustLock = false, deferredDue = false, pendingDue = false;
        const auto pending = Graphics::Recorder::PendingLabelSince();
        // The clock is read only while some label waits (every packet passes here).
        const auto now = queued || pending.has_value() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (queued) {
            if (!labelPacket) {
                record = true;
                mustLock = NeedsRecordedLabels(header) && !selfLocking;
            }
            if (now - deferred.since >= LabelFlushDeadline()) record = mustLock = deferredDue = true;
        }
        if (pending.has_value()) {
            if (!labelPacket && packetFlush) submit = true;
            if (now - *pending >= LabelFlushDeadline()) submit = pendingDue = true;
        }
        const bool capped = !submit && !record && BatchCap() != 0 && Graphics::Recorder::RecordedWorkSinceSubmit() >= BatchCap();
        const bool reap = !submit && !record && !capped && Graphics::Recorder::PendingCompletionLabels() != 0;
        if (!submit && !record && !capped && !reap) return;
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        // The site names the hold too (a try's hold is charged to it; a reap alone shows as 'try').
        if (record) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        else if (submit || capped) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
        // A reap alone is never worth waiting for the mutex (the next packet retries); neither is a
        // submit the self-locking packet makes itself, unless the label's deadline passed.
        if (mustLock || (queue == 0 && (capped || (submit && (!selfLocking || pendingDue))))) {
            gpuLock.lock();
        } else if (!gpuLock.try_lock()) {
            if (selfLocking && record) ++packetLockDeferred;
            else if (selfLocking && submit) ++packetSubmitDeferred;
            return;
        }
        const auto localDevice = device.load();
        if (record) {
            recordDeferredLabels(localDevice.get(), queue);
            // The group goes out with the packet flush (or at once past the deadline), as a label
            // recorded at its own packet would have.
            if ((!labelPacket && packetFlush) || deferredDue) submit = true;
        }
        if (localDevice == nullptr) return;
        if (submit || capped) {
            // Rechecked under the lock: another worker may have submitted meanwhile. A compute
            // worker only submits (no reap: a write-back's sync would hold the mutex against queue 0).
            if (!Graphics::Recorder::PendingLabelSince().has_value() && (BatchCap() == 0 || Graphics::Recorder::RecordedWorkSinceSubmit() < BatchCap())) return;
            localDevice->SubmitRecorded(queue == 0);
            if (capped) ++capFlushes;
            else if (!labelPacket && packetFlush) ++packetFlushes;
            else ++deadlineFlushes;
        } else if (reap) {
            localDevice->ReapRecorded();
            ++boundaryReaps;
        }
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static std::atomic<std::uint64_t> total{0};
        if (profile && (++total & 4095u) == 0) std::fprintf(stderr, "[labels] batch flushes between packets: %llu at the next packet, %llu by deadline, %llu by size cap; %llu boundary reaps; at the packet's own lock: %llu label groups (%.0f ms), %llu submits (%.0f ms incl. queue 0's reaps), left to it by a failed try: %llu groups, %llu submits; after a capture: %llu groups recorded by a try, %llu packets redone for a queued label over the capture; %llu suspend points\n", static_cast<unsigned long long>(packetFlushes.load()), static_cast<unsigned long long>(deadlineFlushes.load()), static_cast<unsigned long long>(capFlushes.load()), static_cast<unsigned long long>(boundaryReaps.load()), static_cast<unsigned long long>(packetLockRecords.load()), packetLockRecordUs.load() / 1000.0, static_cast<unsigned long long>(packetLockSubmits.load()), packetLockSubmitUs.load() / 1000.0, static_cast<unsigned long long>(packetLockDeferred.load()), static_cast<unsigned long long>(packetSubmitDeferred.load()), static_cast<unsigned long long>(captureTryRecords.load()), static_cast<unsigned long long>(captureRetries.load()), static_cast<unsigned long long>(suspendPoints.load()));
    }

    // WAIT_REG_MEM: another queue or the CPU produces the value; the other queues run on their own threads.
    void waitMemory(std::span<const std::uint32_t> packet, std::uint32_t queue, const PacketHistory& context, std::uint64_t received) {
        // The timeout counts from the last sign of GPU progress: while any queue is executing packets,
        // the producer may still be on its way (shader compiles alone take hundreds of milliseconds).
        auto start = std::chrono::steady_clock::now();
        auto lastDone = packetsDone.load();
        // Names this worker's thread in the [lock] GpuMutex wait report.
        GuestMemory::TagGpuLockThread(queue);
        auto& outcomes = waitOutcomes();
        const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
        const std::uint64_t awaited = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
        const std::size_t awaitedBytes = wide ? 8 : 4;
        static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
        if (traceGpu) std::fprintf(stderr, "[gpu] %.1f queue 0x%x waits 0x%llx == 0x%x (now 0x%x)\n", TraceMs(), queue, static_cast<unsigned long long>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)), packet[4],
                                   *reinterpret_cast<const volatile std::uint32_t*>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)));
        struct WaitTrace {
            bool enabled; std::uint32_t queue; std::uint64_t address; std::chrono::steady_clock::time_point begin;
            ~WaitTrace() {
                if (!enabled) return;
                const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
                if (ms >= 0.5) std::fprintf(stderr, "[gpu] %.1f queue 0x%x wait on 0x%llx done after %.1f ms\n", TraceMs(), queue, static_cast<unsigned long long>(address), ms);
            }
        } waitTrace{traceGpu, queue, packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u), std::chrono::steady_clock::now()};
        bool warned = false;
        // The collect epoch advances once the wait is over (at entry, polled, timed out, or from a
        // label another queue recorded: the CPU's writes before that queue's submission are ordered
        // through it), unless a label this queue itself recorded satisfied it, which orders no CPU
        // write. After the wait, never before: completions run inside pollService make memo entries
        // during the wait that may predate the CPU's store of the flag.
        struct EpochPoint {
            bool bump = true;
            ~EpochPoint() {
                if (bump) bumpEpoch(&EpochBumps::waits);
            }
        } epochPoint;
        GuestMemory::CheckRange(reinterpret_cast<const void*>(awaited), awaitedBytes, awaitedBytes);
        if (Pm4::WaitSatisfiedUnchecked(packet)) {
            ++outcomes.atEntry;
            return;
        }
        // Debug aids: APS5_NO_LABEL_SHORTCUT=1 never satisfies a wait from the pending-label table;
        // APS5_NO_WAIT_OVERLAP_SUBMIT=1 submits the open batch at every wait, as before, instead of
        // only when it writes the awaited range.
        static const bool labelShortcut = std::getenv("APS5_NO_LABEL_SHORTCUT") == nullptr;
        static const bool overlapSubmit = std::getenv("APS5_NO_WAIT_OVERLAP_SUBMIT") == nullptr;
        // APS5_WAIT_LOCK=1 takes the GPU mutex at every wait's entry and poll check, as before.
        static const bool waitLock = std::getenv("APS5_WAIT_LOCK") != nullptr;
        // Read before the checks below: a write noted before the (locked or snapshot) check is seen
        // by that check, one noted after it changes the generation the poll loop compares against.
        std::uint64_t seenGeneration = Graphics::Recorder::WriteGeneration();
        // Without the GPU mutex first: the label table has its own small mutex, and the lock-free
        // pending-write snapshot says whether any recorded work writes the range at all. Only an
        // overlap needs the mutex (to submit the open batch); a miss goes straight to polling (its
        // producer is the CPU, an in-flight batch, or a queue that has not recorded the label yet:
        // the poll loop watches the write generation for that). The snapshot may lag the generation
        // (Recorder::noteWrite bumps it before publishing, and NoteLabel follows the note), so a
        // decision made from the snapshot never consumes a generation: an unlocked entry makes the
        // poll loop's first check a locked (try_lock) one, which sees the producer's consistent view.
        const bool unlocked = !waitLock && overlapSubmit;
        if (unlocked) {
            if (labelShortcut) {
                std::uint32_t producer = 0;
                if (const auto value = Graphics::Recorder::LookupLabel(awaited, awaitedBytes, received, producer); value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                    ++outcomes.fromRecorder;
                    ++outcomes.fromRecorderUnlocked;
                    if (producer == queue) {
                        ++outcomes.fromRecorderSameQueue;
                        epochPoint.bump = false;
                    }
                    return;
                }
            }
        }
        const bool lockedEntry = !unlocked || Graphics::Recorder::SnapshotWriteOverlaps(awaited, awaitedBytes);
        if (!lockedEntry) ++outcomes.entriesUnlocked;
        // An overlapping entry has one thing to do under the mutex: submit the open batch when it
        // writes the range (the table lookup already ran without it). It only TRIES the mutex: the
        // wait's hold is microseconds, but waiting for the mutex behind a compute queue's build cost
        // queue 0 ~4 ms per entry ([lock] 'wait' site). A failed try goes to the poll loop, whose
        // first service call repeats the entry's checks under a try of its own (`recheck`) and every
        // later one re-tries as well, so the submit happens as soon as the mutex is free, while the
        // value may arrive meanwhile (an in-flight batch, another queue, the CPU) and end the wait
        // without any hold. Debug aid: APS5_WAIT_LOCK_ENTRY=1 waits for the mutex at entry as before.
        static const bool blockingEntry = std::getenv("APS5_WAIT_LOCK_ENTRY") != nullptr;
        bool recheck = !lockedEntry;
        bool entryTryFailed = false;
        if (lockedEntry) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Wait);
            std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
            if (blockingEntry || !unlocked) {
                gpuLock.lock();
            } else if (!gpuLock.try_lock()) {
                ++outcomes.entryTriesFailed;
                recheck = true;
                entryTryFailed = true;
            }
            if (const auto localDevice = gpuLock.owns_lock() ? device.load() : std::shared_ptr<VulkanDevice>{}) {
                // A label the recorder holds for this range, recorded after the game submitted this
                // packet, will store the value in order (see Recorder::NoteLabel): the wait is over
                // without the GPU round trip. The label's batch is submitted by the flush rules.
                std::uint32_t producer = 0;
                if (labelShortcut) {
                    if (const auto value = localDevice->PendingLabel(awaited, awaitedBytes, received, producer); value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                        ++outcomes.fromRecorder;
                        if (producer == queue) {
                            ++outcomes.fromRecorderSameQueue;
                            epochPoint.bump = false;
                        }
                        return;
                    }
                }
                // Recorded work may write the awaited value: send it to the GPU; the poll below sees
                // the store once it lands in the imported memory. Work not writing the range stays
                // in the open batch (its producer is the CPU, an in-flight batch or a queue that has
                // not recorded the label yet; the poll loop submits it once it does). Submit only,
                // no reap: this thread must not wait for the GPU under the mutex on another queue's
                // behalf (a reaped write-back can sync a later batch through the flush hook).
                if (!overlapSubmit || localDevice->OpenWriteOverlaps(awaited, awaitedBytes)) localDevice->SubmitRecorded(false);
            }
        }
        // Consumer-driven submission: the producer's label may be recorded into the open batch after
        // this wait began, and the producing worker submits it only at its next packet, so the
        // poller (idle anyway) watches the recorder's write generation and submits the batch itself
        // when it writes the awaited range, or when any label passed its deadline. In the sleeping
        // phase it also reaps finished batches when labels wait in completion actions (a wait on
        // such a label converges without a reaper thread). The mutex is only tried: a failed try
        // keeps the generation unconsumed and the next check retries. Returns true when the label
        // table satisfied the wait meanwhile: a label recorded after this wait began (the common
        // queue 0 -> compute -> queue 0 chain when queue 0 reaches its wait first) is accepted on
        // the same stamp argument as at entry, sparing the GPU round trip. Every submit here is
        // reap-free: a poller never waits for the GPU under the mutex on the producer's behalf.
        // An unlocked entry decided from the snapshot alone (or one whose try failed): the first
        // check re-does the entry's overlap test under the mutex (a note whose snapshot was not yet
        // published when the entry looked must still get its batch submitted; see the entry comment).
        const auto pollService = [&](bool sleeping) {
            const auto generation = Graphics::Recorder::WriteGeneration();
            const bool changed = generation != seenGeneration || recheck;
            const bool labelDue = LabelFlushDue();
            const bool reap = sleeping && Graphics::Recorder::PendingCompletionLabels() != 0;
            if (!changed && !labelDue && !reap) return false;
            if (changed && unlocked && labelShortcut) {
                // The table first, without the mutex: a hit ends the wait. A miss is not final (the
                // producer may be between its generation bump and its table entry or snapshot), so
                // the generation is consumed only below, under a successful try_lock, where the
                // table, the open batch and the snapshot are consistent (a try_lock never waits,
                // so this costs no mutex wait; a failed one leaves the generation for the next check).
                std::uint32_t producer = 0;
                if (const auto value = Graphics::Recorder::LookupLabel(awaited, awaitedBytes, received, producer); value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                    ++outcomes.fromRecorderPolling;
                    ++outcomes.fromRecorderUnlocked;
                    if (producer == queue) {
                        ++outcomes.fromRecorderSameQueue;
                        epochPoint.bump = false;
                    }
                    return true;
                }
            }
            std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::try_to_lock);
            if (!gpuLock.owns_lock()) return false;
            const auto localDevice = device.load();
            if (localDevice == nullptr) {
                seenGeneration = generation;
                recheck = false;
                return false;
            }
            bool submitted = false;
            if (changed) {
                seenGeneration = generation;
                recheck = false;
                if (labelShortcut) {
                    std::uint32_t producer = 0;
                    if (const auto value = localDevice->PendingLabel(awaited, awaitedBytes, received, producer); value.has_value() && Pm4::WaitComparesValue(packet, *value)) {
                        ++outcomes.fromRecorderPolling;
                        if (producer == queue) {
                            ++outcomes.fromRecorderSameQueue;
                            epochPoint.bump = false;
                        }
                        return true;
                    }
                }
                if (localDevice->OpenWriteOverlaps(awaited, awaitedBytes)) {
                    localDevice->SubmitRecorded(false);
                    submitted = true;
                }
            }
            if (labelDue && !submitted && Graphics::Recorder::PendingLabelSince().has_value()) {
                localDevice->SubmitRecorded(false);
                submitted = true;
            }
            if (submitted) ++outcomes.pollSubmits;
            if (reap) {
                localDevice->ReapRecorded();
                ++outcomes.pollReaps;
            }
            return false;
        };
        // A failed entry try is retried once at once, not only after the spin's first 64 pauses:
        // the mutex may have freed during the entry's own lookup, and the submit a blocking entry
        // made the instant it got the mutex should not wait for the loop's first service.
        if (entryTryFailed && pollService(false)) return;
        // The producer is usually another queue or the CPU: spin briefly with pause (the value is
        // often written within microseconds; the graphics queue's waits average under a millisecond
        // and it is the frame-critical thread, so it keeps its core longer than the compute queues,
        // which mostly wait tens of milliseconds for it), then poll every 200 us so waiting queues
        // do not burn cores. While spinning, the clock, the failure flag and the progress counters
        // are read every 64 pauses only; the timeout accounting below stays on the sleeping path.
        // Debug aid: APS5_NO_PAUSE_SPIN=1 yields 4000 times before sleeping as before.
        static const bool pauseSpin = std::getenv("APS5_NO_PAUSE_SPIN") == nullptr;
        const auto spinLimit = queue == 0 ? std::chrono::microseconds(1500) : std::chrono::microseconds(100);
        const auto spinStart = std::chrono::steady_clock::now();
        bool spinning = pauseSpin;
        std::uint32_t polls = 0;
        while (!Pm4::WaitSatisfiedUnchecked(packet)) {
            ++polls;
            if (spinning) {
                _mm_pause();
                if ((polls & 63u) != 0) continue;
                if (std::chrono::steady_clock::now() - spinStart > spinLimit) spinning = false;
            } else if (pauseSpin || polls >= 4000) {
                PollSleep();
            } else {
                std::this_thread::yield();
            }
            CheckFailure();
            if (pollService(!spinning)) return;
            if (const auto done = packetsDone.load(); done != lastDone || packetsInFlight.load() != 0) {
                lastDone = done;
                start = std::chrono::steady_clock::now();
            }
            // Some producers (skipped shaders, interrupt-driven CPU work) are not modeled; rather than deadlock
            // the queue, give up on a wait after a while and report it once.
            if (!warned && std::chrono::steady_clock::now() - start > std::chrono::milliseconds(WaitTimeoutMs())) {
                warned = true;
                ++outcomes.timedOut;
                static std::set<std::uint64_t> reported;
                static std::uint64_t timeouts = 0;
                if (++timeouts % 20 == 0) std::fprintf(stderr, "[gpu] %llu GPU waits have timed out\n", static_cast<unsigned long long>(timeouts));
                if (!reported.insert(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)).second) return;
                std::fprintf(stderr, "[gpu] queue 0x%x WAIT_REG_MEM at 0x%llx timed out after %dms (function %u ref 0x%x mask 0x%x value 0x%x)\n", queue,
                             static_cast<unsigned long long>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)), WaitTimeoutMs(), packet[1] & 7u, packet[4], packet[5],
                             *reinterpret_cast<const volatile std::uint32_t*>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)));
                const auto address = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
                for (const auto& record : writeHistory()) {
                    if (record.length != 0 && record.target <= address && address < record.target + std::max<std::uint64_t>(record.length, 4))
                        std::fprintf(stderr, "[gpu]   earlier write by queue 0x%x opcode 0x%x at 0x%llx+0x%llx\n", record.queue, record.opcode, static_cast<unsigned long long>(record.target), static_cast<unsigned long long>(record.length));
                }
                context.Each([](const std::string& line) { std::fprintf(stderr, "[gpu]   preceding packet %s\n", line.c_str()); });
            }
            if (warned) return;
        }
        ++outcomes.polled;
    }

    void execute(const Submission& submission) {
        if (submission.suspend) {
            // A suspend point only marks where the system may suspend the title (the system, not the
            // title, waits for the GPU there); this drained the device under the mutex once per
            // frame, which was every 'idle' fence wait of queue 0 in the [recorder] line (~10 ms
            // each, the frame's whole batch list) although nothing reads results here. Now the
            // frame's batches are only submitted, this worker's queued labels first; the register
            // reset that follows needs no idle GPU (recorded work keeps what it uses).
            // Debug aid: APS5_SUSPEND_DRAIN=1 drains as before.
            static const bool suspendDrain = std::getenv("APS5_SUSPEND_DRAIN") != nullptr;
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            ++suspendPoints;
            if (const auto localDevice = device.load()) {
                recordDeferredLabels(localDevice.get(), submission.queue);
                if (suspendDrain) localDevice->WaitIdle();
                else localDevice->SubmitRecorded(submission.queue == 0);
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
        // Debug aid: APS5_DUMP_QUEUE=<hex queue> prints the packets of that queue's first 40 submissions.
        static const long dumpQueue = [] { const char* text = std::getenv("APS5_DUMP_QUEUE"); return text ? std::strtol(text, nullptr, 16) : -1L; }();
        if (static_cast<long>(submission.queue) == dumpQueue) {
            // Only this queue's thread gets here.
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
        // APS5_PROFILE_DRAW: per-queue time spent handling each packet type, reported every 10 s, so
        // the packets a frame's submission spends its time on are visible.
        static const bool profilePackets = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        struct PacketProfile {
            std::map<std::uint32_t, std::pair<std::uint64_t, double>> byOpcode;
            std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
            std::uint64_t submissions = 0;
            // Time in flushBetweenPackets (deferred label submits, reaps and, on queue 0, the mutex
            // wait for them), kept apart from the packets so their buckets show their own cost.
            double flushMs = 0;
        };
        thread_local PacketProfile packetProfile;
        ++packetProfile.submissions;
        // A submission orders every CPU write the game made before submitting it (the collect memo
        // of this worker starts over here); inside it only the ordering points below do, unless
        // APS5_PACKET_EPOCH=1 restores an epoch per packet.
        bumpEpoch(&EpochBumps::submissions);
        for (std::size_t cursor = 0; cursor < submission.commands.size();) {
            if (PacketEpoch()) bumpEpoch(&EpochBumps::packets);
            CheckFailure();
            const auto header = submission.commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            const auto count = Pm4::PacketWords(header);
            const auto packet = std::span(submission.commands).subspan(cursor, count);
            const auto opcode = (header >> 8u) & 0xffu;
            // Names this packet for the flush hook's sync attribution ([hooksync]); the flip is 0xffff.
            GuestMemory::SetCurrentPacket(header == FlipPacketHeader ? 0xffffu : opcode, submission.queue);
            // Pending labels and full batches go to the GPU before this packet's own work starts
            // (see flushBetweenPackets); labels themselves only check the deadline, so a label
            // group shares one submission. Timed on its own, before the packet's timer starts.
            const auto flushStart = profilePackets ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            flushBetweenPackets(submission.queue, header, opcode == 0x49 || opcode == 0x37);
            struct PacketTimer {
                bool enabled; std::uint32_t key; std::uint32_t queue; PacketProfile& profile; std::chrono::steady_clock::time_point start;
                ~PacketTimer() {
                    if (!enabled) return;
                    const auto now = std::chrono::steady_clock::now();
                    auto& entry = profile.byOpcode[key];
                    ++entry.first;
                    entry.second += std::chrono::duration<double, std::milli>(now - start).count();
                    if (now - profile.lastReport < std::chrono::seconds(10)) return;
                    profile.lastReport = now;
                    std::vector<std::pair<std::uint32_t, std::pair<std::uint64_t, double>>> hot(profile.byOpcode.begin(), profile.byOpcode.end());
                    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
                    std::string report;
                    for (std::size_t i = 0; i < hot.size() && i < 10; ++i) {
                        char text[96];
                        std::snprintf(text, sizeof(text), " %s x%llu %.0fms", hot[i].first == 0xffffu ? "flip" : Pm4::Name(hot[i].first << 8u).c_str(), static_cast<unsigned long long>(hot[i].second.first), hot[i].second.second);
                        report += text;
                    }
                    auto& waits = waitOutcomes();
                    auto& epochs = epochBumps();
                    std::fprintf(stderr, "[packets] queue 0x%x %llu submissions, time by packet (10 s):%s, flush %.0fms; waits satisfied: at entry %llu, from recorder %llu (%llu while polling, same queue %llu, %llu without the GPU mutex), polled %llu (%llu entered without the GPU mutex, %llu entry tries failed), timed out %llu; poll submits %llu, poll reaps %llu; epoch bumps: submissions %llu, waits %llu, drains %llu, reaps %llu, packets %llu\n", queue, static_cast<unsigned long long>(profile.submissions), report.c_str(), profile.flushMs, static_cast<unsigned long long>(waits.atEntry), static_cast<unsigned long long>(waits.fromRecorder + waits.fromRecorderPolling), static_cast<unsigned long long>(waits.fromRecorderPolling), static_cast<unsigned long long>(waits.fromRecorderSameQueue), static_cast<unsigned long long>(waits.fromRecorderUnlocked), static_cast<unsigned long long>(waits.polled), static_cast<unsigned long long>(waits.entriesUnlocked), static_cast<unsigned long long>(waits.entryTriesFailed), static_cast<unsigned long long>(waits.timedOut), static_cast<unsigned long long>(waits.pollSubmits), static_cast<unsigned long long>(waits.pollReaps), static_cast<unsigned long long>(epochs.submissions), static_cast<unsigned long long>(epochs.waits), static_cast<unsigned long long>(epochs.drains), static_cast<unsigned long long>(epochs.reaps), static_cast<unsigned long long>(epochs.packets));
                    waits = WaitOutcomes{};
                    epochs = EpochBumps{};
                    profile.byOpcode.clear();
                    profile.submissions = 0;
                    profile.flushMs = 0;
                }
            } packetTimer{profilePackets, header == FlipPacketHeader ? 0xffffu : opcode, submission.queue, packetProfile, std::chrono::steady_clock::now()};
            if (profilePackets) packetProfile.flushMs += std::chrono::duration<double, std::milli>(packetTimer.start - flushStart).count();
            // Label writes are recorded on the GPU behind the work they signal whenever possible, so
            // the CPU never waits for them. Otherwise packets that write guest memory in order with
            // GPU work (labels, copies, constant RAM dumps), draws and flips drain the recorded batches
            // first; reads (indirect arguments, register lists, waits) go through the flush hook.
            // Debug aid: APS5_DRAIN_ALL=1 drains before every memory packet as before.
            static const bool drainAll = std::getenv("APS5_DRAIN_ALL") != nullptr;
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            bool wroteOnGpu = false;
            // A label the CPU writes while the recorder is idle needs no drain: nothing the GPU was
            // given is still running (draws complete synchronously), so the write is already ordered
            // after every earlier packet. That was 70% of all drains at the movie stage.
            bool orderedAlready = false;
            // Labels that store nothing (RELEASE_MEM without data select or destination:
            // interrupt-only) need no drain because Pm4::Execute is a no-op for them. The label
            // counters and the [sync] report live at namespace scope (see reportSync).
            if (!drainAll && (opcode == 0x49 || opcode == 0x37)) {
                if (const auto label = Pm4::DecodeLabelWrite(packet)) {
                    const auto bytes = label->Bytes();
                    if (DeferLabels() && bytes.size() <= DeferredLabel::Capacity && bytes.size() % 4 == 0 && label->address % 4 == 0) {
                        // Queued on this worker, no mutex: recorded with the group by
                        // flushBetweenPackets before the next packet that needs it (or by the
                        // deadline), still ahead of every later packet of this queue.
                        auto& deferred = deferredLabels();
                        if (deferred.labels.empty()) deferred.since = std::chrono::steady_clock::now();
                        auto& entry = deferred.labels.emplace_back();
                        entry.address = label->address;
                        entry.size = bytes.size();
                        std::memcpy(entry.bytes.data(), bytes.data(), bytes.size());
                        ++queuedLabels;
                        wroteOnGpu = true;
                    } else {
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        const auto localDevice = device.load();
                        // Earlier labels of this queue go first (queue order). The stamp is taken
                        // under the mutex, right before the label is noted, so it orders after every
                        // submission the game made before this point. Without a device (reason 4)
                        // the packet drains nothing and Pm4::Execute stores it, as before.
                        recordDeferredLabels(localDevice.get(), submission.queue);
                        const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label->address, bytes, ++eventSerial, submission.queue) : 4;
                        wroteOnGpu = reason == 0 || reason == 5 || reason == 6;
                        if (reason == 1) {
                            // Idle recorder: stored here, still under the mutex, as
                            // recordDeferredLabels does. Stored later by Pm4::Execute outside it,
                            // another worker could record work over the range first and the flush
                            // hook would then sync for the store.
                            GuestMemory::Write(label->address, bytes, 4);
                            wroteOnGpu = true;
                        }
                        countLabelOutcome(reason);
                        ++immediateLabels;
                    }
                } else if (opcode == 0x49 ? ((packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0) : (packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)) == 0) {
                    orderedAlready = true;
                    ++noOpLabels;
                } else {
                    ++labelFallbacks[4];
                }
            }
            // The other stores the driver makes for the title (COPY_DATA and DMA_DATA to memory,
            // DUMP_CONST_RAM) used to drain the device and store on the CPU. Their bytes are known
            // before the GPU runs them (immediate, constant RAM, or a source read through the flush
            // hook, which waits only for recorded work writing the source), so the store is recorded
            // on the GPU exactly like a label (VulkanDevice::WriteLabelOnGpu: host import,
            // barriers, label table, pending-write note): a destination that recorded work also
            // writes is then ordered by the queue and no CPU wait happens. An idle recorder keeps
            // the CPU store (nothing to order after; done under the mutex so no work can be recorded
            // over the range first). A store WriteLabelOnGpu cannot take (more than 64 KiB, not
            // 4-byte aligned, no device) or that does not decode drains and stores as before.
            // Kill switch: APS5_CPU_STORES=1 keeps the drain and CPU store for every such packet.
            static const bool cpuStores = std::getenv("APS5_CPU_STORES") != nullptr;
            if (!drainAll && !cpuStores && (opcode == 0x40 || opcode == 0x50 || opcode == 0x83)) {
                constexpr std::size_t gpuStoreLimit = 65536;
                bool drained = true;
                if (const auto store = Pm4::ResolveStore(packet, queue, gpuStoreLimit)) {
                    const auto bytes = store->Bytes();
                    if (bytes.empty()) {
                        // Nothing to store: Pm4::Execute is a no-op for it too.
                        orderedAlready = true;
                        drained = false;
                    } else {
                        // No lock site of its own: these stores count under [lock] 'label'.
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        const auto localDevice = device.load();
                        // Results still on the GPU for the range would be stored over (or skipped
                        // around, as a block MarkWritten stamps) the store later: written back
                        // first, like a buffer fill (a GPU-direct write-back is recorded before the
                        // store and ordered by its ALL_COMMANDS -> TRANSFER barrier). The CPU and
                        // drained paths get this through the flush hook of GuestMemory::Write.
                        Graphics::StorageTexture::FlushPending(store->address, bytes.size(), nullptr, "packet store");
                        // Earlier labels of this queue go first (queue order); the stamp is taken
                        // under the mutex like a label's.
                        recordDeferredLabels(localDevice.get(), submission.queue);
                        const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(store->address, bytes, ++eventSerial, submission.queue) : 4;
                        if (reason == 0 || reason == 5 || reason == 6) {
                            if (reason == 0) ++storesOnGpu;
                            else ++storesBehindCompletions;
                            wroteOnGpu = true;
                            drained = false;
                        } else if (reason == 1) {
                            GuestMemory::Write(store->address, bytes, 1);
                            ++storesOnCpu;
                            wroteOnGpu = true;
                            drained = false;
                        }
                    }
                }
                if (drained) ++storesDrained;
            }
            // Draws no longer drain: a recorded draw follows earlier recorded work in queue order, and
            // a synchronous draw's batch submits the recorder first (CommandBatch); the guest memory a
            // draw's preparation reads goes through the flush hook. Debug aid: APS5_DRAW_DRAIN=1 restores.
            static const bool drawDrain = std::getenv("APS5_DRAW_DRAIN") != nullptr;
            const bool drawPacket = opcode == 0x2d || opcode == 0x35;
            // Flips no longer drain either: the frame's batches are submitted and the presenter's blit
            // follows them on the same queue (see Driver::Present). Debug aid: APS5_SYNC_FLIP=1 restores.
            static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
            const bool drains = drainAll ? ((Pm4::AccessesMemory(header) && opcode != 0x16) || opcode == 0x42 || opcode == 0x46 || opcode == 0x58 || header == FlipPacketHeader)
                                         : (!wroteOnGpu && !orderedAlready && (opcode == 0x49 || opcode == 0x37 || opcode == 0x40 || opcode == 0x50 || opcode == 0x83 || (drawPacket && drawDrain) || (header == FlipPacketHeader && syncFlip)));
            if (drains) {
                // The GPU wait happens without the mutex: the batches are submitted under it, the
                // timeline value is waited for outside (the other workers keep recording), then the
                // completions of the batches up to that serial run under it again, in order, before
                // the packet's own CPU store. Work another thread submits meanwhile is not waited
                // for: a RELEASE_MEM only orders after earlier work, and a CPU store into a range
                // such work writes is caught by the flush hook. vkDeviceWaitIdle is not needed: the
                // presenter waits its own render fence and every other submission is synchronous.
                // The device is held by this shared_ptr across the wait and released under the
                // mutex (a replacement on another thread must not destroy it outside).
                // Debug aids: APS5_NO_UNLOCKED_DRAIN=1 and APS5_DRAIN_ALL=1 drain under the mutex.
                static const bool unlockedDrain = std::getenv("APS5_NO_UNLOCKED_DRAIN") == nullptr && !drainAll;
                // A drain is an ordering point for the collect memo (see GuestMemory::BumpCollectEpoch).
                bumpEpoch(&EpochBumps::drains);
                std::shared_ptr<VulkanDevice> draining;
                std::uint64_t epoch = 0;
                {
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    if (profile) {
                        ++drainCounts[header == FlipPacketHeader ? 0xffffu : opcode];
                        ++drainTotal;
                        if (std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
                            lastSyncReport = std::chrono::steady_clock::now();
                            reportSync();
                        }
                    }
                    draining = device.load();
                    // A drain is the ordering point: this worker's queued labels (an undecodable
                    // label packet skips the record in flushBetweenPackets) go in first, so the
                    // packet's own CPU store lands after them in queue order.
                    recordDeferredLabels(draining.get(), submission.queue);
                    if (draining != nullptr) {
                        if (unlockedDrain && draining->CanWaitUnlocked()) epoch = draining->SubmitAndEpoch();
                        else draining->WaitIdle();
                    }
                    if (epoch == 0) draining.reset();
                }
                if (epoch != 0) {
                    ++unlockedDrains;
                    try {
                        draining->WaitRecorded(epoch);
                    } catch (...) {
                        // A lost device: if Present replaced it meanwhile this copy is the last
                        // owner, and its recorder must still be torn down under the mutex.
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        draining.reset();
                        throw;
                    }
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    draining->ReapRecorded(epoch);
                    draining.reset();
                }
            }
            traceLabel(packet, submission.queue);
            // Waits do not count as progress; every other packet does, including while it runs.
            const bool waitPacket = opcode == 0x3c || opcode == 0x93;
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
            if (header == FlipPacketHeader) {
                CheckFailure();
                if (!drains) {
                    // Frame N's recorded batches go to the GPU now; the presenter's own submission
                    // follows them in queue order, so no wait is needed here and GpuReady returns at
                    // once (the title paces on flipPendingNum, which the presenter drops after showing
                    // the frame).
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    const auto localDevice = device.load();
                    // This worker's queued labels belong to the frame: recorded first (queue order),
                    // then everything goes out (an empty open batch submits nothing) unless the
                    // record submitted already (a second call would only scan the fences again).
                    if (!recordLabelsForPacket(localDevice.get(), submission.queue) && localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
                }
                // Each flip carries a fresh frame timing record; the per-stage frame statistics upstream
                // collects are not gathered on this path.
                auto frame = std::make_shared<FrameTiming>(++frameSerial);
                const auto now = FrameTiming::Clock::now();
                frame->IncludeSubmission(submission.serial, now, now, now, true);
                frame->SetFlip(submission.serial, cursor, now, now);
                submission.flips.at(cursor)->GpuReady(frame);
            } else if (opcode == 0x15) {
                timed(&WorkerProfile::dispatchMs, [&] { tolerate("dispatch", [&] { dispatch(queue, packet, submission); }); });
                Graphics::Recorder::CountRecordedWork();
            } else if (opcode == 0x16) {
                timed(&WorkerProfile::dispatchMs, [&] { tolerate("indirect dispatch", [&] { dispatchIndirect(queue, packet, submission); }); });
                Graphics::Recorder::CountRecordedWork();
            } else if (opcode == 0x3c || opcode == 0x93) {
                static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
                const auto waitStart = std::chrono::steady_clock::now();
                timed(&WorkerProfile::waitMs, [&] { waitMemory(packet, submission.queue, recent, submission.received); });
                if (traceGpu && std::chrono::steady_clock::now() - waitStart > std::chrono::milliseconds(200)) {
                    // List the rest of the submission to show what the stalled queue would have done next.
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
            } else if (opcode == 0x35 || opcode == 0x2d) {
                timed(&WorkerProfile::drawMs, [&] { tolerate("draw", [&] {
                    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
                    const auto color = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                    try {
                        draw(queue, packet, submission);
                        if (traceDraws) std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x ok\n",static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e));
                    } catch (const std::exception& error) {
                        if (traceDraws) std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x failed: %.160s\n",static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e), error.what());
                        // Name the color target so skipped draws can be matched against the scanout buffer.
                        char suffix[48];
                        std::snprintf(suffix, sizeof(suffix), " [color target 0x%llx]", static_cast<unsigned long long>(color));
                        // Debug aid: the first failing draw per color target and reason saves its register banks to
                        // draw_<target>_<reason hash>.regs.
                        static std::set<std::pair<std::uint64_t, std::string>> dumpedTargets;
                        const std::string reason = std::string(error.what()).substr(0, 48);
                        if (dumpedTargets.insert({color, reason}).second) {
                            char name[64];
                            std::snprintf(name, sizeof(name), "draw_%llx_%08x.regs", static_cast<unsigned long long>(color), static_cast<std::uint32_t>(std::hash<std::string>{}(reason)));
                            if (std::FILE* file = std::fopen(name, "w")) {
                                std::fprintf(file, "# %s\n", error.what());
                                recent.Each([&](const std::string& line) { std::fprintf(file, "# packet %s\n", line.c_str()); });
                                for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                                for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                                for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                                std::fclose(file);
                            }
                        }
                        throw std::runtime_error(error.what() + std::string(suffix));
                    }
                }); });
            } else if (opcode != 0x42 && opcode != 0x46 && opcode != 0x58) {
                if (!wroteOnGpu) Pm4::Execute(packet, queue);
            }
            if (drawPacket) Graphics::Recorder::CountRecordedWork();
            cursor += count;
        }
        // The submission's last labels and dispatches go to the GPU before it counts as completed
        // (Driver::WaitIdle and the game threads polling the labels must not depend on a later
        // packet of some queue). Checked lock-free first: an empty open batch needs no submit. A
        // compute worker submits without reaping (it must not wait under the mutex for queue 0).
        if (!deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            const auto localDevice = device.load();
            // This worker's deferred labels are recorded first (they belong to this submission).
            recordDeferredLabels(localDevice.get(), submission.queue);
            if (localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
        }
    }

    // An idle worker retires labels waiting in completion actions (see Recorder::AfterCompletions):
    // without it a label whose only consumer is a game thread, submitted last before the game waits
    // for it, would never land once every worker sleeps. Non-blocking (a fence status check under
    // the mutex, tried only); the caller polls it every millisecond while such labels exist.
    void reapCompletionLabels() {
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::try_to_lock);
        if (!gpuLock.owns_lock()) return;
        // Every idle worker polls this; once another worker reaped the labels there is nothing to
        // do, and the epoch bump below would only inflate the "reaps" and [guestmem] epoch counts.
        if (Graphics::Recorder::PendingCompletionLabels() == 0) return;
        // The write-backs the reap runs collect outside any submission: a fresh epoch, so no walk
        // of this worker's last packet is reused for them.
        bumpEpoch(&EpochBumps::reaps);
        if (const auto localDevice = device.load()) localDevice->ReapRecorded();
    }

    void run(std::uint32_t id) noexcept {
        OnWorkerThread() = true;
        // Named from the start, so a worker's fence waits before its first WAIT_REG_MEM land in its
        // own row of the [recorder] "fence waits by thread" report and "untagged" is exactly the
        // presenter and the game threads.
        GuestMemory::TagGpuLockThread(id);
        if (id == 0) StartWorkerSampler();
        Submission submission;
        try {
            for (;;) {
                submission = Submission{};
                static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
                {
                    std::unique_lock lock(mutex);
                    auto& pending = workers.at(id).pending;
                    if (traceGpu && pending.empty()) std::fprintf(stderr, "[gpu] %.1f idle queue=0x%x\n", TraceMs(), id);
                    const auto ready = [&] { return failure || stopping || !pending.empty(); };
                    // While completion labels are pending, the idle wait wakes every millisecond to
                    // reap them (the driver mutex is dropped for that: it is never held with the GPU
                    // mutex); otherwise it sleeps until work arrives.
                    while (!ready()) {
                        if (Graphics::Recorder::PendingCompletionLabels() == 0) {
                            changed.wait(lock, ready);
                            break;
                        }
                        if (changed.wait_for(lock, std::chrono::milliseconds(1), ready)) break;
                        lock.unlock();
                        reapCompletionLabels();
                        lock.lock();
                    }
                    rethrowFailure();
                    if (pending.empty()) {
                        break;
                    }
                    submission = std::move(pending.front());
                    pending.pop_front();
                }
                execute(submission);
                if (traceGpu) std::fprintf(stderr, "[gpu] %.1f done serial=%llu queue=0x%x\n", TraceMs(), static_cast<unsigned long long>(submission.serial), id);
                {
                    std::lock_guard lock(mutex);
                    rethrowFailure();
                    markCompleted(submission.serial);
                }
                changed.notify_all();
            }
        } catch (...) {
            const auto error = std::current_exception();
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& reason) {
                std::fprintf(stderr, "[gpu] worker failed: %s\n", reason.what());
            } catch (...) {
                std::fprintf(stderr, "[gpu] worker failed with a non-standard exception\n");
            }
            for (const auto& [offset, flip] : submission.flips) flip->Fail(error);
            ReportFailure(error);
            {
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                device.reset();
            }
        }
    }
};

}

void Submit(const Packet* packet, std::uint32_t queue) {
    Driver::Get().Submit(packet, queue);
}

void WaitIdle() {
    Driver::Get().WaitIdle();
}

void RegisterShader(const Shader* shader) {
    Driver::Get().RegisterShader(shader);
}

void SuspendPoint() {
    Driver::Get().SuspendPoint();
}

void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().RegisterVideoOutput(handle, output);
}

void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().UnregisterVideoOutput(handle, output);
}

void PresentClear(const PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, nullptr, opaque, gpuReady, context);
}

void PresentBuffer(const PresentationWindow& window, const DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, &buffer, true, gpuReady, context);
}

void ReleaseWindow(void* window) {
    Driver::Get().ReleaseWindow(window);
}

void ReportFailure(std::exception_ptr error) {
    Driver::Get().ReportFailure(error);
}

}

extern "C" void AgcDriverWaitIdle_nid_postfix() {
    AgcDriver::WaitIdle();
}

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    AgcDriver::RegisterShader(shader);
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() {
    AgcDriver::SuspendPoint();
}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::RegisterVideoOutput(handle, output);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::UnregisterVideoOutput(handle, output);
}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentClear(window, opaque, gpuReady, context);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentBuffer(window, buffer, gpuReady, context);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window) {
    AgcDriver::ReleaseWindow(window);
}

extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error) {
    AgcDriver::ReportFailure(error);
}
