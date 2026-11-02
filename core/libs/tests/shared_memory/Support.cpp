#include "prx/libc/include/general/VabiMacros.hpp"
#include <stdexcept>

extern "C" int* APS5_VABI __error_nid_postfix() {
    static thread_local int error;
    return &error;
}

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    throw std::runtime_error(name);
}
