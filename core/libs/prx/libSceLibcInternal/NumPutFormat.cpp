#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t IosOct = 0x20;
constexpr std::int32_t IosHex = 0x40;
constexpr std::int32_t IosBaseField = 0x70;
constexpr std::int32_t IosShowBase = 0x80;
constexpr std::int32_t IosShowPoint = 0x100;
constexpr std::int32_t IosUppercase = 0x200;
constexpr std::int32_t IosShowPos = 0x400;
constexpr std::int32_t IosScientific = 0x800;
constexpr std::int32_t IosFixed = 0x1000;
constexpr std::int32_t IosFloatField = 0x1800;

constexpr std::size_t IntegerFormatCapacity = 7;
constexpr std::size_t FloatFormatCapacity = 8;

char ReadSpecifier(const char* specifier, std::size_t index) {
    char value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, specifier + index, 1)) throw std::invalid_argument("num_put::_Ifmt: an unreadable specifier");
    return value;
}

template <std::size_t Capacity>
char* WriteFormat(char* format, const std::array<char, Capacity>& text, std::size_t size) {
    if (!GuestCopyOut_nid_no_patch(format, text.data(), size)) throw std::invalid_argument("num_put: an unwritable format buffer");
    return format;
}

char* IntegerFormat(char* format, const char* specifier, std::int32_t flags) {
    std::array<char, IntegerFormatCapacity> text{};
    std::size_t size = 0;
    text[size++] = '%';
    if ((flags & IosShowPos) != 0) text[size++] = '+';
    if ((flags & IosShowBase) != 0) text[size++] = '#';
    const char qualifier = ReadSpecifier(specifier, 0);
    if (qualifier == 'L') {
        text[size++] = 'l';
        text[size++] = 'l';
    } else {
        text[size++] = qualifier;
    }
    const std::int32_t base = flags & IosBaseField;
    if (base == IosOct) {
        text[size++] = 'o';
    } else if (base == IosHex) {
        text[size++] = (flags & IosUppercase) != 0 ? 'X' : 'x';
    } else {
        text[size++] = ReadSpecifier(specifier, 1);
    }
    text[size++] = '\0';
    return WriteFormat(format, text, size);
}

char FloatConversion(std::int32_t flags) {
    const std::int32_t field = flags & IosFloatField;
    const bool uppercase = (flags & IosUppercase) != 0;
    if (field == IosFixed) return 'f';
    if (field == IosFloatField) return uppercase ? 'A' : 'a';
    if (field == IosScientific) return uppercase ? 'E' : 'e';
    return uppercase ? 'G' : 'g';
}

char* FloatFormat(char* format, char specifier, std::int32_t flags) {
    std::array<char, FloatFormatCapacity> text{};
    std::size_t size = 0;
    text[size++] = '%';
    if ((flags & IosShowPos) != 0) text[size++] = '+';
    if ((flags & IosShowPoint) != 0) text[size++] = '#';
    if ((flags & IosFloatField) != IosFloatField) {
        text[size++] = '.';
        text[size++] = '*';
    }
    if (specifier != '\0') text[size++] = specifier;
    text[size++] = FloatConversion(flags);
    text[size++] = '\0';
    return WriteFormat(format, text, size);
}

}

extern "C" {

char* APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix(
    const void*, char* format, const char* specifier, std::int32_t flags) {
    return IntegerFormat(format, specifier, flags);
}

char* APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix(
    const void*, char* format, const char* specifier, std::int32_t flags) {
    return IntegerFormat(format, specifier, flags);
}

char* APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix(
    const void*, char* format, char specifier, std::int32_t flags) {
    return FloatFormat(format, specifier, flags);
}

char* APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix(
    const void*, char* format, char specifier, std::int32_t flags) {
    return FloatFormat(format, specifier, flags);
}

}
