#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <stdexcept>

namespace {

constexpr std::size_t MaximumPathLength = 1024;

}

extern "C" char* APS5_VABI getwd_nid_postfix(char* buffer) {
    if (buffer == nullptr) throw std::invalid_argument("getwd: null buffer");
    if (char* path = getcwd_nid_postfix(buffer, MaximumPathLength); path != nullptr) return path;
    strerror_r_nid_postfix(*__error_nid_postfix(), buffer, MaximumPathLength);
    return nullptr;
}
