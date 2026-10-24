#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace GuestCLocale {

struct Ctypevec {
    const short* table;
    std::int32_t deleteFlag;
};
static_assert(sizeof(Ctypevec) == 16);
static_assert(offsetof(Ctypevec, deleteFlag) == 8);

struct Collvec {
    std::uint64_t handle;
    std::uint64_t page;
};
static_assert(sizeof(Collvec) == 16);

struct Cvtvec {
    std::uint32_t page;
    std::uint32_t maxBytes;
    std::uint8_t reserved[32];
};
static_assert(sizeof(Cvtvec) == 40);
static_assert(offsetof(Cvtvec, reserved) == 8);

constexpr int DateOrderNone = 2;

}

extern "C" {

const short* APS5_VABI _Getpctype_nid_postfix();

GuestCLocale::Ctypevec APS5_VABI _Getctype_nid_postfix() {
    return {_Getpctype_nid_postfix(), 0};
}

GuestCLocale::Collvec APS5_VABI _Getcoll_nid_postfix() {
    return {};
}

GuestCLocale::Cvtvec APS5_VABI _Getcvt_nid_postfix() {
    GuestCLocale::Cvtvec vec{};
    vec.page = 1;
    vec.maxBytes = 1;
    return vec;
}

int APS5_VABI _Getdateorder_nid_postfix() {
    return GuestCLocale::DateOrderNone;
}

int APS5_VABI _Strcoll_nid_postfix(const char* first1, const char* last1, const char* first2, const char* last2, const void*) {
    const int length1 = static_cast<int>(last1 - first1);
    const int length2 = static_cast<int>(last2 - first2);
    const int common = length1 < length2 ? length1 : length2;
    for (int i = 0; i < common; ++i) {
        const int difference = static_cast<unsigned char>(first1[i]) - static_cast<unsigned char>(first2[i]);
        if (difference != 0) return difference;
    }
    if (length1 == length2) return 0;
    return length1 > length2 ? 1 : -1;
}

std::size_t APS5_VABI _Strxfrm_nid_postfix(char* dest, char* destEnd, const char* src, const char* srcEnd, const void*) {
    const std::size_t length = static_cast<std::size_t>(srcEnd - src);
    if (length <= static_cast<std::size_t>(destEnd - dest)) std::memcpy(dest, src, length);
    return length;
}

}
