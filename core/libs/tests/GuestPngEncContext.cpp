#include "prx/libScePngEnc/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Decoder/Png.hpp"
#include "Testing.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = EACCES;
}

struct PngEncContextMirror {
    std::uintptr_t self;
    std::uint32_t size;
    std::uint32_t reserved;
    std::uint32_t maxImageWidth;
    std::uint32_t maxFilterNumber;
    std::uint32_t scratchSize;
};

class GuardedMemory {
public:
    explicit GuardedMemory(std::size_t minimum) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = static_cast<std::size_t>(info.dwPageSize);
        alignedSize = ((std::max<std::size_t>(minimum, 1) + pageSize - 1) / pageSize) * pageSize;
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, alignedSize + pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + alignedSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long configured = sysconf(_SC_PAGESIZE);
        Require(configured > 0);
        pageSize = static_cast<std::size_t>(configured);
        alignedSize = ((std::max<std::size_t>(minimum, 1) + pageSize - 1) / pageSize) * pageSize;
        void* mapping = mmap(nullptr, alignedSize + pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapping);
        Require(mprotect(base + alignedSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedMemory() {
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, alignedSize + pageSize) == 0);
#endif
    }

    GuardedMemory(const GuardedMemory&) = delete;
    GuardedMemory& operator=(const GuardedMemory&) = delete;

    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, alignedSize, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(base, alignedSize, readOnly ? PROT_READ : (PROT_READ | PROT_WRITE)) == 0);
#endif
    }

    std::uint8_t* Data() const { return base; }
    std::uint8_t* GuardStart() const { return base + alignedSize; }
    std::size_t Size() const { return alignedSize; }

private:
    std::uint8_t* base = nullptr;
    std::size_t pageSize = 0;
    std::size_t alignedSize = 0;
};

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    constexpr int invalidParam = static_cast<int>(0x80690103);
    constexpr int invalidHandle = static_cast<int>(0x80690104);

    for (const std::uint32_t width : {1u, 3u, 7u, 9u}) {
        for (std::uint32_t filterNumber = 0; filterNumber <= 4; ++filterNumber) {
            PngEncCreateParam create{sizeof(PngEncCreateParam), 0, width, filterNumber};
            errno = EAGAIN;
            const int memorySize = scePngEncQueryMemorySize(&create);
            Require(errno == EAGAIN);
            std::vector<std::uint8_t> memory(memorySize + 16);
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(memory.data()) + 5;
            void* handle = nullptr;
            errno = EAGAIN;
            Require(scePngEncCreate(&create, reinterpret_cast<void*>(address), static_cast<std::uint32_t>(memory.size()), &handle) == 0);
            Require(errno == EAGAIN);
            const std::uintptr_t aligned = (address + 15) & ~std::uintptr_t{15};
            const auto* context = reinterpret_cast<const PngEncContextMirror*>(handle);
            Require(context->self == aligned);
            Require(context->size == 16u);
            Require(context->reserved == 0u);
            Require(context->maxImageWidth == ((width + 7) & ~std::uint32_t{7}));
            Require(context->maxFilterNumber == filterNumber);
            Require(context->scratchSize == static_cast<std::uint32_t>(memorySize) - 112u);
            errno = EAGAIN;
            Require(scePngEncDelete(handle) == 0);
            Require(errno == EAGAIN);
            errno = EAGAIN;
            Require(scePngEncDelete(handle) == invalidHandle);
            Require(errno == EAGAIN);
        }
    }

    PngEncCreateParam narrow{sizeof(PngEncCreateParam), 0, 1u, 4};
    const int narrowSize = scePngEncQueryMemorySize(&narrow);
    std::vector<std::uint8_t> narrowMemory(narrowSize + 16);
    void* narrowHandle = nullptr;
    Require(scePngEncCreate(&narrow, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(narrowMemory.data()) + 5), static_cast<std::uint32_t>(narrowMemory.size()), &narrowHandle) == 0);

    constexpr std::uint32_t rows = 4;
    std::vector<std::uint8_t> image(rows * 36, 0);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            std::uint8_t* pixel = &image[y * 36 + x * 4];
            pixel[0] = static_cast<std::uint8_t>(x * 40);
            pixel[1] = static_cast<std::uint8_t>(y * 80);
            pixel[2] = static_cast<std::uint8_t>(200 - x * 10);
            pixel[3] = static_cast<std::uint8_t>(100 + x + y);
        }
    }
    std::vector<std::uint8_t> png(4096);
    PngEncEncodeParam encode{image.data(), png.data(), static_cast<std::uint32_t>(image.size()), static_cast<std::uint32_t>(png.size()),
                             8, rows, 36u, 0, 19, 8, 0, 15, 6};
    PngEncOutputInfo info{};
    errno = EAGAIN;
    const int encoded = scePngEncEncode(narrowHandle, &encode, &info);
    Require(errno == EAGAIN);
    Require(encoded > 1 && static_cast<std::size_t>(encoded) < png.size()
            && info.data_size == static_cast<std::uint32_t>(encoded) && info.processed_height == rows);
    const auto decoded = Decoder::Png::Decode(std::span<const std::uint8_t>(png.data(), encoded));
    Require(decoded.has_value() && decoded->width == 8u && decoded->height == rows);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const std::uint8_t* source = &image[y * 36 + x * 4];
            const std::uint8_t* result = &decoded->pixels[(y * 8u + x) * 4u];
            Require(result[0] == source[0] && result[1] == source[1]);
            Require(result[2] == source[2] && result[3] == source[3]);
        }
    }
    encode.image_width = 9;
    errno = EAGAIN;
    Require(scePngEncEncode(narrowHandle, &encode, &info) == invalidParam);
    Require(errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncDelete(narrowHandle) == 0);
    Require(errno == EAGAIN);

    PngEncCreateParam tall{sizeof(PngEncCreateParam), 0, 8u, 4};
    const int tallSize = scePngEncQueryMemorySize(&tall);
    std::vector<std::uint8_t> tallMemory(tallSize + 16);
    void* tallHandle = nullptr;
    Require(scePngEncCreate(&tall, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(tallMemory.data()) + 5), static_cast<std::uint32_t>(tallMemory.size()), &tallHandle) == 0);

    constexpr std::uint32_t maxHeight = 1000000;
    const std::uint8_t channel[4] = {0x11, 0x22, 0x33, 0xFF};
    std::vector<std::uint8_t> tallImage(maxHeight * 32);
    for (std::size_t i = 0; i < tallImage.size(); ++i) tallImage[i] = channel[i % 4];
    std::vector<std::uint8_t> tallPng(4u * 1024u * 1024u);
    PngEncEncodeParam tallEncode{tallImage.data(), tallPng.data(), static_cast<std::uint32_t>(tallImage.size()), static_cast<std::uint32_t>(tallPng.size()),
                                 8, maxHeight, 32u, 0, 19, 8, 0, 0, 6};
    PngEncOutputInfo tallInfo{};
    const int tallEncoded = scePngEncEncode(tallHandle, &tallEncode, &tallInfo);
    Require(tallEncoded > 1 && static_cast<std::size_t>(tallEncoded) < tallPng.size()
            && tallInfo.data_size == static_cast<std::uint32_t>(tallEncoded) && tallInfo.processed_height == maxHeight);
    const auto tallDecoded = Decoder::Png::Decode(std::span<const std::uint8_t>(tallPng.data(), tallEncoded));
    Require(tallDecoded.has_value() && tallDecoded->width == 8u && tallDecoded->height == maxHeight);
    for (std::size_t i = 0; i < tallImage.size(); ++i) {
        Require(tallDecoded->pixels[i] == channel[i % 4]);
    }
    errno = EAGAIN;
    Require(scePngEncDelete(tallHandle) == 0);
    Require(errno == EAGAIN);

    constexpr int invalidAddr = static_cast<int>(0x80690101);
    constexpr int invalidSize = static_cast<int>(0x80690102);

    GuardedMemory smallMem(1);

    errno = EAGAIN;
    Require(scePngEncQueryMemorySize(nullptr) == invalidAddr && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncQueryMemorySize(reinterpret_cast<const PngEncCreateParam*>(0x10)) == invalidAddr && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncQueryMemorySize(reinterpret_cast<const PngEncCreateParam*>(smallMem.GuardStart() - 4)) == invalidAddr && errno == EAGAIN);
    PngEncCreateParam badSizeCreate{sizeof(PngEncCreateParam) - 1, 0, 8, 4};
    errno = EAGAIN;
    Require(scePngEncQueryMemorySize(&badSizeCreate) == invalidSize && errno == EAGAIN);
    PngEncCreateParam badAttrCreate{sizeof(PngEncCreateParam), 1, 8, 4};
    errno = EAGAIN;
    Require(scePngEncQueryMemorySize(&badAttrCreate) == invalidParam && errno == EAGAIN);

    PngEncCreateParam validCreate{sizeof(PngEncCreateParam), 0, 8, 4};
    errno = EAGAIN;
    const int validSize = scePngEncQueryMemorySize(&validCreate);
    Require(validSize > 0 && errno == EAGAIN);
    GuardedMemory workMem(static_cast<std::size_t>(validSize));
    void* guardHandle = nullptr;

    errno = EAGAIN;
    Require(scePngEncCreate(reinterpret_cast<const PngEncCreateParam*>(smallMem.GuardStart() - 4), workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), &guardHandle) == invalidAddr && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncCreate(&validCreate, workMem.GuardStart() - 16, static_cast<std::uint32_t>(workMem.Size()), &guardHandle) == invalidAddr && errno == EAGAIN);
    workMem.Protect(true);
    errno = EAGAIN;
    Require(scePngEncCreate(&validCreate, workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), &guardHandle) == invalidAddr && errno == EAGAIN);
    workMem.Protect(false);
    smallMem.Protect(true);
    errno = EAGAIN;
    Require(scePngEncCreate(&validCreate, workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), reinterpret_cast<void**>(smallMem.Data())) == invalidAddr && errno == EAGAIN);
    smallMem.Protect(false);
    errno = EAGAIN;
    Require(scePngEncCreate(&validCreate, workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), reinterpret_cast<void**>(smallMem.GuardStart() - 4)) == invalidAddr && errno == EAGAIN);
    Require(guardHandle == nullptr);

    errno = EAGAIN;
    Require(scePngEncCreate(&validCreate, workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), &guardHandle) == 0 && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncDelete(nullptr) == invalidHandle && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncDelete(reinterpret_cast<void*>(0x20)) == invalidHandle && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncDelete(smallMem.GuardStart() - 16) == invalidHandle && errno == EAGAIN);
    workMem.Protect(true);
    errno = EAGAIN;
    Require(scePngEncDelete(guardHandle) == invalidAddr && errno == EAGAIN);
    workMem.Protect(false);
    errno = EAGAIN;
    Require(scePngEncDelete(guardHandle) == 0 && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncDelete(guardHandle) == invalidHandle && errno == EAGAIN);

    guardHandle = nullptr;
    Require(scePngEncCreate(&validCreate, workMem.Data(), static_cast<std::uint32_t>(workMem.Size()), &guardHandle) == 0);
    alignas(4) const std::uint8_t smallPixels[]{1, 2, 3, 255, 4, 5, 6, 255};
    std::vector<std::uint8_t> guardPng(4096, 0xA5);
    PngEncOutputInfo outInfo{123, 456};
    PngEncEncodeParam guardEncodeParam{smallPixels, guardPng.data(), sizeof(smallPixels), static_cast<std::uint32_t>(guardPng.size()), 2, 1, 8, 0, 19, 8, 0, 0, 6};

    errno = EAGAIN;
    Require(scePngEncEncode(smallMem.GuardStart() - 16, &guardEncodeParam, &outInfo) == invalidHandle && errno == EAGAIN);
    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, reinterpret_cast<const PngEncEncodeParam*>(smallMem.GuardStart() - 16), &outInfo) == invalidAddr && errno == EAGAIN);

    PngEncEncodeParam badPixelsParam = guardEncodeParam;
    badPixelsParam.image_mem_addr = smallMem.GuardStart() - 4;
    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, &badPixelsParam, &outInfo) == invalidAddr && errno == EAGAIN);

    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, &guardEncodeParam, reinterpret_cast<PngEncOutputInfo*>(0x10)) == invalidAddr && errno == EAGAIN);
    Require(std::all_of(guardPng.begin(), guardPng.end(), [](std::uint8_t b) { return b == 0xA5; }));

    smallMem.Protect(true);
    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, &guardEncodeParam, reinterpret_cast<PngEncOutputInfo*>(smallMem.Data())) == invalidAddr && errno == EAGAIN);
    Require(std::all_of(guardPng.begin(), guardPng.end(), [](std::uint8_t b) { return b == 0xA5; }));
    smallMem.Protect(false);

    std::memset(smallMem.GuardStart() - 4, 0xA5, 4);
    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, &guardEncodeParam, reinterpret_cast<PngEncOutputInfo*>(smallMem.GuardStart() - 4)) == invalidAddr && errno == EAGAIN);
    Require(std::all_of(guardPng.begin(), guardPng.end(), [](std::uint8_t b) { return b == 0xA5; }));
    Require(std::all_of(smallMem.GuardStart() - 4, smallMem.GuardStart(), [](std::uint8_t b) { return b == 0xA5; }));

    std::memset(smallMem.GuardStart() - 8, 0xA5, 8);
    PngEncEncodeParam guardOutParam = guardEncodeParam;
    guardOutParam.png_mem_addr = smallMem.GuardStart() - 8;
    errno = EAGAIN;
    Require(scePngEncEncode(guardHandle, &guardOutParam, &outInfo) == invalidAddr && errno == EAGAIN);
    Require(outInfo.data_size == 123 && outInfo.processed_height == 456);
    Require(std::all_of(smallMem.GuardStart() - 8, smallMem.GuardStart(), [](std::uint8_t b) { return b == 0xA5; }));

    errno = EAGAIN;
    Require(scePngEncDelete(guardHandle) == 0 && errno == EAGAIN);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
}
