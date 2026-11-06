#include "Decoder/JpegEncoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace Decoder::Jpeg {

namespace {

constexpr std::uint8_t kZigZag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

constexpr std::uint8_t kLuminanceQuantization[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};

constexpr std::uint8_t kChrominanceQuantization[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

constexpr std::uint8_t kLuminanceDcBits[17] = {0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t kLuminanceDcValues[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr std::uint8_t kChrominanceDcBits[17] = {0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr std::uint8_t kChrominanceDcValues[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr std::uint8_t kLuminanceAcBits[17] = {0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
constexpr std::uint8_t kLuminanceAcValues[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa};

constexpr std::uint8_t kChrominanceAcBits[17] = {0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
constexpr std::uint8_t kChrominanceAcValues[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa};

struct HuffmanTable {
    std::array<std::uint16_t, 256> Code{};
    std::array<std::uint8_t, 256> Length{};
};

HuffmanTable buildHuffmanTable(const std::uint8_t* bits, const std::uint8_t* values) {
    HuffmanTable table;
    std::uint16_t code = 0;
    std::size_t index = 0;
    for (std::uint8_t length = 1; length <= 16; ++length) {
        for (std::uint8_t count = 0; count < bits[length]; ++count) {
            table.Code[values[index]] = code;
            table.Length[values[index]] = length;
            ++code;
            ++index;
        }
        code = static_cast<std::uint16_t>(code << 1);
    }
    return table;
}

std::array<std::uint8_t, 64> scaleQuantization(const std::uint8_t* base, const int quality) {
    const int factor = quality < 50 ? 5000 / quality : 200 - quality * 2;
    std::array<std::uint8_t, 64> table{};
    for (std::size_t index = 0; index < 64; ++index) {
        const int value = (base[index] * factor + 50) / 100;
        table[index] = static_cast<std::uint8_t>(std::clamp(value, 1, 255));
    }
    return table;
}

void forwardDct(const float* input, float* output) {
    float intermediate[64];
    for (int row = 0; row < 8; ++row) {
        for (int frequency = 0; frequency < 8; ++frequency) {
            float sum = 0.0f;
            for (int column = 0; column < 8; ++column)
                sum += input[row * 8 + column] * std::cos((2 * column + 1) * frequency * 3.14159265358979f / 16.0f);
            const float scale = frequency == 0 ? 0.353553390593f : 0.5f;
            intermediate[row * 8 + frequency] = sum * scale;
        }
    }
    for (int column = 0; column < 8; ++column) {
        for (int frequency = 0; frequency < 8; ++frequency) {
            float sum = 0.0f;
            for (int row = 0; row < 8; ++row)
                sum += intermediate[row * 8 + column] * std::cos((2 * row + 1) * frequency * 3.14159265358979f / 16.0f);
            const float scale = frequency == 0 ? 0.353553390593f : 0.5f;
            output[frequency * 8 + column] = sum * scale;
        }
    }
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& output) : _output(output) {
    }

    void Write(const std::uint16_t code, const std::uint8_t length) {
        for (int index = length - 1; index >= 0; --index) {
            _buffer = static_cast<std::uint8_t>((_buffer << 1) | ((code >> index) & 1));
            ++_count;
            if (_count == 8) {
                _output.push_back(_buffer);
                if (_buffer == 0xFF)
                    _output.push_back(0x00);
                _buffer = 0;
                _count = 0;
            }
        }
    }

    void Flush() {
        while (_count != 0)
            Write(1, 1);
    }

private:
    std::vector<std::uint8_t>& _output;
    std::uint8_t _buffer = 0;
    std::uint8_t _count = 0;
};

std::uint8_t magnitude(const int value) {
    int magnitudeValue = value < 0 ? -value : value;
    std::uint8_t bits = 0;
    while (magnitudeValue != 0) {
        ++bits;
        magnitudeValue >>= 1;
    }
    return bits;
}

void appendU16(std::vector<std::uint8_t>& output, const std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void appendMarker(std::vector<std::uint8_t>& output, const std::uint8_t marker) {
    output.push_back(0xFF);
    output.push_back(marker);
}

void appendQuantizationTable(std::vector<std::uint8_t>& output, const std::array<std::uint8_t, 64>& table, const std::uint8_t identifier) {
    appendMarker(output, 0xDB);
    appendU16(output, 67);
    output.push_back(identifier);
    for (std::size_t index = 0; index < 64; ++index)
        output.push_back(table[kZigZag[index]]);
}

void appendHuffmanTable(std::vector<std::uint8_t>& output, const std::uint8_t identifier, const std::uint8_t* bits, const std::uint8_t* values, const std::size_t valueCount) {
    appendMarker(output, 0xC4);
    appendU16(output, static_cast<std::uint16_t>(19 + valueCount));
    output.push_back(identifier);
    for (std::uint8_t length = 1; length <= 16; ++length)
        output.push_back(bits[length]);
    output.insert(output.end(), values, values + valueCount);
}

struct Component {
    std::vector<std::uint8_t> Samples;
    std::uint32_t Width;
    std::uint32_t Height;
    std::uint8_t HorizontalFactor;
    std::uint8_t VerticalFactor;
    std::uint8_t QuantizationTable;
    std::uint8_t DcTable;
    std::uint8_t AcTable;
    int Predictor;
};

std::uint8_t sampleAt(const Component& component, const std::int64_t x, const std::int64_t y) {
    const auto clampedX = std::clamp<std::int64_t>(x, 0, static_cast<std::int64_t>(component.Width) - 1);
    const auto clampedY = std::clamp<std::int64_t>(y, 0, static_cast<std::int64_t>(component.Height) - 1);
    return component.Samples[static_cast<std::size_t>(clampedY) * component.Width + static_cast<std::size_t>(clampedX)];
}

void encodeBlock(BitWriter& writer, Component& component, const std::int64_t originX, const std::int64_t originY,
                 const std::array<std::uint8_t, 64>& quantization, const HuffmanTable& dc, const HuffmanTable& ac) {
    float block[64];
    for (int row = 0; row < 8; ++row)
        for (int column = 0; column < 8; ++column)
            block[row * 8 + column] = static_cast<float>(sampleAt(component, originX + column, originY + row)) - 128.0f;
    float transformed[64];
    forwardDct(block, transformed);
    int quantized[64];
    for (std::size_t index = 0; index < 64; ++index) {
        const float value = transformed[index] / static_cast<float>(quantization[index]);
        quantized[index] = static_cast<int>(std::lround(value));
    }
    const int difference = quantized[0] - component.Predictor;
    component.Predictor = quantized[0];
    const auto dcBits = magnitude(difference);
    writer.Write(dc.Code[dcBits], dc.Length[dcBits]);
    if (dcBits != 0) {
        const int encoded = difference < 0 ? difference - 1 : difference;
        writer.Write(static_cast<std::uint16_t>(encoded & ((1 << dcBits) - 1)), dcBits);
    }
    int runLength = 0;
    for (std::size_t index = 1; index < 64; ++index) {
        const int value = quantized[kZigZag[index]];
        if (value == 0) {
            ++runLength;
            continue;
        }
        while (runLength > 15) {
            writer.Write(ac.Code[0xF0], ac.Length[0xF0]);
            runLength -= 16;
        }
        const auto acBits = magnitude(value);
        const auto symbol = static_cast<std::uint8_t>((runLength << 4) | acBits);
        writer.Write(ac.Code[symbol], ac.Length[symbol]);
        const int encoded = value < 0 ? value - 1 : value;
        writer.Write(static_cast<std::uint16_t>(encoded & ((1 << acBits) - 1)), acBits);
        runLength = 0;
    }
    if (runLength > 0)
        writer.Write(ac.Code[0x00], ac.Length[0x00]);
}

Component downsample(const std::vector<std::uint8_t>& plane, const std::uint32_t width, const std::uint32_t height,
                     const std::uint8_t horizontal, const std::uint8_t vertical) {
    const std::uint32_t targetWidth = (width + horizontal - 1) / horizontal;
    const std::uint32_t targetHeight = (height + vertical - 1) / vertical;
    Component component{};
    component.Samples.resize(static_cast<std::size_t>(targetWidth) * targetHeight);
    component.Width = targetWidth;
    component.Height = targetHeight;
    for (std::uint32_t y = 0; y < targetHeight; ++y) {
        for (std::uint32_t x = 0; x < targetWidth; ++x) {
            std::uint32_t total = 0;
            std::uint32_t count = 0;
            for (std::uint8_t offsetY = 0; offsetY < vertical; ++offsetY) {
                for (std::uint8_t offsetX = 0; offsetX < horizontal; ++offsetX) {
                    const std::uint32_t sourceX = std::min(x * horizontal + offsetX, width - 1);
                    const std::uint32_t sourceY = std::min(y * vertical + offsetY, height - 1);
                    total += plane[static_cast<std::size_t>(sourceY) * width + sourceX];
                    ++count;
                }
            }
            component.Samples[static_cast<std::size_t>(y) * targetWidth + x] = static_cast<std::uint8_t>((total + count / 2) / count);
        }
    }
    return component;
}

}

std::vector<std::uint8_t> EncodeBaseline(const std::span<const std::uint8_t> pixels, const std::uint32_t width, const std::uint32_t height,
                                         const std::uint32_t channels, const int quality, const Sampling sampling) {
    if (width == 0 || height == 0 || width > 0xFFFF || height > 0xFFFF)
        throw std::invalid_argument("Jpeg::EncodeBaseline: unsupported image size");
    if (channels != 1 && channels != 3)
        throw std::invalid_argument("Jpeg::EncodeBaseline: channels must be 1 or 3");
    if (quality < 1 || quality > 100)
        throw std::invalid_argument("Jpeg::EncodeBaseline: quality must be 1-100");
    if (channels == 1 && sampling != Sampling::Full)
        throw std::invalid_argument("Jpeg::EncodeBaseline: grayscale requires full sampling");
    if (pixels.size() < static_cast<std::size_t>(width) * height * channels)
        throw std::invalid_argument("Jpeg::EncodeBaseline: pixel buffer is too small");

    const auto luminanceQuantization = scaleQuantization(kLuminanceQuantization, quality);
    const auto chrominanceQuantization = scaleQuantization(kChrominanceQuantization, quality);
    const auto luminanceDc = buildHuffmanTable(kLuminanceDcBits, kLuminanceDcValues);
    const auto luminanceAc = buildHuffmanTable(kLuminanceAcBits, kLuminanceAcValues);
    const auto chrominanceDc = buildHuffmanTable(kChrominanceDcBits, kChrominanceDcValues);
    const auto chrominanceAc = buildHuffmanTable(kChrominanceAcBits, kChrominanceAcValues);

    std::vector<Component> components;
    std::uint8_t horizontal = 1;
    std::uint8_t vertical = 1;
    if (sampling == Sampling::Ycc422) horizontal = 2;
    if (sampling == Sampling::Ycc420) {
        horizontal = 2;
        vertical = 2;
    }

    std::vector<std::uint8_t> luminance(static_cast<std::size_t>(width) * height);
    std::vector<std::uint8_t> blue(static_cast<std::size_t>(width) * height);
    std::vector<std::uint8_t> red(static_cast<std::size_t>(width) * height);
    for (std::size_t index = 0; index < static_cast<std::size_t>(width) * height; ++index) {
        if (channels == 1) {
            luminance[index] = pixels[index];
            continue;
        }
        const float r = pixels[index * 3];
        const float g = pixels[index * 3 + 1];
        const float b = pixels[index * 3 + 2];
        luminance[index] = static_cast<std::uint8_t>(std::clamp(std::lround(0.299f * r + 0.587f * g + 0.114f * b), 0L, 255L));
        blue[index] = static_cast<std::uint8_t>(std::clamp(std::lround(-0.168736f * r - 0.331264f * g + 0.5f * b + 128.0f), 0L, 255L));
        red[index] = static_cast<std::uint8_t>(std::clamp(std::lround(0.5f * r - 0.418688f * g - 0.081312f * b + 128.0f), 0L, 255L));
    }

    Component luminanceComponent{std::move(luminance), width, height, horizontal, vertical, 0, 0, 0, 0};
    components.push_back(std::move(luminanceComponent));
    if (channels == 3) {
        auto blueComponent = downsample(blue, width, height, horizontal, vertical);
        blueComponent.HorizontalFactor = 1;
        blueComponent.VerticalFactor = 1;
        blueComponent.QuantizationTable = 1;
        blueComponent.DcTable = 1;
        blueComponent.AcTable = 1;
        auto redComponent = downsample(red, width, height, horizontal, vertical);
        redComponent.HorizontalFactor = 1;
        redComponent.VerticalFactor = 1;
        redComponent.QuantizationTable = 1;
        redComponent.DcTable = 1;
        redComponent.AcTable = 1;
        components.push_back(std::move(blueComponent));
        components.push_back(std::move(redComponent));
    }

    std::vector<std::uint8_t> output;
    appendMarker(output, 0xD8);
    appendMarker(output, 0xE0);
    appendU16(output, 16);
    output.insert(output.end(), {'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0});
    appendQuantizationTable(output, luminanceQuantization, 0);
    if (channels == 3)
        appendQuantizationTable(output, chrominanceQuantization, 1);
    appendMarker(output, 0xC0);
    appendU16(output, static_cast<std::uint16_t>(8 + 3 * components.size()));
    output.push_back(8);
    appendU16(output, static_cast<std::uint16_t>(height));
    appendU16(output, static_cast<std::uint16_t>(width));
    output.push_back(static_cast<std::uint8_t>(components.size()));
    for (std::size_t index = 0; index < components.size(); ++index) {
        output.push_back(static_cast<std::uint8_t>(index + 1));
        output.push_back(static_cast<std::uint8_t>((components[index].HorizontalFactor << 4) | components[index].VerticalFactor));
        output.push_back(components[index].QuantizationTable);
    }
    appendHuffmanTable(output, 0x00, kLuminanceDcBits, kLuminanceDcValues, sizeof(kLuminanceDcValues));
    appendHuffmanTable(output, 0x10, kLuminanceAcBits, kLuminanceAcValues, sizeof(kLuminanceAcValues));
    if (channels == 3) {
        appendHuffmanTable(output, 0x01, kChrominanceDcBits, kChrominanceDcValues, sizeof(kChrominanceDcValues));
        appendHuffmanTable(output, 0x11, kChrominanceAcBits, kChrominanceAcValues, sizeof(kChrominanceAcValues));
    }
    appendMarker(output, 0xDA);
    appendU16(output, static_cast<std::uint16_t>(6 + 2 * components.size()));
    output.push_back(static_cast<std::uint8_t>(components.size()));
    for (std::size_t index = 0; index < components.size(); ++index) {
        output.push_back(static_cast<std::uint8_t>(index + 1));
        output.push_back(static_cast<std::uint8_t>((components[index].DcTable << 4) | components[index].AcTable));
    }
    output.push_back(0);
    output.push_back(63);
    output.push_back(0);

    BitWriter writer(output);
    const std::uint32_t mcuWidth = 8u * horizontal;
    const std::uint32_t mcuHeight = 8u * vertical;
    const std::uint32_t mcuColumns = (width + mcuWidth - 1) / mcuWidth;
    const std::uint32_t mcuRows = (height + mcuHeight - 1) / mcuHeight;
    for (std::uint32_t mcuY = 0; mcuY < mcuRows; ++mcuY) {
        for (std::uint32_t mcuX = 0; mcuX < mcuColumns; ++mcuX) {
            for (std::size_t index = 0; index < components.size(); ++index) {
                auto& component = components[index];
                const auto& quantization = index == 0 ? luminanceQuantization : chrominanceQuantization;
                const auto& dc = index == 0 ? luminanceDc : chrominanceDc;
                const auto& ac = index == 0 ? luminanceAc : chrominanceAc;
                for (std::uint8_t blockY = 0; blockY < component.VerticalFactor; ++blockY) {
                    for (std::uint8_t blockX = 0; blockX < component.HorizontalFactor; ++blockX) {
                        const std::int64_t originX = static_cast<std::int64_t>(mcuX) * component.HorizontalFactor * 8 + blockX * 8;
                        const std::int64_t originY = static_cast<std::int64_t>(mcuY) * component.VerticalFactor * 8 + blockY * 8;
                        encodeBlock(writer, component, originX, originY, quantization, dc, ac);
                    }
                }
            }
        }
    }
    writer.Flush();
    appendMarker(output, 0xD9);
    return output;
}

}
