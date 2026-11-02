#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include <zstd.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <future>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace AgcDriver {
namespace {

static_assert(std::endian::native == std::endian::little, "frame captures are stored in little-endian order");

constexpr std::size_t ChunkHeaderBytes = 64;

void requireCapture(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("frame capture: " + reason);
}

class Encoder {
public:
    template <typename TValue>
    void Put(TValue value) {
        const auto at = bytes.size();
        bytes.resize(at + sizeof(TValue));
        std::memcpy(bytes.data() + at, &value, sizeof(TValue));
    }

    std::vector<std::byte> bytes;
};

class Decoder {
public:
    explicit Decoder(std::span<const std::byte> bytes) : bytes(bytes) {}

    template <typename TValue>
    TValue Take(const char* what) {
        requireCapture(sizeof(TValue) <= bytes.size() - at, std::string("record ends inside ") + what);
        TValue value{};
        std::memcpy(&value, bytes.data() + at, sizeof(TValue));
        at += sizeof(TValue);
        return value;
    }

    std::size_t Count(std::size_t elementBytes, const char* what) {
        const auto count = Take<std::uint32_t>(what);
        requireCapture(count <= (bytes.size() - at) / elementBytes, std::string("count of ") + what + " exceeds its record");
        return count;
    }

    bool Done() const { return at == bytes.size(); }

private:
    std::span<const std::byte> bytes;
    std::size_t at = 0;
};

void putRegisters(Encoder& encoder, const std::vector<ShaderRegister>& registers) {
    encoder.Put(static_cast<std::uint32_t>(registers.size()));
    for (const auto& value : registers) {
        encoder.Put(value.offset);
        encoder.Put(value.value);
    }
}

std::vector<ShaderRegister> takeRegisters(Decoder& decoder) {
    std::vector<ShaderRegister> registers(decoder.Count(8, "registers"));
    for (auto& value : registers) {
        value.offset = decoder.Take<std::uint32_t>("register offset");
        value.value = decoder.Take<std::uint32_t>("register value");
    }
    return registers;
}

std::vector<std::byte> encodeCall(const FrameCall& call) {
    Encoder encoder;
    encoder.Put(call.ordinal);
    encoder.Put(call.timeNs);
    encoder.Put(call.thread);
    encoder.Put(call.accepted);
    encoder.Put(call.completed);
    encoder.Put(call.queue);
    encoder.Put(call.flags);
    encoder.Put(static_cast<std::uint32_t>(call.cpuFlip ? 1u : 0u));
    encoder.Put(call.address);
    encoder.Put(call.size);
    encoder.Put(static_cast<std::uint32_t>(call.values.size()));
    for (const auto value : call.values) encoder.Put(value);
    putRegisters(encoder, call.context);
    putRegisters(encoder, call.primitive);
    return std::move(encoder.bytes);
}

FrameCall decodeCall(FrameCallKind kind, std::span<const std::byte> payload) {
    Decoder decoder(payload);
    FrameCall call;
    call.kind = kind;
    call.ordinal = decoder.Take<std::uint64_t>("ordinal");
    call.timeNs = decoder.Take<std::uint64_t>("time");
    call.thread = decoder.Take<std::uint64_t>("thread");
    call.accepted = decoder.Take<std::uint64_t>("accepted serial");
    call.completed = decoder.Take<std::uint64_t>("completed serial");
    call.queue = decoder.Take<std::uint32_t>("queue");
    call.flags = decoder.Take<std::uint32_t>("flags");
    const auto cpuFlip = decoder.Take<std::uint32_t>("cpu flip");
    requireCapture(cpuFlip <= 1, "cpu flip is not 0 or 1");
    call.cpuFlip = cpuFlip == 1;
    call.address = decoder.Take<std::uint64_t>("address");
    call.size = decoder.Take<std::uint64_t>("size");
    call.values.resize(decoder.Count(8, "values"));
    for (auto& value : call.values) value = decoder.Take<std::uint64_t>("value");
    call.context = takeRegisters(decoder);
    call.primitive = takeRegisters(decoder);
    requireCapture(decoder.Done(), "call record has trailing bytes");
    return call;
}

bool knownCall(std::uint32_t kind) {
    return kind >= static_cast<std::uint32_t>(FrameCallKind::RegisterShader) && kind <= static_cast<std::uint32_t>(FrameCallKind::UnregisterVideoOutput);
}

std::vector<std::byte> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    requireCapture(file.is_open(), "cannot open " + path.string());
    std::vector<std::byte> bytes;
    file.seekg(0, std::ios::end);
    bytes.resize(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    requireCapture(file.good() || bytes.empty(), "cannot read " + path.string());
    return bytes;
}

void requireMagic(std::span<const std::byte> bytes, std::string_view magic, const std::filesystem::path& path) {
    requireCapture(bytes.size() >= magic.size() && std::memcmp(bytes.data(), magic.data(), magic.size()) == 0, path.string() + " is not a frame capture file");
}

struct Run {
    std::uint64_t address;
    std::span<const std::byte> bytes;
};

}

std::uint64_t ReadAroundRanges(std::uint64_t address, std::span<std::byte> bytes, std::span<const std::pair<std::uint64_t, std::uint64_t>> skipped, GuestRead read) {
    auto cursor = address;
    const auto end = address + bytes.size();
    std::uint64_t zeroed = 0;
    for (const auto& [begin, size] : skipped) {
        const auto skip = std::max(begin, cursor);
        const auto stop = std::min(begin + size, end);
        if (stop <= skip) continue;
        read(cursor, bytes.subspan(static_cast<std::size_t>(cursor - address), static_cast<std::size_t>(skip - cursor)));
        std::memset(bytes.data() + (skip - address), 0, static_cast<std::size_t>(stop - skip));
        zeroed += stop - skip;
        cursor = stop;
    }
    read(cursor, bytes.subspan(static_cast<std::size_t>(cursor - address)));
    return zeroed;
}

bool AllZero(std::span<const std::byte> bytes) {
    constexpr std::size_t Word = sizeof(std::uint64_t);
    std::size_t at = 0;
    for (; at + Word <= bytes.size(); at += Word) {
        std::uint64_t value = 0;
        std::memcpy(&value, bytes.data() + at, Word);
        if (value != 0) return false;
    }
    return std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
}

std::string_view FrameCallName(FrameCallKind kind) {
    switch (kind) {
    case FrameCallKind::RegisterShader: return "register-shader";
    case FrameCallKind::ResolveShaderAbi: return "resolve-shader-abi";
    case FrameCallKind::ResolveGraphicsAbi: return "resolve-graphics-abi";
    case FrameCallKind::ResolveGraphicsStagesAbi: return "resolve-graphics-stages-abi";
    case FrameCallKind::RegisterVideoOutput: return "register-video-output";
    case FrameCallKind::UnregisterVideoOutput: return "unregister-video-output";
    case FrameCallKind::SetTessellationFactorRing: return "set-tf-ring";
    case FrameCallKind::Submit: return "submit";
    case FrameCallKind::Flip: return "flip";
    case FrameCallKind::ReserveEopFlip: return "reserve-eop-flip";
    case FrameCallKind::Suspend: return "suspend";
    case FrameCallKind::Memory: return "memory";
    case FrameCallKind::FrameStart: return "frame-start";
    case FrameCallKind::End: return "end";
    case FrameCallKind::DisplayBuffer: return "display-buffer";
    }
    throw std::runtime_error("frame capture: unknown call kind " + std::to_string(static_cast<std::uint32_t>(kind)));
}

FrameCaptureWriter::FrameCaptureWriter(const std::filesystem::path& directory, int level) : directory(directory), level(level) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    requireCapture(!error && std::filesystem::is_directory(directory), "cannot create the directory " + directory.string());
    for (const auto name : {CallsName, MemoryName, HeaderName}) std::filesystem::remove(directory / name, error);
    calls.open(directory / CallsName, std::ios::binary | std::ios::trunc);
    memory.open(directory / MemoryName, std::ios::binary | std::ios::trunc);
    requireCapture(calls.is_open() && memory.is_open(), "cannot create the capture files in " + directory.string());
    calls.write(CallsMagic.data(), static_cast<std::streamsize>(CallsMagic.size()));
    memory.write(MemoryMagic.data(), static_cast<std::streamsize>(MemoryMagic.size()));
    requireCapture(calls.good() && memory.good(), "cannot write the capture files in " + directory.string());
}

void FrameCaptureWriter::WriteCall(const FrameCall& call) {
    FrameCallName(call.kind);
    const auto payload = encodeCall(call);
    requireCapture(payload.size() <= UINT32_MAX, "call record too large");
    std::lock_guard lock(mutex);
    requireCapture(!finished, "call written after the capture finished");
    const auto kind = static_cast<std::uint32_t>(call.kind);
    const auto bytes = static_cast<std::uint32_t>(payload.size());
    calls.write(reinterpret_cast<const char*>(&kind), sizeof(kind));
    calls.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
    calls.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    requireCapture(calls.good(), "cannot write " + (directory / CallsName).string());
}

void FrameCaptureWriter::WriteRange(std::uint64_t address, std::uint64_t bytes, FrameRangeKind kind, std::uint32_t protection, std::uint32_t access) {
    FrameMemoryChunk chunk;
    chunk.kind = FrameMemoryKind::Range;
    chunk.rangeKind = kind;
    chunk.protection = protection;
    chunk.access = access;
    chunk.address = address;
    chunk.bytes = bytes;
    std::lock_guard lock(mutex);
    writeChunk(chunk, {});
}

std::uint64_t FrameCaptureWriter::WriteSnapshot(std::uint64_t address, std::span<const std::byte> bytes) {
    std::vector<Run> runs;
    for (std::size_t at = 0; at < bytes.size();) {
        const auto block = bytes.subspan(at, std::min(BlockBytes, bytes.size() - at));
        if (AllZero(block)) {
            at += block.size();
            continue;
        }
        if (!runs.empty() && runs.back().address + runs.back().bytes.size() == address + at && runs.back().bytes.size() + block.size() <= ChunkBytes) {
            runs.back().bytes = bytes.subspan(runs.back().address - address, runs.back().bytes.size() + block.size());
        } else {
            runs.push_back({address + at, block});
        }
        at += block.size();
    }
    const auto workers = std::max<std::size_t>(1, std::thread::hardware_concurrency() / 2);
    std::uint64_t stored = 0;
    for (std::size_t first = 0; first < runs.size(); first += workers) {
        const auto last = std::min(runs.size(), first + workers);
        std::vector<std::future<std::vector<std::byte>>> compressed;
        for (auto index = first; index < last; ++index) {
            compressed.push_back(std::async(std::launch::async, [this, run = runs[index]] { return compress(run.bytes); }));
        }
        std::lock_guard lock(mutex);
        for (auto index = first; index < last; ++index) {
            const auto payload = compressed[index - first].get();
            FrameMemoryChunk chunk;
            chunk.kind = FrameMemoryKind::Snapshot;
            chunk.address = runs[index].address;
            chunk.bytes = runs[index].bytes.size();
            chunk.compressedBytes = payload.size();
            writeChunk(chunk, payload);
            stored += chunk.bytes;
        }
    }
    return stored;
}

void FrameCaptureWriter::WriteDelta(std::uint64_t group, std::uint64_t completed, std::uint64_t address, std::span<const std::byte> bytes) {
    for (std::size_t at = 0; at < bytes.size(); at += ChunkBytes) {
        const auto part = bytes.subspan(at, std::min(ChunkBytes, bytes.size() - at));
        const auto payload = compress(part);
        FrameMemoryChunk chunk;
        chunk.kind = FrameMemoryKind::Delta;
        chunk.group = group;
        chunk.completed = completed;
        chunk.address = address + at;
        chunk.bytes = part.size();
        chunk.compressedBytes = payload.size();
        std::lock_guard lock(mutex);
        writeChunk(chunk, payload);
    }
}

void FrameCaptureWriter::WriteReference(std::uint64_t index, std::uint64_t address, std::span<const std::byte> pixels) {
    const auto payload = compress(pixels);
    FrameMemoryChunk chunk;
    chunk.kind = FrameMemoryKind::Reference;
    chunk.group = index;
    chunk.address = address;
    chunk.bytes = pixels.size();
    chunk.compressedBytes = payload.size();
    std::lock_guard lock(mutex);
    writeChunk(chunk, payload);
}

void FrameCaptureWriter::Finish(std::string_view header) {
    std::lock_guard lock(mutex);
    requireCapture(!finished, "capture finished twice");
    finished = true;
    calls.flush();
    memory.flush();
    requireCapture(calls.good() && memory.good(), "cannot flush the capture files in " + directory.string());
    calls.close();
    memory.close();
    std::ofstream file(directory / HeaderName, std::ios::binary | std::ios::trunc);
    file.write(header.data(), static_cast<std::streamsize>(header.size()));
    file.flush();
    requireCapture(file.good(), "cannot write " + (directory / HeaderName).string());
}

std::uint64_t FrameCaptureWriter::RawBytes() const {
    std::lock_guard lock(mutex);
    return rawBytes;
}

std::uint64_t FrameCaptureWriter::StoredBytes() const {
    std::lock_guard lock(mutex);
    return storedBytes;
}

const std::filesystem::path& FrameCaptureWriter::Directory() const {
    return directory;
}

void FrameCaptureWriter::writeChunk(const FrameMemoryChunk& chunk, std::span<const std::byte> payload) {
    requireCapture(!finished, "memory written after the capture finished");
    Encoder encoder;
    encoder.Put(static_cast<std::uint32_t>(chunk.kind));
    encoder.Put(static_cast<std::uint32_t>(chunk.rangeKind));
    encoder.Put(chunk.protection);
    encoder.Put(chunk.access);
    encoder.Put(chunk.group);
    encoder.Put(chunk.completed);
    encoder.Put(chunk.address);
    encoder.Put(chunk.bytes);
    encoder.Put(static_cast<std::uint64_t>(payload.size()));
    encoder.Put(std::uint64_t{0});
    memory.write(reinterpret_cast<const char*>(encoder.bytes.data()), static_cast<std::streamsize>(encoder.bytes.size()));
    memory.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    requireCapture(memory.good(), "cannot write " + (directory / MemoryName).string());
    if (chunk.kind != FrameMemoryKind::Range) rawBytes += chunk.bytes;
    storedBytes += payload.size();
}

std::vector<std::byte> FrameCaptureWriter::compress(std::span<const std::byte> bytes) const {
    std::vector<std::byte> output(ZSTD_compressBound(bytes.size()));
    const auto written = ZSTD_compress(output.data(), output.size(), bytes.data(), bytes.size(), level);
    requireCapture(ZSTD_isError(written) == 0, std::string("zstd: ") + ZSTD_getErrorName(written));
    output.resize(written);
    return output;
}

FrameCaptureReader::FrameCaptureReader(const std::filesystem::path& directory) : directory(directory) {
    const auto callsPath = directory / FrameCaptureWriter::CallsName;
    const auto callBytes = readFile(callsPath);
    requireMagic(callBytes, FrameCaptureWriter::CallsMagic, callsPath);
    std::size_t at = FrameCaptureWriter::CallsMagic.size();
    while (at < callBytes.size()) {
        if (callBytes.size() - at < 8) {
            truncated = true;
            break;
        }
        std::uint32_t kind = 0;
        std::uint32_t bytes = 0;
        std::memcpy(&kind, callBytes.data() + at, 4);
        std::memcpy(&bytes, callBytes.data() + at + 4, 4);
        requireCapture(knownCall(kind), callsPath.string() + ": unknown call kind " + std::to_string(kind) + " at byte " + std::to_string(at));
        if (callBytes.size() - at - 8 < bytes) {
            truncated = true;
            break;
        }
        calls.push_back(decodeCall(static_cast<FrameCallKind>(kind), std::span(callBytes).subspan(at + 8, bytes)));
        at += 8 + static_cast<std::size_t>(bytes);
    }

    const auto memoryPath = directory / FrameCaptureWriter::MemoryName;
    std::ifstream memory(memoryPath, std::ios::binary);
    requireCapture(memory.is_open(), "cannot open " + memoryPath.string());
    memory.seekg(0, std::ios::end);
    const auto size = static_cast<std::uint64_t>(memory.tellg());
    std::string magic(FrameCaptureWriter::MemoryMagic.size(), '\0');
    memory.seekg(0);
    memory.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    requireCapture(memory.good() && magic == FrameCaptureWriter::MemoryMagic, memoryPath.string() + " is not a frame capture file");
    std::uint64_t offset = magic.size();
    while (offset < size) {
        if (size - offset < ChunkHeaderBytes) {
            truncated = true;
            break;
        }
        std::array<std::byte, ChunkHeaderBytes> raw{};
        memory.seekg(static_cast<std::streamoff>(offset));
        memory.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
        requireCapture(memory.good(), "cannot read " + memoryPath.string());
        Decoder decoder(raw);
        FrameMemoryChunk chunk;
        const auto kind = decoder.Take<std::uint32_t>("memory kind");
        const auto rangeKind = decoder.Take<std::uint32_t>("range kind");
        requireCapture(kind >= 1 && kind <= 4 && rangeKind >= 1 && rangeKind <= 6, memoryPath.string() + ": corrupt chunk header at byte " + std::to_string(offset));
        chunk.kind = static_cast<FrameMemoryKind>(kind);
        chunk.rangeKind = static_cast<FrameRangeKind>(rangeKind);
        chunk.protection = decoder.Take<std::uint32_t>("protection");
        chunk.access = decoder.Take<std::uint32_t>("access");
        chunk.group = decoder.Take<std::uint64_t>("group");
        chunk.completed = decoder.Take<std::uint64_t>("completed serial");
        chunk.address = decoder.Take<std::uint64_t>("address");
        chunk.bytes = decoder.Take<std::uint64_t>("bytes");
        chunk.compressedBytes = decoder.Take<std::uint64_t>("compressed bytes");
        requireCapture((chunk.kind == FrameMemoryKind::Range) == (chunk.compressedBytes == 0), memoryPath.string() + ": chunk payload does not match its kind at byte " + std::to_string(offset));
        chunk.offset = offset + ChunkHeaderBytes;
        if (size - chunk.offset < chunk.compressedBytes) {
            truncated = true;
            break;
        }
        chunks.push_back(chunk);
        offset = chunk.offset + chunk.compressedBytes;
    }

    const auto headerPath = directory / FrameCaptureWriter::HeaderName;
    if (std::filesystem::exists(headerPath)) {
        const auto bytes = readFile(headerPath);
        header.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    } else {
        truncated = true;
    }
}

const std::vector<FrameCall>& FrameCaptureReader::Calls() const {
    return calls;
}

const std::vector<FrameMemoryChunk>& FrameCaptureReader::Chunks() const {
    return chunks;
}

const std::string& FrameCaptureReader::Header() const {
    return header;
}

bool FrameCaptureReader::Truncated() const {
    return truncated;
}

void FrameCaptureReader::Read(const FrameMemoryChunk& chunk, std::span<std::byte> destination) const {
    requireCapture(chunk.kind != FrameMemoryKind::Range, "a range chunk has no bytes");
    requireCapture(destination.size() == chunk.bytes, "destination size does not match the chunk");
    const auto path = directory / FrameCaptureWriter::MemoryName;
    std::ifstream memory(path, std::ios::binary);
    requireCapture(memory.is_open(), "cannot open " + path.string());
    std::vector<std::byte> compressed(chunk.compressedBytes);
    memory.seekg(static_cast<std::streamoff>(chunk.offset));
    memory.read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(compressed.size()));
    requireCapture(memory.good(), "cannot read " + path.string());
    const auto written = ZSTD_decompress(destination.data(), destination.size(), compressed.data(), compressed.size());
    requireCapture(ZSTD_isError(written) == 0, std::string("zstd: ") + ZSTD_getErrorName(written));
    requireCapture(written == chunk.bytes, "chunk at " + std::to_string(chunk.address) + " decompressed to the wrong size");
}

std::vector<std::byte> FrameCaptureReader::Read(const FrameMemoryChunk& chunk) const {
    std::vector<std::byte> bytes(chunk.bytes);
    Read(chunk, bytes);
    return bytes;
}

}
