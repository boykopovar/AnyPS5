#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" std::int64_t APS5_VABI ftok_nid_postfix(const char* path, int id) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) < 0) return -1;
    return static_cast<std::int64_t>(static_cast<std::uint32_t>(id) << 24 | (status.st_dev & 0xffu) << 16 | (status.st_ino & 0xffffu));
}
