#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTARENADRIVERMEMORY_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTARENADRIVERMEMORY_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/GuestArena.hpp"

namespace GuestArena {

inline constexpr std::uintptr_t AgcDriverMemoryAddress = 0x0000000FE0000000ull + GuestAddressOffset;
inline constexpr std::size_t AgcDriverMemoryBytes = std::size_t{2} << 20;

}

#endif
