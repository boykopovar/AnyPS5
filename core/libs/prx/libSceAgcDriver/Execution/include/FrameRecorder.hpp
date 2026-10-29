#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMERECORDER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMERECORDER_HPP

#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "SceShaders.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

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

private:
    enum class State { Waiting, Recording, Done };

    struct Range {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        FrameRangeKind kind = FrameRangeKind::Watched;
        std::uint32_t protection = 0;
        std::uint32_t access = 0;
        bool readable = false;
    };

    FrameRecorder(std::filesystem::path directory, std::filesystem::path trigger, std::uint64_t atBoundary, std::uint64_t frames);
    struct Span {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        std::uint64_t since = 0;
    };

    FrameCall make(FrameCallKind kind);
    void record(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders = {}, const std::function<void()>& accept = {});
    void recordLocked(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders);
    void emitDelta();
    void emitShaderDelta(std::span<const Shader* const> shaders);
    void writeDelta(std::span<const Span> spans, std::span<const std::uint64_t> hostPages, std::span<const Range> added = {});
    bool triggered();
    void start();
    void finish();
    void snapshot(const Range& range);
    void writeReferenceFrames();
    bool abandonIfDiskLow();
    void stop(const char* reason);
    std::string header() const;
    std::vector<Range> committedRanges();
    std::vector<Range> newRanges();

    std::filesystem::path directory;
    std::filesystem::path trigger;
    std::uint64_t atBoundary = 0;
    std::uint64_t frames = DefaultFrames;
    std::mutex mutex;
    State state = State::Waiting;
    std::unique_ptr<FrameCaptureWriter> writer;
    std::vector<FrameCall> prologue;
    std::deque<FrameCall> reservations;
    std::vector<Range> ranges;
    std::map<std::uint64_t, std::uint64_t> shaderMarks;
    std::map<std::uint64_t, std::uint64_t> hostPageHashes;
    std::mutex displayMutex;
    std::map<std::uint64_t, DisplayBuffer> displayBuffers;
    std::uint64_t ordinal = 0;
    std::uint64_t boundaries = 0;
    std::uint64_t startBoundary = 0;
    std::uint64_t recordedFrames = 0;
    std::uint64_t generation = 0;
    std::uint64_t deltaGroups = 0;
    std::uint64_t deltaBytes = 0;
    std::uint64_t snapshotBytes = 0;
    std::uint64_t recordedCalls = 0;
    std::uint64_t startAttempts = 0;
    std::uint64_t unreadableBytes = 0;
    std::uint64_t allocationsGeneration = 0;
    std::uint64_t lateRanges = 0;
    std::vector<std::string> warnings;
    std::chrono::steady_clock::time_point origin;
    std::chrono::steady_clock::time_point startedAt;
};

}

#endif
