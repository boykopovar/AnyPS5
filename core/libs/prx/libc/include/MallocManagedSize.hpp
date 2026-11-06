#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_MALLOCMANAGEDSIZE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_MALLOCMANAGEDSIZE_HPP

#include <cstddef>
#include <cstdint>

struct MallocManagedSize {
    std::uint16_t size;
    std::uint16_t version;
    std::uint32_t reserved;
    std::size_t maxSystemSize;
    std::size_t currentSystemSize;
    std::size_t maxInuseSize;
    std::size_t currentInuseSize;
};
static_assert(sizeof(MallocManagedSize) == 0x28);

#endif
