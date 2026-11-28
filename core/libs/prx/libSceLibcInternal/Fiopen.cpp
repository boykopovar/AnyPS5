#include "prx/libSceLibcInternal/include/Fiopen.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <stdexcept>

extern "C" FileStream* APS5_VABI _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const std::uint16_t* filename, int mode, int protection) {
    constexpr std::size_t FilenameMax = 1024;
    if (filename == nullptr) throw std::invalid_argument("std::_Fiopen: null wide file name");
    char name[FilenameMax];
    const std::size_t length = wcstombs_nid_postfix(name, filename, sizeof(name));
    if (length == static_cast<std::size_t>(-1)) return nullptr;
    if (length >= sizeof(name)) throw std::invalid_argument("std::_Fiopen: a wide file name that does not fit FILENAME_MAX bytes");
    return _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(name, mode, protection);
}
