#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameDumpIo.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, std::uint32_t size);

namespace {

using AgcDriver::FrameCall;
using AgcDriver::FrameCallKind;
using AgcDriver::FrameCaptureReader;
using AgcDriver::FrameMemoryChunk;
using AgcDriver::FrameMemoryKind;
using AgcDriver::FrameRangeAccess;
using AgcDriver::FrameRangeKind;
using Clock = std::chrono::steady_clock;

struct Dump {
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    std::filesystem::path path;
};

struct Options {
    std::filesystem::path directory;
    std::vector<Dump> dumps;
    std::filesystem::path frames;
    bool pace = true;
    bool strict = false;
    std::chrono::milliseconds paceTimeout{10000};
    std::chrono::milliseconds memoryPaceTimeout{10000};
    std::chrono::milliseconds releaseAfter{3};
    std::chrono::milliseconds drainTimeout{10000};
    bool release = true;
    bool synthesize = true;
};

class Frames {
public:
    void MarkReady() {
        std::lock_guard lock(mutex);
        ready.push_back(Clock::now());
    }

    void Failed(std::exception_ptr error) {
        std::lock_guard lock(mutex);
        if (!failure) failure = error;
    }

    std::vector<Clock::time_point> Times() const {
        std::lock_guard lock(mutex);
        return ready;
    }

    std::exception_ptr Failure() const {
        std::lock_guard lock(mutex);
        return failure;
    }

private:
    mutable std::mutex mutex;
    std::vector<Clock::time_point> ready;
    std::exception_ptr failure;
};

class ReplayFlip final : public AgcDriver::IFlipRequest {
public:
    explicit ReplayFlip(Frames& frames) : frames(frames) {}
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override { frames.MarkReady(); }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }

private:
    Frames& frames;
};

class ReplayRenderingWait final : public AgcDriver::IRenderingWait {
public:
    void Wait() override {}
};

class ReplayOutput final : public AgcDriver::IVideoOutput {
public:
    explicit ReplayOutput(Frames& frames) : frames(frames) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<ReplayFlip>(frames); }
    std::shared_ptr<AgcDriver::IRenderingWait> CaptureRenderingWait(std::uint32_t) override { return std::make_shared<ReplayRenderingWait>(); }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }

private:
    Frames& frames;
};

std::uint64_t headerNumber(const std::string& header, std::string_view key) {
    const auto quoted = "\"" + std::string(key) + "\":";
    const auto at = header.find(quoted);
    if (at == std::string::npos) throw std::runtime_error("frame replay: the capture header has no " + std::string(key));
    return std::strtoull(header.c_str() + at + quoted.size(), nullptr, 10);
}

void restore(const FrameCaptureReader& reader, const FrameMemoryChunk& chunk) {
    reader.Read(chunk, std::span(reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(chunk.address)), static_cast<std::size_t>(chunk.bytes)));
}

bool committedWritable(std::uint64_t address, std::uint64_t bytes) {
#ifdef _WIN32
    for (auto at = address; at < address + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(at)), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT) return false;
        const auto access = info.Protect & 0xffu;
        if ((info.Protect & PAGE_GUARD) != 0 || (access != PAGE_READWRITE && access != PAGE_EXECUTE_READWRITE)) return false;
        at = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

std::pair<std::uint64_t, std::uint64_t> progress();

struct Skipped {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::uint64_t bytes = 0;

    void Add(const FrameMemoryChunk& chunk) {
        ranges.emplace_back(chunk.address, chunk.address + chunk.bytes);
        bytes += chunk.bytes;
    }

    bool Overlaps(const FrameMemoryChunk& chunk) const {
        return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return chunk.address < range.second && range.first < chunk.address + chunk.bytes; });
    }
};

void mapHostRanges(const FrameCaptureReader& reader, Skipped& skipped) {
    std::vector<const FrameMemoryChunk*> host;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Range && chunk.rangeKind == FrameRangeKind::Host) host.push_back(&chunk);
    }
#ifdef _WIN32
    constexpr std::uint64_t Granularity = 64u * 1024u;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> envelopes;
    for (const auto* chunk : host) {
        const auto begin = chunk->address & ~(Granularity - 1);
        const auto end = (chunk->address + chunk->bytes + Granularity - 1) & ~(Granularity - 1);
        if (!envelopes.empty() && begin <= envelopes.back().second) envelopes.back().second = std::max(envelopes.back().second, end);
        else envelopes.emplace_back(begin, end);
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> refused;
    for (const auto& [begin, end] : envelopes) {
        if (VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(begin)), static_cast<SIZE_T>(end - begin), MEM_RESERVE, PAGE_NOACCESS) == nullptr) refused.emplace_back(begin, end);
    }
    for (const auto* chunk : host) {
        const bool blocked = std::any_of(refused.begin(), refused.end(), [&](const auto& range) { return chunk->address < range.second && range.first < chunk->address + chunk->bytes; });
        if (!blocked && VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(chunk->address)), static_cast<SIZE_T>(chunk->bytes), MEM_COMMIT, PAGE_READWRITE) != nullptr) continue;
        if ((chunk->access & FrameRangeAccess::Gpu) != 0) throw std::runtime_error("frame replay: cannot map the GPU-visible host range at " + std::to_string(chunk->address));
        skipped.Add(*chunk);
    }
#else
    for (const auto* chunk : host) skipped.Add(*chunk);
#endif
}

Skipped restoreMemory(const FrameCaptureReader& reader) {
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    if (!reader.Header().empty() && headerNumber(reader.Header(), "arena_base") != base) throw std::runtime_error("frame replay: the guest arena is at a different address than in the capture");
    progress();
    Skipped skipped;
    mapHostRanges(reader, skipped);
    {
        GuestAllocations::Mutation mutation;
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind != FrameMemoryKind::Range || chunk.rangeKind == FrameRangeKind::Host) continue;
            const bool gpu = (chunk.access & FrameRangeAccess::Gpu) != 0;
            if (committedWritable(chunk.address, chunk.bytes)) {
                if (!gpu) skipped.Add(chunk);
                else std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(chunk.address)), 0, static_cast<std::size_t>(chunk.bytes));
                continue;
            }
            auto* const pointer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(chunk.address));
            const auto bytes = static_cast<std::size_t>(chunk.bytes);
            try {
                GuestArena::GuestArenaMarkUsed_nid_postfix(pointer, bytes);
            } catch (const std::exception&) {
                if (gpu) throw;
                skipped.Add(chunk);
                continue;
            }
            GuestArena::GuestArenaCommit_nid_postfix(pointer, bytes, 0x04, bytes);
            mutation.Add(pointer, bytes, (chunk.access & FrameRangeAccess::Readable) != 0, (chunk.access & FrameRangeAccess::Writable) != 0, gpu);
        }
    }
    std::vector<const FrameMemoryChunk*> snapshot;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Snapshot && !skipped.Overlaps(chunk)) snapshot.push_back(&chunk);
    }
    const auto workers = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    for (std::size_t first = 0; first < snapshot.size(); first += workers) {
        std::vector<std::future<void>> pending;
        for (auto index = first; index < std::min(snapshot.size(), first + workers); ++index) {
            pending.push_back(std::async(std::launch::async, [&reader, chunk = snapshot[index]] { restore(reader, *chunk); }));
        }
        for (auto& future : pending) future.get();
    }
    return skipped;
}

std::pair<std::uint64_t, std::uint64_t> progress() {
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    AgcDriverSerialProgress_nid_postfix(&accepted, &completed);
    return {accepted, completed};
}

struct Replay {
    const Options& options;
    const FrameCaptureReader& reader;
    Frames frames;
    std::map<std::uint32_t, std::shared_ptr<ReplayOutput>> outputs;
    std::mutex memoryMutex;
    std::map<std::uint64_t, std::vector<const FrameMemoryChunk*>> groups;
    Skipped skipped;
    std::atomic<std::uint64_t> paceTimeouts{0};
    std::atomic<std::uint64_t> staleRegistrations{0};
    std::atomic<std::uint64_t> failedRegistrations{0};
    std::atomic<std::uint64_t> submits{0};
    std::atomic<std::uint64_t> deltaBytes{0};
    std::atomic<std::int64_t> paceWaitedNs{0};
    std::atomic<bool> stalled{false};
    std::atomic<std::uint64_t> stalledAt{0};
    std::atomic<std::uint64_t> earlyReleases{0};
    std::atomic<std::uint64_t> lateReleases{0};
    std::atomic<std::uint64_t> syntheticReleases{0};
    std::atomic<std::size_t> sequenced{0};
    std::uint64_t unfinished = 0;
    std::size_t threads = 0;
    bool recording = false;
    Clock::time_point startedAt{};
    Clock::time_point endedAt{};

    double paceWaitedSeconds() const {
        return static_cast<double>(paceWaitedNs.load()) / 1e9;
    }

    void report() const {
        const auto [accepted, completed] = progress();
        std::fprintf(stderr, "[frame-replay] %llu submits, %zu frames ready, %.1f s since the first frame, pacing waited %.1f s (%llu timeouts), serials %llu/%llu\n",
                     static_cast<unsigned long long>(submits.load()), frames.Times().size(), std::chrono::duration<double>(Clock::now() - startedAt).count(), paceWaitedSeconds(),
                     static_cast<unsigned long long>(paceTimeouts.load()), static_cast<unsigned long long>(completed), static_cast<unsigned long long>(accepted));
    }

    static bool paced(FrameCallKind kind) {
        return kind == FrameCallKind::Submit || kind == FrameCallKind::Flip || kind == FrameCallKind::ReserveEopFlip || kind == FrameCallKind::Suspend || kind == FrameCallKind::Memory;
    }

    void pace(const FrameCall& call) {
        if (!options.pace || !recording || !paced(call.kind) || call.accepted < call.completed) return;
        if (stalled.load()) {
            if (progress().second == stalledAt.load()) return;
            stalled.store(false);
        }
        const auto inFlight = call.accepted - call.completed;
        const bool memory = call.kind == FrameCallKind::Memory;
        const auto began = Clock::now();
        const auto deadline = began + (memory ? options.memoryPaceTimeout : options.paceTimeout);
        const auto waited = [&] { paceWaitedNs += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - began).count(); };
        while (true) {
            const auto [accepted, completed] = progress();
            if (accepted - completed <= inFlight) return waited();
            if (Clock::now() >= deadline) {
                waited();
                if (memory) return;
                stalled.store(true);
                stalledAt.store(completed);
                std::fprintf(stderr, "[frame-replay] pacing gave up at call %llu (%s): serials %llu/%llu, the capture had %llu in flight\n", static_cast<unsigned long long>(call.ordinal),
                             std::string(AgcDriver::FrameCallName(call.kind)).c_str(), static_cast<unsigned long long>(completed), static_cast<unsigned long long>(accepted), static_cast<unsigned long long>(inFlight));
                ++paceTimeouts;
                if (options.strict) throw std::runtime_error("frame replay: call " + std::to_string(call.ordinal) + " waited longer than the pacing timeout");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    void execute(const FrameCall& call) {
        pace(call);
        switch (call.kind) {
        case FrameCallKind::RegisterShader:
            registration([&] { AgcDriverRegisterShader_nid_postfix(shader(call.address)); });
            return;
        case FrameCallKind::ResolveShaderAbi:
            registration([&] { AgcDriverResolveShaderAbi_nid_postfix(shader(call.address), call.context, call.primitive); });
            return;
        case FrameCallKind::ResolveGraphicsAbi:
            registration([&] { AgcDriverResolveGraphicsAbi_nid_postfix(shader(call.values.at(0)), shader(call.values.at(1)), static_cast<std::uint32_t>(call.values.at(2))); });
            return;
        case FrameCallKind::ResolveGraphicsStagesAbi: {
            std::vector<const Shader*> stages;
            for (const auto value : call.values) stages.push_back(shader(value));
            registration([&] { AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, call.context, call.primitive); });
            return;
        }
        case FrameCallKind::RegisterVideoOutput: {
            const auto handle = static_cast<std::uint32_t>(call.values.at(0));
            auto output = std::make_shared<ReplayOutput>(frames);
            AgcDriverRegisterVideoOutput_nid_postfix(handle, output);
            outputs.emplace(handle, std::move(output));
            return;
        }
        case FrameCallKind::UnregisterVideoOutput: {
            const auto handle = static_cast<std::uint32_t>(call.values.at(0));
            AgcDriverUnregisterVideoOutput_nid_postfix(handle, outputs.at(handle));
            outputs.erase(handle);
            return;
        }
        case FrameCallKind::SetTessellationFactorRing:
            sceAgcDriverSetTFRing(reinterpret_cast<const volatile void*>(static_cast<std::uintptr_t>(call.address)), static_cast<std::uint32_t>(call.size));
            return;
        case FrameCallKind::Submit: {
            const Packet packet{reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(call.address)), static_cast<std::uint32_t>(call.size), static_cast<std::uint8_t>(call.flags), {}};
            if (call.queue == 0) sceAgcDriverSubmitDcb(&packet);
            else sceAgcDriverSubmitAcb(call.queue, &packet);
            if (++submits % 25 == 0) report();
            return;
        }
        case FrameCallKind::Flip:
            AgcDriverSubmitFlip_nid_postfix(flipInfo(call));
            return;
        case FrameCallKind::ReserveEopFlip: {
            const auto token = static_cast<std::uint32_t>(call.values.at(4));
            if (AgcDriverReserveEopFlipAt_nid_postfix(flipInfo(call), token) != token) throw std::runtime_error("frame replay: EOP flip token " + std::to_string(token) + " was not reserved");
            return;
        }
        case FrameCallKind::Suspend:
            AgcDriverSuspendPoint_nid_postfix();
            return;
        case FrameCallKind::Memory: {
            std::lock_guard lock(memoryMutex);
            const auto found = groups.find(call.values.at(0));
            if (found == groups.end()) return;
            for (const auto* chunk : found->second) {
                if (skipped.Overlaps(*chunk)) continue;
                restore(reader, *chunk);
                deltaBytes += chunk->bytes;
            }
            return;
        }
        case FrameCallKind::FrameStart:
        case FrameCallKind::End:
        case FrameCallKind::DisplayBuffer:
            return;
        }
        throw std::runtime_error("frame replay: unknown call kind");
    }

    static std::uint64_t load(std::uint64_t address, std::size_t bytes) {
        std::uint64_t value = 0;
        std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), bytes);
        return value;
    }

    static bool satisfies(const AgcDriver::AwaitedWait& wait, std::uint64_t value) {
        const auto masked = value & wait.mask;
        const auto reference = wait.reference & wait.mask;
        switch (wait.function) {
        case 1: return masked < reference;
        case 2: return masked <= reference;
        case 3: return masked == reference;
        case 4: return masked != reference;
        case 5: return masked >= reference;
        case 6: return masked > reference;
        default: return false;
        }
    }

    static std::optional<std::uint64_t> satisfying(const AgcDriver::AwaitedWait& wait, std::uint64_t current) {
        const auto keep = current & ~wait.mask;
        const auto reference = wait.reference & wait.mask;
        switch (wait.function) {
        case 2:
        case 3:
        case 5: return keep | reference;
        case 1: return reference == 0 ? std::nullopt : std::optional<std::uint64_t>(keep);
        case 4: return keep | (~reference & wait.mask);
        case 6: return reference == wait.mask ? std::nullopt : std::optional<std::uint64_t>(keep | ((reference + 1) & wait.mask));
        default: return std::nullopt;
        }
    }

    bool writeRecordedValue(const FrameCall& call, const AgcDriver::AwaitedWait& wait) {
        if (call.kind != FrameCallKind::Memory) return false;
        const auto found = groups.find(call.values.at(0));
        if (found == groups.end()) return false;
        const auto address = wait.address;
        for (const auto* chunk : found->second) {
            if (chunk->address > address || address + 4 > chunk->address + chunk->bytes || skipped.Overlaps(*chunk)) continue;
            const auto bytes = reader.Read(*chunk);
            const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(wait.bytes == 8 ? 8 : 4, chunk->address + chunk->bytes - address));
            const auto* const recorded = bytes.data() + (address - chunk->address);
            auto* const target = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(address));
            if (std::memcmp(target, recorded, length) == 0) return false;
            std::uint64_t value = 0;
            std::memcpy(&value, recorded, length);
            if (wait.function != 0 && !satisfies(wait, value)) return false;
            std::memcpy(target, recorded, length);
            return true;
        }
        return false;
    }

    void releaseAwaited(std::span<const FrameCall> calls, std::size_t from) {
        std::lock_guard lock(memoryMutex);
        std::array<AgcDriver::AwaitedWait, 64> awaited{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(awaited.data(), awaited.size());
        for (std::size_t slot = 0; slot < count; ++slot) {
            const auto& wait = awaited[slot];
            if (wait.address == 0) continue;
            const std::size_t bytes = wait.bytes == 8 ? 8 : 4;
            if (wait.function != 0 && satisfies(wait, load(wait.address, bytes))) continue;
            bool released = false;
            for (auto index = from; index < calls.size() && !released; ++index) released = writeRecordedValue(calls[index], wait);
            for (auto index = std::min(from, calls.size()); index > 0 && !released; --index) {
                released = writeRecordedValue(calls[index - 1], wait);
                if (released) ++lateReleases;
            }
            if (!released && options.synthesize) {
                if (const auto value = satisfying(wait, load(wait.address, bytes))) {
                    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(wait.address)), &*value, bytes);
                    ++syntheticReleases;
                    released = true;
                }
            }
            if (released) ++earlyReleases;
        }
    }

    void runWindow(std::span<const FrameCall> calls) {
        std::map<std::uint64_t, std::vector<std::size_t>> byThread;
        for (std::size_t index = 0; index < calls.size(); ++index) byThread[calls[index].thread].push_back(index);
        threads = byThread.size();
        std::mutex mutex;
        std::condition_variable turn;
        std::size_t started = 0;
        std::exception_ptr failure;
        std::atomic<bool> finished{false};
        std::thread breaker([&] {
            if (!options.release) return;
            auto lastCompleted = progress().second;
            auto lastProgress = Clock::now();
            while (!finished.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const auto completed = progress().second;
                if (completed != lastCompleted) {
                    lastCompleted = completed;
                    lastProgress = Clock::now();
                    continue;
                }
                if (Clock::now() - lastProgress < options.releaseAfter) continue;
                releaseAwaited(calls, sequenced.load());
                lastProgress = Clock::now();
            }
        });
        std::vector<std::thread> workers;
        for (const auto& entry : byThread) {
            workers.emplace_back([&, &indices = entry.second] {
                for (const auto index : indices) {
                    std::unique_lock lock(mutex);
                    turn.wait(lock, [&] { return started == index || failure != nullptr; });
                    if (failure != nullptr) return;
                    const auto advance = [&] {
                        ++started;
                        sequenced.store(started);
                        lock.unlock();
                        turn.notify_all();
                    };
                    const auto kind = calls[index].kind;
                    const bool ordered = kind == FrameCallKind::Memory || kind == FrameCallKind::RegisterVideoOutput || kind == FrameCallKind::UnregisterVideoOutput;
                    if (!ordered) advance();
                    try {
                        execute(calls[index]);
                        if (ordered) advance();
                    } catch (...) {
                        if (!lock.owns_lock()) lock.lock();
                        if (failure == nullptr) failure = std::current_exception();
                        turn.notify_all();
                        return;
                    }
                }
            });
        }
        for (auto& worker : workers) worker.join();
        if (failure == nullptr) drain();
        finished.store(true);
        breaker.join();
        if (failure != nullptr) std::rethrow_exception(failure);
    }

    void drain() {
        auto lastCompleted = progress().second;
        auto lastProgress = Clock::now();
        while (true) {
            const auto [accepted, completed] = progress();
            unfinished = accepted - completed;
            if (unfinished == 0) return;
            if (completed != lastCompleted) {
                lastCompleted = completed;
                lastProgress = Clock::now();
            } else if (Clock::now() - lastProgress >= options.drainTimeout) {
                std::fprintf(stderr, "[frame-replay] %llu submissions did not complete after the last captured call (serials %llu/%llu)\n", static_cast<unsigned long long>(unfinished),
                             static_cast<unsigned long long>(completed), static_cast<unsigned long long>(accepted));
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    template <typename TFunction>
    void registration(TFunction&& function) {
        try {
            function();
        } catch (const std::exception& error) {
            if (recording && options.strict) throw;
            ++(recording ? failedRegistrations : staleRegistrations);
            std::fprintf(stderr, "[frame-replay] %s registration failed: %s\n", recording ? "recorded" : "prologue", error.what());
        }
    }

    static const Shader* shader(std::uint64_t address) {
        return reinterpret_cast<const Shader*>(static_cast<std::uintptr_t>(address));
    }

    static AgcDriver::FlipInfo flipInfo(const FrameCall& call) {
        return {static_cast<std::uint32_t>(call.values.at(0)), static_cast<std::int32_t>(call.values.at(1)), static_cast<std::uint32_t>(call.values.at(2)), static_cast<std::int64_t>(call.values.at(3))};
    }
};

int usage() {
    std::fprintf(stderr, "usage: agc_frame_replay <capture directory> [--no-pace] [--no-release] [--release-after-ms <n>] [--strict]\n"
                         "                        [--dump <address>:<bytes>:<file>]...\n"
                         "  replays the driver calls and guest memory that APS5_FRAME_CAPTURE=<directory> recorded, through this\n"
                         "  build's AGC driver with no title running, and prints one JSON line with the frame times\n"
                         "  --no-pace           issues calls without waiting for the in-flight depth the title had\n"
                         "  --no-release        never copies recorded CPU stores early to release a GPU wait\n"
                         "  --release-after-ms  milliseconds without a completion before awaited dwords are released (3)\n"
                         "  --no-synthesize     releases a wait only with a value the capture recorded, never with its reference value\n"
                         "  --strict            fails on a pacing timeout or a failed registration inside the captured frames\n"
                         "  --dump              writes guest memory to a file after the replay (hexadecimal or decimal)\n"
                         "  --frames <dir>      writes each recorded and replayed display buffer as reference-<n>.bmp and replay-<n>.bmp\n");
    return 2;
}

Dump parseDump(std::string_view text) {
    const auto first = text.find(':');
    const auto second = first == std::string_view::npos ? std::string_view::npos : text.find(':', first + 1);
    if (second == std::string_view::npos) throw std::invalid_argument("frame replay: --dump takes <address>:<bytes>:<file>");
    const auto number = [](std::string_view part) {
        const std::string value(part);
        char* end = nullptr;
        const auto parsed = std::strtoull(value.c_str(), &end, 0);
        if (value.empty() || end != value.c_str() + value.size()) throw std::invalid_argument("frame replay: '" + value + "' is not a number");
        return static_cast<std::uint64_t>(parsed);
    };
    return {number(text.substr(0, first)), number(text.substr(first + 1, second - first - 1)), std::filesystem::path(std::string(text.substr(second + 1)))};
}

void writeDump(const Dump& dump) {
    auto* const pointer = reinterpret_cast<const std::byte*>(static_cast<std::uintptr_t>(dump.address));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, static_cast<std::size_t>(dump.bytes));
    std::ofstream file(dump.path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(pointer), static_cast<std::streamsize>(dump.bytes));
    if (!file.good()) throw std::runtime_error("frame replay: cannot write " + dump.path.string());
}

struct FrameCheck {
    std::size_t compared = 0;
    double identical = 1.0;
    int maxDifference = 0;
};

void writeFrame(const std::filesystem::path& path, const AgcDriver::DisplayBuffer& buffer, std::span<const std::byte> pixels) {
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    const auto failure = file != nullptr ? AgcDriver::WriteBmpAndClose_nid_no_patch(file, buffer.width, buffer.height, pixels) : AgcDriver::FrameDumpIoFailure{"open", errno};
    if (failure) std::fprintf(stderr, "[frame-replay] cannot write %s (%s failed)\n", path.string().c_str(), failure.operation);
}

FrameCheck compareFrames(const FrameCaptureReader& reader, const Options& options) {
    FrameCheck check;
    if (!options.frames.empty()) std::filesystem::create_directories(options.frames);
    for (const auto& call : reader.Calls()) {
        if (call.kind != FrameCallKind::DisplayBuffer || call.values.size() < 9) continue;
        AgcDriver::DisplayBuffer buffer{};
        buffer.address = call.address;
        buffer.pixelFormat = call.values[1];
        buffer.width = static_cast<std::uint32_t>(call.values[2]);
        buffer.height = static_cast<std::uint32_t>(call.values[3]);
        buffer.tilingMode = static_cast<std::uint32_t>(call.values[4]);
        buffer.pitchInPixel = static_cast<std::uint32_t>(call.values[5]);
        buffer.dccAddress = call.values[6];
        buffer.dccClearColor = call.values[7];
        buffer.alignment = static_cast<std::uint32_t>(call.values[8]);
        const auto chunk = std::find_if(reader.Chunks().begin(), reader.Chunks().end(), [&](const FrameMemoryChunk& candidate) { return candidate.kind == FrameMemoryKind::Reference && candidate.group == call.values[0]; });
        if (chunk == reader.Chunks().end()) continue;
        const auto reference = reader.Read(*chunk);
        const auto replayed = AgcDriver::ReadDisplayBuffer(buffer);
        ++check.compared;
        std::size_t same = 0;
        const auto pixels = std::min(reference.size(), replayed.size()) / 4;
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            bool equal = true;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const auto difference = std::abs(static_cast<int>(reference[pixel * 4 + channel]) - static_cast<int>(replayed[pixel * 4 + channel]));
                check.maxDifference = std::max(check.maxDifference, difference);
                equal = equal && difference == 0;
            }
            same += equal ? 1 : 0;
        }
        const auto total = std::max(reference.size(), replayed.size()) / 4;
        check.identical = std::min(check.identical, total == 0 ? 1.0 : static_cast<double>(same) / static_cast<double>(total));
        if (options.frames.empty()) continue;
        const auto index = std::to_string(call.values[0]);
        writeFrame(options.frames / ("reference-" + index + ".bmp"), buffer, reference);
        writeFrame(options.frames / ("replay-" + index + ".bmp"), buffer, replayed);
    }
    return check;
}

int run(const Options& options) {
    const auto loadStart = Clock::now();
    const FrameCaptureReader reader(options.directory);
    if (reader.Truncated()) std::fprintf(stderr, "[frame-replay] the capture is truncated; replaying the complete records\n");
    auto skipped = restoreMemory(reader);
    const auto loaded = Clock::now();
    Replay replay{options, reader};
    replay.skipped = std::move(skipped);
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) replay.groups[chunk.group].push_back(&chunk);
    }
    const auto& calls = reader.Calls();
    const auto frameStart = std::find_if(calls.begin(), calls.end(), [](const FrameCall& call) { return call.kind == FrameCallKind::FrameStart; });
    for (auto call = calls.begin(); call != frameStart; ++call) replay.execute(*call);
    replay.recording = true;
    replay.startedAt = Clock::now();
    if (frameStart != calls.end()) replay.runWindow(std::span(calls).subspan(static_cast<std::size_t>(frameStart - calls.begin()) + 1));
    if (frameStart == calls.end()) AgcDriverWaitIdle_nid_postfix();
    replay.endedAt = Clock::now();
    if (const auto failure = replay.frames.Failure()) std::rethrow_exception(failure);
    FrameCheck check;
    if (replay.unfinished == 0) {
        for (const auto& dump : options.dumps) writeDump(dump);
        check = compareFrames(reader, options);
    } else if (!options.dumps.empty()) {
        std::fprintf(stderr, "[frame-replay] no dumps written: the GPU work did not finish\n");
    }
    const auto ready = replay.frames.Times();
    std::vector<double> frameMs;
    for (std::size_t index = 1; index < ready.size(); ++index) frameMs.push_back(std::chrono::duration<double, std::milli>(ready[index] - ready[index - 1]).count());
    std::string frameList;
    for (const auto value : frameMs) frameList += (frameList.empty() ? "" : ",") + std::to_string(static_cast<long long>(value));
    std::sort(frameMs.begin(), frameMs.end());
    const auto percentile = [&](double fraction) { return frameMs.empty() ? 0.0 : frameMs[std::min(frameMs.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(frameMs.size())))]; };
    const auto seconds = std::chrono::duration<double>(replay.endedAt - replay.startedAt).count();
    std::printf("{\"frames\":%zu,\"replay_s\":%.3f,\"fps\":%.3f,\"frame_ms\":{\"p50\":%.3f,\"p90\":%.3f,\"max\":%.3f,\"each\":[%s]},\"submits\":%llu,\"delta_mib\":%.1f,\"pace_timeouts\":%llu,\"pace_wait_s\":%.3f,\"stale_registrations\":%llu,\"failed_registrations\":%llu,\"skipped_mib\":%.1f,\"threads\":%zu,\"released_waits\":%llu,\"of_them_from_earlier_writes\":%llu,\"of_them_synthesized\":%llu,\"unfinished\":%llu,\"frames_compared\":%zu,\"pixels_identical\":%.6f,\"max_channel_diff\":%d,\"load_s\":%.3f}\n",
                ready.size(), seconds, seconds > 0 ? static_cast<double>(ready.size()) / seconds : 0.0, percentile(0.5), percentile(0.9), frameMs.empty() ? 0.0 : frameMs.back(), frameList.c_str(),
                static_cast<unsigned long long>(replay.submits.load()), static_cast<double>(replay.deltaBytes.load()) / (1024.0 * 1024.0), static_cast<unsigned long long>(replay.paceTimeouts.load()),
                replay.paceWaitedSeconds(), static_cast<unsigned long long>(replay.staleRegistrations.load()), static_cast<unsigned long long>(replay.failedRegistrations.load()),
                static_cast<double>(replay.skipped.bytes) / (1024.0 * 1024.0), replay.threads, static_cast<unsigned long long>(replay.earlyReleases.load()), static_cast<unsigned long long>(replay.lateReleases.load()),
                static_cast<unsigned long long>(replay.syntheticReleases.load()),
                static_cast<unsigned long long>(replay.unfinished), check.compared, check.identical, check.maxDifference, std::chrono::duration<double>(loaded - loadStart).count());
    std::fflush(stdout);
    return replay.unfinished == 0 ? 0 : 3;
}

}

int main(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--no-pace") options.pace = false;
        else if (argument == "--no-release") options.release = false;
        else if (argument == "--no-synthesize") options.synthesize = false;
        else if (argument == "--release-after-ms" && index + 1 < argc) options.releaseAfter = std::chrono::milliseconds(std::strtoll(argv[++index], nullptr, 10));
        else if (argument == "--strict") options.strict = true;
        else if (argument == "--dump" && index + 1 < argc) options.dumps.push_back(parseDump(argv[++index]));
        else if (argument == "--frames" && index + 1 < argc) options.frames = argv[++index];
        else if (!argument.starts_with("-") && options.directory.empty()) options.directory = argument;
        else return usage();
    }
    if (options.directory.empty()) return usage();
#ifdef _WIN32
    _putenv_s("APS5_FRAME_CAPTURE", "");
#else
    unsetenv("APS5_FRAME_CAPTURE");
#endif
    int code = 2;
    try {
        code = run(options);
        if (code == 0) {
            AgcDriverShutdown_nid_postfix();
            return 0;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "agc_frame_replay: %s\n", error.what());
    }
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(code);
}
