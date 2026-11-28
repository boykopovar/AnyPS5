#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

std::map<void*, std::size_t> liveAllocations;

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libSceJxr metadata guest pointer test failed at line %d\n", line);
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

template <typename TCall>
void ExpectThrows(TCall&& call, int line) {
    bool threw = false;
    try {
        call();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, line);
}

#define CHECK_THROWS(expr) ExpectThrows([&] { expr; }, __LINE__)

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

struct GuardedMemory {
    std::size_t pageSize = 0;
    void* unmapped = nullptr;
    void* readOnly = nullptr;
    void* boundary = nullptr;

    GuardedMemory() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        unmapped = VirtualAlloc(nullptr, pageSize, MEM_RESERVE, PAGE_NOACCESS);
        CHECK(unmapped != nullptr);
        readOnly = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(readOnly != nullptr);
        std::memset(readOnly, 0, pageSize);
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
        boundary = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(boundary != MAP_FAILED);
        std::memset(boundary, 0, pageSize * 2);
        SetPageProtection(static_cast<std::byte*>(boundary) + pageSize, pageSize, false, false);
#endif
    }

    ~GuardedMemory() {
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

using MetadataSetter = std::int64_t (APS5_VABI*)(tagPKImageEncode*, const std::uint8_t*, std::uint32_t);

struct SetterEntry {
    MetadataSetter setter;
    std::uint8_t* Jxr::ImageEncodeMetadataPrefix::* dataField;
    std::uint32_t Jxr::ImageEncodeMetadataPrefix::* sizeField;
    bool clearsSizeOnHeaderDone;
};

}

int main() {
    InitializeHeap();
    GuardedMemory mem;
    auto* badEncoder = static_cast<tagPKImageEncode*>(mem.unmapped);
    auto* roPrefix = static_cast<Jxr::ImageEncodeMetadataPrefix*>(mem.readOnly);
    auto* roEncoder = reinterpret_cast<tagPKImageEncode*>(roPrefix);
    const auto* badBytes = static_cast<const std::uint8_t*>(mem.unmapped);
    auto* partialBytes = static_cast<std::uint8_t*>(mem.boundary) + mem.pageSize - 8;
    std::memcpy(partialBytes, "<xmp>abc", 8);

    const std::array<SetterEntry, 5> setters{{
        {&PKImageEncode_SetEXIFMetadata_WMP_nid_postfix, &Jxr::ImageEncodeMetadataPrefix::exifMetadata, &Jxr::ImageEncodeMetadataPrefix::exifMetadataByteCount, false},
        {&PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix, &Jxr::ImageEncodeMetadataPrefix::gpsInfoMetadata, &Jxr::ImageEncodeMetadataPrefix::gpsInfoMetadataByteCount, false},
        {&PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix, &Jxr::ImageEncodeMetadataPrefix::iptcNaaMetadata, &Jxr::ImageEncodeMetadataPrefix::iptcNaaMetadataByteCount, false},
        {&PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix, &Jxr::ImageEncodeMetadataPrefix::photoshopMetadata, &Jxr::ImageEncodeMetadataPrefix::photoshopMetadataByteCount, false},
        {&PKImageEncode_SetXMPMetadata_WMP_nid_postfix, &Jxr::ImageEncodeMetadataPrefix::xmpMetadata, &Jxr::ImageEncodeMetadataPrefix::xmpMetadataByteCount, true},
    }};

    const std::array<std::uint8_t, 4> sample{0x41, 0x42, 0x43, 0x44};

    for (const auto& entry : setters) {
        CHECK_THROWS(entry.setter(badEncoder, sample.data(), static_cast<std::uint32_t>(sample.size())));
        CHECK(liveAllocations.empty());

        SetPageProtection(mem.readOnly, mem.pageSize, true, true);
        std::memset(roPrefix, 0, sizeof(*roPrefix));
        void* existing = nullptr;
        CHECK(PKAlloc_nid_postfix(&existing, 4) == 0 && existing != nullptr);
        roPrefix->*(entry.dataField) = static_cast<std::uint8_t*>(existing);
        roPrefix->*(entry.sizeField) = 4;
        SetPageProtection(mem.readOnly, mem.pageSize, true, false);

        CHECK_THROWS(entry.setter(roEncoder, sample.data(), static_cast<std::uint32_t>(sample.size())));
        CHECK(roPrefix->*(entry.dataField) == existing && roPrefix->*(entry.sizeField) == 4);
        CHECK(liveAllocations.size() == 1);

        SetPageProtection(mem.readOnly, mem.pageSize, true, true);
        roPrefix->headerDone = 1;
        SetPageProtection(mem.readOnly, mem.pageSize, true, false);
        if (entry.clearsSizeOnHeaderDone) {
            CHECK_THROWS(entry.setter(roEncoder, sample.data(), static_cast<std::uint32_t>(sample.size())));
            CHECK(roPrefix->*(entry.sizeField) == 4);
        } else {
            CHECK(entry.setter(roEncoder, sample.data(), static_cast<std::uint32_t>(sample.size())) == Jxr::WmpErrOutOfSequence);
            CHECK(roPrefix->*(entry.sizeField) == 4);
        }

        SetPageProtection(mem.readOnly, mem.pageSize, true, true);
        CHECK(PKFree_nid_postfix(&existing) == 0);
        roPrefix->*(entry.dataField) = nullptr;
        roPrefix->*(entry.sizeField) = 0;
        CHECK(liveAllocations.empty());

        Jxr::ImageEncodeMetadataPrefix writable{};
        auto* writableEncoder = reinterpret_cast<tagPKImageEncode*>(&writable);
        CHECK_THROWS(entry.setter(writableEncoder, badBytes, 16));
        CHECK(writable.*(entry.dataField) == nullptr && writable.*(entry.sizeField) == 0);
        CHECK(liveAllocations.empty());

        CHECK_THROWS(entry.setter(writableEncoder, partialBytes, 16));
        CHECK(writable.*(entry.dataField) == nullptr && writable.*(entry.sizeField) == 0);
        CHECK(liveAllocations.empty());

        CHECK(entry.setter(writableEncoder, sample.data(), static_cast<std::uint32_t>(sample.size())) == 0);
        CHECK(writable.*(entry.dataField) != nullptr && writable.*(entry.sizeField) == sample.size());
        CHECK(std::memcmp(writable.*(entry.dataField), sample.data(), sample.size()) == 0);
        void* stored = writable.*(entry.dataField);
        CHECK(PKFree_nid_postfix(&stored) == 0);
        CHECK(liveAllocations.empty());
    }

    Jxr::ImageEncodeMetadataPrefix xmpEncoder{};
    const char xmpInput[] = "a<dc:format>old</dc:format>z";
    const char xmpExpected[] = "a<dc:format>image/vnd.ms-photo</dc:format>z";
    CHECK(PKImageEncode_SetXMPMetadata_WMP_nid_postfix(
              reinterpret_cast<tagPKImageEncode*>(&xmpEncoder),
              reinterpret_cast<const std::uint8_t*>(xmpInput),
              static_cast<std::uint32_t>(sizeof(xmpInput) - 1)) == 0);
    CHECK(xmpEncoder.xmpMetadata != nullptr && xmpEncoder.xmpMetadataByteCount == sizeof(xmpExpected) - 1);
    CHECK(std::memcmp(xmpEncoder.xmpMetadata, xmpExpected, sizeof(xmpExpected) - 1) == 0);
    void* xmpStored = xmpEncoder.xmpMetadata;
    CHECK(PKFree_nid_postfix(&xmpStored) == 0);
    xmpEncoder.xmpMetadata = nullptr;
    xmpEncoder.xmpMetadataByteCount = 0;
    CHECK(liveAllocations.empty());

    const char xmpEmptyTail[] = "a<dc:format>old</dc:format>";
    const char xmpEmptyTailExpected[] = "a<dc:format>image/vnd.ms-photo</dc:format>";
    CHECK(PKImageEncode_SetXMPMetadata_WMP_nid_postfix(
              reinterpret_cast<tagPKImageEncode*>(&xmpEncoder),
              reinterpret_cast<const std::uint8_t*>(xmpEmptyTail),
              static_cast<std::uint32_t>(sizeof(xmpEmptyTail) - 1)) == 0);
    CHECK(xmpEncoder.xmpMetadata != nullptr && xmpEncoder.xmpMetadataByteCount == sizeof(xmpEmptyTailExpected) - 1);
    CHECK(std::memcmp(xmpEncoder.xmpMetadata, xmpEmptyTailExpected, sizeof(xmpEmptyTailExpected)) == 0);
    xmpStored = xmpEncoder.xmpMetadata;
    CHECK(PKFree_nid_postfix(&xmpStored) == 0);
    CHECK(liveAllocations.empty());

    return 0;
}
