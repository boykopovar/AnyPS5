#include "prx/libSceJpegEnc/include/Api.hpp"
#include "prx/libScePngEnc/include/Api.hpp"
#include "Testing.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

class Pages {
public:
    explicit Pages(std::size_t minimum) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::size_t page = info.dwPageSize;
#else
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
        size = ((minimum + page - 1) / page) * page;
#ifdef _WIN32
        data = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(data);
#else
        data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != MAP_FAILED);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        Require(VirtualFree(data, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(data, size) == 0);
#endif
    }

    void ReadOnly() { Protect(false); }
    void ReadWrite() { Protect(true); }

    void* data = nullptr;
    std::size_t size = 0;

private:
    void Protect(bool writable) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(data, size, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, size, writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0);
#endif
    }
};

}

int main() {
    constexpr std::int32_t jpegInvalidAddress = static_cast<std::int32_t>(0x80650101);
    constexpr std::int32_t jpegInvalidHandle = static_cast<std::int32_t>(0x80650104);
    constexpr std::int32_t pngInvalidAddress = static_cast<std::int32_t>(0x80690101);
    constexpr std::int32_t pngInvalidHandle = static_cast<std::int32_t>(0x80690104);

    JpegEncCreateParam jpegParam{sizeof(JpegEncCreateParam), 0};
    Pages jpegMemory(0x800);
    void* jpegHandle = nullptr;
    Require(sceJpegEncCreate(&jpegParam, jpegMemory.data, 0x800, &jpegHandle) == 0);
    jpegMemory.ReadOnly();
    Require(sceJpegEncDelete(jpegHandle) == jpegInvalidAddress);
    jpegMemory.ReadWrite();
    Require(sceJpegEncDelete(jpegHandle) == 0);
    Require(sceJpegEncDelete(jpegHandle) == jpegInvalidHandle);

    PngEncCreateParam pngParam{sizeof(PngEncCreateParam), 0, 16, 4};
    const int pngSize = scePngEncQueryMemorySize(&pngParam);
    Require(pngSize > 0);
    Pages pngMemory(static_cast<std::size_t>(pngSize));
    void* pngHandle = nullptr;
    Require(scePngEncCreate(&pngParam, pngMemory.data, pngSize, &pngHandle) == 0);
    pngMemory.ReadOnly();
    Require(scePngEncDelete(pngHandle) == pngInvalidAddress);
    pngMemory.ReadWrite();
    Require(scePngEncDelete(pngHandle) == 0);
    Require(scePngEncDelete(pngHandle) == pngInvalidHandle);
}
