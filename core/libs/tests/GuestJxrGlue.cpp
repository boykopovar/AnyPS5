#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

extern "C" const void* GetPixelFormatFromHash(unsigned char hash);

namespace {

void Require(bool value) { if (!value) std::abort(); }

void* lastCalloc = nullptr;
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastFreed = nullptr;
int frees = 0;
bool failCalloc = false;

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
    TestXmpMetadata();
    Require(IDPEmpty_nid_postfix(nullptr) == 0);
    int codec = 0;
    Require(IDPEmpty_nid_postfix(&codec) == 0 && codec == 0);
    return 0;
}
