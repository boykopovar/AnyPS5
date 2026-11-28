#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <stdexcept>

extern "C" {

size_t APS5_VABI _Strxfrmx_nid_postfix(char* destination, const char* source, size_t count, LibcCollationState* state) {
    (void)state;
    if (source == nullptr) throw std::invalid_argument("_Strxfrmx: null source");
    if (destination == nullptr && count != 0) throw std::invalid_argument("_Strxfrmx: null destination");
    return strxfrm_nid_postfix(destination, source, count);
}

int APS5_VABI _Wcscollx_nid_postfix(const char16_t* first, const char16_t* second, LibcCollationState* state) {
    (void)state;
    if (first == nullptr || second == nullptr) throw std::invalid_argument("_Wcscollx: null string");
    return wcscoll_nid_postfix(first, second);
}

size_t APS5_VABI _Wcsxfrmx_nid_postfix(char16_t* destination, const char16_t* source, size_t count, LibcCollationState* state) {
    (void)state;
    if (source == nullptr) throw std::invalid_argument("_Wcsxfrmx: null source");
    if (destination == nullptr && count != 0) throw std::invalid_argument("_Wcsxfrmx: null destination");
    return wcsxfrm_nid_postfix(destination, source, count);
}

size_t APS5_VABI _Wcsftime_nid_postfix(char16_t* destination, size_t count, const char16_t* format, const GuestTm* time, void* locale) {
    (void)locale;
    if (format == nullptr) throw std::invalid_argument("_Wcsftime: null format");
    if (time == nullptr) throw std::invalid_argument("_Wcsftime: null time");
    if (destination == nullptr && count != 0) throw std::invalid_argument("_Wcsftime: null destination");
    return wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(destination), count, reinterpret_cast<const std::uint16_t*>(format), time);
}

}
