#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMECAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMECAPTURE_HPP

#include "SceShaders.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver {

enum class FrameCallKind : std::uint32_t {
    RegisterShader = 1,
    ResolveShaderAbi,
    ResolveGraphicsAbi,
    ResolveGraphicsStagesAbi,
    RegisterVideoOutput,
    SetTessellationFactorRing,
    Submit,
    Flip,
    ReserveEopFlip,
    Suspend,
    Memory,
    FrameStart,
    End,
    DisplayBuffer,
    UnregisterVideoOutput,
};

struct FrameCall {
    FrameCallKind kind = FrameCallKind::End;
    std::uint64_t ordinal = 0;
    std::uint64_t timeNs = 0;
    std::uint64_t thread = 0;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    std::uint32_t queue = 0;
    std::uint32_t flags = 0;
    bool cpuFlip = false;
    std::uint64_t address = 0;
    std::uint64_t size = 0;
    std::vector<std::uint64_t> values;
    std::vector<ShaderRegister> context;
    std::vector<ShaderRegister> primitive;
};

enum class FrameMemoryKind : std::uint32_t { Range = 1, Snapshot, Delta, Reference };

enum class FrameRangeKind : std::uint32_t { Watched = 1, Unwatched, Section, Alias, File, Host };

struct FrameRangeAccess {
    static constexpr std::uint32_t Readable = 1;
    static constexpr std::uint32_t Writable = 2;
    static constexpr std::uint32_t Gpu = 4;
};

struct FrameMemoryChunk {
    FrameMemoryKind kind = FrameMemoryKind::Range;
    FrameRangeKind rangeKind = FrameRangeKind::Watched;
    std::uint32_t protection = 0;
    std::uint32_t access = 0;
    std::uint64_t group = 0;
    std::uint64_t completed = 0;
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    std::uint64_t compressedBytes = 0;
    std::uint64_t offset = 0;
};

class FrameCaptureWriter {
public:
    static constexpr std::string_view CallsName = "calls.bin";
    static constexpr std::string_view MemoryName = "memory.bin";
    static constexpr std::string_view HeaderName = "capture.json";
    static constexpr std::string_view CallsMagic = "aps5-frame-calls 1\n";
    static constexpr std::string_view MemoryMagic = "aps5-frame-memory 1\n";
    static constexpr std::size_t BlockBytes = 64u * 1024u;
    static constexpr std::size_t ChunkBytes = 4u * 1024u * 1024u;

    explicit FrameCaptureWriter(const std::filesystem::path& directory, int level = 1);
    FrameCaptureWriter(const FrameCaptureWriter&) = delete;
    FrameCaptureWriter& operator=(const FrameCaptureWriter&) = delete;

    void WriteCall(const FrameCall& call);
    void WriteRange(std::uint64_t address, std::uint64_t bytes, FrameRangeKind kind, std::uint32_t protection, std::uint32_t access);
    std::uint64_t WriteSnapshot(std::uint64_t address, std::span<const std::byte> bytes);
    void WriteDelta(std::uint64_t group, std::uint64_t completed, std::uint64_t address, std::span<const std::byte> bytes);
    void WriteReference(std::uint64_t index, std::uint64_t address, std::span<const std::byte> pixels);
    void Finish(std::string_view header);

    std::uint64_t RawBytes() const;
    std::uint64_t StoredBytes() const;
    const std::filesystem::path& Directory() const;

private:
    void writeChunk(const FrameMemoryChunk& chunk, std::span<const std::byte> payload);
    std::vector<std::byte> compress(std::span<const std::byte> bytes) const;

    std::filesystem::path directory;
    int level;
    mutable std::mutex mutex;
    std::ofstream calls;
    std::ofstream memory;
    std::uint64_t rawBytes = 0;
    std::uint64_t storedBytes = 0;
    bool finished = false;
};

class FrameCaptureReader {
public:
    explicit FrameCaptureReader(const std::filesystem::path& directory);

    const std::vector<FrameCall>& Calls() const;
    const std::vector<FrameMemoryChunk>& Chunks() const;
    const std::string& Header() const;
    bool Truncated() const;
    void Read(const FrameMemoryChunk& chunk, std::span<std::byte> destination) const;
    std::vector<std::byte> Read(const FrameMemoryChunk& chunk) const;

private:
    std::filesystem::path directory;
    std::vector<FrameCall> calls;
    std::vector<FrameMemoryChunk> chunks;
    std::string header;
    bool truncated = false;
};

std::string_view FrameCallName(FrameCallKind kind);
bool AllZero(std::span<const std::byte> bytes);

}

#endif
