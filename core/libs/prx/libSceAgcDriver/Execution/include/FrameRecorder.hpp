#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMERECORDER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMERECORDER_HPP

#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "SceShaders.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver {

struct StartedProgressSnapshot;

class FrameRecorder {
public:
    static constexpr std::uint64_t DefaultFrames = 10;
    static constexpr std::size_t ReservationHistory = 256;
    static constexpr std::size_t SnapshotPieceBytes = 64u * 1024u * 1024u;
    static constexpr std::uint64_t MinimumFreeBytes = std::uint64_t{4} << 30;
    static constexpr std::size_t HostPageBytes = 4096;

    static FrameRecorder* Active();

    void RegisterShader(const Shader* shader);
    void ResolveShaderAbi(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
    void ResolveGraphicsStagesAbi(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
    void ResolveGraphicsAbi(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);
    void RegisterVideoOutput(std::uint32_t handle);
    void UnregisterVideoOutput(std::uint32_t handle);
    void Submit(const Packet& packet, std::uint32_t queue, const std::function<void()>& accept);
    void Flip(const FlipInfo& info, const std::function<void()>& accept);
    void ReserveEopFlip(const FlipInfo& info, std::uint32_t token);
    void Suspend(const std::function<void()>& accept);
    void PresentBuffer(const DisplayBuffer& buffer);
    void FrameBoundary();
    static void NoteGpuStore(std::uint64_t address, std::span<const std::byte> bytes) {
        if (notingGpuStores.load(std::memory_order_relaxed)) Active()->noteGpuStore(address, bytes);
    }
    static void NoteGpuFill(std::uint64_t address, std::uint64_t bytes, const std::array<std::uint32_t, 4>& pattern) {
        if (notingGpuStores.load(std::memory_order_relaxed)) Active()->noteGpuFill(address, bytes, pattern);
    }
    void RewindTailFlips(std::uint64_t flips);

private:
    enum class State { Waiting, Recording, Done };

    struct StoreOwner {
        std::uint64_t group = 0;
        std::uint64_t accepted = 0;
    };
    struct StoreHazard {
        std::uint64_t group = 0;
        std::uint64_t page = 0;
        std::uint64_t serial = 0;
    };
    struct UnwatchedPage {
        std::uint64_t hash = 0;
        std::uint64_t stores = 0;
    };
    struct UnwatchedCounts {
        std::uint64_t deltas = 0;
        std::uint64_t changed = 0;
        std::uint64_t stored = 0;
    };
    static void observeDriverStore(std::uint64_t begin, std::uint64_t end, std::uint64_t serial);
    void noteDriverStore(std::uint64_t begin, std::uint64_t end, std::uint64_t serial);
    std::uint32_t holdMask(std::span<const std::pair<std::uint64_t, std::uint64_t>> blocks);
    void countUnwatched(std::span<const std::pair<std::uint64_t, std::uint64_t>> blocks);
    void recordUnwatched(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stores);
    void arm(std::span<const std::pair<std::uint64_t, std::uint64_t>> blocks, std::uint64_t group, std::uint64_t accepted);
    bool stopOnHazard();
    void awaitLateStores();

    struct Range {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        FrameRangeKind kind = FrameRangeKind::Watched;
        std::uint32_t protection = 0;
        std::uint32_t access = 0;
        bool readable = false;
        std::uint64_t since = 0;
    };

    FrameRecorder(std::filesystem::path directory, std::filesystem::path trigger, std::uint64_t atBoundary, std::uint64_t frames);
    struct GpuFill {
        std::uint64_t end = 0;
        std::array<std::uint32_t, 4> pattern{};
    };
    static constexpr std::size_t GpuStoreWordLimit = 64u * 1024u;

    struct Span {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        std::uint64_t since = 0;
    };

    FrameCall make(FrameCallKind kind);
    void record(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders = {}, const std::function<void()>& accept = {});
    void recordLocked(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders);
    void emitDelta(const StartedProgressSnapshot* decision = nullptr);
    static void collectShaderPages(std::span<const Shader* const> shaders, std::set<std::uint64_t>& blocks, std::set<std::uint64_t>& hostPages);
    void emitShaderDelta(std::span<const Shader* const> shaders, const StartedProgressSnapshot* decision = nullptr);
    void writeDelta(std::span<Span> spans, std::span<const std::uint64_t> hostPages, std::span<const Range> added = {}, const StartedProgressSnapshot* decision = nullptr);
    void frameBoundaryLocked();
    bool triggered();
    void start();
    void finish();
    void snapshot(const Range& range);
    void readGuest(std::uint64_t address, std::span<std::byte> bytes);
    void noteGpuStore(std::uint64_t address, std::span<const std::byte> bytes);
    void noteGpuFill(std::uint64_t address, std::uint64_t bytes, const std::array<std::uint32_t, 4>& pattern);
    void endGpuStores();
    void forgetGpuStores(std::uint64_t begin, std::uint64_t end);
    void keepTitleWords(std::uint64_t address, std::span<const std::byte> bytes, std::vector<std::pair<std::size_t, std::size_t>>& runs, bool zero);
    void writeReferenceFrames();
    bool abandonIfDiskLow();
    void stop(const char* reason);
    std::string header() const;
    std::vector<Range> committedRanges();
    std::vector<Range> newRanges();
    std::uint64_t blockSince(std::uint64_t block) const;

    std::filesystem::path directory;
    std::filesystem::path trigger;
    std::uint64_t atBoundary = 0;
    std::uint64_t frames = DefaultFrames;
    std::mutex mutex;
    State state = State::Waiting;
    std::unique_ptr<FrameCaptureWriter> writer;
    std::vector<FrameCall> prologue;
    std::set<std::uint64_t> prologueHostPages;
    std::deque<FrameCall> reservations;
    std::vector<Range> ranges;
    std::map<std::uint64_t, std::uint64_t> shaderMarks;
    std::map<std::uint64_t, std::uint64_t> hostPageHashes;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::pair<std::uint64_t, std::uint64_t>> unwatchedHashes;
    inline static std::atomic<bool> notingGpuStores{false};
    std::mutex gpuStoresMutex;
    std::map<std::uint64_t, std::uint32_t> gpuWords;
    std::map<std::uint64_t, GpuFill> gpuFills;
    std::uint64_t gpuStoreBytes = 0;
    std::mutex displayMutex;
    std::map<std::uint64_t, DisplayBuffer> displayBuffers;
    std::uint64_t ordinal = 0;
    std::uint64_t boundaries = 0;
    std::uint64_t rewindTailFlips = 0;
    std::uint64_t startBoundary = 0;
    std::uint64_t recordedFrames = 0;
    std::uint64_t startGeneration = 0;
    std::uint64_t deltaGroups = 0;
    std::uint64_t deltaBytes = 0;
    std::uint64_t snapshotBytes = 0;
    std::uint64_t recordedCalls = 0;
    std::uint64_t startAttempts = 0;
    std::uint64_t unreadableBytes = 0;
    std::uint64_t hostOnlySampleBytes = 0;
    std::uint64_t allocationsGeneration = 0;
    std::uint64_t lateRanges = 0;
    std::array<std::uint64_t, 17> submitDepths{};
    mutable std::mutex storeMutex;
    std::unordered_map<std::uint64_t, std::uint64_t> driverPages;
    std::uint64_t driverStores = 0;
    std::uint64_t startStores = 0;
    std::unordered_map<std::uint64_t, StoreOwner> storeOwners;
    StoreHazard storeHazard;
    std::uint64_t unattributedStores = 0;
    std::uint64_t unverifiedDeltas = 0;
    std::uint64_t inFlightDeltas = 0;
    std::uint64_t heldDeltas = 0;
    std::array<std::uint64_t, 13> holdReasons{};
    std::map<std::uint32_t, std::uint64_t> holdMasks;
    std::unordered_map<std::uint64_t, UnwatchedPage> unwatchedPages;
    std::map<std::pair<std::uint64_t, std::uint64_t>, UnwatchedCounts> unwatchedCounts;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> finishedWaitDwords;
    std::set<std::uint64_t> callThreads;
    static std::atomic<FrameRecorder*> observing;
    std::vector<std::string> warnings;
    std::chrono::steady_clock::time_point origin;
    std::chrono::steady_clock::time_point startedAt;
};

}

#endif
