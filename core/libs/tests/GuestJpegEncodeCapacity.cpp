#include "prx/libSceJpegEnc/include/Api.hpp"
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
    JpegEncCreateParam create{sizeof(create), 0};
    const int required = sceJpegEncQueryMemorySize(&create);
    Require(required > 0);
    std::vector<std::max_align_t> work((required + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    void* handle = nullptr;
    Require(sceJpegEncCreate(&create, work.data(), required, &handle) == 0);
    std::size_t cases = 0;
    for (unsigned format : {0, 1, 11}) {
        const unsigned channels = format == 11 ? 1 : 4;
        const unsigned width = 16, height = 16;
        std::vector<unsigned char> pixels(width * height * channels);
        for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<unsigned char>(i * 37 + i / 19);
        const auto saved = pixels;
        std::vector<unsigned char> reference(16384);
        JpegEncEncodeParam p{};
        p.image = pixels.data(); p.image_size = pixels.size();
        p.image_width = width; p.image_height = height; p.image_pitch = width * channels;
        p.pixel_format = format; p.color_space = format == 11 ? 2 : 1;
        p.sampling_type = format == 11 ? 0 : 2; p.compression_ratio = 80;
        p.jpeg = reference.data(); p.jpeg_size = reference.size();
        JpegEncOutputInfo info{};
        const int referenceSize = sceJpegEncEncode(handle, &p, &info);
        Require(referenceSize > 4 && info.size == static_cast<unsigned>(referenceSize) && info.height == height);
        const auto baseline = Decoder::Jpeg::Decode({reference.data(), static_cast<std::size_t>(referenceSize)});
        Require(baseline && baseline->width == width && baseline->height == height);
        Require((baseline->channels == 1 || baseline->channels == 3) && baseline->pixels.size() == width * height * baseline->channels);
        std::vector<std::size_t> capacities{1,2,3,4,8,16,31,32,63,64,127,128,255,256};
        for (int delta = -4; delta <= 4; ++delta) capacities.push_back(referenceSize + delta);
        capacities.push_back(referenceSize * 2);
        for (auto capacity : capacities) {
            Guarded output(capacity), description(sizeof(info));
            auto* bytes = output.End(capacity);
            auto* resultInfo = std::construct_at(reinterpret_cast<JpegEncOutputInfo*>(description.End(sizeof(info))));
            p.jpeg = bytes; p.jpeg_size = capacity;
            const int result = sceJpegEncEncode(handle, &p, resultInfo);
            output.Prefix(bytes); description.Prefix(resultInfo);
            Require(pixels == saved);
            if (result > 0) {
                Require(static_cast<std::size_t>(result) <= capacity && resultInfo->size == static_cast<unsigned>(result));
                const auto image = Decoder::Jpeg::Decode({bytes, static_cast<std::size_t>(result)});
                Require(image && image->width == width && image->height == resultInfo->height);
                Require((image->channels == 1 || image->channels == 3) && image->pixels.size() == width * image->height * image->channels);
                if (capacity >= static_cast<std::size_t>(referenceSize)) Require(image->height == height && image->pixels == baseline->pixels);
            } else Require(capacity < static_cast<std::size_t>(referenceSize));
            ++cases;
        }
    }
    Require(sceJpegEncDelete(handle) == 0);
    std::printf("%zu guarded JPEG capacity cases passed\n", cases);
}
