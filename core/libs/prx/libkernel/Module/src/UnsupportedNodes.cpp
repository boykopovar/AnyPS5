#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>

extern "C" int APS5_VABI dladdr_nid_postfix(const void*, void*) {
    NotImplemented_nid_no_patch(__func__);
}

extern "C" int APS5_VABI mknod_nid_postfix(const char*, std::uint16_t, std::uint64_t) {
    NotImplemented_nid_no_patch(__func__);
}
