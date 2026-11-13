#include "prx/libScePngEnc/include/Api.hpp"
#include "Decoder/Png.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

int main() {
    PngEncCreateParam create{sizeof(create), 0, 32, 4};
    const int required = scePngEncQueryMemorySize(&create);
    Require(required > 0);
    std::vector<std::max_align_t> work((required + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    void* handle = nullptr;
    Require(scePngEncCreate(&create, work.data(), required, &handle) == 0);
    std::size_t cases = 0;
    for (unsigned width : {1,5,17}) for (unsigned height : {1,2,7}) for (unsigned padding : {0,4,28}) {
        const unsigned pitch = width * 4 + padding;
        const unsigned span = pitch * height;
        Guarded input(span);
        auto* pixels = input.End(span);
        for (unsigned i = 0; i < span; ++i) pixels[i] = static_cast<unsigned char>(i * 53 + i / pitch);
        const std::vector<unsigned char> saved(pixels, pixels + span);
        input.Protect(true);
        for (unsigned format : {0,1}) for (unsigned space : {3,19}) {
            Guarded output(8192);
            auto* png = output.End(8192);
            PngEncEncodeParam p{pixels, png, span, 8192, width, height, pitch, static_cast<std::uint16_t>(format),
                               static_cast<std::uint16_t>(space), 8, 0, 15, 6};
            PngEncOutputInfo info{};
            const int size = scePngEncEncode(handle, &p, &info);
            Require(size > 0 && size <= 8192 && info.data_size == static_cast<unsigned>(size) && info.processed_height == height);
            const auto image = Decoder::Png::Decode({png, static_cast<std::size_t>(size)});
            Require(image && image->width == width && image->height == height && image->pixels.size() == width * height * 4);
            for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
                const auto* source = pixels + y * pitch + x * 4;
                const auto* result = image->pixels.data() + (y * width + x) * 4;
                Require(result[0] == source[format == 1 ? 2 : 0] && result[1] == source[1]);
                Require(result[2] == source[format == 1 ? 0 : 2] && result[3] == (space == 19 ? source[3] : 255));
            }
            Require(std::equal(saved.begin(), saved.end(), pixels));
            input.Prefix(pixels); output.Prefix(png);
            ++cases;
            if (padding != 0) {
                const unsigned shortSpan = pitch * (height - 1) + width * 4;
                Guarded shorter(shortSpan);
                auto* shortPixels = shorter.End(shortSpan);
                std::copy(saved.begin(), saved.begin() + shortSpan, shortPixels);
                shorter.Protect(true);
                p.image_mem_addr = shortPixels; p.image_mem_size = shortSpan;
                Require(scePngEncEncode(handle, &p, &info) == static_cast<int>(0x80690103));
                Require(std::equal(saved.begin(), saved.begin() + shortSpan, shortPixels));
                shorter.Prefix(shortPixels); output.Prefix(png);
                ++cases;
            }
        }
    }
    Require(scePngEncDelete(handle) == 0);
    std::printf("%zu guarded PNG image extents and short-row rejections passed\n", cases);
}
