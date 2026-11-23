#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace JpegParser {

using Allocate = void* (APS5_VABI*)(std::size_t);
using ZeroAllocate = void* (APS5_VABI*)(std::size_t, std::size_t);
using Free = void (APS5_VABI*)(void*);

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

struct ImageInfo {
    FileStat stat{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool multiPicture = false;
    std::vector<std::uint8_t> exif;
};

int ReadImage_nid_no_patch(const char* path, ImageInfo& image);
int ReadFields_nid_no_patch(const char* path, void* request, void* output, Allocate allocate, ZeroAllocate zero, Free release);

}
