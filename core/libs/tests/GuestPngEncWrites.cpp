#include "prx/libScePngEnc/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "PNG encode writes: %s\n", message);
    std::exit(1);
}

class WorkMemory {
public:
    explicit WorkMemory(std::size_t size) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = (size + info.dwPageSize - 1) / info.dwPageSize * info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr, "allocate work memory");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "page size");
        const auto page = static_cast<std::size_t>(pageSize);
        bytes = (size + page - 1) / page * page;
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != MAP_FAILED, "allocate work memory");
#endif
        std::memset(data, 0xA5, bytes);
    }
    ~WorkMemory() {
#ifdef _WIN32
        Require(VirtualFree(data, 0, MEM_RELEASE) != 0, "release memory");
#else
        Require(munmap(data, bytes) == 0, "release memory");
#endif
    }
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(data, bytes, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0, "protect work memory");
#else
        Require(mprotect(data, bytes, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0, "protect work memory");
#endif
    }
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename T>
static T Resolve(void* module, const char* name) {
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    auto* address = dlsym(module, name);
#endif
    Require(address != nullptr, name);
    return reinterpret_cast<T>(address);
}

int main(int argc, char** argv) {
    Require(argc == 3, "module and raw/nid arguments required");
#ifdef _WIN32
    void* module = LoadLibraryA(argv[1]);
#else
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr, "load encoder module");
    const bool nid = std::strcmp(argv[2], "nid") == 0;
    const auto query = Resolve<decltype(&scePngEncQueryMemorySize)>(module, nid ? "9030RnBDoh4" : "scePngEncQueryMemorySize");
    const auto create = Resolve<decltype(&scePngEncCreate)>(module, nid ? "7aGTPfrqT9s" : "scePngEncCreate");
    const auto erase = Resolve<decltype(&scePngEncDelete)>(module, nid ? "RUrWdwTWZy8" : "scePngEncDelete");
    const auto encode = Resolve<decltype(&scePngEncEncode)>(module, nid ? "xgDjJKpcyHo" : "scePngEncEncode");
    constexpr int invalidAddr = static_cast<int>(0x80690101);
    const PngEncCreateParam createParam{sizeof(PngEncCreateParam), 0, 16, 4};
    const int size = query(&createParam);
    Require(size > 0, "query work size");
    WorkMemory work(size), png(4096);
    void* handle = nullptr;
    Require(create(&createParam, work.data, size, &handle) == 0, "create encoder");
    alignas(4) const std::uint8_t image[]{11, 22, 33, 255};
    PngEncEncodeParam param{image, static_cast<std::uint8_t*>(png.data), sizeof(image), 4096, 1, 1, 4, 0, 19, 8, 0, 0, 6};
    PngEncOutputInfo info{123, 456};
    png.Protect(true);
    Require(encode(handle, &param, &info) == invalidAddr, "read-only PNG reports invalid address");
    Require(info.data_size == 123 && info.processed_height == 456, "refused PNG preserves metadata");
    const auto* pngBytes = static_cast<const std::uint8_t*>(png.data);
    Require(std::all_of(pngBytes, pngBytes + png.bytes, [](auto b) { return b == 0xA5; }), "refused PNG preserves buffer");
    param.png_mem_size = 1;
    Require(encode(handle, &param, &info) == static_cast<int>(0x80690110), "overflow ignores unwritten PNG memory");
    Require(info.data_size == 0 && info.processed_height == 0, "overflow metadata");
    png.Protect(false);
    param.png_mem_size = 4096;
    const int encoded = encode(handle, &param, &info);
    Require(encoded > 8 && info.data_size == static_cast<unsigned>(encoded) && info.processed_height == 1, "encoder survives refused encode");
    const std::uint8_t signature[]{137, 80, 78, 71, 13, 10, 26, 10};
    Require(std::memcmp(png.data, signature, sizeof(signature)) == 0, "PNG signature");
    Require(erase(handle) == 0, "writable delete succeeds");
    Require(erase(handle) == static_cast<int>(0x80690104), "deleted handle invalidated");
#ifdef _WIN32
    Require(FreeLibrary(static_cast<HMODULE>(module)) != 0, "unload module");
#else
    Require(dlclose(module) == 0, "unload module");
#endif
}
