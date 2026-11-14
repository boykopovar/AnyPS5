#include "prx/libSceJpegDec/include/Api.hpp"
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
    std::fprintf(stderr, "JPEG delete: %s\n", message);
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

static std::int32_t APS5_VABI ReadChunk(JpegDecInputChunk* chunk, void* argument) {
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
    const auto query = Resolve<decltype(&sceJpegDecQueryMemorySize)>(module, nid ? "uNAUmANZMEw" : "sceJpegDecQueryMemorySize");
    const auto create = Resolve<decltype(&sceJpegDecCreate)>(module, nid ? "JPh3Zgg0Zwc" : "sceJpegDecCreate");
    const auto erase = Resolve<decltype(&sceJpegDecDelete)>(module, nid ? "Hwh11+m5KoI" : "sceJpegDecDelete");
    const auto control = Resolve<decltype(&sceJpegDecDecodeWithInputControl)>(module, nid ? "919MhccOiII" : "sceJpegDecDecodeWithInputControl");
    const JpegDecCreateParam createParam{sizeof(JpegDecCreateParam), 1, 1};
    const auto size = query(&createParam);
    Require(size > 0, "query work size");
    WorkMemory work(static_cast<std::size_t>(size));
    void* handle = nullptr;
    Require(create(&createParam, work.data, static_cast<std::uint32_t>(size), &handle) == 0, "create decoder");
    static const std::uint8_t jpeg[] = {
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03,
        0x03, 0x03, 0x03, 0x04, 0x03, 0x03, 0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0A, 0x07,
        0x07, 0x06, 0x08, 0x0C, 0x0A, 0x0C, 0x0C, 0x0B, 0x0A, 0x0B, 0x0B, 0x0D, 0x0E, 0x12, 0x10, 0x0D,
        0x0E, 0x11, 0x0E, 0x0B, 0x0B, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0C, 0x0F,
        0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01,
        0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03,
        0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00,
        0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
        0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72,
        0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
        0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
        0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2,
        0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
        0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
        0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA,
        0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x2B, 0xFF, 0xD9
    };
    std::vector<std::uint8_t> encoded(std::begin(jpeg), std::end(jpeg));
    const std::array<std::uint8_t, 2> scanMarker{0xFF, 0xDA};
    const auto scan = std::search(encoded.begin(), encoded.end(), scanMarker.begin(), scanMarker.end());
    Require(scan != encoded.end(), "literal JPEG scan marker");
    std::vector<std::uint8_t> comment(2052, 0);
    comment[0] = 0xFF;
    comment[1] = 0xFE;
    comment[2] = 0x08;
    comment[3] = 0x02;
    encoded.insert(scan, comment.begin(), comment.end());
    Source source{encoded.data(), encoded.size()};
    JpegDecInputControlParam param{};
    param.input = &ReadChunk;
    param.input_argument = &source;
    param.operation = 0x10;
    param.down_scale = 1;
    JpegDecImageInfo info{};
    Require(control(handle, &param, &info) == 0, "parse callback stream");
    Require(info.image_width == 1 && info.image_height == 1, "literal JPEG dimensions");
    const auto callsBeforeDelete = source.calls;
    const auto* bytes = static_cast<const std::uint8_t*>(work.data);
    const std::vector<std::uint8_t> before(bytes, bytes + work.bytes);
    work.Protect(true);
    const auto result = erase(handle);
    std::printf("delete result=0x%08x\n", static_cast<unsigned>(result));
    if (argc != 4) Require(result == static_cast<std::int32_t>(0x80650001), "read-only delete must report invalid address");
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
    Require(pixels == std::array<std::uint8_t, 4>{0x80, 0x80, 0x80, 0x7F}, "literal pixel oracle");
    Require(source.calls > callsBeforeDelete, "continue existing callback stream");
    Require(erase(handle) == 0, "retry writable delete");
    Require(erase(handle) == static_cast<std::int32_t>(0x80650004), "deleted handle is invalid");
    std::puts("JPEG delete protected-memory and stream-retention checks passed");
}
