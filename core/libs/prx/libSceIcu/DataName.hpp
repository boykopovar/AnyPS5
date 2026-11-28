#pragma once

#include "prx/libc/include/General.hpp"
#include "unicode/utypes.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>

namespace Icu {

inline const char* CopyDataName(const char* source, std::string& text, UErrorCode& status, const char* caller) {
    if (!source || U_FAILURE(status)) return nullptr;
    constexpr int GuestOutOfMemory = 12;
    constexpr int GuestNameTooLong = 63;
    constexpr std::size_t maxSize = std::numeric_limits<std::int32_t>::max();
    for (std::size_t size = 256;; size = std::min(size * 2, maxSize)) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(source, size, text);
        errno = savedErrno;
        if (error == 0) return text.c_str();
        if (error == GuestOutOfMemory) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        if (error != GuestNameTooLong) NotImplemented_nid_no_patch(caller);
        if (size == maxSize) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return nullptr;
        }
    }
}

} // namespace Icu
