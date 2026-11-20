#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cerrno>
#include <cstdint>

extern "C" {

std::int8_t APS5_VABI u_isISOControl_nid_postfix(std::int32_t character) {
    return static_cast<std::uint32_t>(character) <= 0x9f && (character <= 0x1f || character >= 0x7f);
}

std::int32_t APS5_VABI u_forDigit_nid_postfix(std::int32_t digit, std::int8_t radix) {
    if (radix < 2 || radix > 36 || digit < 0 || digit >= radix) return 0;
    return digit < 10 ? '0' + digit : 'a' - 10 + digit;
}


std::int32_t APS5_VABI u_getISOComment_nid_postfix(std::int32_t, char* destination, std::int32_t capacity, std::int32_t* status) {
    if (!status) return 0;
    const int savedErrno = errno;
    std::int32_t previous;
    const bool read = GuestCopyIn_nid_no_patch(&previous, status, sizeof(previous));
    errno = savedErrno;
    if (!read) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (previous > 0) return 0;

    std::int32_t updated = previous;
    if (capacity < 0 || (capacity > 0 && !destination)) {
        updated = Icu::IllegalArgument;
    } else if (capacity == 0) {
        updated = Icu::NotTerminated;
    } else if (updated == Icu::NotTerminated) {
        updated = 0;
    }

    if (updated != previous) {
        const bool writable = GuestWritable_nid_no_patch(status, sizeof(updated));
        errno = savedErrno;
        if (!writable) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (capacity > 0 && destination && updated != Icu::IllegalArgument) {
        constexpr char terminator = 0;
        const bool copied = GuestCopyOut_nid_no_patch(destination, &terminator, sizeof(terminator));
        errno = savedErrno;
        if (!copied) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (updated != previous) {
        const bool copied = GuestCopyOut_nid_no_patch(status, &updated, sizeof(updated));
        errno = savedErrno;
        if (!copied) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return 0;
}

}
