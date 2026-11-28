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
size_t JxrWmpLossyFormatsReferenceParameterSize();
void JxrWmpLossyFormatsReferenceParameters(void* value, uint8_t alphaMode, uint8_t quality, uint8_t overlap, uint8_t bitstreamFormat, uint8_t tileRows, uint8_t tileCols);
const void* JxrWmpLossyFormatsReferenceFormat(int32_t format);
int64_t JxrWmpLossyFormatsReferenceEncode(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                           const uint8_t* pixels, uint32_t stride, int32_t banded,
                                           uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written);
int64_t JxrWmpLossyFormatsReferenceEncodeBandedError(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                                     const uint8_t* pixels, uint32_t stride, int32_t linesPerCall, int32_t lastCall,
                                                     uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written);
int64_t JxrWmpLossyFormatsReferenceDecode(const uint8_t* data, size_t size, uint8_t* pixels, uint32_t stride,
                                           int32_t* width, int32_t* height);
}

namespace {

struct Decoder {
    std::int64_t (APS5_VABI* initialize)(Decoder*, class Jxr::Stream*);
    std::int64_t (APS5_VABI* getPixelFormat)(Decoder*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(Decoder*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(Decoder*, float*, float*);
    std::int64_t (APS5_VABI* getColorContext)(Decoder*, std::uint8_t*, std::uint32_t*);
    std::int64_t (APS5_VABI* getDescriptiveMetadata)(Decoder*, void*);
    std::int64_t (APS5_VABI* getRawStream)(Decoder*, class Jxr::Stream**);
    std::int64_t (APS5_VABI* copy)(Decoder*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* getFrameCount)(Decoder*, std::uint32_t*);
    std::int64_t (APS5_VABI* selectFrame)(Decoder*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(Decoder**);
    class Jxr::Stream* stream;
    std::int32_t streamOwner;
};
static_assert(offsetof(Decoder, stream) == 88 && offsetof(Decoder, streamOwner) == 96);

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestJxrWmpLossyFormats.cpp:%d: requirement failed\n", line);
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

std::vector<std::uint8_t> MakePixels(std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel) {
    std::vector<std::uint8_t> pixels(width * height * bytesPerPixel);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<std::uint8_t>((i * 37 + i / 97) & 0xff);
    }
    return pixels;
}

class Jxr::ImageEncode* CreateEncoder() {
    class Jxr::ImageEncode* encoder = nullptr;
    Require(PKImageEncode_Create_WMP_nid_postfix(&encoder) == 0 && encoder != nullptr);
    return encoder;
}

Decoder* CreateDecoder() {
    void* decoder = nullptr;
    Require(PKImageDecode_Create_WMP_nid_postfix(&decoder) == 0 && decoder != nullptr);
    return static_cast<Decoder*>(decoder);
}

class Jxr::Stream* MemoryStream(std::vector<std::uint8_t>& buffer, std::size_t size) {
    class Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, buffer.data(), size) == 0 && stream != nullptr);
    return stream;
}

}

int main() {
    InitializeHeap();

    struct FormatCase {
        int32_t format;
        uint32_t bytesPerPixel;
        JxrGuestGuid guid;
        uint8_t alphaMode;
    };

    const FormatCase formats[] = {
        {0, 2, GUID_PKPixelFormat16bppRGB555_nid_no_patch, 0},
        {1, 2, GUID_PKPixelFormat16bppRGB565_nid_no_patch, 0},
        {2, 3, GUID_PKPixelFormat24bppBGR_nid_no_patch, 0},
        {3, 4, GUID_PKPixelFormat32bppBGR_nid_no_patch, 0},
        {4, 4, GUID_PKPixelFormat32bppRGBA_nid_no_patch, 2},
        {5, 2, GUID_PKPixelFormat16bppGray_nid_no_patch, 0},
        {6, 6, GUID_PKPixelFormat48bppRGB_nid_no_patch, 0},
        {7, 8, GUID_PKPixelFormat64bppRGBA_nid_no_patch, 2},
    };

    const uint8_t overlapValues[] = {0, 2};
    const uint8_t qualityValues[] = {10, 25, 55};

    struct Layout {
        uint8_t bitstreamFormat;
        uint8_t tileRows;
        uint8_t tileCols;
    };

    const Layout layouts[] = {
        {0, 0, 0},
        {1, 0, 0},
        {0, 1, 1},
        {1, 1, 1},
    };

    for (const auto& f : formats) {
        for (const auto& layout : layouts) {
            for (const auto overlap : overlapValues) {
                for (const auto quality : qualityValues) {
                    for (const auto& size : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{32u, 32u}, {16u, 16u}}}) {
                        const auto width = size.first;
                        const auto height = size.second;

                        if ((layout.tileRows != 0 || layout.tileCols != 0) && (width == 16 && height == 16)) {
                            continue;
                        }

                        const auto bytesPerPixel = f.bytesPerPixel;
                        const auto stride = width * bytesPerPixel;
                        const auto pixels = MakePixels(width, height, bytesPerPixel);

                        const auto& expectedFormat = f.guid;
                        const auto alphaMode = f.alphaMode;

                        std::vector<std::uint8_t> guestOutput(Capacity);
                        auto* guestStream = MemoryStream(guestOutput, guestOutput.size());
                        auto* guestEncoder = CreateEncoder();
                        std::vector<std::uint8_t> guestParams(JxrWmpLossyFormatsReferenceParameterSize());
                        JxrWmpLossyFormatsReferenceParameters(guestParams.data(), alphaMode, quality, overlap, layout.bitstreamFormat, layout.tileRows, layout.tileCols);
                        Require(guestEncoder->initialize(guestEncoder, guestStream, guestParams.data(), guestParams.size()) == 0);
                        Require(guestEncoder->setPixelFormat(guestEncoder, expectedFormat) == 0);
                        Require(guestEncoder->setSize(guestEncoder, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)) == 0);
                        auto* guestWritable = const_cast<std::uint8_t*>(pixels.data());
                        Require(guestEncoder->writePixels(guestEncoder, height, guestWritable, stride) == 0);
                        Require(guestEncoder->terminate(guestEncoder) == 0);
                        std::size_t guestWritten = 0;
                        Require(guestStream->getPosition(guestStream, &guestWritten) == 0);
                        Require(guestEncoder->release(&guestEncoder) == 0 && guestEncoder == nullptr);

                        std::vector<std::uint8_t> refOutput(Capacity);
                        std::vector<std::uint8_t> refParamsVec(JxrWmpLossyFormatsReferenceParameterSize());
                        JxrWmpLossyFormatsReferenceParameters(refParamsVec.data(), alphaMode, quality, overlap, layout.bitstreamFormat, layout.tileRows, layout.tileCols);
                        std::size_t refWritten = 0;
                        int64_t err = JxrWmpLossyFormatsReferenceEncode(refParamsVec.data(), f.format, width, height, pixels.data(), stride, false, nullptr, refOutput.data(), refOutput.size(), &refWritten);
                        Require(err == 0);
                        Require(refWritten > 0);

                        Require(guestWritten == refWritten);
                        Require(guestOutput == refOutput);

                        std::vector<std::uint8_t> nativeDecoded(stride * height);
                        std::int32_t nativeWidth = 0, nativeHeight = 0;
                        const int64_t nativeDecodeResult = JxrWmpLossyFormatsReferenceDecode(
                            guestOutput.data(), guestOutput.size(), nativeDecoded.data(), stride, &nativeWidth, &nativeHeight);

                        std::vector<std::uint8_t> guestDecoded(stride * height);
                        auto* guestDecodeStream = MemoryStream(guestOutput, guestOutput.size());
                        auto* guestDecoder = CreateDecoder();
                        bool guestFailed = guestDecoder->initialize(guestDecoder, guestDecodeStream) != 0;
                        std::int32_t decWidth = 0, decHeight = 0;
                        if (!guestFailed) {
                            JxrGuestGuid formatOut{};
                            Require(guestDecoder->getPixelFormat(guestDecoder, &formatOut) == 0);
                            Require(guestDecoder->getSize(guestDecoder, &decWidth, &decHeight) == 0);
                            Require(decWidth == static_cast<std::int32_t>(width) && decHeight == static_cast<std::int32_t>(height));
                            Rect rect{0, 0, decWidth, decHeight};
                            guestFailed = guestDecoder->copy(guestDecoder, &rect, guestDecoded.data(), stride) != 0;
                        }

                        Require(nativeDecodeResult == 0);
                        Require(!guestFailed);
                        Require(nativeWidth == static_cast<std::int32_t>(width) && nativeHeight == static_cast<std::int32_t>(height));
                        Require(std::memcmp(guestDecoded.data(), nativeDecoded.data(), stride * height) == 0);

                        Require(guestDecoder->release(&guestDecoder) == 0 && guestDecoder == nullptr);
                        Require(guestDecodeStream->close(&guestDecodeStream) == 0 && guestDecodeStream == nullptr);
                    }
                }
            }
        }
    }

    Require(liveBlocks == 0);
    return 0;
}
