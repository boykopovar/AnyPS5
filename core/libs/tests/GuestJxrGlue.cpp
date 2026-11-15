#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
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
std::array<std::byte, 64>* writeSourceConverter = nullptr;
std::array<std::byte, 160>* writeSourceEncoder = nullptr;
void* writeSourceRectangle = nullptr;
JxrGuestGuid writeSourceFrom = JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch;
JxrGuestGuid writeSourceTo = JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch;
std::int64_t writeSourceFirstResult = 0;
std::int64_t writeSourceSecondResult = 0;
std::int64_t writeSourceCopyResult = 1;
std::int64_t writeSourceWriteResult = 9;
int writeSourceFirstCalls = 0;
int writeSourceSecondCalls = 0;
int writeSourceCopyCalls = 0;
int writeSourceWriteCalls = 0;
std::uint32_t writeSourceObservedHeight = 0;
std::uint32_t writeSourceObservedStride = 0;

std::int64_t APS5_VABI WriteSourceGetTargetReplacement(void*, JxrGuestGuid*);

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

template <typename T>
void WriteField(std::byte* record, std::size_t offset, const T& value) {
    std::memcpy(record + offset, &value, sizeof(value));
}

std::uintptr_t CallbackAddress(auto callback) { return reinterpret_cast<std::uintptr_t>(callback); }

std::int64_t APS5_VABI WriteSourceGetSource(void* converter, JxrGuestGuid* format) {
    ++writeSourceFirstCalls;
    WriteField(reinterpret_cast<std::byte*>(converter), 16,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, JxrGuestGuid*)>(&WriteSourceGetTargetReplacement)));
    if (writeSourceFirstResult < 0) return writeSourceFirstResult;
    *format = writeSourceFrom;
    return writeSourceFirstResult;
}

std::int64_t APS5_VABI WriteSourceGetTargetReplacement(void*, JxrGuestGuid* format) {
    ++writeSourceSecondCalls;
    if (writeSourceSecondResult < 0) return writeSourceSecondResult;
    *format = writeSourceTo;
    return writeSourceSecondResult;
}

std::int64_t APS5_VABI WriteSourceGetTarget(void*, JxrGuestGuid* format) {
    ++writeSourceSecondCalls;
    if (writeSourceSecondResult < 0) return writeSourceSecondResult;
    *format = JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch;
    return writeSourceSecondResult;
}

std::int64_t APS5_VABI WriteSourceWriteMutated(void*, std::uint32_t height, std::uint8_t* buffer,
                                               std::uint32_t stride) {
    ++writeSourceWriteCalls;
    writeSourceObservedHeight = height;
    writeSourceObservedStride = stride;
    for (std::uint32_t index = 0; index < height * stride; ++index) Require(buffer[index] == 0x5a);
    return writeSourceWriteResult;
}

std::int64_t APS5_VABI WriteSourceWriteInitial(void*, std::uint32_t height, std::uint8_t*, std::uint32_t stride) {
    ++writeSourceWriteCalls;
    writeSourceObservedHeight = height;
    writeSourceObservedStride = stride;
    return -99;
}

std::int64_t APS5_VABI WriteSourceCopy(void*, void* rectangle, std::uint8_t* buffer, std::uint32_t stride) {
    ++writeSourceCopyCalls;
    Require(rectangle == writeSourceRectangle && reinterpret_cast<std::uintptr_t>(buffer) % 128 == 0);
    Require(stride == 16);
    std::memset(buffer, 0x5a, 32);
    const std::int32_t height = 1;
    std::memcpy(static_cast<std::byte*>(writeSourceRectangle) + 12, &height, sizeof(height));
    WriteField(writeSourceEncoder->data(), 56,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, std::uint32_t, std::uint8_t*, std::uint32_t)>(
                   &WriteSourceWriteMutated)));
    return writeSourceCopyResult;
}

void PrepareWriteSource() {
    *writeSourceConverter = {};
    *writeSourceEncoder = {};
    writeSourceFirstCalls = 0;
    writeSourceSecondCalls = 0;
    writeSourceCopyCalls = 0;
    writeSourceWriteCalls = 0;
    writeSourceFirstResult = 0;
    writeSourceSecondResult = 0;
    writeSourceCopyResult = 1;
    writeSourceWriteResult = 9;
    writeSourceFrom = JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch;
    writeSourceTo = JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch;
    WriteField(writeSourceConverter->data(), 24,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, JxrGuestGuid*)>(&WriteSourceGetSource)));
    WriteField(writeSourceConverter->data(), 16,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, JxrGuestGuid*)>(&WriteSourceGetTarget)));
    WriteField(writeSourceConverter->data(), 48,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, void*, std::uint8_t*, std::uint32_t)>(
                   &WriteSourceCopy)));
    WriteField(writeSourceEncoder->data(), 56,
               CallbackAddress(static_cast<std::int64_t(APS5_VABI*)(void*, std::uint32_t, std::uint8_t*, std::uint32_t)>(
                   &WriteSourceWriteInitial)));
    WriteField(writeSourceEncoder->data(), 136, writeSourceTo);
    WriteField(writeSourceEncoder->data(), 152, std::uint32_t{3});
}

void TestWriteSource() {
    alignas(16) static std::array<std::byte, 64> converter{};
    alignas(16) static std::array<std::byte, 160> encoder{};
    alignas(16) static std::array<std::byte, 16> rectangle{};
    writeSourceConverter = &converter;
    writeSourceEncoder = &encoder;
    writeSourceRectangle = rectangle.data();
    const std::int32_t rectValues[4] = {0, 0, 4, 2};
    std::memcpy(rectangle.data(), rectValues, sizeof(rectValues));
    PrepareWriteSource();

    writeSourceFirstResult = -7;
    Require(PKImageEncode_WriteSource_nid_postfix(nullptr, converter.data(), nullptr) == -7);
    Require(writeSourceFirstCalls == 1 && writeSourceSecondCalls == 0);
    PrepareWriteSource();
    writeSourceSecondResult = -8;
    Require(PKImageEncode_WriteSource_nid_postfix(nullptr, converter.data(), nullptr) == -8);
    Require(writeSourceFirstCalls == 1 && writeSourceSecondCalls == 1);

    PrepareWriteSource();
    writeSourceTo = JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch;
    WriteField(encoder.data(), 136, JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch);
    Require(PKImageEncode_WriteSource_nid_postfix(encoder.data(), converter.data(), nullptr) == -106);
    Require(writeSourceFirstCalls == 1 && writeSourceSecondCalls == 1 && writeSourceCopyCalls == 0);

    PrepareWriteSource();
    std::memcpy(rectangle.data(), rectValues, sizeof(rectValues));
    const int before = frees;
    Require(PKImageEncode_WriteSource_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == 9);
    Require(writeSourceFirstCalls == 1 && writeSourceSecondCalls == 1 && writeSourceCopyCalls == 1 && writeSourceWriteCalls == 1);
    Require(writeSourceObservedHeight == 1 && writeSourceObservedStride == 16 && frees == before + 1);

    PrepareWriteSource();
    std::memcpy(rectangle.data(), rectValues, sizeof(rectValues));
    const int beforeAllocationError = frees;
    failCalloc = true;
    Require(PKImageEncode_WriteSource_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == -101);
    failCalloc = false;
    Require(writeSourceCopyCalls == 0 && writeSourceWriteCalls == 0 && frees == beforeAllocationError);

    PrepareWriteSource();
    std::memcpy(rectangle.data(), rectValues, sizeof(rectValues));
    writeSourceCopyResult = -11;
    const int beforeCopyError = frees;
    Require(PKImageEncode_WriteSource_nid_postfix(encoder.data(), converter.data(), rectangle.data()) == -11);
    Require(writeSourceCopyCalls == 1 && writeSourceWriteCalls == 0 && frees == beforeCopyError + 1);
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

std::int64_t APS5_VABI PreserveConvert(JxrGuestFormatConverter*, const void*, std::uint8_t*, std::uint32_t) {
    return 77;
}

JxrGuestFormatConvert ConverterCallback(const JxrGuestFormatConverter& converter) {
    JxrGuestFormatConvert callback;
    std::memcpy(&callback, converter.beforeDecoder + 7 * sizeof(void*), sizeof(callback));
    return callback;
}

void SetConverterCallback(JxrGuestFormatConverter& converter, JxrGuestFormatConvert callback) {
    std::memcpy(converter.beforeDecoder + 7 * sizeof(void*), &callback, sizeof(callback));
}

void TestInitializeConvert() {
    JxrGuestFormatConverter converter;
    std::memset(&converter, 0x5a, sizeof(converter));
    SetConverterCallback(converter, PreserveConvert);
    converter.decoder = reinterpret_cast<void*>(std::uintptr_t{0x12345678});
    const auto previousDecoder = converter.decoder;
    const auto previousCallback = ConverterCallback(converter);
    std::array<std::byte, 7 * sizeof(void*)> originalCallbackFields{};
    std::memcpy(originalCallbackFields.data(), converter.beforeDecoder, originalCallbackFields.size());
    std::array<std::byte, sizeof(void*)> previousReleaseField{};
    std::memcpy(previousReleaseField.data(), converter.beforeDecoder + 8 * sizeof(void*), previousReleaseField.size());

    const auto rgb = JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch;
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, rgb, nullptr, rgb) == 0);
    Require(ConverterCallback(converter) == previousCallback && std::memcmp(&converter.pixelFormat, &rgb, sizeof(rgb)) == 0);
    Require(converter.decoder == previousDecoder);
    Require(std::memcmp(converter.beforeDecoder, originalCallbackFields.data(), originalCallbackFields.size()) == 0);
    Require(std::memcmp(converter.beforeDecoder + 8 * sizeof(void*), previousReleaseField.data(), previousReleaseField.size()) == 0);

    const auto floatRgb = JxrGuest_GUID_PKPixelFormat128bppRGBFloat_nid_no_patch;
    const auto floatRgba = JxrGuest_GUID_PKPixelFormat128bppRGBAFloat_nid_no_patch;
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, floatRgb, nullptr, floatRgba) == 0);
    Require(ConverterCallback(converter) == previousCallback && std::memcmp(&converter.pixelFormat, &floatRgba, sizeof(floatRgba)) == 0);

    const auto dontCare = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    const auto rgb555 = JxrGuest_GUID_PKPixelFormat16bppRGB555_nid_no_patch;
    const auto rgb565 = JxrGuest_GUID_PKPixelFormat16bppRGB565_nid_no_patch;
    const auto bgra = JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch;
    for (const auto& format : {rgb555, rgb565, bgra}) {
        Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, dontCare, nullptr, format) == 0);
        Require(ConverterCallback(converter) == previousCallback && std::memcmp(&converter.pixelFormat, &format, sizeof(format)) == 0);
    }

    const auto gray16 = JxrGuest_GUID_PKPixelFormat16bppGray_nid_no_patch;
    const auto rgba = JxrGuest_GUID_PKPixelFormat32bppRGBA_nid_no_patch;
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, gray16, nullptr, rgba) == -106);
    Require(ConverterCallback(converter) == previousCallback && std::memcmp(&converter.pixelFormat, &rgba, sizeof(rgba)) == 0);

    char bmp[] = ".bmp";
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, rgb, bmp, rgb) == 0);
    Require(ConverterCallback(converter) != nullptr && ConverterCallback(converter) != previousCallback);
    Require(std::memcmp(&converter.pixelFormat, &rgb, sizeof(rgb)) == 0);
    Require(converter.decoder == previousDecoder);
    Require(std::memcmp(converter.beforeDecoder, originalCallbackFields.data(), originalCallbackFields.size()) == 0);
    Require(std::memcmp(converter.beforeDecoder + 8 * sizeof(void*), previousReleaseField.data(), previousReleaseField.size()) == 0);

    struct Rect { std::int32_t x; std::int32_t y; std::int32_t width; std::int32_t height; } rectangle{0, 0, 1, 1};
    std::array<std::uint8_t, 8> pixels{0x11, 0x22, 0x33, 0xcc, 0x44, 0x55, 0x66, 0xdd};
    Require(ConverterCallback(converter)(&converter, &rectangle, pixels.data(), 4) == 0);
    Require(pixels == std::array<std::uint8_t, 8>{0x33, 0x22, 0x11, 0xcc, 0x44, 0x55, 0x66, 0xdd});

    std::memset(&converter, 0x5a, sizeof(converter));
    SetConverterCallback(converter, PreserveConvert);
    converter.decoder = nullptr;
    char tif[] = ".tif";
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, bgra, tif, bgra) == 0);
    Require(ConverterCallback(converter) != nullptr && ConverterCallback(converter) != PreserveConvert);
    Require(std::memcmp(&converter.pixelFormat, &bgra, sizeof(bgra)) == 0);
    Require(converter.decoder == nullptr);

    char tiff[] = ".tiff";
    Require(PKFormatConverter_InitializeConvert_nid_postfix(&converter, bgra, tiff, bgra) == 0);
    Require(ConverterCallback(converter) != nullptr && ConverterCallback(converter) != PreserveConvert);
    Require(std::memcmp(&converter.pixelFormat, &bgra, sizeof(bgra)) == 0);
    Require(converter.decoder == nullptr);

    RequireThrows([] {
        PKFormatConverter_InitializeConvert_nid_postfix(nullptr, JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch,
                                                        nullptr, JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch);
    });
}

std::int64_t getPixelFormatResult = 0;
int getPixelFormatCalls = 0;
void* getPixelFormatDecoder = nullptr;
JxrGuestGuid getPixelFormatValue{};
bool mutateDecoderInGetPixelFormat = false;

std::int64_t APS5_VABI GetGuestPixelFormat(void* decoder, JxrGuestGuid* format) {
    ++getPixelFormatCalls;
    getPixelFormatDecoder = decoder;
    if (mutateDecoderInGetPixelFormat) {
        const std::uint64_t marker = 0xfedcba9876543210;
        std::memcpy(decoder, &marker, sizeof(marker));
    }
    if (getPixelFormatResult >= 0) *format = getPixelFormatValue;
    return getPixelFormatResult;
}

void SetDecoderGetPixelFormat(std::array<std::byte, 64>& decoder) {
    const auto callback = &GetGuestPixelFormat;
    std::memcpy(decoder.data() + sizeof(void*), &callback, sizeof(callback));
}

void TestFormatConverterInitialize() {
    alignas(8) std::array<std::byte, 64> decoder{};
    SetDecoderGetPixelFormat(decoder);
    alignas(8) JxrGuestFormatConverter converter{};
    auto* convertBytes = reinterpret_cast<std::byte*>(&converter) + 7 * sizeof(void*);
    std::uint64_t previousCallback = 0x9182736455aabbcc;
    std::memcpy(convertBytes, &previousCallback, sizeof(previousCallback));

    getPixelFormatValue = JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch;
    mutateDecoderInGetPixelFormat = true;
    getPixelFormatResult = -0x100000003LL;
    getPixelFormatCalls = 0;
    const auto untouched = converter;
    Require(PKFormatConverter_Initialize_nid_postfix(&converter, decoder.data(), nullptr,
            JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch) == getPixelFormatResult);
    Require(getPixelFormatCalls == 1 && getPixelFormatDecoder == decoder.data());
    Require(std::memcmp(&converter, &untouched, sizeof(converter)) == 0);
    std::uint64_t decoderMutation = 0;
    std::memcpy(&decoderMutation, decoder.data(), sizeof(decoderMutation));
    Require(decoderMutation == 0xfedcba9876543210);

    getPixelFormatCalls = 0;
    Require(PKFormatConverter_Initialize_nid_postfix(nullptr, decoder.data(), nullptr,
            JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch) == getPixelFormatResult);
    Require(getPixelFormatCalls == 1);

    getPixelFormatResult = 5;
    getPixelFormatCalls = 0;
    Require(PKFormatConverter_Initialize_nid_postfix(&converter, decoder.data(), nullptr,
            JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch) == 0);
    Require(getPixelFormatCalls == 1 && getPixelFormatDecoder == decoder.data());
    Require(converter.decoder == decoder.data());
    Require(std::memcmp(&converter.pixelFormat, &JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch, 16) == 0);
    JxrGuestFormatConvert convert = nullptr;
    std::memcpy(&convert, convertBytes, sizeof(convert));
    Require(convert != nullptr);
    struct Rect { std::int32_t X, Y, Width, Height; } rect{0, 0, 1, 1};
    std::array<std::uint8_t, 8> pixels{1, 2, 3, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
    Require(convert(&converter, &rect, pixels.data(), 3) == 0);
    Require(pixels[0] == 3 && pixels[1] == 2 && pixels[2] == 1);
    for (std::size_t index = 3; index < pixels.size(); ++index) Require(pixels[index] == 0xa5);

    getPixelFormatValue = JxrGuest_GUID_PKPixelFormat32bppRGBA_nid_no_patch;
    getPixelFormatCalls = 0;
    Require(PKFormatConverter_Initialize_nid_postfix(&converter, decoder.data(), nullptr,
            JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch) == 0);
    Require(getPixelFormatCalls == 1 && converter.decoder == decoder.data());
    std::memcpy(&convert, convertBytes, sizeof(convert));
    std::array<std::uint8_t, 8> rgba{4, 5, 6, 7, 0xcc, 0xcc, 0xcc, 0xcc};
    Require(convert(&converter, &rect, rgba.data(), 4) == 0);
    Require(rgba[0] == 6 && rgba[1] == 5 && rgba[2] == 4 && rgba[3] == 7);
    for (std::size_t index = 4; index < rgba.size(); ++index) Require(rgba[index] == 0xcc);

    getPixelFormatValue = JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch;
    converter.decoder = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    converter.pixelFormat = {};
    const auto decoderBeforeFailure = converter.decoder;
    std::uint64_t callbackBeforeFailure = 0;
    std::memcpy(&callbackBeforeFailure, convertBytes, sizeof(callbackBeforeFailure));
    Require(PKFormatConverter_Initialize_nid_postfix(&converter, decoder.data(), nullptr,
            JxrGuest_GUID_PKPixelFormat128bppRGBFloat_nid_no_patch) < 0);
    Require(converter.decoder == decoderBeforeFailure);
    Require(std::memcmp(&converter.pixelFormat, &JxrGuest_GUID_PKPixelFormat128bppRGBFloat_nid_no_patch, 16) == 0);
    Require(std::memcmp(convertBytes, &callbackBeforeFailure, sizeof(callbackBeforeFailure)) == 0);

    RequireThrows([] {
        PKFormatConverter_Initialize_nid_postfix(nullptr, nullptr, nullptr,
                                                  JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch);
    });
    alignas(8) std::array<std::byte, 64> nullCallbackDecoder{};
    RequireThrows([&] {
        PKFormatConverter_Initialize_nid_postfix(&converter, nullCallbackDecoder.data(), nullptr,
                                                  JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch);
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

void TestXmpMetadata() {
    constexpr char format[] = "<dc:format>image/vnd.ms-photo</dc:format>";
    constexpr std::size_t extra = 1 + sizeof("<dc:format>") - 1 + sizeof("</dc:format>") - 1 + sizeof(format) - 1;
    struct EncoderStorage { Jxr::ImageEncodeMetadataPrefix prefix; std::array<std::byte, 32> guard; };
    EncoderStorage storage;
    std::memset(&storage, 0xa5, sizeof(storage));
    auto& encoder = storage.prefix;
    encoder.headerDone = 0;
    encoder.xmpMetadata = nullptr;
    encoder.xmpMetadataByteCount = 0;
    encoder.width = 0x12345678;
    encoder.height = 0x87654321;
    encoder.currentLine = 17;
    encoder.pixelFormat[3] = std::byte{0xa5};
    const auto setMetadata = [&storage](Jxr::ImageEncodeMetadataPrefix* value, const std::uint8_t* bytes, std::uint32_t size) {
        EncoderStorage expected;
        std::memcpy(&expected, &storage, sizeof(storage));
        const auto result = PKImageEncode_SetXMPMetadata_WMP_nid_postfix(reinterpret_cast<tagPKImageEncode*>(value), bytes, size);
        expected.prefix.xmpMetadata = value->xmpMetadata;
        expected.prefix.xmpMetadataByteCount = value->xmpMetadataByteCount;
        Require(std::memcmp(&expected, &storage, sizeof(storage)) == 0);
        return result;
    };
    auto* previous = static_cast<std::uint8_t*>(std::malloc(4));
    std::memcpy(previous, "old!", 4);
    encoder.xmpMetadata = previous;
    encoder.xmpMetadataByteCount = 4;
    const char replaced[] = "prefix<dc:format>old</dc:format>tail!";
    const char replacedExpected[] = "prefix<dc:format>image/vnd.ms-photo</dc:format>tail!";
    const auto freesBeforeReplace = frees;
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(replaced), sizeof(replaced) - 1) == 0);
    Require(frees == freesBeforeReplace + 1 && lastFreed == previous);
    Require(encoder.xmpMetadata != nullptr);
    Require(encoder.xmpMetadataByteCount == sizeof(replacedExpected) - 1);
    Require(std::memcmp(encoder.xmpMetadata, replacedExpected, sizeof(replacedExpected) - 1) == 0);
    Require(lastSize == sizeof(replaced) - 1 + extra);
    Require(encoder.width == 0x12345678 && encoder.height == 0x87654321 && encoder.currentLine == 17);
    Require(encoder.pixelFormat[3] == std::byte{0xa5});

    const char multiple[] = "<dc:format>first</dc:format>|<dc:format>second</dc:format>";
    const char multipleExpected[] = "<dc:format>image/vnd.ms-photo</dc:format>|<dc:format>second</dc:format>";
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(multiple), sizeof(multiple) - 1) == 0);
    Require(encoder.xmpMetadataByteCount == sizeof(multipleExpected) - 1);
    Require(std::memcmp(encoder.xmpMetadata, multipleExpected, sizeof(multipleExpected) - 1) == 0);

    const char longer[] = "before<dc:format>this original format text is considerably longer than the replacement value</dc:format>after";
    const char longerExpected[] = "before<dc:format>image/vnd.ms-photo</dc:format>after";
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(longer), sizeof(longer) - 1) == 0);
    Require(encoder.xmpMetadataByteCount == sizeof(longerExpected) - 1);
    Require(std::memcmp(encoder.xmpMetadata, longerExpected, sizeof(longerExpected) - 1) == 0);
    Require(setMetadata(&encoder, nullptr, 0) == 0);
    Require(encoder.xmpMetadata != nullptr && encoder.xmpMetadataByteCount == 0 && lastSize == extra);

    const char unchanged[] = "<xmp>no format</xmp>";
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(unchanged), sizeof(unchanged) - 1) == 0);
    Require(encoder.xmpMetadataByteCount == sizeof(unchanged) - 1);
    Require(std::memcmp(encoder.xmpMetadata, unchanged, sizeof(unchanged) - 1) == 0);

    const std::uint8_t embeddedNull[] = {'a', 'b', 'c', 0, 'd', 'e', 'f'};
    Require(setMetadata(&encoder, embeddedNull, sizeof(embeddedNull)) == 0);
    Require(encoder.xmpMetadataByteCount == 3);
    Require(std::memcmp(encoder.xmpMetadata, embeddedNull, sizeof(embeddedNull)) == 0);
    Require(encoder.xmpMetadata[sizeof(embeddedNull)] == 0);

    for (const char* malformed : {"<dc:format>missing", "<dc:format>nested<dc:format></dc:format>"}) {
        Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(malformed), std::strlen(malformed)) == -1);
        Require(encoder.xmpMetadata == nullptr && encoder.xmpMetadataByteCount == 0);
    }

    auto* guarded = static_cast<std::uint8_t*>(std::malloc(3));
    encoder.xmpMetadata = guarded;
    encoder.xmpMetadataByteCount = 3;
    encoder.headerDone = 1;
    const int freesBeforeGuard = frees;
    const auto allocationBeforeGuard = lastCalloc;
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(unchanged), sizeof(unchanged) - 1) == -109);
    Require(encoder.xmpMetadata == guarded && encoder.xmpMetadataByteCount == 0);
    Require(frees == freesBeforeGuard && lastCalloc == allocationBeforeGuard);
    encoder.headerDone = 0;

    failCalloc = true;
    const auto freesBeforeFailure = frees;
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(unchanged), sizeof(unchanged) - 1) == -101);
    Require(encoder.xmpMetadata == nullptr && encoder.xmpMetadataByteCount == 0);
    Require(frees == freesBeforeFailure + 1 && lastFreed == guarded);
    Require(setMetadata(&encoder, nullptr, 1) == -101);
    auto* replacedOnOverflow = static_cast<std::uint8_t*>(std::malloc(1));
    encoder.xmpMetadata = replacedOnOverflow;
    encoder.xmpMetadataByteCount = 1;
    Require(setMetadata(&encoder, reinterpret_cast<const std::uint8_t*>(unchanged), UINT32_MAX) == -101);
    Require(lastSize == extra - 1);
    Require(encoder.xmpMetadata == nullptr && encoder.xmpMetadataByteCount == 0 && lastFreed == replacedOnOverflow);
    failCalloc = false;
    RequireThrows([&encoder, &setMetadata] { setMetadata(&encoder, nullptr, 1); });
    Require(encoder.xmpMetadata == nullptr && encoder.xmpMetadataByteCount == 0);
    RequireThrows([] { PKImageEncode_SetXMPMetadata_WMP_nid_postfix(nullptr, nullptr, 0); });
}

}

int main() {
    InitializeHeap();
    TestPixelFormatHash();
    TestImageIids();
    TestAlignedAllocation();
    TestInitializeConvert();
    TestFormatConverterInitialize();
    TestTranscodeBuffered();
    TestTranscodeWmp();
    TestWriteSource();
    TestXmpMetadata();
    Require(IDPEmpty_nid_postfix(nullptr) == 0);
    int codec = 0;
    Require(IDPEmpty_nid_postfix(&codec) == 0 && codec == 0);
    return 0;
}
