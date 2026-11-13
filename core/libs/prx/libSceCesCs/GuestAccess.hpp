#ifndef CORE_LIBS_PRX_LIBSCECESCS_GUESTACCESS_HPP
#define CORE_LIBS_PRX_LIBSCECESCS_GUESTACCESS_HPP

#include <cerrno>
#include <cstddef>

#include "prx/libc/include/General.hpp"

namespace CesGuest {

inline bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

inline bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename T>
bool Store(T* destination, const T& value) {
    return CopyOut(destination, &value, sizeof(T));
}

}

#endif
