#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/urename.h"
#include "unicode/uvernum.h"
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {

std::int32_t ustr_hashUCharsN(const std::uint16_t* s, std::int32_t length);
std::int32_t ustr_hashCharsN(const char* s, std::int32_t length);
std::int32_t ustr_hashICharsN(const char* s, std::int32_t length);

}

namespace {

constexpr std::int32_t MaximumSamples = 63;

template <typename Unit>
std::int32_t HashSamples(const Unit* s, std::int32_t length, std::int32_t (*hash)(const Unit*, std::int32_t),
                         const char* function) {
    if (!s || length <= 0) return hash(s, length);
    const std::int64_t step = (length - 32) / 32 + 1;
    Unit samples[MaximumSamples];
    std::int32_t count = 0;
    for (std::int64_t offset = 0; offset < length; offset += step) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&samples[count], s + offset, sizeof(Unit));
        errno = savedErrno;
        if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable string");
        ++count;
    }
    return hash(samples, count);
}

}

extern "C" {

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const std::uint16_t* s, std::int32_t length) {
    return HashSamples(s, length, ustr_hashUCharsN, __func__);
}

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* s, std::int32_t length) {
    return HashSamples(s, length, ustr_hashCharsN, __func__);
}

std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* s, std::int32_t length) {
    return HashSamples(s, length, ustr_hashICharsN, __func__);
}

}
