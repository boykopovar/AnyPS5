#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstring>
#include <stdexcept>

extern "C" int* APS5_VABI __error_nid_postfix() {
    static thread_local int error;
    return &error;
}

extern "C" [[noreturn]] void (NotImplemented_nid_no_patch)(const char* name) {
    throw std::runtime_error(name);
}

extern "C" int CopyGuestPath_nid_no_patch(const char* source, std::string& destination) {
    if (source == nullptr) return 14;
    constexpr std::size_t maxPath = 1024;
    const auto length = ::strnlen(source, maxPath);
    if (length == maxPath) return 63;
    destination.assign(source, length);
    return 0;
}
