#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTFILENUMBER_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTFILENUMBER_HPP

#include <cstdint>

inline constexpr std::uint32_t GuestFileNumber_nid_no_patch(std::uint64_t identity) noexcept {
    const auto folded = static_cast<std::uint32_t>(identity) ^ (static_cast<std::uint32_t>(identity >> 32) * 0x9E3779B1u);
    return folded == 0 ? 1 : folded;
}

#endif
