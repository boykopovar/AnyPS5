#include <cstddef>
#include <stdexcept>

#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI dl_iterate_phdr_nid_postfix(int (*callback)(dl_phdr_info*, std::size_t, void*), void* data) {
#if defined(__linux__)
    return dl_iterate_phdr(callback, data);
#else
    (void)callback;
    (void)data;
    throw std::runtime_error("dl_iterate_phdr not available on this platform");
#endif
}

}
