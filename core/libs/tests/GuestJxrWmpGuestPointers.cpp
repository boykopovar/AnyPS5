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
#include <exception>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {
std::size_t JxrWmpReferenceParameterSize();
void JxrWmpReferenceParameters(void* value, std::int32_t spatial, std::uint8_t alphaMode);
void JxrWmpReferenceConfigureDecoder(void* value);
void JxrWmpReferenceRegion(void* value, std::uint32_t left, std::uint32_t top, std::uint32_t width, std::uint32_t height);
}

namespace {

std::map<void*, std::size_t> liveAllocations;

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libSceJxr WMP guest pointer test failed at line %d\n", line);
    std::abort();
}

#define CHECK(cond) Require((cond), __LINE__)

void* APS5_VABI TrackMalloc(std::size_t bytes) {
    void* memory = std::malloc(bytes ? bytes : 1);
    CHECK(memory != nullptr);
    liveAllocations.emplace(memory, bytes);
    return memory;
}

void APS5_VABI TrackFree(void* memory) {
    if (!memory) return;
    CHECK(liveAllocations.erase(memory) == 1);
    std::free(memory);
}

void* APS5_VABI TrackCalloc(std::size_t count, std::size_t size) {
    const std::size_t total = count * size;
    void* memory = std::calloc(total ? total : 1, 1);
    CHECK(memory != nullptr);
    liveAllocations.emplace(memory, total);
    return memory;
}

void* APS5_VABI TrackRealloc(void* memory, std::size_t bytes) {
    if (memory) CHECK(liveAllocations.erase(memory) == 1);
    void* grown = std::realloc(memory, bytes ? bytes : 1);
    CHECK(grown != nullptr);
    liveAllocations.emplace(grown, bytes);
    return grown;
}

void* APS5_VABI TrackMemalign(std::size_t, std::size_t bytes) { return TrackMalloc(bytes); }
void* APS5_VABI TrackReallocalign(void* memory, std::size_t bytes, std::size_t) { return TrackRealloc(memory, bytes); }
int APS5_VABI TrackPosixMemalign(void** out, std::size_t, std::size_t bytes) { *out = TrackMalloc(bytes); return 0; }

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
    write(replacement, 0x20, reinterpret_cast<void*>(&TrackMalloc));
    write(replacement, 0x28, reinterpret_cast<void*>(&TrackFree));
    write(replacement, 0x30, reinterpret_cast<void*>(&TrackCalloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&TrackRealloc));
    write(replacement, 0x40, reinterpret_cast<void*>(&TrackMemalign));
    write(replacement, 0x48, reinterpret_cast<void*>(&TrackReallocalign));
    write(replacement, 0x50, reinterpret_cast<void*>(&TrackPosixMemalign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

template <typename TException, typename TCall>
void ExpectThrowsAs(TCall&& call, int line) {
    bool threw = false;
    try {
        call();
    } catch (const TException&) {
        threw = true;
    }
    Require(threw, line);
}

#define CHECK_THROWS_AS(type, expr) ExpectThrowsAs<type>([&] { expr; }, __LINE__)

void SetPageProtection(void* address, std::size_t size, bool readable, bool writable) {
#ifdef _WIN32
    DWORD oldProtect = 0;
    const DWORD protect = !readable ? PAGE_NOACCESS : (writable ? PAGE_READWRITE : PAGE_READONLY);
    CHECK(VirtualProtect(address, size, protect, &oldProtect) != 0);
#else
    const int prot = !readable ? PROT_NONE : (writable ? (PROT_READ | PROT_WRITE) : PROT_READ);
    CHECK(mprotect(address, size, prot) == 0);
#endif
}

struct ProtectedPages {
    std::size_t pageSize = 0;
    void* unmapped = nullptr;
    void* readOnly = nullptr;
    void* boundary = nullptr;

    ProtectedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        unmapped = VirtualAlloc(nullptr, pageSize, MEM_RESERVE, PAGE_NOACCESS);
        CHECK(unmapped != nullptr);
        readOnly = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(readOnly != nullptr);
        std::memset(readOnly, 0, pageSize);
        SetPageProtection(readOnly, pageSize, true, false);
        boundary = VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(boundary != nullptr);
        std::memset(boundary, 0, pageSize * 2);
        SetPageProtection(static_cast<std::byte*>(boundary) + pageSize, pageSize, false, false);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        unmapped = mmap(nullptr, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(unmapped != MAP_FAILED);
        readOnly = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(readOnly != MAP_FAILED);
        std::memset(readOnly, 0, pageSize);
        SetPageProtection(readOnly, pageSize, true, false);
        boundary = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(boundary != MAP_FAILED);
        std::memset(boundary, 0, pageSize * 2);
        SetPageProtection(static_cast<std::byte*>(boundary) + pageSize, pageSize, false, false);
#endif
    }

    ~ProtectedPages() {
#ifdef _WIN32
        VirtualFree(unmapped, 0, MEM_RELEASE);
        VirtualFree(readOnly, 0, MEM_RELEASE);
        VirtualFree(boundary, 0, MEM_RELEASE);
#else
        munmap(unmapped, pageSize);
        munmap(readOnly, pageSize);
        munmap(boundary, pageSize * 2);
#endif
    }
};

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
};

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Stride = Width * 3;
constexpr std::size_t StreamCapacity = 1 << 18;

Jxr::Stream* CreateMemoryStream(std::vector<std::uint8_t>& buffer) {
    Jxr::Stream* stream = nullptr;
    CHECK(CreateWS_Memory_nid_postfix(&stream, buffer.data(), buffer.size()) == 0 && stream != nullptr);
    return stream;
}

}

int main() {
    InitializeHeap();
    ProtectedPages pages;

    auto** badEncoderOut = static_cast<Jxr::ImageEncode**>(pages.unmapped);
    auto** roEncoderOut = static_cast<Jxr::ImageEncode**>(pages.readOnly);
    auto** badDecoderOut = static_cast<void**>(pages.unmapped);
    auto** roDecoderOut = static_cast<void**>(pages.readOnly);

    CHECK_THROWS_AS(std::invalid_argument, PKImageEncode_Create_WMP_nid_postfix(nullptr));
    CHECK_THROWS_AS(std::runtime_error, PKImageEncode_Create_WMP_nid_postfix(badEncoderOut));
    CHECK_THROWS_AS(std::runtime_error, PKImageEncode_Create_WMP_nid_postfix(roEncoderOut));
    CHECK(liveAllocations.empty());

    CHECK_THROWS_AS(std::invalid_argument, PKImageDecode_Create_WMP_nid_postfix(nullptr));
    CHECK_THROWS_AS(std::runtime_error, PKImageDecode_Create_WMP_nid_postfix(badDecoderOut));
    CHECK_THROWS_AS(std::runtime_error, PKImageDecode_Create_WMP_nid_postfix(roDecoderOut));
    CHECK(liveAllocations.empty());

    std::vector<std::uint8_t> parameters(JxrWmpReferenceParameterSize());
    JxrWmpReferenceParameters(parameters.data(), 0, 0);
    std::vector<std::uint8_t> sourcePixels(Height * Stride);
    for (std::size_t i = 0; i < sourcePixels.size(); ++i)
        sourcePixels[i] = static_cast<std::uint8_t>((i * 29 + 7) & 0xFF);
    std::vector<std::uint8_t> encoded(StreamCapacity);

    auto* encodeStream = CreateMemoryStream(encoded);
    Jxr::ImageEncode* encoder = nullptr;
    CHECK(PKImageEncode_Create_WMP_nid_postfix(&encoder) == 0 && encoder != nullptr);
    CHECK_THROWS_AS(std::invalid_argument, encoder->release(nullptr));
    CHECK_THROWS_AS(std::runtime_error, encoder->release(badEncoderOut));

    auto* boundaryParams = static_cast<std::byte*>(pages.boundary) + pages.pageSize - 8;
    CHECK_THROWS_AS(std::runtime_error, encoder->initialize(encoder, encodeStream, pages.unmapped, parameters.size()));
    CHECK_THROWS_AS(std::runtime_error, encoder->initialize(encoder, encodeStream, boundaryParams, parameters.size()));
    CHECK(encoder->initialize(encoder, encodeStream, parameters.data(), parameters.size()) == 0);
    CHECK(encoder->setPixelFormat(encoder, GUID_PKPixelFormat24bppBGR_nid_no_patch) == 0);
    CHECK(encoder->setSize(encoder, Width, Height) == 0);

    auto* boundaryPixels = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - 64);
    auto* smallStridePixels = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - 40);
    CHECK_THROWS_AS(std::runtime_error, encoder->writePixels(encoder, Height, nullptr, Stride));
    CHECK_THROWS_AS(std::runtime_error, encoder->writePixels(encoder, Height, static_cast<std::uint8_t*>(pages.unmapped), 0));
    CHECK_THROWS_AS(std::runtime_error, encoder->writePixels(encoder, Height, smallStridePixels, 1));
    CHECK_THROWS_AS(std::runtime_error, encoder->writePixels(encoder, Height, static_cast<std::uint8_t*>(pages.unmapped), Stride));
    CHECK_THROWS_AS(std::runtime_error, encoder->writePixels(encoder, Height, boundaryPixels, Stride));
    constexpr std::uint32_t PaddedStride = Stride + 16;
    constexpr std::size_t ExactPaddedSpan = static_cast<std::size_t>(Height - 1) * PaddedStride + Stride;
    CHECK(pages.pageSize >= ExactPaddedSpan);
    auto* unpaddedLastRowSrc = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - ExactPaddedSpan);
    for (std::uint32_t row = 0; row < Height; ++row)
        std::memcpy(unpaddedLastRowSrc + static_cast<std::size_t>(row) * PaddedStride, sourcePixels.data() + static_cast<std::size_t>(row) * Stride, Stride);
    CHECK(encoder->writePixels(encoder, Height, unpaddedLastRowSrc, PaddedStride) == 0);
    std::size_t encodedBytes = 0;
    CHECK(encodeStream->getPosition(encodeStream, &encodedBytes) == 0 && encodedBytes > 0);
    CHECK(encoder->terminate(encoder) == 0);

    SetPageProtection(pages.readOnly, pages.pageSize, true, true);
    *roEncoderOut = encoder;
    SetPageProtection(pages.readOnly, pages.pageSize, true, false);
    const auto liveBeforeRoRelease = liveAllocations.size();
    CHECK_THROWS_AS(std::runtime_error, encoder->release(roEncoderOut));
    CHECK(*roEncoderOut == encoder && liveAllocations.size() == liveBeforeRoRelease);
    SetPageProtection(pages.readOnly, pages.pageSize, true, true);
    *roEncoderOut = nullptr;
    SetPageProtection(pages.readOnly, pages.pageSize, true, false);

    CHECK(encoder->release(&encoder) == 0 && encoder == nullptr);
    CHECK(liveAllocations.empty());

    std::vector<std::uint8_t> bandedOutput(StreamCapacity);
    auto* bandedStream = CreateMemoryStream(bandedOutput);
    Jxr::ImageEncode* bandedEncoder = nullptr;
    CHECK(PKImageEncode_Create_WMP_nid_postfix(&bandedEncoder) == 0 && bandedEncoder != nullptr);
    CHECK(bandedEncoder->initialize(bandedEncoder, bandedStream, parameters.data(), parameters.size()) == 0);
    CHECK(bandedEncoder->setPixelFormat(bandedEncoder, GUID_PKPixelFormat24bppBGR_nid_no_patch) == 0);
    CHECK(bandedEncoder->setSize(bandedEncoder, Width, Height) == 0);
    CHECK(bandedEncoder->writePixelsBandedBegin(bandedEncoder, nullptr) == 0);
    CHECK_THROWS_AS(std::runtime_error, bandedEncoder->writePixelsBanded(bandedEncoder, 16, nullptr, Stride, 0));
    CHECK_THROWS_AS(std::runtime_error, bandedEncoder->writePixelsBanded(bandedEncoder, 16, static_cast<std::uint8_t*>(pages.unmapped), 0, 0));
    CHECK_THROWS_AS(std::runtime_error, bandedEncoder->writePixelsBanded(bandedEncoder, 16, smallStridePixels, 1, 0));
    CHECK_THROWS_AS(std::runtime_error, bandedEncoder->writePixelsBanded(bandedEncoder, 16, static_cast<std::uint8_t*>(pages.unmapped), Stride, 0));
    CHECK_THROWS_AS(std::runtime_error, bandedEncoder->writePixelsBanded(bandedEncoder, 16, boundaryPixels, Stride, 0));
    CHECK(bandedEncoder->release(&bandedEncoder) == 0 && bandedEncoder == nullptr);
    CHECK(liveAllocations.empty());

    auto* decodeStream = CreateMemoryStream(encoded);
    void* decoderRaw = nullptr;
    CHECK(PKImageDecode_Create_WMP_nid_postfix(&decoderRaw) == 0 && decoderRaw != nullptr);
    auto* decoder = static_cast<Decoder*>(decoderRaw);
    CHECK(decoder->initialize(decoder, decodeStream) == 0);

    std::int32_t width = 0;
    std::int32_t height = 0;
    CHECK(decoder->getSize(decoder, &width, &height) == 0);
    CHECK(width == static_cast<std::int32_t>(Width) && height == static_cast<std::int32_t>(Height));

    std::int32_t validInt = 0;
    auto* badInt = static_cast<std::int32_t*>(pages.unmapped);
    auto* roInt = static_cast<std::int32_t*>(pages.readOnly);
    CHECK_THROWS_AS(std::runtime_error, decoder->getSize(decoder, badInt, &validInt));
    CHECK_THROWS_AS(std::runtime_error, decoder->getSize(decoder, &validInt, badInt));
    CHECK_THROWS_AS(std::runtime_error, decoder->getSize(decoder, roInt, &validInt));
    CHECK_THROWS_AS(std::runtime_error, decoder->getSize(decoder, &validInt, roInt));

    Jxr::Stream* rawStream = nullptr;
    CHECK(decoder->getRawStream(decoder, &rawStream) == 0 && rawStream == decodeStream);
    CHECK_THROWS_AS(std::runtime_error, decoder->getRawStream(decoder, static_cast<Jxr::Stream**>(pages.unmapped)));
    CHECK_THROWS_AS(std::runtime_error, decoder->getRawStream(decoder, static_cast<Jxr::Stream**>(pages.readOnly)));

    CHECK_THROWS_AS(std::runtime_error, decoder->getColorContext(decoder, nullptr, static_cast<std::uint32_t*>(pages.unmapped)));
    CHECK_THROWS_AS(std::runtime_error, decoder->getColorContext(decoder, nullptr, static_cast<std::uint32_t*>(pages.readOnly)));
    std::uint32_t nonzeroSize = 8;
    CHECK_THROWS_AS(std::runtime_error, decoder->getColorContext(decoder, static_cast<std::uint8_t*>(pages.readOnly), &nonzeroSize));

    auto* regionStream = CreateMemoryStream(encoded);
    void* regionDecoderRaw = nullptr;
    CHECK(PKImageDecode_Create_WMP_nid_postfix(&regionDecoderRaw) == 0 && regionDecoderRaw != nullptr);
    auto* regionDecoder = static_cast<Decoder*>(regionDecoderRaw);
    CHECK(regionDecoder->initialize(regionDecoder, regionStream) == 0);
    JxrWmpReferenceConfigureDecoder(regionDecoder);
    constexpr std::uint32_t RegionLeft = 8;
    constexpr std::uint32_t RegionWidth = 16;
    constexpr std::uint32_t RegionStride = RegionWidth * 3;
    JxrWmpReferenceRegion(regionDecoder, RegionLeft, 0, RegionWidth, Height);
    const Rect regionRect{0, 0, width, height};
    auto* regionPixels = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - Height * RegionStride);
    CHECK_THROWS_AS(std::runtime_error, regionDecoder->copy(regionDecoder, &regionRect, regionPixels + 1, RegionStride));
    CHECK(regionDecoder->copy(regionDecoder, &regionRect, regionPixels, RegionStride) == 0);
    for (std::uint32_t row = 0; row < Height; ++row)
        CHECK(std::memcmp(regionPixels + row * RegionStride, sourcePixels.data() + row * Stride + RegionLeft * 3, RegionStride) == 0);
    CHECK(regionDecoder->release(&regionDecoder) == 0 && regionDecoder == nullptr);
    CHECK(regionStream->close(&regionStream) == 0 && regionStream == nullptr);

    JxrWmpReferenceConfigureDecoder(decoder);
    const Rect validRect{0, 0, width, height};
    std::vector<std::uint8_t> decodedPixels(Height * Stride);
    for (const std::int32_t rectangleHeight : {-1, std::numeric_limits<std::int32_t>::min()}) {
        const Rect negativeHeightRect{0, 0, width, rectangleHeight};
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &negativeHeightRect, nullptr, Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &negativeHeightRect, static_cast<std::uint8_t*>(pages.unmapped), Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &negativeHeightRect, static_cast<std::uint8_t*>(pages.readOnly), Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &negativeHeightRect, decodedPixels.data(), Stride));
    }
    auto* boundaryRect = static_cast<std::byte*>(pages.boundary) + pages.pageSize - 8;
    CHECK_THROWS_AS(std::invalid_argument, decoder->copy(decoder, nullptr, decodedPixels.data(), Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, pages.unmapped, decodedPixels.data(), Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, boundaryRect, decodedPixels.data(), Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, nullptr, Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, static_cast<std::uint8_t*>(pages.unmapped), 0));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, smallStridePixels, 1));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, static_cast<std::uint8_t*>(pages.unmapped), Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, static_cast<std::uint8_t*>(pages.readOnly), Stride));
    CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &validRect, boundaryPixels, Stride));
    for (const std::int32_t rectangleWidth : {0, 1, -1, width + 1}) {
        const Rect ignoredWidthRect{0, 0, rectangleWidth, height};
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &ignoredWidthRect, nullptr, Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &ignoredWidthRect, static_cast<std::uint8_t*>(pages.unmapped), Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &ignoredWidthRect, static_cast<std::uint8_t*>(pages.readOnly), Stride));
        CHECK_THROWS_AS(std::runtime_error, decoder->copy(decoder, &ignoredWidthRect, boundaryPixels, Stride));
    }
    auto* unpaddedLastRowDst = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - ExactPaddedSpan);
    CHECK(decoder->copy(decoder, &validRect, unpaddedLastRowDst, PaddedStride) == 0);
    for (std::uint32_t row = 0; row < Height; ++row)
        std::memcpy(decodedPixels.data() + static_cast<std::size_t>(row) * Stride,
                    unpaddedLastRowDst + static_cast<std::size_t>(row) * PaddedStride, Stride);
    CHECK(decodedPixels == sourcePixels);

    auto* partialStream = CreateMemoryStream(encoded);
    void* partialDecoderRaw = nullptr;
    CHECK(PKImageDecode_Create_WMP_nid_postfix(&partialDecoderRaw) == 0 && partialDecoderRaw != nullptr);
    auto* partialDecoder = static_cast<Decoder*>(partialDecoderRaw);
    CHECK(partialDecoder->initialize(partialDecoder, partialStream) == 0);
    JxrWmpReferenceConfigureDecoder(partialDecoder);
    JxrWmpReferenceRegion(partialDecoder, 0, 0, 0, 0);
    auto* partialPixels = reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(pages.boundary) + pages.pageSize - Height * Stride);
    const Rect firstBand{0, 0, 1, static_cast<std::int32_t>(Height / 2)};
    const Rect secondBand{0, static_cast<std::int32_t>(Height / 2), 0, static_cast<std::int32_t>(Height / 2)};
    CHECK(partialDecoder->copy(partialDecoder, &firstBand, partialPixels, Stride) == 0);
    CHECK(partialDecoder->copy(partialDecoder, &secondBand, partialPixels + Height / 2 * Stride, Stride) == 0);
    CHECK(std::memcmp(partialPixels, sourcePixels.data(), Height * Stride) == 0);
    CHECK(partialDecoder->release(&partialDecoder) == 0 && partialDecoder == nullptr);
    CHECK(partialStream->close(&partialStream) == 0 && partialStream == nullptr);

    auto* tightDecodeStream = CreateMemoryStream(encoded);
    void* tightDecoderRaw = nullptr;
    CHECK(PKImageDecode_Create_WMP_nid_postfix(&tightDecoderRaw) == 0 && tightDecoderRaw != nullptr);
    auto* tightDecoder = static_cast<Decoder*>(tightDecoderRaw);
    CHECK(tightDecoder->initialize(tightDecoder, tightDecodeStream) == 0);
    JxrWmpReferenceConfigureDecoder(tightDecoder);
    std::vector<std::uint8_t> tightDecodedPixels(Height * Stride);
    const Rect ignoredWidthRect{0, 0, 0, height};
    CHECK(tightDecoder->copy(tightDecoder, &ignoredWidthRect, tightDecodedPixels.data(), Stride) == 0);
    CHECK(tightDecodedPixels == sourcePixels);
    CHECK(tightDecoder->release(&tightDecoder) == 0 && tightDecoder == nullptr);
    CHECK(tightDecodeStream->close(&tightDecodeStream) == 0 && tightDecodeStream == nullptr);

    CHECK_THROWS_AS(std::runtime_error, decoder->release(reinterpret_cast<Decoder**>(badDecoderOut)));
    SetPageProtection(pages.readOnly, pages.pageSize, true, true);
    *roDecoderOut = decoder;
    SetPageProtection(pages.readOnly, pages.pageSize, true, false);
    const auto liveBeforeRoDecoderRelease = liveAllocations.size();
    CHECK_THROWS_AS(std::runtime_error, decoder->release(reinterpret_cast<Decoder**>(roDecoderOut)));
    CHECK(*roDecoderOut == decoder && liveAllocations.size() == liveBeforeRoDecoderRelease);
    SetPageProtection(pages.readOnly, pages.pageSize, true, true);
    *roDecoderOut = nullptr;
    SetPageProtection(pages.readOnly, pages.pageSize, true, false);

    CHECK(decoder->release(nullptr) == 0);
    CHECK(decoder->release(&decoder) == 0 && decoder == nullptr);
    CHECK(decodeStream->close(&decodeStream) == 0 && decodeStream == nullptr);
    CHECK(liveAllocations.empty());
    return 0;
}
