#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/Api.hpp"

namespace {

char LowerCharacter(char character) {
    return static_cast<char>(_Tolower_nid_postfix(static_cast<unsigned char>(character), nullptr));
}

char UpperCharacter(char character) {
    return static_cast<char>(_Toupper_nid_postfix(static_cast<unsigned char>(character), nullptr));
}

std::uint16_t LowerWide(std::uint16_t character) {
    return static_cast<std::uint16_t>(_Towlower_nid_postfix(character, nullptr));
}

std::uint16_t WidenCharacter(char character) {
    return static_cast<std::uint16_t>(_Btowc_nid_postfix(static_cast<unsigned char>(character)));
}

char NarrowWide(std::uint16_t character, char fallback) {
    const int narrowed = _Wctob_nid_postfix(character);
    return narrowed == -1 ? fallback : static_cast<char>(narrowed);
}

template <typename TCharacter, typename TConvert>
const TCharacter* ConvertRange(TCharacter* first, const TCharacter* last, TConvert convert) {
    for (; first != last; ++first) *first = convert(*first);
    return last;
}

}

extern "C" {

char APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(const GuestLocale::Facet*, char character) {
    return LowerCharacter(character);
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(const GuestLocale::Facet*, char* first, const char* last) {
    return ConvertRange(first, last, LowerCharacter);
}

char APS5_VABI _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(const GuestLocale::Facet*, char character) {
    return UpperCharacter(character);
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(const GuestLocale::Facet*, char* first, const char* last) {
    return ConvertRange(first, last, UpperCharacter);
}

std::uint16_t APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(const GuestLocale::Facet*, std::uint16_t character) {
    return LowerWide(character);
}

const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEPwPKw_nid_postfix(const GuestLocale::Facet*, std::uint16_t* first, const std::uint16_t* last) {
    return ConvertRange(first, last, LowerWide);
}

std::uint16_t APS5_VABI _ZNKSt5ctypeIwE8do_widenEc_nid_postfix(const GuestLocale::Facet*, char character) {
    return WidenCharacter(character);
}

const char* APS5_VABI _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(const GuestLocale::Facet*, const char* first, const char* last, std::uint16_t* destination) {
    for (; first != last; ++first, ++destination) *destination = WidenCharacter(*first);
    return first;
}

char APS5_VABI _ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(const GuestLocale::Facet*, std::uint16_t character, char fallback) {
    return NarrowWide(character, fallback);
}

const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(const GuestLocale::Facet*, const std::uint16_t* first, const std::uint16_t* last, char fallback, char* destination) {
    for (; first != last; ++first, ++destination) *destination = NarrowWide(*first, fallback);
    return first;
}

}
