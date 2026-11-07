#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

void* lastFreed = nullptr;
int frees = 0;
std::array<void*, 32> freeLog{};
bool failAllocation = false;
bool fillMalloc = false;
int mallocCalls = 0;
int failMallocAt = -1;
std::array<void*, 32> mallocLog{};
std::array<std::size_t, 32> mallocSizeLog{};
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;
using JxrCopyCallback = std::int64_t (APS5_VABI*)(void*, const void*, std::uint8_t*, std::uint32_t);
JxrGuestFormatConverter* activeCopyConverter = nullptr;
const void* observedCopyRectangle = nullptr;
std::uint8_t* observedCopyBuffer = nullptr;
std::uint32_t observedCopyStride = 0;
void* observedDecoderCopy = nullptr;
void* observedConverterConvert = nullptr;
std::int64_t decoderCopyResult = 0;
std::int64_t converterConvertResult = 0;
int decoderCopyCalls = 0;
int converterConvertACalls = 0;
int converterConvertBCalls = 0;
std::array<int, 4> copyEventOrder{};
std::size_t copyEventCount = 0;
bool replaceConverterCallback = false;

std::int64_t APS5_VABI ConverterConvertB(void* converter, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride);

void* APS5_VABI Allocate(std::size_t size) {
    ++mallocCalls;
    void* pointer = mallocCalls == failMallocAt ? nullptr : std::malloc(size);
    if (pointer && fillMalloc) std::memset(pointer, 0xa5, size);
    if (mallocCalls <= static_cast<int>(mallocLog.size())) {
        mallocLog[static_cast<std::size_t>(mallocCalls - 1)] = pointer;
        mallocSizeLog[static_cast<std::size_t>(mallocCalls - 1)] = size;
    }
    return pointer;
}
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        if (frees < static_cast<int>(freeLog.size())) freeLog[static_cast<std::size_t>(frees)] = pointer;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    lastCount = count;
    lastSize = size;
    lastAllocated = failAllocation ? nullptr : std::calloc(count, size);
    return lastAllocated;
}
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

std::int64_t APS5_VABI DecoderCopyProbe(void* decoder, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride) {
    ++decoderCopyCalls;
    copyEventOrder[copyEventCount++] = 1;
    observedDecoderCopy = decoder;
    observedCopyRectangle = rectangle;
    observedCopyBuffer = buffer;
    observedCopyStride = stride;
    if (buffer) {
        buffer[0] = 0x41;
        buffer[1] = 0x42;
    }
    if (replaceConverterCallback) {
        const JxrCopyCallback callback = &ConverterConvertB;
        std::memcpy(reinterpret_cast<std::byte*>(activeCopyConverter) + 7 * sizeof(void*), &callback, sizeof(callback));
    }
    return decoderCopyResult;
}

std::int64_t APS5_VABI ConverterConvertA(void* converter, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride) {
    ++converterConvertACalls;
    copyEventOrder[copyEventCount++] = 2;
    observedConverterConvert = converter;
    observedCopyRectangle = rectangle;
    observedCopyBuffer = buffer;
    observedCopyStride = stride;
    return converterConvertResult;
}

std::int64_t APS5_VABI ConverterConvertB(void* converter, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride) {
    ++converterConvertBCalls;
    copyEventOrder[copyEventCount++] = 3;
    observedConverterConvert = converter;
    observedCopyRectangle = rectangle;
    observedCopyBuffer = buffer;
    observedCopyStride = stride;
    return converterConvertResult;
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

void TestExifMetadata() {
    struct Fixture {
        Jxr::ImageEncodeMetadataPrefix encoder;
        std::array<std::byte, 32> trailing;
    } fixture;
    std::memset(&fixture, 0xa5, sizeof(fixture));
    fixture.encoder.headerDone = 0;
    fixture.encoder.exifMetadata = nullptr;
    fixture.encoder.exifMetadataByteCount = 0xfeed;
    auto& encoder = fixture.encoder;
    auto* const encoderPointer = reinterpret_cast<tagPKImageEncode*>(&encoder);
    const auto snapshot = [&] {
        std::array<std::byte, sizeof(fixture)> bytes;
        std::memcpy(bytes.data(), &fixture, sizeof(fixture));
        return bytes;
    };
    const auto requireOnlyExif = [&](const std::array<std::byte, sizeof(fixture)>& before, const std::uint8_t* pointer,
                                     std::uint32_t size) {
        auto expected = before;
        std::memcpy(expected.data() + offsetof(Fixture, encoder) + offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadata),
                    &pointer, sizeof(pointer));
        std::memcpy(expected.data() + offsetof(Fixture, encoder) + offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadataByteCount),
                    &size, sizeof(size));
        Require(std::memcmp(expected.data(), &fixture, sizeof(fixture)) == 0);
    };
    const std::array<std::uint8_t, 5> first {0x11, 0x22, 0x33, 0x44, 0x55};
    const std::array<std::uint8_t, 3> second {0xa1, 0xb2, 0xc3};
    auto before = snapshot();
    const auto firstCalloc = callocCalls;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == 0);
    Require(callocCalls == firstCalloc + 1 && lastCount == 1 && lastSize == first.size());
    Require(encoder.exifMetadata != first.data() && encoder.exifMetadataByteCount == first.size());
    Require(std::memcmp(encoder.exifMetadata, first.data(), static_cast<std::uint32_t>(first.size())) == 0);
    requireOnlyExif(before, encoder.exifMetadata, static_cast<std::uint32_t>(first.size()));

    auto* const previous = encoder.exifMetadata;
    const auto previousFrees = frees;
    before = snapshot();
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    Require(frees == previousFrees + 1 && lastFreed == previous);
    Require(encoder.exifMetadata != second.data() && encoder.exifMetadataByteCount == second.size());
    Require(std::memcmp(encoder.exifMetadata, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    requireOnlyExif(before, encoder.exifMetadata, static_cast<std::uint32_t>(second.size()));

    auto* const guardedPointer = encoder.exifMetadata;
    const auto guardedSize = encoder.exifMetadataByteCount;
    const auto guardedFrees = frees;
    const auto guardedCallocs = callocCalls;
    encoder.headerDone = 1;
    before = snapshot();
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == -109);
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())) == -109);
    Require(encoder.exifMetadata == guardedPointer && encoder.exifMetadataByteCount == guardedSize);
    Require(snapshot() == before);
    Require(frees == guardedFrees && callocCalls == guardedCallocs);
    encoder.headerDone = 0;

    bool threw = false;
    try { PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(nullptr, first.data(), static_cast<std::uint32_t>(first.size())); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    before = snapshot();
    const auto nullInputFrees = frees;
    const auto nullInputCallocs = callocCalls;
    try { PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && frees == nullInputFrees + 1 && lastFreed == guardedPointer);
    Require(callocCalls == nullInputCallocs + 1 && encoder.exifMetadata == lastAllocated && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, encoder.exifMetadata, 0);
    void* nullInputAllocation = encoder.exifMetadata;
    Require(PKFree_nid_postfix(&nullInputAllocation) == 0);
    encoder.exifMetadata = static_cast<std::uint8_t*>(nullInputAllocation);

    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    auto* const failurePrevious = encoder.exifMetadata;
    const auto failedCallocs = callocCalls;
    const auto failedFrees = frees;
    before = snapshot();
    failAllocation = true;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == -101);
    failAllocation = false;
    Require(callocCalls == failedCallocs + 1 && lastCount == 1 && lastSize == first.size());
    Require(lastAllocated == nullptr && encoder.exifMetadata == nullptr && encoder.exifMetadataByteCount == 0);
    Require(frees == failedFrees + 1 && lastFreed == failurePrevious);
    requireOnlyExif(before, nullptr, 0);

    failAllocation = false;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    auto* const nullFailurePrevious = encoder.exifMetadata;
    before = snapshot();
    const auto failedNullCallocs = callocCalls;
    const auto failedNullFrees = frees;
    failAllocation = true;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())) == -101);
    Require(callocCalls == failedNullCallocs + 1 && frees == failedNullFrees + 1 && lastFreed == nullFailurePrevious);
    Require(encoder.exifMetadata == nullptr && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, nullptr, 0);
    failAllocation = false;

    std::uint8_t emptyInput = 0;
    before = snapshot();
    const auto zeroResult = PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, &emptyInput, 0);
    Require(zeroResult == (lastAllocated ? 0 : -101));
    Require(encoder.exifMetadata == lastAllocated && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, encoder.exifMetadata, 0);
    if (encoder.exifMetadata) {
        void* emptyAllocation = encoder.exifMetadata;
        Require(PKFree_nid_postfix(&emptyAllocation) == 0);
        encoder.exifMetadata = static_cast<std::uint8_t*>(emptyAllocation);
    }
}

void TestInitPostProc() {
    fillMalloc = true;
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[4][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    constexpr std::size_t width = 2;
    const auto firstAllocation = mallocCalls;
    Require(initPostProc_nid_postfix(slots, width, 2) == 0);
    Require(mallocCalls == firstAllocation + 4);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const auto logIndex = static_cast<std::size_t>(firstAllocation + channel * 2 + slot);
            auto* const base = static_cast<JxrGuestPostProcInfo*>(mallocLog[logIndex]);
            Require(base != nullptr && slots[channel][slot] == base + 1);
            Require(mallocSizeLog[logIndex] == (width + 2) * sizeof(JxrGuestPostProcInfo));
            const auto checkBoundary = [](const JxrGuestPostProcInfo* value) {
                Require(value->mbTexture == 3);
                for (const auto& row : value->blockTexture)
                    for (const auto texture : row) Require(texture == 3);
            };
            checkBoundary(slots[channel][slot] - 1);
            checkBoundary(slots[channel][slot] + width);
            for (std::size_t i = 0; i < width; ++i) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(slots[channel][slot] + i);
                for (std::size_t j = 0; j < sizeof(JxrGuestPostProcInfo); ++j) Require(bytes[j] == 0xa5);
            }
            Require(std::memcmp(base, slots[channel][slot] + width, sizeof(*base)) == 0);
        }
    }
    for (std::size_t channel = 2; channel < 4; ++channel)
        for (const auto* slot : slots[channel]) Require(slot == &sentinel);
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t slot = 0; slot < 2; ++slot) free_nid_postfix(slots[channel][slot] - 1);

    const auto allocationsBeforeZeroChannels = mallocCalls;
    Require(initPostProc_nid_postfix(nullptr, std::numeric_limits<std::size_t>::max(), 0) == 0);
    Require(mallocCalls == allocationsBeforeZeroChannels);

    JxrGuestPostProcInfo* overflowSlots[1][2] = {{&sentinel, &sentinel}};
    const auto allocationsBeforeOverflow = mallocCalls;
    constexpr auto overflowWidth = std::size_t{745} * 65536 - 2;
    Require(initPostProc_nid_postfix(overflowSlots, overflowWidth, 1) == -1);
    Require(mallocCalls == allocationsBeforeOverflow);
    Require(overflowSlots[0][0] == &sentinel && overflowSlots[0][1] == &sentinel);

    JxrGuestPostProcInfo* partialSlots[3][2];
    for (auto& channel : partialSlots)
        for (auto& slot : channel) slot = &sentinel;
    const auto firstPartialAllocation = mallocCalls;
    failMallocAt = mallocCalls + 3;
    Require(initPostProc_nid_postfix(partialSlots, 1, 2) == -1);
    failMallocAt = -1;
    Require(mallocCalls == firstPartialAllocation + 3);
    Require(partialSlots[0][0] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation)]) + 1);
    Require(partialSlots[0][1] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation + 1)]) + 1);
    Require(partialSlots[1][0] == nullptr && partialSlots[1][1] == &sentinel);
    Require(partialSlots[2][0] == &sentinel && partialSlots[2][1] == &sentinel);
    free_nid_postfix(partialSlots[0][0] - 1);
    free_nid_postfix(partialSlots[0][1] - 1);
    fillMalloc = false;
}

}

void TestTermPostProc() {
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[3][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    std::array<JxrGuestPostProcInfo*, 3> base{};
    const auto firstAllocation = mallocCalls;
    for (auto& pointer : base) {
        pointer = static_cast<JxrGuestPostProcInfo*>(malloc_nid_postfix(sizeof(JxrGuestPostProcInfo)));
        Require(pointer != nullptr);
    }
    Require(mallocCalls == firstAllocation + 3);
    for (std::size_t i = 0; i < base.size(); ++i) {
        const auto index = static_cast<std::size_t>(firstAllocation) + i;
        Require(mallocLog[index] == base[i]);
        Require(mallocSizeLog[index] == 88);
    }

    slots[0][0] = base[0] + 1;
    slots[0][1] = nullptr;
    slots[1][0] = base[1] + 1;
    slots[1][1] = base[2] + 1;
    const auto firstFree = frees;
    termPostProc_nid_postfix(slots, 2);
    Require(frees == firstFree + 3);
    for (std::size_t i = 0; i < base.size(); ++i)
        Require(freeLog[static_cast<std::size_t>(firstFree) + i] == base[i]);
    Require(slots[0][0] == base[0] + 1 && slots[0][1] == nullptr);
    Require(slots[1][0] == base[1] + 1 && slots[1][1] == base[2] + 1);
    for (std::size_t slot = 0; slot < 2; ++slot) Require(slots[2][slot] == &sentinel);

    const auto freesBeforeZeroChannels = frees;
    termPostProc_nid_postfix(nullptr, 0);
    Require(frees == freesBeforeZeroChannels);

    bool threw = false;
    try { termPostProc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

int main() {
    InitializeHeap();
    void* memory = malloc_nid_postfix(64);
    Require(memory != nullptr);
    void* const allocated = memory;
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1 && lastFreed == allocated);
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1);
    Require(PKFree_nid_postfix(nullptr) == 0);
    Require(frees == 1);
    for (const auto size : {std::size_t{1}, std::size_t{257}, std::size_t{4097}, std::size_t{0}}) {
        memory = &memory;
        const auto calls = callocCalls;
        const auto result = PKAlloc_nid_postfix(&memory, size);
        Require(callocCalls == calls + 1 && lastCount == 1 && lastSize == size);
        Require(memory == lastAllocated);
        Require(result == (memory ? 0 : -101));
        if (size != 0) Require(memory != nullptr);
        auto* bytes = static_cast<unsigned char*>(memory);
        for (std::size_t i = 0; i < size; ++i) Require(bytes[i] == 0);
        if (size != 0) std::memset(memory, 0xa5, size);
        const auto previousFrees = frees;
        const auto pointer = memory;
        Require(PKFree_nid_postfix(&memory) == 0 && memory == nullptr);
        Require(frees == previousFrees + (pointer ? 1 : 0));
        if (pointer) Require(lastFreed == pointer);
    }
    failAllocation = true;
    memory = &memory;
    Require(PKAlloc_nid_postfix(&memory, 37) == -101);
    Require(memory == nullptr && lastCount == 1 && lastSize == 37);
    failAllocation = false;
    bool threw = false;
    try { PKAlloc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    alignas(8) std::array<std::byte, 80> decoder{};
    std::memset(decoder.data(), 0x6b, decoder.size());
    const JxrCopyCallback decoderCopyCallback = &DecoderCopyProbe;
    std::memcpy(decoder.data() + 7 * sizeof(void*), &decoderCopyCallback, sizeof(decoderCopyCallback));
    const auto decoderBefore = decoder;
    const JxrCopyCallback converterCallbackA = &ConverterConvertA;
    const JxrCopyCallback converterCallbackB = &ConverterConvertB;
    JxrGuestFormatConverter converter{};
    std::memset(converter.beforeDecoder, 0x5a, sizeof(converter.beforeDecoder));
    converter.decoder = decoder.data();
    std::memcpy(reinterpret_cast<std::byte*>(&converter) + 7 * sizeof(void*), &converterCallbackA, sizeof(converterCallbackA));
    const auto converterBeforeCopy = converter;
    auto converterAfterExpectedMutation = converterBeforeCopy;
    std::memcpy(reinterpret_cast<std::byte*>(&converterAfterExpectedMutation) + 7 * sizeof(void*), &converterCallbackB, sizeof(converterCallbackB));
    activeCopyConverter = &converter;
    struct GuardedBuffer {
        std::uint8_t before[8];
        std::uint8_t bytes[8];
        std::uint8_t after[8];
    } output;
    std::memset(&output, 0xa5, sizeof(output));
    std::array<std::uint8_t, 16> rectangle{};
    for (std::size_t i = 0; i < rectangle.size(); ++i) rectangle[i] = static_cast<std::uint8_t>(i + 1);
    const auto rectangleBefore = rectangle;
    constexpr std::uint32_t copyStride = 0x12345678;
    decoderCopyResult = 0x123456789abcdef0;
    converterConvertResult = -0x123456789abcdef;
    replaceConverterCallback = true;
    copyEventCount = 0;
    auto copyCalls = decoderCopyCalls;
    auto convertACalls = converterConvertACalls;
    auto convertBCalls = converterConvertBCalls;
    Require(PKFormatConverter_Copy_nid_postfix(&converter, rectangle.data(), output.bytes, copyStride) == converterConvertResult);
    Require(decoderCopyCalls == copyCalls + 1 && converterConvertACalls == convertACalls && converterConvertBCalls == convertBCalls + 1);
    Require(copyEventCount == 2 && copyEventOrder[0] == 1 && copyEventOrder[1] == 3);
    Require(observedDecoderCopy == decoder.data() && observedConverterConvert == &converter);
    Require(observedCopyRectangle == rectangle.data() && observedCopyBuffer == output.bytes && observedCopyStride == copyStride);
    Require(output.bytes[0] == 0x41 && output.bytes[1] == 0x42);
    for (const auto value : output.before) Require(value == 0xa5);
    for (const auto value : output.after) Require(value == 0xa5);
    Require(rectangle == rectangleBefore);
    Require(std::memcmp(decoder.data(), decoderBefore.data(), decoder.size()) == 0);
    Require(std::memcmp(&converter, &converterAfterExpectedMutation, sizeof(converter)) == 0);

    const JxrCopyCallback nullConverterCallback = nullptr;
    std::memcpy(reinterpret_cast<std::byte*>(&converter) + 7 * sizeof(void*), &nullConverterCallback, sizeof(nullConverterCallback));
    replaceConverterCallback = false;
    decoderCopyResult = -0x123456789abcdef;
    copyEventCount = 0;
    copyCalls = decoderCopyCalls;
    convertACalls = converterConvertACalls;
    convertBCalls = converterConvertBCalls;
    Require(PKFormatConverter_Copy_nid_postfix(&converter, nullptr, nullptr, copyStride) == decoderCopyResult);
    Require(decoderCopyCalls == copyCalls + 1 && converterConvertACalls == convertACalls && converterConvertBCalls == convertBCalls);
    Require(copyEventCount == 1 && copyEventOrder[0] == 1);
    Require(observedDecoderCopy == decoder.data() && observedCopyRectangle == nullptr && observedCopyBuffer == nullptr && observedCopyStride == copyStride);
    Require(converter.decoder == decoder.data());

    std::memcpy(reinterpret_cast<std::byte*>(&converter) + 7 * sizeof(void*), &converterCallbackA, sizeof(converterCallbackA));
    decoderCopyResult = 0x123456789abcdef0;
    converterConvertResult = 0x23456789abcdef01;
    copyEventCount = 0;
    copyCalls = decoderCopyCalls;
    convertACalls = converterConvertACalls;
    Require(PKFormatConverter_Copy_nid_postfix(&converter, nullptr, nullptr, copyStride) == converterConvertResult);
    Require(decoderCopyCalls == copyCalls + 1 && converterConvertACalls == convertACalls + 1);
    Require(copyEventCount == 2 && copyEventOrder[0] == 1 && copyEventOrder[1] == 2);
    Require(observedDecoderCopy == decoder.data() && observedConverterConvert == &converter);
    Require(observedCopyRectangle == nullptr && observedCopyBuffer == nullptr && observedCopyStride == copyStride);
    Require(std::memcmp(decoder.data(), decoderBefore.data(), decoder.size()) == 0);

    copyCalls = decoderCopyCalls;
    convertACalls = converterConvertACalls;
    threw = false;
    try { PKFormatConverter_Copy_nid_postfix(nullptr, nullptr, nullptr, copyStride); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && decoderCopyCalls == copyCalls && converterConvertACalls == convertACalls);
    JxrGuestFormatConverter invalidConverter{};
    threw = false;
    try { PKFormatConverter_Copy_nid_postfix(&invalidConverter, nullptr, nullptr, copyStride); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && decoderCopyCalls == copyCalls && converterConvertACalls == convertACalls);
    invalidConverter.decoder = decoder.data();
    std::memcpy(reinterpret_cast<std::byte*>(&invalidConverter) + 7 * sizeof(void*), &nullConverterCallback, sizeof(nullConverterCallback));
    copyEventCount = 0;
    decoderCopyResult = 0;
    threw = false;
    try { PKFormatConverter_Copy_nid_postfix(&invalidConverter, nullptr, nullptr, copyStride); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && decoderCopyCalls == copyCalls + 1 && converterConvertACalls == convertACalls);
    const JxrCopyCallback nullDecoderCallback = nullptr;
    std::memcpy(decoder.data() + 7 * sizeof(void*), &nullDecoderCallback, sizeof(nullDecoderCallback));
    copyCalls = decoderCopyCalls;
    threw = false;
    try { PKFormatConverter_Copy_nid_postfix(&invalidConverter, nullptr, nullptr, copyStride); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && decoderCopyCalls == copyCalls);
    TestTermPostProc();
    TestInitPostProc();
    TestExifMetadata();
    return 0;
}
