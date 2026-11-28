#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

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

template <typename TSource, typename TTarget, typename TConvert>
void ConvertRangeOut(const char* function, const TSource* first, const TSource* last, TTarget* destination, TConvert convert) {
    const auto start = reinterpret_cast<std::uintptr_t>(first);
    const auto end = reinterpret_cast<std::uintptr_t>(last);
    if (end < start || (end - start) % sizeof(TSource) != 0) throw std::invalid_argument(std::string(function) + ": invalid range");
    constexpr std::size_t chunkCount = 0x1000 / sizeof(TSource);
    TSource source[chunkCount];
    TTarget target[chunkCount];
    const std::size_t total = (end - start) / sizeof(TSource);
    for (std::size_t done = 0; done < total;) {
        const auto count = (std::min)(total - done, chunkCount);
        if (!GuestCopyIn_nid_no_patch(source, first + done, count * sizeof(TSource))) throw std::invalid_argument(std::string(function) + ": unreadable range");
        for (std::size_t index = 0; index < count; ++index) target[index] = convert(source[index]);
        if (!GuestCopyOut_nid_no_patch(destination + done, target, count * sizeof(TTarget)))
            throw std::invalid_argument(std::string(function) + ": unwritable destination");
        done += count;
    }
}

}

extern "C" {

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
    ConvertRangeOut(__func__, first, last, destination, WidenCharacter);
    return last;
}

char APS5_VABI _ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(const GuestLocale::Facet*, std::uint16_t character, char fallback) {
    return NarrowWide(character, fallback);
}

const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(const GuestLocale::Facet*, const std::uint16_t* first, const std::uint16_t* last, char fallback, char* destination) {
    ConvertRangeOut(__func__, first, last, destination, [fallback](std::uint16_t character) { return NarrowWide(character, fallback); });
    return last;
}

const short* APS5_VABI _Getctyptab_nid_postfix() {
    return _Getpctype_nid_postfix();
}

}
