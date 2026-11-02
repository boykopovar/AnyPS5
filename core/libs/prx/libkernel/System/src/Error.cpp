#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cstdint>

extern "C" std::int32_t APS5_VABI sceKernelError(std::int32_t posixError) {
    if (posixError == 0) return 0;
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(posixError) + 0x80020000u);
}
