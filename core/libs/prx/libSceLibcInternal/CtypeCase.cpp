#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

const void* Address(const void* base, std::size_t offset) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (!base || offset > std::numeric_limits<std::uintptr_t>::max() - address) APS5_INVALID_ARG_EX;
    return reinterpret_cast<const void*>(address + offset);
}

std::uint8_t Map(const GuestLocale::CtypeFacet* self, std::uint8_t character, std::size_t tableOffset) {
    const std::int16_t* table;
    if (!GuestCopyIn_nid_no_patch(&table, Address(self, tableOffset), sizeof(table))) APS5_INVALID_ARG_EX;
    std::uint8_t mapped;
    if (!GuestCopyIn_nid_no_patch(&mapped, Address(table, sizeof(std::int16_t) * character), sizeof(mapped))) {
        APS5_INVALID_ARG_EX;
    }
    return mapped;
}

const char* MapRange(const GuestLocale::CtypeFacet* self, char* first, const char* last,
                     std::size_t tableOffset) {
    auto current = reinterpret_cast<std::uintptr_t>(first);
    const auto end = reinterpret_cast<std::uintptr_t>(last);
    if (current > end) APS5_INVALID_ARG_EX;
    while (current != end) {
        auto* destination = reinterpret_cast<void*>(current);
        std::uint8_t character;
        if (!GuestCopyIn_nid_no_patch(&character, destination, sizeof(character))) APS5_INVALID_ARG_EX;
        const std::uint8_t mapped = Map(self, character, tableOffset);
        if (!GuestCopyOut_nid_no_patch(destination, &mapped, sizeof(mapped))) APS5_INVALID_ARG_EX;
        ++current;
    }
    return last;
}

}

extern "C" {

char APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(const GuestLocale::CtypeFacet* self, char character) {
    return std::bit_cast<char>(Map(self, static_cast<std::uint8_t>(character), offsetof(GuestLocale::CtypeFacet, lower)));
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(
    const GuestLocale::CtypeFacet* self, char* first, const char* last) {
    return MapRange(self, first, last, offsetof(GuestLocale::CtypeFacet, lower));
}

char APS5_VABI _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(const GuestLocale::CtypeFacet* self, char character) {
    return std::bit_cast<char>(Map(self, static_cast<std::uint8_t>(character), offsetof(GuestLocale::CtypeFacet, upper)));
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(
    const GuestLocale::CtypeFacet* self, char* first, const char* last) {
    return MapRange(self, first, last, offsetof(GuestLocale::CtypeFacet, upper));
}

}
