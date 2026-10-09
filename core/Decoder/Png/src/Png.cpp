#include "Decoder/Png.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <stdexcept>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace Decoder::Png {

namespace {

constexpr std::array<std::uint8_t, 8> SIGNATURE = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::size_t CHUNK_OVERHEAD = 12;
constexpr std::size_t IHDR_SIZE = 13;
constexpr std::uint32_t MAX_DIMENSION = 0x7FFFFFFF;
constexpr std::uint8_t FILTER_TYPE_COUNT = 5;
constexpr std::array<std::uint8_t, 5> COLOR_TYPE_BY_CHANNELS = {0, 0, 4, 2, 6};
constexpr std::array<std::uint32_t, 256> CRC_TABLE = [] {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < table.size(); ++index) {
        std::uint32_t value = index;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) != 0 ? 0xEDB88320u : 0u);
        table[index] = value;
    }
    return table;
}();

std::uint32_t readBigEndian32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16
        | static_cast<std::uint32_t>(bytes[2]) << 8 | static_cast<std::uint32_t>(bytes[3]);
}

bool isChunkType(const std::uint8_t* bytes, const char* type) {
    return std::equal(bytes, bytes + 4, type);
}

bool isValidFormat(std::uint8_t bitDepth, std::uint8_t colorType) {
    switch (colorType) {
    case 0:
        return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8 || bitDepth == 16;
    case 3:
        return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8;
    case 2:
    case 4:
    case 6:
        return bitDepth == 8 || bitDepth == 16;
    default:
        return false;
    }
}

void appendBigEndian32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void appendChunk(std::vector<std::uint8_t>& output, const char* type, std::span<const std::uint8_t> data) {
    appendBigEndian32(output, static_cast<std::uint32_t>(data.size()));
    const std::size_t checkedFrom = output.size();
    output.insert(output.end(), type, type + 4);
    output.insert(output.end(), data.begin(), data.end());
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = checkedFrom; i < output.size(); ++i) crc = (crc >> 8) ^ CRC_TABLE[(crc ^ output[i]) & 0xFF];
    appendBigEndian32(output, ~crc);
}

int paethPredictor(int left, int up, int upLeft) {
    const int estimate = left + up - upLeft;
    const int leftDistance = std::abs(estimate - left);
    const int upDistance = std::abs(estimate - up);
    const int upLeftDistance = std::abs(estimate - upLeft);
    if (leftDistance <= upDistance && leftDistance <= upLeftDistance) return left;
    return upDistance <= upLeftDistance ? up : upLeft;
}

std::uint64_t filterRow(const std::uint8_t* row, const std::uint8_t* previousRow, std::size_t rowSize, std::size_t pixelSize,
                        std::uint8_t filterType, std::uint8_t* filtered) {
    std::uint64_t cost = 0;
    for (std::size_t i = 0; i < rowSize; ++i) {
        const int left = i >= pixelSize ? row[i - pixelSize] : 0;
        const int up = previousRow ? previousRow[i] : 0;
        const int upLeft = previousRow && i >= pixelSize ? previousRow[i - pixelSize] : 0;
        int prediction = 0;
        switch (filterType) {
        case 1:
            prediction = left;
            break;
        case 2:
            prediction = up;
            break;
        case 3:
            prediction = (left + up) >> 1;
            break;
        case 4:
            prediction = paethPredictor(left, up, upLeft);
            break;
        default:
            break;
        }
        filtered[i] = static_cast<std::uint8_t>(row[i] - prediction);
        cost += filtered[i] < 0x80 ? filtered[i] : 0x100u - filtered[i];
    }
    return cost;
}

}  // namespace

std::optional<Header> ParseHeader(std::span<const std::uint8_t> png) {
    const std::size_t firstChunk = SIGNATURE.size();
    if (png.size() < firstChunk + CHUNK_OVERHEAD + IHDR_SIZE) return std::nullopt;
    if (!std::equal(SIGNATURE.begin(), SIGNATURE.end(), png.begin())) return std::nullopt;
    if (readBigEndian32(&png[firstChunk]) != IHDR_SIZE || !isChunkType(&png[firstChunk + 4], "IHDR")) return std::nullopt;

    const std::uint8_t* ihdr = &png[firstChunk + 8];
    Header header{};
    header.width = readBigEndian32(ihdr);
    header.height = readBigEndian32(ihdr + 4);
    header.bitDepth = ihdr[8];
    header.colorType = static_cast<ColorType>(ihdr[9]);
    header.interlaced = ihdr[12] == 1;
    if (header.width == 0 || header.height == 0 || header.width > MAX_DIMENSION || header.height > MAX_DIMENSION) return std::nullopt;
    if (!isValidFormat(ihdr[8], ihdr[9]) || ihdr[10] != 0 || ihdr[11] != 0 || ihdr[12] > 1) return std::nullopt;

    std::size_t offset = firstChunk + CHUNK_OVERHEAD + IHDR_SIZE;
    while (png.size() - offset >= CHUNK_OVERHEAD) {
        const std::uint32_t length = readBigEndian32(&png[offset]);
        const std::uint8_t* type = &png[offset + 4];
        if (length > png.size() - offset - CHUNK_OVERHEAD) break;
        if (isChunkType(type, "tRNS")) header.hasTransparency = true;
        if (isChunkType(type, "tRNS") || isChunkType(type, "IDAT") || isChunkType(type, "IEND")) break;
        offset += CHUNK_OVERHEAD + length;
    }
    return header;
}

std::optional<Image> Decode(std::span<const std::uint8_t> png) {
    if (png.empty() || png.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &width, &height, &channels, 4);
    if (!decoded) return std::nullopt;

    Image image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
    image.pixels.assign(decoded, decoded + static_cast<std::size_t>(image.width) * image.height * 4);
    stbi_image_free(decoded);
    return image;
}

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, EncodeOptions options) {
    if (channels < 1 || channels > 4) throw std::invalid_argument("Png::Encode: channels must be 1-4");
    if (options.compressionLevel < 0 || options.compressionLevel > 9 || options.filters == 0 || (options.filters & ~FILTER_ALL) != 0) {
        throw std::invalid_argument("Png::Encode: unsupported options");
    }
    const std::uint64_t rowSize = static_cast<std::uint64_t>(width) * channels;
    if (width == 0 || height == 0 || rowSize > INT_MAX || (rowSize + 1) * height > INT_MAX) {
        throw std::invalid_argument("Png::Encode: unsupported image size");
    }
    if (pixels.size() < rowSize * height) {
        throw std::invalid_argument("Png::Encode: pixel buffer is too small");
    }

    std::vector<std::uint8_t> filtered((rowSize + 1) * height);
    std::vector<std::uint8_t> candidate(rowSize);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t* row = pixels.data() + rowSize * y;
        const std::uint8_t* previousRow = y == 0 ? nullptr : row - rowSize;
        std::uint8_t* filteredRow = filtered.data() + (rowSize + 1) * y;
        std::uint64_t bestCost = UINT64_MAX;
        for (std::uint8_t filterType = 0; filterType < FILTER_TYPE_COUNT; ++filterType) {
            if ((options.filters & (1u << filterType)) == 0) continue;
            const std::uint64_t cost =filterRow(row, previousRow, rowSize, channels, filterType, candidate.data());
            if (cost >= bestCost) continue;
            bestCost = cost;
            filteredRow[0] = filterType;
            std::copy(candidate.begin(), candidate.end(), filteredRow + 1);
        }
    }

    int compressedSize = 0;
    const std::unique_ptr<unsigned char, void (*)(unsigned char*)> compressed(
        stbi_zlib_compress(filtered.data(), static_cast<int>(filtered.size()), &compressedSize, options.compressionLevel),
        [](unsigned char* memory) { STBIW_FREE(memory); });
    if (!compressed) throw std::runtime_error("Png::Encode: encoding failed");

    std::vector<std::uint8_t> header;
    appendBigEndian32(header, width);
    appendBigEndian32(header, height);
    header.insert(header.end(), {8, COLOR_TYPE_BY_CHANNELS[channels], 0, 0, 0});

    std::vector<std::uint8_t> output(SIGNATURE.begin(), SIGNATURE.end());
    output.reserve(SIGNATURE.size() + 3 * CHUNK_OVERHEAD + IHDR_SIZE + static_cast<std::size_t>(compressedSize));
    appendChunk(output, "IHDR", header);
    appendChunk(output, "IDAT", std::span<const std::uint8_t>(compressed.get(), static_cast<std::size_t>(compressedSize)));
    appendChunk(output, "IEND", {});
    return output;
}

}  // namespace Decoder::Png
