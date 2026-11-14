#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMECAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAMECAPTURE_HPP

#include "SceShaders.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
    static constexpr std::string_view CallsMagic = "aps5-frame-calls 3\n";
    static constexpr std::string_view CallsMagicV2 = "aps5-frame-calls 2\n";
    static constexpr std::string_view CallsMagicV1 = "aps5-frame-calls 1\n";
    static constexpr std::string_view MemoryMagic = "aps5-frame-memory 2\n";
    static constexpr std::string_view MemoryMagicV1 = "aps5-frame-memory 1\n";
    static constexpr std::size_t BlockBytes = 64u * 1024u;
    static constexpr std::size_t ChunkBytes = 4u * 1024u * 1024u;

    using DeltaHash = std::array<std::uint64_t, 2> (*)(std::span<const std::byte> bytes);

    explicit FrameCaptureWriter(const std::filesystem::path& directory, int level = 1, DeltaHash deltaHash = nullptr);
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
    std::uint64_t SharedBytes() const;
    const std::filesystem::path& Directory() const;

private:
    struct DeltaPayload {
        std::uint64_t hash = 0;
        std::uint64_t check = 0;
        std::uint64_t offset = 0;
        std::uint64_t compressedBytes = 0;
    };

    std::uint64_t writeChunk(const FrameMemoryChunk& chunk, std::span<const std::byte> payload, std::uint64_t sharedOffset = 0);
    std::vector<std::byte> compress(std::span<const std::byte> bytes) const;
    bool storedEqual(const DeltaPayload& payload, std::span<const std::byte> bytes);

    std::filesystem::path directory;
    int level;
    mutable std::mutex mutex;
    std::ofstream calls;
    std::ofstream memory;
    std::uint64_t rawBytes = 0;
    std::uint64_t storedBytes = 0;
    std::uint64_t sharedBytes = 0;
    std::uint64_t memoryOffset = 0;
    std::map<std::pair<std::uint64_t, std::uint64_t>, DeltaPayload> deltaPayloads;
    DeltaHash deltaHash = nullptr;
    std::ifstream storedMemory;
    std::vector<std::byte> storedCompressed;
    std::vector<std::byte> storedBytesRead;
    bool finished = false;
};

class FrameCaptureReader {
public:
    explicit FrameCaptureReader(const std::filesystem::path& directory);
    ~FrameCaptureReader();

    const std::vector<FrameCall>& Calls() const;
    const std::vector<FrameMemoryChunk>& Chunks() const;
    const std::string& Header() const;
    std::uint32_t Version() const;
    bool Truncated() const;
    void Read(const FrameMemoryChunk& chunk, std::span<std::byte> destination) const;
    std::vector<std::byte> Read(const FrameMemoryChunk& chunk) const;

private:
    struct Stream;

    std::unique_ptr<Stream> takeStream() const;
    void returnStream(std::unique_ptr<Stream> stream) const;

    std::filesystem::path memoryPath;
    mutable std::mutex streamMutex;
    mutable std::vector<std::unique_ptr<Stream>> streams;
    std::vector<FrameCall> calls;
    std::vector<FrameMemoryChunk> chunks;
    std::string header;
    std::uint32_t version = 3;
    bool truncated = false;
};

std::string_view FrameCallName(FrameCallKind kind);
bool AllZero(std::span<const std::byte> bytes);
using GuestRead = void (*)(std::uint64_t address, std::span<std::byte> destination);
std::uint64_t ReadAroundRanges(std::uint64_t address, std::span<std::byte> bytes, std::span<const std::pair<std::uint64_t, std::uint64_t>> skipped, GuestRead read);

}

#endif
