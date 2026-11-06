#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

extern "C" const void* GetPixelFormatFromHash(unsigned char hash);
extern "C" std::size_t JxrTranscodeFixtureOverlapOffset();

namespace {

void Require(bool value) { if (!value) std::abort(); }

void* lastCalloc = nullptr;
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastFreed = nullptr;
int frees = 0;
bool failCalloc = false;
JxrGuestGuid transcodeFormat{};
std::vector<int> transcodeOrder;
std::int64_t copyResult = 0;
std::int64_t writeResult = 0;
std::int64_t transcodeResult = 0;
std::int64_t sourceFormatResult = 0;
std::int64_t targetFormatResult = 0;
std::size_t copiedStride = 0;
std::uint32_t writtenLines = 0;
std::uint32_t writtenStride = 0;
bool sawTranscodeParameters = false;
void* expectedDecoder = nullptr;
std::byte* encoderForMutation = nullptr;
bool mutateWriteCallback = false;
void Require(bool value);
template <typename Function>
std::uintptr_t FunctionAddress(Function function);
std::int64_t APS5_VABI WritePixelsAfterMutation(void*, std::uint32_t, std::uint8_t*, std::uint32_t);

std::int64_t APS5_VABI GetSourcePixelFormat(void*, JxrGuestGuid* format) {
    transcodeOrder.push_back(1);
    *format = transcodeFormat;
    return sourceFormatResult;
}

std::int64_t APS5_VABI GetPixelFormat(void*, JxrGuestGuid* format) {
    transcodeOrder.push_back(2);
    *format = transcodeFormat;
    return targetFormatResult;
}

std::int64_t APS5_VABI CopyPixels(void*, const void*, std::uint8_t* output, std::uint32_t stride) {
    transcodeOrder.push_back(3);
    copiedStride = stride;
    std::memset(output, 0x5a, stride * 2);
    if (mutateWriteCallback) {
        const auto callback = FunctionAddress(&WritePixelsAfterMutation);
        std::memcpy(encoderForMutation + 56, &callback, sizeof(callback));
    }
    return copyResult;
}

std::int64_t APS5_VABI WritePixels(void*, std::uint32_t lines, std::uint8_t* pixels, std::uint32_t stride) {
    transcodeOrder.push_back(4);
    writtenLines = lines;
    writtenStride = stride;
    for (std::size_t index = 0; index < static_cast<std::size_t>(lines) * stride; ++index)
        Require(pixels[index] == 0x5a);
    return writeResult;
}

std::int64_t APS5_VABI WritePixelsAfterMutation(void*, std::uint32_t lines, std::uint8_t* pixels, std::uint32_t stride) {
    transcodeOrder.push_back(6);
    writtenLines = lines;
    writtenStride = stride;
    for (std::size_t index = 0; index < static_cast<std::size_t>(lines) * stride; ++index)
        Require(pixels[index] == 0x5a);
    return 38;
}

struct TestTranscodingParam {
    std::size_t leftX;
    std::size_t width;
    std::size_t topY;
    std::size_t height;
    std::int32_t bitstreamFormat;
    std::uint8_t alphaMode;
    std::uint8_t padding[3];
    std::int32_t subband;
    std::int32_t orientation;
    std::int32_t ignoreOverlap;
};

static_assert(sizeof(TestTranscodingParam) == 56);

std::int64_t APS5_VABI TranscodePixels(void*, void* decoder, void* parameter) {
    transcodeOrder.push_back(5);
    const auto* value = static_cast<const TestTranscodingParam*>(parameter);
    sawTranscodeParameters = decoder == expectedDecoder && value->leftX == 11 && value->topY == 13 &&
                             value->width == 17 && value->height == 19 && value->orientation == 3 &&
                             value->alphaMode == 2 && value->bitstreamFormat == 4 && value->subband == 5 &&
                             value->ignoreOverlap == 0x13579;
    return transcodeResult;
}

template <typename T>
void Store(std::byte* target, std::size_t offset, const T& value) {
    std::memcpy(target + offset, &value, sizeof(value));
}

template <typename Function>
std::uintptr_t FunctionAddress(Function function) {
    return reinterpret_cast<std::uintptr_t>(function);
}

void PrepareTranscode(std::vector<std::byte>& encoder, std::vector<std::byte>& converter,
                      std::vector<std::byte>& decoder, std::array<std::byte, 16>& rectangle, bool wmp) {
    std::fill(encoder.begin(), encoder.end(), std::byte{0});
    std::fill(converter.begin(), converter.end(), std::byte{0});
    std::fill(decoder.begin(), decoder.end(), std::byte{0});
    rectangle.fill(std::byte{0});
    Store(encoder.data(), 136, transcodeFormat);
    Store(encoder.data(), 152, std::uint32_t{3});
    Store(encoder.data(), 520, static_cast<std::int32_t>(wmp));
    Store(encoder.data(), 56, FunctionAddress(&WritePixels));
    Store(encoder.data(), 96, FunctionAddress(&TranscodePixels));
    Store(converter.data(), 16, FunctionAddress(&GetPixelFormat));
    Store(converter.data(), 24, FunctionAddress(&GetSourcePixelFormat));
    Store(converter.data(), 48, FunctionAddress(&CopyPixels));
    Store(converter.data(), 72, reinterpret_cast<std::uintptr_t>(decoder.data()));
    const std::int32_t dimensions[4] = {0, 0, 4, 2};
    std::memcpy(rectangle.data(), dimensions, sizeof(dimensions));
    Store(decoder.data(), 288, std::size_t{11});
    Store(decoder.data(), 296, std::size_t{17});
    Store(decoder.data(), 304, std::size_t{13});
    Store(decoder.data(), 312, std::size_t{19});
    Store(decoder.data(), 344, std::int32_t{3});
    Store(decoder.data(), 388, std::int32_t{4});
    Store(decoder.data(), 400, std::uint8_t{2});
    Store(decoder.data(), 404, std::int32_t{5});
    Store(decoder.data(), JxrTranscodeFixtureOverlapOffset(), std::int32_t{0x13579});
    expectedDecoder = decoder.data();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    lastCount = count;
    lastSize = size;
    lastCalloc = failCalloc ? nullptr : std::calloc(count, size);
    return lastCalloc;
}
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

void TestPixelFormatHash() {
    int found = 0;
    for (int hash = 0; hash < 256; ++hash) {
        const void* expected = GetPixelFormatFromHash(static_cast<unsigned char>(hash));
        const void* actual = GetPixelFormatFromHash_nid_postfix(static_cast<std::uint8_t>(hash));
        Require((expected == nullptr) == (actual == nullptr));
        if (!actual) continue;
        ++found;
        Require(std::memcmp(expected, actual, 16) == 0);
        Require(static_cast<const std::uint8_t*>(actual)[15] == hash);
    }
    Require(found > 0 && found < 256);
    const std::uint8_t gray8[16] = {0x24, 0xc3, 0xdd, 0x6f, 0x03, 0x4e, 0xfe, 0x4b,
                                    0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x08};
    const void* gray = GetPixelFormatFromHash_nid_postfix(0x08);
    Require(gray && std::memcmp(gray, gray8, sizeof(gray8)) == 0);
}

template <typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    } catch (const std::length_error&) {
        threw = true;
    }
    Require(threw);
}

void TestImageIids() {
    const std::uint32_t sentinel = 7;
    const std::uint32_t* iid = &sentinel;
    Require(GetImageEncodeIID_nid_postfix(".jxr", &iid) == 0 && *iid == 101);
    Require(GetImageDecodeIID_nid_postfix(".jxr", &iid) == 0 && *iid == 201);
    Require(GetImageEncodeIID_nid_postfix(".JXR", &iid) == 0 && *iid == 101);
    Require(GetImageEncodeIID_nid_postfix(".jxr.tmp", &iid) == 0 && *iid == 101);
    Require(GetImageEncodeIID_nid_postfix(".wdp", &iid) == 0 && *iid == 100);
    Require(GetImageDecodeIID_nid_postfix(".WDP", &iid) == 0 && *iid == 201);
    Require(GetImageEncodeIID_nid_postfix(".hdp", &iid) == 0 && *iid == 100);
    Require(GetImageDecodeIID_nid_postfix(".Hdp", &iid) == 0 && *iid == 201);
    const std::uint32_t* encode = nullptr;
    const std::uint32_t* again = nullptr;
    Require(GetImageEncodeIID_nid_postfix(".jxr", &encode) == 0);
    Require(GetImageEncodeIID_nid_postfix(".jxr", &again) == 0 && encode == again);
    for (const char* extension : {".png", ".jx", "", "jxr", ".j\xc3r", ".jpg"}) {
        iid = &sentinel;
        Require(GetImageEncodeIID_nid_postfix(extension, &iid) == -106 && iid == &sentinel);
        Require(GetImageDecodeIID_nid_postfix(extension, &iid) == -106 && iid == &sentinel);
        Require(GetImageEncodeIID_nid_postfix(extension, nullptr) == -106);
        Require(GetImageDecodeIID_nid_postfix(extension, nullptr) == -106);
    }
    RequireThrows([] { GetImageEncodeIID_nid_postfix(".jxr", nullptr); });
    RequireThrows([] { GetImageDecodeIID_nid_postfix(".wdp", nullptr); });
    RequireThrows([] {
        const std::uint32_t* out = nullptr;
        GetImageEncodeIID_nid_postfix(nullptr, &out);
    });
    RequireThrows([] {
        const std::uint32_t* out = nullptr;
        GetImageDecodeIID_nid_postfix(nullptr, &out);
    });
}

void TestAlignedAllocation() {
    for (std::size_t alignment : {std::size_t{8}, std::size_t{16}, std::size_t{24}, std::size_t{64},
                                  std::size_t{4096}}) {
        for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{100}}) {
            void* memory = reinterpret_cast<void*>(std::uintptr_t{1});
            const int before = frees;
            Require(PKAllocAligned_nid_postfix(&memory, size, alignment) == 0);
            Require(memory != nullptr && lastCalloc != nullptr);
            Require(lastCount == 1 && lastSize == size + sizeof(void*) + alignment - 1);
            const auto address = reinterpret_cast<std::uintptr_t>(memory);
            Require(address % alignment == 0);
            const auto offset = address - reinterpret_cast<std::uintptr_t>(lastCalloc);
            Require(offset >= sizeof(void*) && offset + size <= lastSize);
            void* stored = nullptr;
            std::memcpy(&stored, static_cast<std::uint8_t*>(memory) - sizeof(void*), sizeof(stored));
            Require(stored == lastCalloc);
            for (std::size_t index = 0; index < size; ++index)
                Require(static_cast<std::uint8_t*>(memory)[index] == 0);
            void* const original = lastCalloc;
            Require(PKFreeAligned_nid_postfix(&memory) == 0);
            Require(memory == nullptr && frees == before + 1 && lastFreed == original);
        }
    }
    void* memory = reinterpret_cast<void*>(std::uintptr_t{1});
    failCalloc = true;
    Require(PKAllocAligned_nid_postfix(&memory, 32, 16) == -101 && memory == nullptr);
    failCalloc = false;
    const int before = frees;
    Require(PKFreeAligned_nid_postfix(&memory) == 0 && frees == before);
    Require(PKFreeAligned_nid_postfix(nullptr) == 0 && frees == before);
    RequireThrows([] { PKAllocAligned_nid_postfix(nullptr, 32, 16); });
    memory = reinterpret_cast<void*>(std::uintptr_t{1});
    RequireThrows([&memory] { PKAllocAligned_nid_postfix(&memory, 32, 0); });
    Require(memory == nullptr);
    for (std::size_t alignment : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{7}}) {
        const int before = frees;
        void* const previous = lastCalloc;
        lastCalloc = nullptr;
        memory = reinterpret_cast<void*>(std::uintptr_t{1});
        RequireThrows([&memory, alignment] { PKAllocAligned_nid_postfix(&memory, 32, alignment); });
        Require(memory == nullptr && frees == before && lastCalloc == nullptr);
        lastCalloc = previous;
    }
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    RequireThrows([] {
        void* out = nullptr;
        PKAllocAligned_nid_postfix(&out, maximum - 4, 16);
    });
    RequireThrows([] {
        void* out = nullptr;
        PKAllocAligned_nid_postfix(&out, 16, maximum - 8);
    });
}

void TestTranscodeBuffered() {
    std::vector<std::byte> encoder(262144);
    std::vector<std::byte> converter(262144);
    std::vector<std::byte> decoder(262144);
    std::array<std::byte, 16> rectangle{};
    transcodeFormat = JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch;
    PrepareTranscode(encoder, converter, decoder, rectangle, false);
    transcodeOrder.clear();
    copyResult = 0;
    writeResult = 37;
    sourceFormatResult = -121;
    Require(PKImageEncode_Transcode_nid_postfix(nullptr, converter.data(), nullptr) == -121);
    Require((transcodeOrder == std::vector<int>{1}));

    transcodeOrder.clear();
    sourceFormatResult = 0;
    targetFormatResult = -122;
    Require(PKImageEncode_Transcode_nid_postfix(nullptr, converter.data(), nullptr) == -122);
    Require((transcodeOrder == std::vector<int>{1, 2}));

    transcodeOrder.clear();
    targetFormatResult = 0;
    const auto freesBefore = frees;
    Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == 37);
    Require((transcodeOrder == std::vector<int>{1, 2, 3, 4}));
    Require(copiedStride == 4 && writtenStride == 4 && writtenLines == 2);
    Require(frees == freesBefore + 1 && lastFreed != nullptr);

    transcodeOrder.clear();
    encoderForMutation = encoder.data();
    mutateWriteCallback = true;
    Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == 38);
    Require((transcodeOrder == std::vector<int>{1, 2, 3, 6}));
    Require(frees == freesBefore + 2);
    mutateWriteCallback = false;
    encoderForMutation = nullptr;

    transcodeOrder.clear();
    copyResult = -123;
    Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == -123);
    Require((transcodeOrder == std::vector<int>{1, 2, 3}));
    Require(frees == freesBefore + 3);

    auto incompatible = JxrGuest_GUID_PKPixelFormatBlackWhite_nid_no_patch;
    Store(encoder.data(), 136, incompatible);
    transcodeOrder.clear();
    const auto mismatchFrees = frees;
    Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), nullptr) == -106);
    Require((transcodeOrder == std::vector<int>{1, 2}));
    Require(frees == mismatchFrees);
    const std::array<std::pair<JxrGuestGuid, std::uint32_t>, 3> formats{{
        {JxrGuest_GUID_PKPixelFormatBlackWhite_nid_no_patch, 1},
        {JxrGuest_GUID_PKPixelFormat12bppYCC420_nid_no_patch, 12},
        {JxrGuest_GUID_PKPixelFormat16bppYCC422_nid_no_patch, 8}}};
    for (const auto& [format, expectedStride] : formats) {
        transcodeFormat = format;
        PrepareTranscode(encoder, converter, decoder, rectangle, false);
        copyResult = 0;
        writeResult = 0;
        Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == 0);
        Require(copiedStride == expectedStride && writtenStride == expectedStride);
    }

}

void TestTranscodeWmp() {
    std::vector<std::byte> encoder(262144);
    std::vector<std::byte> converter(262144);
    std::vector<std::byte> decoder(262144);
    std::array<std::byte, 16> rectangle{};
    transcodeFormat = JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch;
    PrepareTranscode(encoder, converter, decoder, rectangle, true);
    transcodeOrder.clear();
    transcodeResult = 41;
    sawTranscodeParameters = false;
    Require(PKImageEncode_Transcode_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == 41);
    Require((transcodeOrder == std::vector<int>{1, 2, 5}));
    Require(sawTranscodeParameters);
}

}

int main() {
    InitializeHeap();
    TestPixelFormatHash();
    TestImageIids();
    TestAlignedAllocation();
    TestTranscodeBuffered();
    TestTranscodeWmp();
    Require(IDPEmpty_nid_postfix(nullptr) == 0);
    int codec = 0;
    Require(IDPEmpty_nid_postfix(&codec) == 0 && codec == 0);
    return 0;
}
