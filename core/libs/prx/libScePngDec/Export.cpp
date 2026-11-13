#include "prx/libScePngDec/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Decoder/Png.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(PngDecCreateParam) == 0xC);
static_assert(sizeof(PngDecParseParam) == 0x10);
static_assert(sizeof(PngDecDecodeParam) == 0x20);
static_assert(sizeof(PngDecImageInfo) == 0x10);
static_assert(sizeof(PngDecInputChunk) == 0x10);
static_assert(sizeof(PngDecInputControlParam) == 0x28);
static_assert(offsetof(PngDecInputControlParam, operation) == 0x1c);
static_assert(offsetof(PngDecInputControlParam, reserved) == 0x22);

namespace {

constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80690001);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80690002);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80690003);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80690004);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY = static_cast<std::int32_t>(0x80690005);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_DATA = static_cast<std::int32_t>(0x80690010);
constexpr std::int32_t SCE_PNG_DEC_ERROR_DECODE_ERROR = static_cast<std::int32_t>(0x80690012);

constexpr std::uint32_t ATTRIBUTE_NONE = 0;
constexpr std::uint32_t ATTRIBUTE_BIT_DEPTH_16 = 1;
constexpr std::uint32_t MAX_IMAGE_WIDTH = 1000000;
constexpr std::uint32_t WORK_MEMORY_OVERHEAD = 53392;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 16;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;

constexpr std::uint16_t COLOR_SPACE_GRAYSCALE = 2;
constexpr std::uint16_t COLOR_SPACE_RGB = 3;
constexpr std::uint16_t COLOR_SPACE_CLUT = 4;
constexpr std::uint16_t COLOR_SPACE_GRAYSCALE_ALPHA = 18;
constexpr std::uint16_t COLOR_SPACE_RGBA = 19;

constexpr std::uint32_t IMAGE_FLAG_ADAM7_INTERLACE = 1;
constexpr std::uint32_t IMAGE_FLAG_TRNS_CHUNK_EXIST = 2;

constexpr std::uint32_t BYTES_PER_PIXEL = 4;
constexpr std::uint32_t MAX_PACKED_DIMENSION = 32767;

struct Context {
    Context* self;
    std::uint32_t size;
    std::uint32_t attribute;
    std::uint32_t maxImageWidth;
    std::uint32_t memorySize;
};

static_assert(sizeof(Context) == 24);

std::uint32_t requiredMemory(const PngDecCreateParam& param) {
    return (((param.max_image_width + 7u) & ~7u) + 64u) * (8u << param.attribute) + WORK_MEMORY_OVERHEAD;
}

std::int32_t validateCreateParam(const PngDecCreateParam* param, PngDecCreateParam* outParam = nullptr) {
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    PngDecCreateParam local{};
    if (!GuestCopyIn_nid_no_patch(&local, param, sizeof(local))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (local.this_size != sizeof(PngDecCreateParam)) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (local.attribute != ATTRIBUTE_NONE && local.attribute != ATTRIBUTE_BIT_DEPTH_16) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (local.max_image_width - 1 >= MAX_IMAGE_WIDTH) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (outParam) *outParam = local;
    return 0;
}

Context* toContext(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* context = reinterpret_cast<Context*>(handle);
    Context selfCheck{};
    if (!GuestCopyIn_nid_no_patch(&selfCheck, context, sizeof(selfCheck))) return nullptr;
    return selfCheck.self == context ? context : nullptr;
}

std::uint16_t toColorSpace(Decoder::Png::ColorType colorType) {
    switch (colorType) {
    case Decoder::Png::ColorType::Grayscale:
        return COLOR_SPACE_GRAYSCALE;
    case Decoder::Png::ColorType::Rgb:
        return COLOR_SPACE_RGB;
    case Decoder::Png::ColorType::Palette:
        return COLOR_SPACE_CLUT;
    case Decoder::Png::ColorType::GrayscaleAlpha:
        return COLOR_SPACE_GRAYSCALE_ALPHA;
    case Decoder::Png::ColorType::Rgba:
        return COLOR_SPACE_RGBA;
    }
    throw std::runtime_error("libScePngDec: unknown PNG color type");
}

void fillImageInfo(const Decoder::Png::Header& header, PngDecImageInfo* info) {
    info->image_width = header.width;
    info->image_height = header.height;
    info->color_space = toColorSpace(header.colorType);
    info->bit_depth = header.bitDepth;
    info->image_flag = (header.interlaced ? IMAGE_FLAG_ADAM7_INTERLACE : 0) | (header.hasTransparency ? IMAGE_FLAG_TRNS_CHUNK_EXIST : 0);
}

bool hasAlpha(const Decoder::Png::Header& header) {
    return header.hasTransparency || header.colorType == Decoder::Png::ColorType::Palette || header.colorType == Decoder::Png::ColorType::GrayscaleAlpha
        || header.colorType == Decoder::Png::ColorType::Rgba;
}

constexpr std::array<std::uint8_t, 8> SIGNATURE = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::size_t CHUNK_HEADER_SIZE = 8;
constexpr std::size_t CHUNK_CRC_SIZE = 4;
constexpr std::uint32_t IHDR_SIZE = 13;
constexpr std::uint32_t MAX_CHUNK_LENGTH = 0x7FFFFFFF;
constexpr std::uint32_t MAX_PALETTE_SIZE = 256 * 3;
constexpr std::uint8_t ANCILLARY_BIT = 0x20;
constexpr std::array<std::uint32_t, 256> CRC_TABLE = [] {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < table.size(); ++index) {
        std::uint32_t value = index;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) != 0 ? 0xEDB88320u : 0u);
        table[index] = value;
    }
    return table;
}();

constexpr std::size_t INPUT_BLOCK_SIZE = 0x400;
constexpr std::size_t PARSE_BLOCK_SIZE = 16;

std::uint32_t readBigEndian32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16
        | static_cast<std::uint32_t>(bytes[2]) << 8 | static_cast<std::uint32_t>(bytes[3]);
}

std::uint32_t chunkCrc(const std::uint8_t* type, std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < 4; ++i) crc = (crc >> 8) ^ CRC_TABLE[(crc ^ type[i]) & 0xFF];
    for (const std::uint8_t byte : data) crc = (crc >> 8) ^ CRC_TABLE[(crc ^ byte) & 0xFF];
    return ~crc;
}

void appendChunk(std::vector<std::uint8_t>& output, const char* type, std::span<const std::uint8_t> data, std::optional<std::uint32_t> knownCrc = std::nullopt) {
    const auto size = static_cast<std::uint32_t>(data.size());
    const auto* typeBytes = reinterpret_cast<const std::uint8_t*>(type);
    const std::uint32_t crc = knownCrc.value_or(chunkCrc(typeBytes, data));
    for (int shift = 24; shift >= 0; shift -= 8) output.push_back(static_cast<std::uint8_t>(size >> shift));
    output.insert(output.end(), typeBytes, typeBytes + 4);
    output.insert(output.end(), data.begin(), data.end());
    for (int shift = 24; shift >= 0; shift -= 8) output.push_back(static_cast<std::uint8_t>(crc >> shift));
}

struct Chunk {
    const std::uint8_t* type;
    std::uint32_t length;
    bool complete;
    std::span<const std::uint8_t> data;
    bool crcValid;
    std::uint32_t crc = 0;

    bool is(const char* name) const {
        return std::equal(type, type + 4, name);
    }

    bool critical() const {
        return (type[0] & ANCILLARY_BIT) == 0;
    }
};

std::optional<Chunk> readChunk(std::span<const std::uint8_t> png, std::size_t offset) {
    if (png.size() - offset < CHUNK_HEADER_SIZE) return std::nullopt;
    Chunk chunk{&png[offset + 4], readBigEndian32(&png[offset]), false, {}, false};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::uint8_t letter = chunk.type[i] & static_cast<std::uint8_t>(~ANCILLARY_BIT);
        if (letter < 'A' || letter > 'Z') return std::nullopt;
    }
    if (chunk.length > MAX_CHUNK_LENGTH) return std::nullopt;
    const std::size_t available = png.size() - offset - CHUNK_HEADER_SIZE;
    if (chunk.length <= available) chunk.data = png.subspan(offset + CHUNK_HEADER_SIZE, chunk.length);
    if (static_cast<std::uint64_t>(chunk.length) + CHUNK_CRC_SIZE <= available) {
        chunk.complete = true;
        chunk.crc = chunkCrc(chunk.type, chunk.data);
        chunk.crcValid = readBigEndian32(&png[offset + CHUNK_HEADER_SIZE + chunk.length]) == chunk.crc;
    }
    return chunk;
}

struct ChunkScan {
    std::int32_t headerResult = 0;
    std::int32_t imageResult = 0;
    std::vector<std::uint8_t> png;
};

bool validTransparency(const Chunk& chunk, std::uint8_t colorType, bool hasPalette, std::uint32_t paletteEntries) {
    switch (static_cast<Decoder::Png::ColorType>(colorType)) {
    case Decoder::Png::ColorType::Grayscale:
        return chunk.length == 2;
    case Decoder::Png::ColorType::Rgb:
        return chunk.length == 6;
    case Decoder::Png::ColorType::Palette:
        return hasPalette && chunk.length != 0 && chunk.length <= paletteEntries;
    default:
        return false;
    }
}

std::int32_t scanImageChunks(std::span<const std::uint8_t> png, std::size_t offset, std::vector<std::uint8_t>& output) {
    bool inImageData = true;
    std::int32_t result = 0;
    while (true) {
        const std::optional<Chunk> chunk = readChunk(png, offset);
        if (!chunk) {
            result = SCE_PNG_DEC_ERROR_DECODE_ERROR;
            break;
        }
        if (chunk->is("IDAT")) {
            if (!inImageData) {
                result = SCE_PNG_DEC_ERROR_DECODE_ERROR;
                break;
            }
            if (chunk->data.size() == chunk->length) appendChunk(output, "IDAT", chunk->data, chunk->complete ? std::optional<std::uint32_t>(chunk->crc) : std::nullopt);
            if (!chunk->complete || !chunk->crcValid) {
                result = SCE_PNG_DEC_ERROR_DECODE_ERROR;
                break;
            }
        } else {
            inImageData = false;
            if (chunk->is("IHDR")) {
                result = SCE_PNG_DEC_ERROR_INVALID_DATA;
                break;
            }
            const bool known = chunk->is("IEND") || chunk->is("PLTE");
            if ((chunk->critical() && !known) || !chunk->complete || (chunk->critical() && !chunk->crcValid)) {
                result = SCE_PNG_DEC_ERROR_DECODE_ERROR;
                break;
            }
            if (chunk->is("IEND")) break;
        }
        offset += CHUNK_HEADER_SIZE + chunk->length + CHUNK_CRC_SIZE;
    }
    appendChunk(output, "IEND", {});
    return result;
}

ChunkScan scanChunks(std::span<const std::uint8_t> png) {
    ChunkScan scan;
    if (png.size() < SIGNATURE.size() || !std::equal(SIGNATURE.begin(), SIGNATURE.end(), png.begin())) {
        scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
        return scan;
    }
    scan.png.assign(SIGNATURE.begin(), SIGNATURE.end());
    std::optional<std::uint8_t> colorType;
    bool paletteSeen = false;
    bool hasPalette = false;
    bool hasTransparency = false;
    std::uint32_t paletteEntries = 0;
    std::vector<std::uint8_t> palette;
    std::vector<std::uint8_t> transparency;
    std::size_t offset = SIGNATURE.size();
    while (true) {
        const std::optional<Chunk> chunk = readChunk(png, offset);
        if (!chunk) {
            scan.headerResult = SCE_PNG_DEC_ERROR_DECODE_ERROR;
            return scan;
        }
        if (!colorType) {
            if (chunk->is("IDAT") || chunk->is("PLTE") || chunk->is("tRNS")) {
                scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
                return scan;
            }
            if (chunk->critical() && !chunk->is("IHDR")) {
                scan.headerResult = SCE_PNG_DEC_ERROR_DECODE_ERROR;
                return scan;
            }
        }
        if (chunk->is("IHDR")) {
            if (colorType || chunk->length != IHDR_SIZE) {
                scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
                return scan;
            }
        } else if (chunk->is("PLTE")) {
            const bool validLength = chunk->length % 3 == 0 && chunk->length <= MAX_PALETTE_SIZE;
            if (paletteSeen || (*colorType == static_cast<std::uint8_t>(Decoder::Png::ColorType::Palette) && !validLength)) {
                scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
                return scan;
            }
        } else if (chunk->is("IDAT")) {
            if (*colorType == static_cast<std::uint8_t>(Decoder::Png::ColorType::Palette) && !hasPalette) {
                scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
                return scan;
            }
            break;
        } else if (chunk->critical() && !chunk->is("IEND")) {
            scan.headerResult = SCE_PNG_DEC_ERROR_DECODE_ERROR;
            return scan;
        }
        if (!chunk->complete || (chunk->critical() && !chunk->crcValid) || chunk->is("IEND")) {
            scan.headerResult = SCE_PNG_DEC_ERROR_DECODE_ERROR;
            return scan;
        }
        if (chunk->is("IHDR")) {
            std::vector<std::uint8_t> header(SIGNATURE.begin(), SIGNATURE.end());
            appendChunk(header, "IHDR", chunk->data);
            if (!Decoder::Png::ParseHeader(header)) {
                scan.headerResult = SCE_PNG_DEC_ERROR_INVALID_DATA;
                return scan;
            }
            colorType = chunk->data[9];
            appendChunk(scan.png, "IHDR", chunk->data, chunk->crc);
        } else if (chunk->is("PLTE")) {
            paletteSeen = true;
            if (*colorType == static_cast<std::uint8_t>(Decoder::Png::ColorType::Palette)) {
                hasPalette = true;
                paletteEntries = chunk->length / 3;
                palette.assign(chunk->data.begin(), chunk->data.end());
                palette.resize(MAX_PALETTE_SIZE);
            }
        } else if (chunk->is("tRNS") && chunk->crcValid && !hasTransparency
                   && validTransparency(*chunk, *colorType, hasPalette, paletteEntries)) {
            hasTransparency = true;
            transparency.assign(chunk->data.begin(), chunk->data.end());
        }
        offset += CHUNK_HEADER_SIZE + chunk->length + CHUNK_CRC_SIZE;
    }
    if (hasPalette) appendChunk(scan.png, "PLTE", palette);
    if (hasTransparency) appendChunk(scan.png, "tRNS", transparency);
    scan.imageResult = scanImageChunks(png, offset, scan.png);
    return scan;
}

struct InputStream {
    PngDecInputCallback input = nullptr;
    void* argument = nullptr;
    std::vector<std::uint8_t> data;
    const std::uint8_t* chunk = nullptr;
    std::uint32_t chunkSize = 0;
    std::int32_t status = 0;
    std::atomic<bool> busy = false;
};

std::mutex streamMutex;
std::unordered_map<const Context*, std::shared_ptr<InputStream>> streams;

class StreamUse {
public:
    explicit StreamUse(std::shared_ptr<InputStream> stream) : stream(std::move(stream)) {
        if (this->stream->busy.exchange(true)) throw std::runtime_error("scePngDecDecodeWithInputControl: a call on a handle whose input stream another call is still reading is not implemented");
    }
    ~StreamUse() { stream->busy = false; }
    StreamUse(const StreamUse&) = delete;
    StreamUse& operator=(const StreamUse&) = delete;
    InputStream& operator*() const { return *stream; }

private:
    std::shared_ptr<InputStream> stream;
};

std::shared_ptr<InputStream> findStream(const Context* context) {
    std::lock_guard lock(streamMutex);
    const auto found = streams.find(context);
    return found == streams.end() ? nullptr : found->second;
}

void forgetStream(const Context* context) {
    std::lock_guard lock(streamMutex);
    streams.erase(context);
}

bool pull(InputStream& stream, std::size_t size) {
    while (size > 0 && stream.status == 0) {
        if (stream.chunkSize == 0) {
            PngDecInputChunk chunk{};
            const std::int32_t result = stream.input(&chunk, stream.argument);
            if (result < 0) {
                stream.status = result;
                break;
            }
            if (!chunk.data || chunk.size == 0) {
                stream.status = 1;
                break;
            }
            stream.chunk = static_cast<const std::uint8_t*>(chunk.data);
            stream.chunkSize = chunk.size;
        }
        const std::size_t count = std::min(size, static_cast<std::size_t>(stream.chunkSize));
        stream.data.insert(stream.data.end(), stream.chunk, stream.chunk + count);
        stream.chunk += count;
        stream.chunkSize -= static_cast<std::uint32_t>(count);
        size -= count;
    }
    return stream.status == 0;
}

void pullImage(InputStream& stream) {
    std::size_t offset = SIGNATURE.size();
    while (stream.status == 0) {
        const std::size_t headerEnd = offset + CHUNK_HEADER_SIZE;
        if (stream.data.size() < headerEnd && !pull(stream, headerEnd - stream.data.size())) return;
        const std::uint32_t length = readBigEndian32(stream.data.data() + offset);
        if (length > MAX_CHUNK_LENGTH) return;
        const bool isEnd = std::memcmp(stream.data.data() + offset + 4, "IEND", 4) == 0;
        const std::size_t chunkEnd = headerEnd + length + CHUNK_CRC_SIZE;
        while (stream.data.size() < chunkEnd && stream.status == 0) {
            pull(stream, std::min(INPUT_BLOCK_SIZE, chunkEnd - stream.data.size()));
        }
        if (stream.data.size() < chunkEnd || isEnd) return;
        offset = chunkEnd;
    }
}

bool headerAvailable(const std::vector<std::uint8_t>& data) {
    constexpr std::size_t firstChunk = SIGNATURE.size();
    constexpr std::size_t chunkOverhead = 12;
    constexpr std::size_t ihdrSize = 13;
    if (data.size() < firstChunk) return false;
    if (!std::equal(SIGNATURE.begin(), SIGNATURE.end(), data.begin())) return true;
    if (data.size() < firstChunk + chunkOverhead + ihdrSize) return false;
    if (readBigEndian32(&data[firstChunk]) != ihdrSize || std::memcmp(&data[firstChunk + 4], "IHDR", 4) != 0) return true;
    std::size_t offset = firstChunk + chunkOverhead + ihdrSize;
    while (data.size() - offset >= chunkOverhead) {
        const std::uint32_t length = readBigEndian32(&data[offset]);
        const std::uint8_t* type = &data[offset + 4];
        if (std::memcmp(type, "IDAT", 4) == 0 || std::memcmp(type, "IEND", 4) == 0) return true;
        if (length > data.size() - offset - chunkOverhead) return false;
        if (std::memcmp(type, "tRNS", 4) == 0) return true;
        offset += chunkOverhead + length;
    }
    return false;
}

} // namespace

extern "C" {

int32_t APS5_VABI scePngDecCreate(const PngDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    PngDecCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    if (!memoryAddress) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (memorySize < requiredMemory(localParam)) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memoryAddress);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    Context localContext{};
    localContext.self = reinterpret_cast<Context*>(aligned);
    localContext.size = sizeof(PngDecCreateParam);
    localContext.attribute = localParam.attribute;
    localContext.maxImageWidth = (localParam.max_image_width + 7u) & ~7u;
    localContext.memorySize = requiredMemory(localParam) - 144u;
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(aligned), &localContext, sizeof(localContext))) {
        return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    }
    auto* context = reinterpret_cast<Context*>(aligned);
    if (!GuestCopyOut_nid_no_patch(handle, &context, sizeof(context))) {
        Context cleared{};
        GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(aligned), &cleared, sizeof(cleared));
        return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    }
    return 0;
}

int32_t APS5_VABI scePngDecDecode(void* handle, const PngDecDecodeParam* param, PngDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_PNG_DEC_ERROR_INVALID_HANDLE;
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    PngDecDecodeParam localParam{};
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (!localParam.png_mem_addr || !localParam.image_mem_addr || (reinterpret_cast<std::uintptr_t>(localParam.image_mem_addr) & 3u) != 0) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (localParam.png_mem_size == 0 || localParam.image_mem_size == 0) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (localParam.pixel_format == 132) throw std::runtime_error("scePngDecDecode: pixel format 132 is not implemented");
    if (localParam.pixel_format != PIXEL_FORMAT_R8G8B8A8 && localParam.pixel_format != PIXEL_FORMAT_B8G8R8A8) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (localParam.alpha_value > 0xFF) return SCE_PNG_DEC_ERROR_INVALID_PARAM;

    std::vector<std::uint8_t> pngBytes(localParam.png_mem_size);
    if (!GuestCopyIn_nid_no_patch(pngBytes.data(), localParam.png_mem_addr, pngBytes.size())) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const ChunkScan scan = scanChunks(pngBytes);
    if (scan.headerResult != 0) return scan.headerResult;
    const std::span<const std::uint8_t> png = scan.png;
    const std::optional<Decoder::Png::Header> header = Decoder::Png::ParseHeader(png);
    if (!header) return SCE_PNG_DEC_ERROR_INVALID_DATA;
    if (imageInfo) {
        PngDecImageInfo localInfo{};
        fillImageInfo(*header, &localInfo);
        if (!GuestCopyOut_nid_no_patch(imageInfo, &localInfo, sizeof(localInfo))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    }
    const bool packedDimensions = header->width <= MAX_PACKED_DIMENSION && header->height <= MAX_PACKED_DIMENSION;
    if (!imageInfo && !packedDimensions) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const std::uint64_t rowSize = static_cast<std::uint64_t>(header->width) * BYTES_PER_PIXEL;
    const std::uint64_t pitch = localParam.image_pitch == 0 ? rowSize : localParam.image_pitch;
    if (localParam.image_pitch != 0 && (pitch < rowSize || (pitch & 3u) != 0)) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (localParam.image_pitch == 0) {
        if (header->height * rowSize > localParam.image_mem_size) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    } else if (header->height * pitch >= 0x80000000ull || header->height * pitch > localParam.image_mem_size) {
        return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    }
    const std::uint64_t alignedWidth = (static_cast<std::uint64_t>(header->width) + 7u) & ~std::uint64_t{7u};
    const std::uint64_t alignedMaxWidth = (static_cast<std::uint64_t>(context->maxImageWidth) + 7u) & ~std::uint64_t{7u};
    if (alignedWidth > alignedMaxWidth) return SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY;
    if (header->bitDepth > (8u << (context->attribute & 31u))) return SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY;

    const std::optional<Decoder::Png::Image> image = Decoder::Png::Decode(png);
    if (!image || image->width != header->width || image->height != header->height) return SCE_PNG_DEC_ERROR_DECODE_ERROR;

    const bool swapRedBlue = localParam.pixel_format == PIXEL_FORMAT_B8G8R8A8;
    const bool fillAlpha = !hasAlpha(*header);
    const auto alpha = static_cast<std::uint8_t>(localParam.alpha_value);
    auto* output = static_cast<std::uint8_t*>(localParam.image_mem_addr);
    std::vector<std::uint8_t> rowBuffer(rowSize);
    for (std::uint32_t y = 0; y < image->height; ++y) {
        const std::uint8_t* in = image->pixels.data() + y * rowSize;
        std::memcpy(rowBuffer.data(), in, rowSize);
        for (std::uint32_t x = 0; x < image->width; ++x) {
            std::uint8_t* pixel = rowBuffer.data() + x * BYTES_PER_PIXEL;
            if (swapRedBlue) std::swap(pixel[0], pixel[2]);
            if (fillAlpha) pixel[3] = alpha;
        }
        std::uint8_t* out = output + y * pitch;
        if (!GuestCopyOut_nid_no_patch(out, rowBuffer.data(), rowSize)) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    }

    if (scan.imageResult != 0) return scan.imageResult;
    if (!packedDimensions) return 0;
    return static_cast<std::int32_t>(header->width << 16 | header->height);
}

int32_t APS5_VABI scePngDecDelete(void* handle) {
    Context* context = toContext(handle);
    if (!context) return SCE_PNG_DEC_ERROR_INVALID_HANDLE;
    Context cleared{};
    if (!GuestCopyOut_nid_no_patch(context, &cleared, sizeof(cleared))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    forgetStream(context);
    return 0;
}

int32_t APS5_VABI scePngDecParseHeader(const PngDecParseParam* param, PngDecImageInfo* imageInfo) {
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    PngDecParseParam localParam{};
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (!localParam.png_mem_addr) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (localParam.png_mem_size == 0) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (localParam.reserved0 != 0) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (!imageInfo) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    std::vector<std::uint8_t> pngBytes(localParam.png_mem_size);
    if (!GuestCopyIn_nid_no_patch(pngBytes.data(), localParam.png_mem_addr, pngBytes.size())) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const ChunkScan scan = scanChunks(pngBytes);
    if (scan.headerResult != 0) return scan.headerResult;
    const std::optional<Decoder::Png::Header> header = Decoder::Png::ParseHeader(scan.png);
    if (!header) return SCE_PNG_DEC_ERROR_INVALID_DATA;
    PngDecImageInfo localInfo{};
    fillImageInfo(*header, &localInfo);
    if (!GuestCopyOut_nid_no_patch(imageInfo, &localInfo, sizeof(localInfo))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    return 0;
}

int32_t APS5_VABI scePngDecQueryMemorySize(const PngDecCreateParam* param) {
    PngDecCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    return static_cast<std::int32_t>(requiredMemory(localParam));
}

int32_t APS5_VABI scePngDecDecodeWithInputControl(void* handle, const PngDecInputControlParam* param, PngDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_PNG_DEC_ERROR_INVALID_HANDLE;
    PngDecInputControlParam localParam{};
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (!localParam.input) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (localParam.reserved != 0) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (localParam.operation == 0x10) {
        if (!imageInfo) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
        auto stream = std::make_shared<InputStream>();
        stream->input = localParam.input;
        stream->argument = localParam.input_argument;
        {
            std::lock_guard lock(streamMutex);
            streams[context] = stream;
        }
        const StreamUse use(stream);
        while (!headerAvailable((*use).data) && (*use).status == 0) pull(*use, PARSE_BLOCK_SIZE);
        if ((*use).status < 0) return (*use).status;
        const std::optional<Decoder::Png::Header> header = Decoder::Png::ParseHeader((*use).data);
        if (!header) return SCE_PNG_DEC_ERROR_INVALID_DATA;
        PngDecImageInfo localInfo{};
        fillImageInfo(*header, &localInfo);
        if (!GuestCopyOut_nid_no_patch(imageInfo, &localInfo, sizeof(localInfo))) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
        return 0;
    }
    if (localParam.operation != 0x20) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (!localParam.image_mem_addr || (reinterpret_cast<std::uintptr_t>(localParam.image_mem_addr) & 3u) != 0) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (localParam.image_mem_size == 0) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (localParam.pixel_format == 132) throw std::runtime_error("scePngDecDecodeWithInputControl: pixel format 132 is not implemented");
    if (localParam.pixel_format != PIXEL_FORMAT_R8G8B8A8 && localParam.pixel_format != PIXEL_FORMAT_B8G8R8A8) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (localParam.alpha_value > 0xFF) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    const std::shared_ptr<InputStream> stream = findStream(context);
    if (!stream) throw std::runtime_error("scePngDecDecodeWithInputControl: decoding without a parsed header is not implemented");
    const StreamUse use(stream);
    pullImage(*use);
    if ((*use).status < 0) return (*use).status;
    PngDecDecodeParam decode{};
    decode.png_mem_addr = (*use).data.data();
    decode.image_mem_addr = localParam.image_mem_addr;
    decode.png_mem_size = static_cast<std::uint32_t>((*use).data.size());
    decode.image_mem_size = localParam.image_mem_size;
    decode.pixel_format = localParam.pixel_format;
    decode.alpha_value = localParam.alpha_value;
    decode.image_pitch = localParam.image_pitch;
    return scePngDecDecode(handle, &decode, imageInfo);
}

}
