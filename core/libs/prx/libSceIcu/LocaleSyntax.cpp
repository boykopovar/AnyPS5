#include "prx/libSceIcu/LocaleSyntax.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "unicode/uloc.h"
#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    if (!status) throw std::invalid_argument(std::string(caller) + ": invalid argument");
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument(std::string(caller) + ": unreadable status");
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!writable) throw std::invalid_argument(std::string(caller) + ": unwritable status");
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* locale, char* result,
                                                 std::int32_t capacity, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (!locale) locale = Icu::DefaultLocaleName();
    if (!result && capacity > 0) APS5_INVALID_ARG_EX;
    auto local = static_cast<UErrorCode>(incoming);
    const auto length = uloc_getVariant(locale, result, capacity, &local);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keyword, const char* value,
                                                      char* buffer, std::int32_t capacity,
                                                      std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (keyword && *keyword && capacity > 1 && !buffer) APS5_INVALID_ARG_EX;
    auto local = static_cast<UErrorCode>(incoming);
    const auto length = uloc_setKeywordValue(keyword, value, buffer, capacity, &local);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

}
