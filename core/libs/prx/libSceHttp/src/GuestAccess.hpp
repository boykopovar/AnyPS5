#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_GUESTACCESS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_GUESTACCESS_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "prx/libc/include/General.hpp"

namespace HttpGuest {

template <typename T>
bool Store(T* destination, const T& value) {
    return GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
}

inline bool CopyText(const char* source, std::size_t limit, std::string& destination) {
    destination.clear();
    char chunk[256];
    auto address = reinterpret_cast<std::uintptr_t>(source);
    while (destination.size() < limit) {
        const std::size_t count = std::min({sizeof(chunk), static_cast<std::size_t>(0x1000 - address % 0x1000), limit - destination.size()});
        if (address > UINTPTR_MAX - count || !GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), count)) return false;
        if (const void* end = std::memchr(chunk, 0, count)) {
            destination.append(chunk, static_cast<const char*>(end) - chunk);
            return true;
        }
        destination.append(chunk, count);
        address += count;
    }
    return true;
}

}

#endif
