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

void SetPageWritable(void* address, std::size_t size, bool writable) {
#ifdef _WIN32
    DWORD oldProtect = 0;
    CHECK(VirtualProtect(address, size, writable ? PAGE_READWRITE : PAGE_READONLY, &oldProtect) != 0);
#else
    CHECK(mprotect(address, size, writable ? (PROT_READ | PROT_WRITE) : PROT_READ) == 0);
#endif
}

struct ProtectedPages {
    std::size_t pageSize = 0;
    void* unmapped = nullptr;
    void* readOnly = nullptr;

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
        SetPageWritable(readOnly, pageSize, false);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        unmapped = mmap(nullptr, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(unmapped != MAP_FAILED);
        readOnly = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(readOnly != MAP_FAILED);
        std::memset(readOnly, 0, pageSize);
        SetPageWritable(readOnly, pageSize, false);
#endif
    }

    ~ProtectedPages() {
#ifdef _WIN32
        VirtualFree(unmapped, 0, MEM_RELEASE);
        VirtualFree(readOnly, 0, MEM_RELEASE);
#else
        munmap(unmapped, pageSize);
        munmap(readOnly, pageSize);
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

}

int main() {
    InitializeHeap();
    ProtectedPages pages;

    auto** badEncoderOut = static_cast<Jxr::ImageEncode**>(pages.unmapped);
    auto** roEncoderOut = static_cast<Jxr::ImageEncode**>(pages.readOnly);
    auto** badDecoderOut = static_cast<void**>(pages.unmapped);
    auto** roDecoderOut = static_cast<void**>(pages.readOnly);

    CHECK_THROWS(PKImageEncode_Create_WMP_nid_postfix(nullptr));
    CHECK_THROWS(PKImageEncode_Create_WMP_nid_postfix(badEncoderOut));
    CHECK_THROWS(PKImageEncode_Create_WMP_nid_postfix(roEncoderOut));
    CHECK(liveAllocations.empty());

    CHECK_THROWS(PKImageDecode_Create_WMP_nid_postfix(nullptr));
    CHECK_THROWS(PKImageDecode_Create_WMP_nid_postfix(badDecoderOut));
    CHECK_THROWS(PKImageDecode_Create_WMP_nid_postfix(roDecoderOut));
    CHECK(liveAllocations.empty());

    Jxr::ImageEncode* encoder = nullptr;
    CHECK(PKImageEncode_Create_WMP_nid_postfix(&encoder) == 0 && encoder != nullptr);
    CHECK_THROWS(encoder->release(nullptr));
    CHECK_THROWS(encoder->release(badEncoderOut));

    SetPageWritable(pages.readOnly, pages.pageSize, true);
    *roEncoderOut = encoder;
    SetPageWritable(pages.readOnly, pages.pageSize, false);
    const auto liveBeforeRoRelease = liveAllocations.size();
    CHECK_THROWS(encoder->release(roEncoderOut));
    CHECK(*roEncoderOut == encoder && liveAllocations.size() == liveBeforeRoRelease);
    SetPageWritable(pages.readOnly, pages.pageSize, true);
    *roEncoderOut = nullptr;
    SetPageWritable(pages.readOnly, pages.pageSize, false);

    void* encoderRaw = encoder;
    CHECK(PKFree_nid_postfix(&encoderRaw) == 0);
    CHECK(liveAllocations.empty());

    void* decoderRaw = nullptr;
    CHECK(PKImageDecode_Create_WMP_nid_postfix(&decoderRaw) == 0 && decoderRaw != nullptr);
    auto* decoder = static_cast<Decoder*>(decoderRaw);

    std::int32_t validInt = 0;
    auto* badInt = static_cast<std::int32_t*>(pages.unmapped);
    auto* roInt = static_cast<std::int32_t*>(pages.readOnly);
    CHECK_THROWS(decoder->getSize(decoder, badInt, &validInt));
    CHECK_THROWS(decoder->getSize(decoder, &validInt, badInt));
    CHECK_THROWS(decoder->getSize(decoder, roInt, &validInt));
    CHECK_THROWS(decoder->getSize(decoder, &validInt, roInt));
    CHECK_THROWS(decoder->getRawStream(decoder, static_cast<Jxr::Stream**>(pages.unmapped)));
    CHECK_THROWS(decoder->getRawStream(decoder, static_cast<Jxr::Stream**>(pages.readOnly)));
    CHECK_THROWS(decoder->getColorContext(decoder, nullptr, static_cast<std::uint32_t*>(pages.unmapped)));
    CHECK_THROWS(decoder->getColorContext(decoder, nullptr, static_cast<std::uint32_t*>(pages.readOnly)));
    std::uint32_t nonzeroSize = 8;
    CHECK_THROWS(decoder->getColorContext(decoder, static_cast<std::uint8_t*>(pages.readOnly), &nonzeroSize));

    CHECK_THROWS(decoder->release(reinterpret_cast<Decoder**>(badDecoderOut)));
    SetPageWritable(pages.readOnly, pages.pageSize, true);
    *roDecoderOut = decoder;
    SetPageWritable(pages.readOnly, pages.pageSize, false);
    const auto liveBeforeRoDecoderRelease = liveAllocations.size();
    CHECK_THROWS(decoder->release(reinterpret_cast<Decoder**>(roDecoderOut)));
    CHECK(*roDecoderOut == decoder && liveAllocations.size() == liveBeforeRoDecoderRelease);
    SetPageWritable(pages.readOnly, pages.pageSize, true);
    *roDecoderOut = nullptr;
    SetPageWritable(pages.readOnly, pages.pageSize, false);

    CHECK(decoder->release(nullptr) == 0);
    CHECK(decoder->release(&decoder) == 0 && decoder == nullptr);
    CHECK(liveAllocations.empty());
    return 0;
}
