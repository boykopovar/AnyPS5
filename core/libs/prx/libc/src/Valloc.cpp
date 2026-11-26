#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>

extern "C" void* APS5_VABI valloc_nid_postfix(std::size_t size) {
    void* result = nullptr;
    if (posix_memalign_nid_postfix(&result, static_cast<std::size_t>(getpagesize_nid_postfix()), size) != 0) return nullptr;
    return result;
}
