// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

template<typename TValue>
TValue Require(TValue value) {
    if (!value) throw std::invalid_argument("ICU platform helper: null pointer");
    return value;
}

void* Allocate(std::size_t size) {
    return std::malloc(size);
}

constexpr bool Invariant(std::uint32_t value) {
    constexpr std::array<std::uint32_t, 4> bits{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};
    return value < 128 && (bits[value / 32] & (std::uint32_t{1} << (value % 32)));
}

constexpr std::uint8_t Lower(std::uint8_t value, bool ebcdic = false) {
    if (!ebcdic) return value >= 'A' && value <= 'Z' ? value + 32 : value;
    return ((value >= 0xc1 && value <= 0xc9) || (value >= 0xd1 && value <= 0xd9) ||
            (value >= 0xe2 && value <= 0xe9)) ? value - 0x40 : value;
}

constexpr auto AsciiFromEbcdic = [] {
    std::array<std::uint8_t, 256> table{};
    constexpr std::array<std::uint8_t, 32> controls{
        0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x15, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26, 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f};
    for (std::uint8_t i = 0; i < 32; ++i) table[controls[i]] = i;
    table[0x25] = 10;
    table[0x07] = 127;
    constexpr std::pair<std::uint8_t, char> punctuation[]{
        {0x40, ' '}, {0x4b, '.'}, {0x4c, '<'}, {0x4d, '('}, {0x4e, '+'}, {0x4f, '|'},
        {0x50, '&'}, {0x5a, '!'}, {0x5b, '$'}, {0x5c, '*'}, {0x5d, ')'}, {0x5e, ';'}, {0x5f, '^'},
        {0x60, '-'}, {0x61, '/'}, {0x6b, ','}, {0x6c, '%'}, {0x6d, '_'}, {0x6e, '>'}, {0x6f, '?'},
        {0x79, '`'}, {0x7a, ':'}, {0x7b, '#'}, {0x7c, '@'}, {0x7d, '\''}, {0x7e, '='}, {0x7f, '"'},
        {0xa1, '~'}, {0xad, '['}, {0xb0, '^'}, {0xba, '['}, {0xbb, ']'}, {0xbd, ']'},
        {0xc0, '{'}, {0xd0, '}'}, {0xe0, '\\'}};
    for (const auto& [index, value] : punctuation) table[index] = value;
    for (std::uint8_t i = 0; i < 26; ++i) {
        const auto index = i < 9 ? 0x81 + i : i < 18 ? 0x91 + i - 9 : 0xa2 + i - 18;
        table[index] = 'a' + i;
        table[index + 0x40] = 'A' + i;
    }
    for (std::uint8_t i = 0; i < 10; ++i) table[0xf0 + i] = '0' + i;
    return table;
}();

constexpr auto EbcdicFromAscii = [] {
    std::array<std::uint8_t, 256> table{};
    for (std::size_t i = 1; i < 256; ++i) {
        const auto value = AsciiFromEbcdic[i];
        if (value && Invariant(value)) table[value] = static_cast<std::uint8_t>(i);
    }
    return table;
}();

std::int32_t Compare(const char* left, const char* right, std::uint64_t count) {
    if (!left) return right ? -1 : 0;
    if (!right) return 1;
    while (count--) {
        const auto first = static_cast<std::uint8_t>(*left++);
        const auto second = static_cast<std::uint8_t>(*right++);
        if (!first) return second ? -1 : 0;
        if (!second) return 1;
        const auto difference = static_cast<int>(Lower(first)) - Lower(second);
        if (difference) return difference;
    }
    return 0;
}

template<typename TChar>
std::int8_t IsInvariant(const TChar* text, std::int32_t length) {
    for (std::int64_t i = 0; length < 0 || i < length; ++i) {
        const auto value = static_cast<std::uint32_t>(Require(text)[i]);
        if (!value && length < 0) break;
        if (!Invariant(value)) return 0;
    }
    return 1;
}

bool Ignorable(std::uint8_t value, bool ebcdic) {
    if (!ebcdic) return value == '-' || value == '_' || value == ' ' || (value >= 9 && value <= 13);
    return value == 0x60 || value == 0x6d || value == 0x40 || value == 0x05 || value == 0x15 || value == 0x25 ||
           value == 0x0b || value == 0x0c || value == 0x0d;
}

std::uint8_t PropertyCharacter(const char*& text, bool ebcdic) {
    std::uint8_t value;
    std::int32_t consumed = 0;
    do {
        if (consumed == 0x7fffff) throw std::invalid_argument("ICU property name: advance overflow");
        ++consumed;
        value = static_cast<std::uint8_t>(*text++);
    } while (Ignorable(value, ebcdic));
    return Lower(value, ebcdic);
}

std::int32_t CompareNames(const char* left, const char* right, bool ebcdic) {
    Require(left);
    Require(right);
    for (;;) {
        const auto first = PropertyCharacter(left, ebcdic);
        const auto second = PropertyCharacter(right, ebcdic);
        if (first != second) return static_cast<std::int32_t>(first) - second;
        if (!first) return 0;
    }
}

std::uint8_t* Convert(std::uint8_t* destination, const std::uint8_t* source, std::int32_t count, bool toEbcdic) {
    Require(source);
    auto* result = destination;
    if (count == -1) {
        const auto length = std::strlen(reinterpret_cast<const char*>(source));
        count = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(length + 1));
    }
    while (*source && count > 0) {
        auto value = toEbcdic ? EbcdicFromAscii[*source++] : AsciiFromEbcdic[*source++];
        if (toEbcdic && !value) value = 0x6f;
        *Require(destination) = value;
        ++destination;
        --count;
    }
    while (count > 0) {
        *Require(destination) = 0;
        ++destination;
        --count;
    }
    return result;
}

void APS5_VABI PrintError_nid_no_patch(const Icu::UDataSwapper* swapper, const char* format, ...) {
    if (!swapper->printError) return;
    Icu::TraceArguments arguments;
#ifdef _WIN32
    __builtin_sysv_va_start(arguments, format);
#else
    va_start(arguments, format);
#endif
    swapper->printError(swapper->printErrorContext, format, arguments);
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
}

char* Item(char* array, std::int32_t index, std::int32_t width) {
    const auto offset = static_cast<std::int64_t>(index) * width;
    if (offset < std::numeric_limits<std::int32_t>::min() || offset > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU sort: offset overflow");
    return Require(array) + offset;
}

void InsertionSort(char* array, std::int32_t count, std::int32_t width, Icu::ArrayComparator compare, const void* context, void* temporary) {
    for (std::int32_t i = 1; i < count; ++i) {
        auto* item = Item(array, i, width);
        auto insertion = uprv_stableBinarySearch_nid_postfix(array, i, item, width, compare, context);
        insertion = insertion < 0 ? ~insertion : insertion + 1;
        if (insertion >= i) continue;
        auto* destination = Item(array, insertion, width);
        std::memcpy(temporary, item, width);
        std::memmove(destination + width, destination, static_cast<std::size_t>(i - insertion) * width);
        std::memcpy(destination, temporary, width);
    }
}

void QuickSort(char* array, std::int32_t start, std::int32_t limit, std::int32_t width,
               Icu::ArrayComparator compare, const void* context, void* pivot, void* temporary) {
    do {
        if (start > std::numeric_limits<std::int32_t>::max() - 9)
            throw std::invalid_argument("ICU sort: partition overflow");
        if (start + 9 >= limit) {
            InsertionSort(Item(array, start, width), limit - start, width, compare, context, pivot);
            break;
        }
        if (start > std::numeric_limits<std::int32_t>::max() - limit)
            throw std::invalid_argument("ICU sort: midpoint overflow");
        auto left = start;
        auto right = limit;
        std::memcpy(pivot, Item(array, (start + limit) / 2, width), width);
        do {
            while (compare(context, Item(array, left, width), pivot) < 0) {
                if (++left >= limit) throw std::invalid_argument("ICU sort: inconsistent comparator");
            }
            while (compare(context, pivot, Item(array, right - 1, width)) < 0) {
                if (--right <= start) throw std::invalid_argument("ICU sort: inconsistent comparator");
            }
            if (left < right) {
                --right;
                if (left < right) {
                    std::memcpy(temporary, Item(array, left, width), width);
                    std::memcpy(Item(array, left, width), Item(array, right, width), width);
                    std::memcpy(Item(array, right, width), temporary, width);
                }
                ++left;
            }
        } while (left < right);
        if (right - start < limit - left) {
            if (start < right - 1) QuickSort(array, start, right, width, compare, context, pivot, temporary);
            start = left;
        } else {
            if (left < limit - 1) QuickSort(array, left, limit, width, compare, context, pivot, temporary);
            limit = right;
        }
    } while (start < limit - 1);
}

}

extern "C" {

std::int32_t APS5_VABI uprv_uint16Comparator_nid_postfix(const void*, const void* left, const void* right) {
    return static_cast<std::int32_t>(*Require(static_cast<const std::uint16_t*>(left))) - *Require(static_cast<const std::uint16_t*>(right));
}

std::int32_t APS5_VABI uprv_int32Comparator_nid_postfix(const void*, const void* left, const void* right) {
    const auto result = static_cast<std::int64_t>(*Require(static_cast<const std::int32_t*>(left))) - *Require(static_cast<const std::int32_t*>(right));
    if (result < std::numeric_limits<std::int32_t>::min() || result > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU comparator: subtraction overflow");
    return static_cast<std::int32_t>(result);
}

std::int32_t APS5_VABI uprv_uint32Comparator_nid_postfix(const void*, const void* left, const void* right) {
    const auto first = *Require(static_cast<const std::uint32_t*>(left));
    const auto second = *Require(static_cast<const std::uint32_t*>(right));
    return first < second ? -1 : first > second ? 1 : 0;
}

std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item, std::int32_t width, Icu::ArrayComparator compare, const void* context) {
    std::int32_t start = 0;
    bool found = false;
    while (static_cast<std::int64_t>(limit) - start >= 9) {
        if (start > std::numeric_limits<std::int32_t>::max() - limit) throw std::invalid_argument("ICU search: midpoint overflow");
        const auto middle = (start + limit) / 2;
        const auto difference = Require(compare)(context, item, Item(array, middle, width));
        if (difference < 0) limit = middle;
        else {
            found = found || !difference;
            start = middle + (difference == 0 ? 1 : 0);
        }
    }
    while (start < limit) {
        const auto difference = Require(compare)(context, item, Item(array, start, width));
        if (difference < 0) break;
        if (!difference) found = true;
        ++start;
    }
    return found ? start - 1 : ~start;
}

void APS5_VABI uprv_sortArray_nid_postfix(void* array, std::int32_t count, std::int32_t width, Icu::ArrayComparator compare, const void* context, std::int8_t stable, std::int32_t* status) {
    if (!status || *status > 0) return;
    if ((count > 0 && !array) || count < 0 || width <= 0 || !compare) {
        *status = 1;
        return;
    }
    if (count <= 1) return;
    const bool quick = count >= 9 && !stable;
    constexpr auto alignment = sizeof(std::max_align_t);
    constexpr auto stackUnits = (200 + alignment - 1) / alignment;
    std::array<std::max_align_t, stackUnits * 2> stack;
    const auto units = (static_cast<std::size_t>(width) + alignment - 1) / alignment;
    std::unique_ptr<void, decltype(&std::free)> heap(nullptr, std::free);
    auto* temporary = stack.data();
    if (units > stackUnits) {
        heap.reset(Allocate(units * alignment * (quick ? 2 : 1)));
        if (!heap) {
            *status = 7;
            return;
        }
        temporary = static_cast<std::max_align_t*>(heap.get());
    }
    if (quick) QuickSort(static_cast<char*>(array), 0, count, width, compare, context, temporary, temporary + units);
    else InsertionSort(static_cast<char*>(array), count, width, compare, context, temporary);
}

char APS5_VABI uprv_toupper_nid_postfix(char value) {
    return value >= 'a' && value <= 'z' ? value - 32 : value;
}

char APS5_VABI uprv_asciitolower_nid_postfix(char value) {
    return static_cast<char>(Lower(static_cast<std::uint8_t>(value)));
}

char APS5_VABI uprv_ebcdictolower_nid_postfix(char value) {
    return static_cast<char>(Lower(static_cast<std::uint8_t>(value), true));
}

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

std::int32_t APS5_VABI uprv_stricmp_nid_postfix(const char* left, const char* right) {
    return Compare(left, right, std::numeric_limits<std::uint64_t>::max());
}

std::int32_t APS5_VABI uprv_strnicmp_nid_postfix(const char* left, const char* right, std::uint32_t count) {
    return Compare(left, right, count);
}

std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* text, std::int32_t length) {
    return IsInvariant(reinterpret_cast<const std::uint8_t*>(text), length);
}

std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* text, std::int32_t length) {
    return IsInvariant(text, length);
}

std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* left, const char* right) {
    return CompareNames(left, right, false);
}

std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char* left, const char* right) {
    return CompareNames(left, right, true);
}

std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char* left, const char* right) {
    Require(left);
    Require(right);
    for (;;) {
        const auto first = static_cast<std::uint8_t>(*left++);
        const auto second = static_cast<std::uint8_t>(*right++);
        if (first != second) {
            const auto mappedFirst = AsciiFromEbcdic[first];
            const auto mappedSecond = AsciiFromEbcdic[second];
            const int firstOrder = !first ? 0 : mappedFirst && Invariant(mappedFirst) ? mappedFirst : -static_cast<int>(first);
            const int secondOrder = !second ? 0 : mappedSecond && Invariant(mappedSecond) ? mappedSecond : -static_cast<int>(second);
            return firstOrder - secondOrder;
        }
        if (!first) return 0;
    }
}

char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char value) {
    return static_cast<char>(AsciiFromEbcdic[static_cast<std::uint8_t>(value)]);
}

char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char value) {
    const auto byte = static_cast<std::uint8_t>(value);
    return byte == 0xe0 ? '|' : static_cast<char>(Lower(AsciiFromEbcdic[byte]));
}

std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t count) {
    return Convert(destination, source, count, false);
}

std::uint8_t* APS5_VABI uprv_eastrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t count) {
    return Convert(destination, source, count, true);
}

std::int32_t APS5_VABI uprv_ebcdicFromAscii_nid_postfix(const Icu::UDataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!swapper || !input || length < 0 || (length && !output)) {
        *status = 1;
        return 0;
    }
    for (std::int32_t i = 0; i < length; ++i) {
        const auto value = static_cast<const std::uint8_t*>(input)[i];
        if (!Invariant(value)) {
            PrintError_nid_no_patch(swapper, "uprv_ebcdicFromAscii() string[%d] contains a variant character in position %d\n", length, i);
            *status = 10;
            return 0;
        }
        static_cast<std::uint8_t*>(output)[i] = EbcdicFromAscii[value];
    }
    return length;
}

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth) {
    Require(buffer);
    if (!radix) throw std::invalid_argument("ICU itou: zero radix");
    std::int32_t length = 0;
    do {
        const auto digit = std::bit_cast<std::int32_t>(value % radix);
        if (digit > std::numeric_limits<std::int32_t>::max() - 55)
            throw std::invalid_argument("ICU itou: digit overflow");
        buffer[length++] = static_cast<char16_t>(digit + (digit <= 9 ? 48 : 55));
        value /= radix;
    } while (value && length < capacity);
    while (length < minimumWidth) buffer[length++] = u'0';
    if (length < capacity) buffer[length] = 0;
    std::reverse(buffer, buffer + length);
    return length;
}

std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    return path && *path == '/';
}

double APS5_VABI uprv_getRawUTCtime_nid_postfix() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<double>(std::chrono::floor<std::chrono::milliseconds>(now).count());
}

double APS5_VABI uprv_getUTCtime_nid_postfix() {
    return uprv_getRawUTCtime_nid_postfix();
}

}
