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
const std::vector<FrameMemoryChunk>& FrameCaptureReader::Chunks() const {
    return chunks;
}

const std::string& FrameCaptureReader::Header() const {
    return header;
}

std::uint32_t FrameCaptureReader::Version() const {
    return version;
}

bool FrameCaptureReader::Truncated() const {
    return truncated;
}

void FrameCaptureReader::Read(const FrameMemoryChunk& chunk, std::span<std::byte> destination) const {
    requireCapture(chunk.kind != FrameMemoryKind::Range, "a range chunk has no bytes");
    requireCapture(destination.size() == chunk.bytes, "destination size does not match the chunk");
    auto stream = takeStream();
    const auto compressedBytes = static_cast<std::size_t>(chunk.compressedBytes);
    if (stream->compressed.size() < compressedBytes) stream->compressed.resize(compressedBytes);
    stream->file.clear();
    stream->file.seekg(static_cast<std::streamoff>(chunk.offset));
    stream->file.read(reinterpret_cast<char*>(stream->compressed.data()), static_cast<std::streamsize>(compressedBytes));
    if (!stream->file.good()) throw std::runtime_error("frame capture: cannot read " + memoryPath.string());
    const auto written = ZSTD_decompressDCtx(stream->context, destination.data(), destination.size(), stream->compressed.data(), compressedBytes);
    if (ZSTD_isError(written) != 0) throw std::runtime_error(std::string("frame capture: zstd: ") + ZSTD_getErrorName(written));
    if (written != chunk.bytes) throw std::runtime_error("frame capture: chunk at " + std::to_string(chunk.address) + " decompressed to the wrong size");
    returnStream(std::move(stream));
}

std::vector<std::byte> FrameCaptureReader::Read(const FrameMemoryChunk& chunk) const {
    std::vector<std::byte> bytes(chunk.bytes);
    Read(chunk, bytes);
    return bytes;
}

FrameCaptureReader::~FrameCaptureReader() = default;

std::unique_ptr<FrameCaptureReader::Stream> FrameCaptureReader::takeStream() const {
    {
        const std::lock_guard lock(streamMutex);
        if (!streams.empty()) {
            auto stream = std::move(streams.back());
            streams.pop_back();
            return stream;
        }
    }
    auto stream = std::make_unique<Stream>();
    stream->file.open(memoryPath, std::ios::binary);
    if (!stream->file.is_open()) throw std::runtime_error("frame capture: cannot open " + memoryPath.string());
    stream->context = ZSTD_createDCtx();
    if (stream->context == nullptr) throw std::runtime_error("frame capture: zstd: cannot create a decompression context");
    return stream;
}

void FrameCaptureReader::returnStream(std::unique_ptr<Stream> stream) const {
    const std::lock_guard lock(streamMutex);
    streams.push_back(std::move(stream));
}

}
}
