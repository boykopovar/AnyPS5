#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" {
size_t JxrWmpRoundtripReferenceParameterSize();
void JxrWmpRoundtripReferenceParameters(void* value, int32_t spatial, uint8_t alphaMode, uint8_t quality);
const void* JxrWmpRoundtripReferenceFormat(int32_t format);
int64_t JxrWmpRoundtripReferenceEncode(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                       const uint8_t* pixels, uint32_t stride, int32_t banded,
                                       uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written);
int64_t JxrWmpRoundtripReferenceEncodeBandedError(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                                   const uint8_t* pixels, uint32_t stride, int32_t linesPerCall, int32_t lastCall,
                                                   uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written);
int64_t JxrWmpRoundtripReferenceDecode(const uint8_t* data, size_t size, uint8_t* pixels, uint32_t stride,
                                       int32_t* width, int32_t* height);
void JxrWmpRoundtripReferenceSetDecoderAlphaMode(void* value, uint8_t alphaMode);
}

namespace {

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestJxrWmpRoundtrip.cpp:%d: requirement failed\n", line);
    std::abort();
}
#define Require(value) Check((value), __LINE__)

constexpr std::size_t Capacity = 1 << 20;

long liveBlocks = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++liveBlocks;
    return std::malloc(size);
}
void APS5_VABI Free(void* pointer) {
    if (pointer) --liveBlocks;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++liveBlocks;
    return std::calloc(count, size);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

struct Decoder {
    std::int64_t (APS5_VABI* initialize)(Decoder*, Jxr::Stream*);
    std::int64_t (APS5_VABI* getPixelFormat)(Decoder*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(Decoder*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(Decoder*, float*, float*);
    std::int64_t (APS5_VABI* getColorContext)(Decoder*, std::uint8_t*, std::uint32_t*);
    std::int64_t (APS5_VABI* getDescriptiveMetadata)(Decoder*, void*);
    std::int64_t (APS5_VABI* getRawStream)(Decoder*, Jxr::Stream**);
    std::int64_t (APS5_VABI* copy)(Decoder*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* getFrameCount)(Decoder*, std::uint32_t*);
    std::int64_t (APS5_VABI* selectFrame)(Decoder*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(Decoder**);
    Jxr::Stream* stream;
    std::int32_t streamOwner;
};

static_assert(offsetof(Decoder, stream) == 88 && offsetof(Decoder, streamOwner) == 96);

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

std::vector<std::uint8_t> MakeParameters(std::int32_t spatial, std::uint8_t alphaMode, std::uint8_t quality) {
    std::vector<std::uint8_t> value(JxrWmpRoundtripReferenceParameterSize());
    JxrWmpRoundtripReferenceParameters(value.data(), spatial, alphaMode, quality);
    return value;
}

std::vector<std::uint8_t> MakePixels(std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel) {
    std::vector<std::uint8_t> pixels(width * height * bytesPerPixel);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<std::uint8_t>((i * 37 + i / 97) & 0xff);
    }
    return pixels;
}

std::vector<std::uint8_t> MakePixelsStrided(std::uint32_t width, std::uint32_t height, std::uint32_t stride, std::uint32_t bytesPerPixel) {
    std::vector<std::uint8_t> pixels(stride * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width * bytesPerPixel; ++x) {
            std::size_t idx = y * width * bytesPerPixel + x;
            pixels[y * stride + x] = static_cast<std::uint8_t>((idx * 37 + idx / 97) & 0xff);
        }
        for (std::uint32_t x = width * bytesPerPixel; x < stride; ++x) {
            pixels[y * stride + x] = 0xAA;
        }
    }
    return pixels;
}

Jxr::ImageEncode* CreateEncoder() {
    Jxr::ImageEncode* encoder = nullptr;
    Require(PKImageEncode_Create_WMP_nid_postfix(&encoder) == 0 && encoder != nullptr);
    return encoder;
}

Decoder* CreateDecoder() {
    void* decoder = nullptr;
    Require(PKImageDecode_Create_WMP_nid_postfix(&decoder) == 0 && decoder != nullptr);
    return static_cast<Decoder*>(decoder);
}

Jxr::Stream* MemoryStream(std::vector<std::uint8_t>& buffer, std::size_t size) {
    Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, buffer.data(), size) == 0 && stream != nullptr);
    return stream;
}

void CheckDecodedPixels(const std::vector<std::uint8_t>& decoded, const std::vector<std::uint8_t>& pixels,
                        const std::vector<std::uint8_t>& encoded, std::uint32_t width, std::uint32_t height,
                        std::uint32_t stride, std::uint32_t bytesPerPixel, int32_t format, std::uint8_t alphaMode) {
    if (format == 1 && alphaMode == 0) {
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                Require(std::memcmp(decoded.data() + y * stride + x * 4, pixels.data() + y * stride + x * 4, 3) == 0);
            }
        }
        std::vector<std::uint8_t> expectedDecoded(stride * height);
        std::int32_t refWidth = 0, refHeight = 0;
        Require(JxrWmpRoundtripReferenceDecode(encoded.data(), encoded.size(), expectedDecoded.data(), stride,
                                               &refWidth, &refHeight) == 0);
        Require(refWidth == static_cast<std::int32_t>(width) && refHeight == static_cast<std::int32_t>(height));
        for (std::uint32_t y = 0; y < height; ++y) {
            Require(std::memcmp(decoded.data() + y * stride, expectedDecoded.data() + y * stride,
                                width * bytesPerPixel) == 0);
        }
    } else {
        for (std::uint32_t y = 0; y < height; ++y) {
            Require(std::memcmp(decoded.data() + y * stride, pixels.data() + y * stride, width * bytesPerPixel) == 0);
        }
    }
}

struct Encoded {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> temporary;
};

Encoded RunGuestEncoder(std::vector<std::uint8_t>& parameters, std::uint32_t width,
                        std::uint32_t height, const std::vector<std::uint8_t>& pixels, std::uint32_t stride,
                        const JxrGuestGuid& expectedFormat, std::uint8_t alphaMode, bool banded,
                        std::uint32_t bandHeight, std::int32_t lastFlag, std::size_t expectedSize) {
    const bool planarTemporary = banded && (alphaMode == 2);
    std::vector<std::uint8_t> output(Capacity);
    std::vector<std::uint8_t> temporaryBuffer(planarTemporary ? Capacity : 0);
    Jxr::Stream* temporary = planarTemporary ? MemoryStream(temporaryBuffer, temporaryBuffer.size()) : nullptr;
    auto* stream = MemoryStream(output, output.size());
    auto* encoder = CreateEncoder();
    Require(encoder->initialize(encoder, stream, parameters.data(), parameters.size()) == 0);
    Require(encoder->setPixelFormat(encoder, expectedFormat) == 0);
    Require(encoder->setSize(encoder, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)) == 0);
    auto* writable = const_cast<std::uint8_t*>(pixels.data());
    if (!banded) {
        Require(encoder->writePixels(encoder, height, writable, stride) == 0);
    } else {
        Require(encoder->writePixelsBandedBegin(encoder, temporary) == 0);
        if (bandHeight == height) {
            Require(encoder->writePixelsBanded(encoder, height, writable, stride, lastFlag) == 0);
        } else {
            std::uint32_t linesWritten = 0;
            while (linesWritten + bandHeight <= height) {
                Require(encoder->writePixelsBanded(encoder, bandHeight, writable + linesWritten * stride, stride, 0) == 0);
                linesWritten += bandHeight;
            }
            if (linesWritten < height) {
                Require(encoder->writePixelsBanded(encoder, height - linesWritten, writable + linesWritten * stride, stride, lastFlag) == 0);
            }
        }
        Require(encoder->writePixelsBandedEnd(encoder) == 0);
    }
    Require(encoder->terminate(encoder) == 0);
    std::size_t written = 0;
    Require(stream->getPosition(stream, &written) == 0);
    Require(encoder->release(&encoder) == 0 && encoder == nullptr);
    if (temporary != nullptr) Require(temporary->close(&temporary) == 0 && temporary == nullptr);
    Require(written == expectedSize);
    return {std::move(output), std::move(temporaryBuffer)};
}

void DecodeAndCheck(std::vector<std::uint8_t>& output, const std::vector<std::uint8_t>& pixels,
                    std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                    std::uint32_t bytesPerPixel, int32_t format, std::uint8_t alphaMode,
                    const JxrGuestGuid& expectedFormat) {
    auto* decodeStream = MemoryStream(output, output.size());
    auto* decoder = CreateDecoder();
    Require(decoder->initialize(decoder, decodeStream) == 0);
    JxrWmpRoundtripReferenceSetDecoderAlphaMode(decoder, alphaMode);
    JxrGuestGuid formatOut{};
    Require(decoder->getPixelFormat(decoder, &formatOut) == 0);
    Require(std::memcmp(&formatOut, &expectedFormat, sizeof(formatOut)) == 0);
    std::int32_t decWidth = 0, decHeight = 0;
    Require(decoder->getSize(decoder, &decWidth, &decHeight) == 0);
    Require(decWidth == static_cast<std::int32_t>(width) && decHeight == static_cast<std::int32_t>(height));
    std::vector<std::uint8_t> decoded(stride * height);
    const Rect rect{0, 0, decWidth, decHeight};
    Require(decoder->copy(decoder, &rect, decoded.data(), stride) == 0);
    CheckDecodedPixels(decoded, pixels, output, width, height, stride, bytesPerPixel, format, alphaMode);
    Require(decoder->release(&decoder) == 0 && decoder == nullptr);
    Require(decodeStream->close(&decodeStream) == 0 && decodeStream == nullptr);
}

void TestLosslessRoundtrip(int32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel,
                           const JxrGuestGuid& expectedFormat, std::uint8_t alphaMode, std::uint32_t stridePadding) {
    const auto stride = width * bytesPerPixel + stridePadding;
    const auto pixels = stridePadding ? MakePixelsStrided(width, height, stride, bytesPerPixel)
                                      : MakePixels(width, height, bytesPerPixel);
    auto parameters = MakeParameters(0, alphaMode, 1);
    const auto blocksBefore = liveBlocks;

    std::vector<std::uint8_t> expected(Capacity);
    std::size_t expectedSize = 0;
    Require(JxrWmpRoundtripReferenceEncode(parameters.data(), format, width, height, pixels.data(), stride, 0,
                                           nullptr, expected.data(), expected.size(), &expectedSize) == 0);
    Require(expectedSize > 0);

    auto encoded = RunGuestEncoder(parameters, width, height, pixels, stride, expectedFormat, alphaMode,
                                   false, 0, 0, expectedSize);
    Require(encoded.bytes == expected);

    DecodeAndCheck(encoded.bytes, pixels, width, height, stride, bytesPerPixel, format, alphaMode, expectedFormat);
    Require(liveBlocks == blocksBefore);
}

void TestBandedRoundtrip(int32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel,
                         const JxrGuestGuid& expectedFormat, std::uint8_t alphaMode) {
    const auto stride = width * bytesPerPixel;
    const auto pixels = MakePixels(width, height, bytesPerPixel);
    auto parameters = MakeParameters(0, alphaMode, 1);
    const bool planarTemporary = (alphaMode == 2);
    const auto blocksBefore = liveBlocks;

    std::vector<std::uint8_t> expected(Capacity);
    std::size_t expectedSize = 0;
    std::vector<std::uint8_t> expectedTemporary(planarTemporary ? Capacity : 0);
    Require(JxrWmpRoundtripReferenceEncode(parameters.data(), format, width, height, pixels.data(), stride, 1,
                                           planarTemporary ? expectedTemporary.data() : nullptr,
                                           expected.data(), expected.size(), &expectedSize) == 0);
    Require(expectedSize > 0);

    auto encoded = RunGuestEncoder(parameters, width, height, pixels, stride, expectedFormat, alphaMode,
                                   true, 16, 1, expectedSize);
    Require(encoded.bytes == expected);
    if (planarTemporary) {
        Require(std::memcmp(encoded.temporary.data(), expectedTemporary.data(), expectedTemporary.size()) == 0);
    }

    DecodeAndCheck(encoded.bytes, pixels, width, height, stride, bytesPerPixel, format, alphaMode, expectedFormat);
    Require(liveBlocks == blocksBefore);
}

void TestBandedErrorNonMultipleOf16(int32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel,
                                      std::uint8_t alphaMode) {
    const auto stride = width * bytesPerPixel;
    const auto pixels = MakePixels(width, height, bytesPerPixel);
    auto parameters = MakeParameters(0, alphaMode, 1);
    const bool planarTemporary = (alphaMode == 2);
    JxrGuestGuid expectedFormat{};
    std::memcpy(&expectedFormat, JxrWmpRoundtripReferenceFormat(format), sizeof(expectedFormat));
    const auto blocksBefore = liveBlocks;

    std::vector<std::uint8_t> output(Capacity);
    std::vector<std::uint8_t> temporaryBuffer(planarTemporary ? Capacity : 0);
    std::size_t written = 0;
    const auto err = JxrWmpRoundtripReferenceEncodeBandedError(parameters.data(), format, width, height,
                                                                 pixels.data(), stride, 15, 0,
                                                                 planarTemporary ? temporaryBuffer.data() : nullptr,
                                                                 output.data(), output.size(), &written);
    Require(err == -111);

    auto* stream = MemoryStream(output, output.size());
    Jxr::Stream* temporary = planarTemporary ? MemoryStream(temporaryBuffer, temporaryBuffer.size()) : nullptr;
    auto* encoder = CreateEncoder();
    Require(encoder->initialize(encoder, stream, parameters.data(), parameters.size()) == 0);
    Require(encoder->setPixelFormat(encoder, expectedFormat) == 0);
    Require(encoder->setSize(encoder, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)) == 0);
    Require(encoder->writePixelsBandedBegin(encoder, temporary) == 0);
    auto* writable = const_cast<std::uint8_t*>(pixels.data());
    Require(encoder->writePixelsBanded(encoder, 15, writable, stride, 0) == -111);
    Require(encoder->terminate(encoder) == 0);
    Require(encoder->release(&encoder) == 0 && encoder == nullptr);
    if (temporary != nullptr) Require(temporary->close(&temporary) == 0 && temporary == nullptr);
    Require(liveBlocks == blocksBefore);
}

void TestBandedSuccessNonMultipleOf16Last(int32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel,
                                           const JxrGuestGuid& expectedFormat, std::uint8_t alphaMode) {
    const auto stride = width * bytesPerPixel;
    const auto pixels = MakePixels(width, height, bytesPerPixel);
    auto parameters = MakeParameters(0, alphaMode, 1);
    const bool planarTemporary = (alphaMode == 2);
    const auto blocksBefore = liveBlocks;

    std::vector<std::uint8_t> expected(Capacity);
    std::size_t expectedSize = 0;
    std::vector<std::uint8_t> expectedTemporary(planarTemporary ? Capacity : 0);
    Require(JxrWmpRoundtripReferenceEncodeBandedError(parameters.data(), format, width, height,
                                                       pixels.data(), stride, height, 1,
                                                       planarTemporary ? expectedTemporary.data() : nullptr,
                                                       expected.data(), expected.size(), &expectedSize) == 0);
    Require(expectedSize > 0);

    auto encoded = RunGuestEncoder(parameters, width, height, pixels, stride, expectedFormat, alphaMode,
                                   true, height, 1, expectedSize);
    Require(encoded.bytes == expected);

    DecodeAndCheck(encoded.bytes, pixels, width, height, stride, bytesPerPixel, format, alphaMode, expectedFormat);
    Require(liveBlocks == blocksBefore);
}

void RunAllTests() {
    struct FormatCase {
        std::int32_t format;
        std::uint32_t bytesPerPixel;
        JxrGuestGuid guid;
        std::uint8_t alphaMode;
    };
    struct SizeCase {
        std::uint32_t width;
        std::uint32_t height;
        bool strided;
    };
    const FormatCase formats[] = {
        {0, 3, JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch, 0},
        {1, 4, JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch, 0},
        {1, 4, JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch, 2},
        {2, 1, JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch, 0},
    };
    const SizeCase sizes[] = {
        {32, 32, true},
        {17, 19, true},
        {33, 13, false},
        {23, 17, false},
    };

    for (const auto& f : formats) {
        for (const auto& s : sizes) {
            TestLosslessRoundtrip(f.format, s.width, s.height, f.bytesPerPixel, f.guid, f.alphaMode, 0);
        }
        for (const auto& s : sizes) {
            if (s.strided) {
                TestLosslessRoundtrip(f.format, s.width, s.height, f.bytesPerPixel, f.guid, f.alphaMode, 7);
            }
        }
        for (const auto& s : sizes) {
            TestBandedRoundtrip(f.format, s.width, s.height, f.bytesPerPixel, f.guid, f.alphaMode);
        }
        TestBandedErrorNonMultipleOf16(f.format, 17, 19, f.bytesPerPixel, f.alphaMode);
        TestBandedSuccessNonMultipleOf16Last(f.format, 17, 19, f.bytesPerPixel, f.guid, f.alphaMode);
    }
}

}

int main() {
    InitializeHeap();
    RunAllTests();
    Require(liveBlocks == 0);
    return 0;
}
