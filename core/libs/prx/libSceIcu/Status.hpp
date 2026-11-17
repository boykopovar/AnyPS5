#pragma once
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace Icu {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidCharacter = 10;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t NotTerminated = -124;

inline void RequireStatus(const std::int32_t* status) {
    if (!status) NotImplemented_nid_no_patch("libSceIcu: null UErrorCode");
}

inline bool Failure(const std::int32_t* status) {
    RequireStatus(status);
    return *status > 0;
}

template<class T>
void Terminate(T* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    if (*status > 0 || length < 0) return;
    if (length < capacity) {
        destination[length] = 0;
        if (*status == NotTerminated) *status = 0;
    } else {
        *status = length == capacity ? NotTerminated : BufferOverflow;
    }
}

inline bool Surrogate(std::int32_t value) {
    return (static_cast<std::uint32_t>(value) & 0xfffff800u) == 0xd800u;
}

inline std::int32_t Checked(std::int64_t length) {
    if (length > INT32_MAX) NotImplemented_nid_no_patch("libSceIcu: length exceeds INT32_MAX");
    return static_cast<std::int32_t>(length);
}

}
