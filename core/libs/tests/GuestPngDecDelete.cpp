#include "prx/libScePngDec/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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
    std::fprintf(stderr, "PNG delete: %s\n", message);
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
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        bytes = (size + page - 1) / page * page;
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != MAP_FAILED, "allocate work memory");
#endif
        std::memset(data, 0xA5, bytes);
    }
    ~WorkMemory() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
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

struct Source {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t offset = 0;
    std::size_t calls = 0;
};

static std::int32_t APS5_VABI ReadChunk(PngDecInputChunk* chunk, void* argument) {
    auto& source = *static_cast<Source*>(argument);
    ++source.calls;
    const auto count = std::min<std::size_t>(7, source.size - source.offset);
    chunk->data = source.data + source.offset;
    chunk->size = static_cast<std::uint32_t>(count);
    chunk->reserved = 0;
    source.offset += count;
    return 0;
}

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
    Require(argc == 3 || argc == 4, "module and raw/nid arguments required");
#ifdef _WIN32
    void* module = LoadLibraryA(argv[1]);
    if (!module) std::fprintf(stderr, "LoadLibrary error %lu\n", GetLastError());
#else
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "dlopen: %s\n", dlerror());
#endif
    Require(module != nullptr, "load decoder module");
    const bool nid = std::strcmp(argv[2], "nid") == 0;
    const auto query = Resolve<decltype(&scePngDecQueryMemorySize)>(module, nid ? "-6srIGbLTIU" : "scePngDecQueryMemorySize");
    const auto create = Resolve<decltype(&scePngDecCreate)>(module, nid ? "m0uW+8pFyaw" : "scePngDecCreate");
    const auto erase = Resolve<decltype(&scePngDecDelete)>(module, nid ? "QbD+eENEwo8" : "scePngDecDelete");
    const auto control = Resolve<decltype(&scePngDecDecodeWithInputControl)>(module, nid ? "cJ--1xAbj-I" : "scePngDecDecodeWithInputControl");
    const PngDecCreateParam createParam{sizeof(PngDecCreateParam), 0, 1};
    const auto size = query(&createParam);
    Require(size > 0, "query work size");
    WorkMemory work(static_cast<std::size_t>(size));
    void* handle = nullptr;
    Require(create(&createParam, work.data, static_cast<std::uint32_t>(size), &handle) == 0, "create decoder");
    static const std::uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x54, 0x32, 0x06, 0x00, 0x00, 0xAE, 0x00, 0x67, 0x15, 0x1E, 0xA1, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
    Source source{png, sizeof(png)};
    PngDecInputControlParam param{};
    param.input = &ReadChunk;
    param.input_argument = &source;
    param.operation = 0x10;
    PngDecImageInfo info{};
    Require(control(handle, &param, &info) == 0, "parse callback stream");
    Require(info.image_width == 1 && info.image_height == 1, "literal PNG dimensions");
    const auto callsBeforeDelete = source.calls;
    const auto* bytes = static_cast<const std::uint8_t*>(work.data);
    const std::vector<std::uint8_t> before(bytes, bytes + work.bytes);
    work.Protect(true);
    const auto result = erase(handle);
    std::printf("delete result=0x%08x\n", static_cast<unsigned>(result));
    if (argc != 4) Require(result == static_cast<std::int32_t>(0x80690001), "read-only delete must report invalid address");
    Require(std::memcmp(work.data, before.data(), work.bytes) == 0, "refused delete preserves work memory");
    Require(source.calls == callsBeforeDelete, "delete does not consume callback input");
    work.Protect(false);
    alignas(4) std::array<std::uint8_t, 4> pixels{0xA5, 0xA5, 0xA5, 0xA5};
    param.operation = 0x20;
    param.image_mem_addr = pixels.data();
    param.image_mem_size = pixels.size();
    param.alpha_value = 0x7F;
    bool completed = false;
    std::int32_t decoded = 0;
    try { decoded = control(handle, &param, &info); completed = true; }
    catch (const std::exception& error) { std::fprintf(stderr, "decode continuation: %s\n", error.what()); }
    Require(completed && decoded == 0x10001, "refused delete preserves the pending decode stream");
    Require(pixels == std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x7F}, "literal pixel oracle");
    Require(source.calls > callsBeforeDelete, "continue existing callback stream");
    Require(erase(handle) == 0, "retry writable delete");
    Require(erase(handle) == static_cast<std::int32_t>(0x80690004), "deleted handle is invalid");
    std::puts("PNG delete protected-memory and stream-retention checks passed");
}
