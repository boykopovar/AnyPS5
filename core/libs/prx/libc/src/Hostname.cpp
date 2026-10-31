#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>

extern "C" int APS5_VABI gethostname_nid_postfix(char* name, std::size_t length) {
    constexpr char hostname[] = "";
    if (name == nullptr) return 0;
    const int savedError = errno;
    const auto count = std::min(length, sizeof(hostname));
    if (count != 0 && !GuestCopyOut_nid_no_patch(name, hostname, count)) {
        errno = 14;
        return -1;
    }
    if (length < sizeof(hostname)) {
        errno = 63;
        return -1;
    }
    errno = savedError;
    return 0;
}
