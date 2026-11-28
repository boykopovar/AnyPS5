#pragma once

#include "SceTypes.hpp"
#include <cstddef>

namespace PngWriter {
struct Allocator {
    void* (APS5_VABI* allocate)(std::size_t);
    void (APS5_VABI* release)(void*);
};
Allocator GetAllocator_nid_no_patch();
}
