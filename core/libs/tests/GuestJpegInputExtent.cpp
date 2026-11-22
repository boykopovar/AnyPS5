#include "prx/libSceJpegDec/include/Api.hpp"
#include "Decoder/Jpeg.hpp"

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
    JpegDecCreateParam create{sizeof(create), 1, 64};
    const int required = sceJpegDecQueryMemorySize(&create);
    Require(required > 0);
    std::vector<std::max_align_t> work((required + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    void* handle = nullptr;
    Require(sceJpegDecCreate(&create, work.data(), required, &handle) == 0);
    std::size_t cases = 0;
    for (unsigned channels : {1,3}) {
        const unsigned width = 7, height = 5;
        std::vector<unsigned char> pixels(width * height * channels);
        for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<unsigned char>(i * 37 + 19);
        const auto jpeg = Decoder::Jpeg::Encode(pixels, width, height, channels, 80);
        Require(!jpeg.empty());
        const auto baseline = Decoder::Jpeg::Decode(jpeg);
        Require(baseline && baseline->width == width && baseline->height == height);
        Require((baseline->channels == 1 || baseline->channels == 3) && baseline->pixels.size() == width * height * baseline->channels);
        for (std::size_t length = 0; length <= jpeg.size(); ++length) {
            Guarded input(length), output(width * height * 4), description(sizeof(JpegDecImageInfo));
            auto* bytes = input.End(length);
            std::copy(jpeg.begin(), jpeg.begin() + length, bytes);
            input.Protect(true);
            auto* image = output.End(width * height * 4);
            auto* info = std::construct_at(reinterpret_cast<JpegDecImageInfo*>(description.End(sizeof(JpegDecImageInfo))));
            JpegDecDecodeParam p{};
            p.jpeg_mem_addr = bytes; p.jpeg_mem_size = length;
            p.image_mem_addr = image; p.image_mem_size = width * height * 4;
            p.down_scale = 1; p.pixel_format = 0; p.alpha_value = 0x5a; p.image_pitch = width * 4;
            const int result = sceJpegDecDecode(handle, &p, info);
            if (length == jpeg.size()) {
                Require(result == static_cast<int>((width << 16) | height));
                Require(info->image_width == width && info->image_height == height);
                for (unsigned i = 0; i < width * height; ++i) {
                    for (unsigned channel = 0; channel < 3; ++channel)
                        Require(image[i * 4 + channel] == baseline->pixels[i * baseline->channels + (baseline->channels == 1 ? 0 : channel)]);
                    Require(image[i * 4 + 3] == 0x5a);
                }
            }
            Require(std::equal(jpeg.begin(), jpeg.begin() + length, bytes));
            input.Prefix(bytes); output.Prefix(image); description.Prefix(info);
            ++cases;
        }
    }
    Require(sceJpegDecDelete(handle) == 0);
    std::printf("%zu protected JPEG input prefixes passed\n", cases);
}
