#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>

namespace GifParser {

struct Allocator {
    void* (APS5_VABI* allocate)(std::size_t);
    void (APS5_VABI* release)(void*);
};

Allocator GetAllocator_nid_no_patch();

} // namespace GifParser
