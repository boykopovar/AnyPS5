#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU platform helper check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

struct Record {
    std::int32_t key;
    std::int32_t order;
    std::array<char, 240> padding;
};

std::int32_t APS5_VABI Compare_nid_no_patch(const void* context, const void* left, const void* right) {
    Require(context && *static_cast<const int*>(context) == 42);
    return static_cast<const Record*>(left)->key - static_cast<const Record*>(right)->key;
}

std::string diagnostic;
void APS5_VABI Print_nid_no_patch(void* context, const char* format, Icu::TraceArguments arguments) {
    Require(context && *static_cast<const int*>(context) == 42);
    char buffer[200];
    const auto count = va_arg(arguments, int);
    const auto position = va_arg(arguments, int);
    std::snprintf(buffer, sizeof(buffer), format, count, position);
    diagnostic = buffer;
}

void CharacterMappings() {
    constexpr std::array<std::uint32_t, 4> invariant{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};
    for (int i = 0; i < 256; ++i) {
        const char byte = static_cast<char>(i);
        const char16_t unit = static_cast<char16_t>(i);
        const bool member = i < 128 && (invariant[i / 32] & (1u << (i % 32)));
        Require(uprv_isInvariantString_nid_postfix(&byte, 1) == member);
        Require(uprv_isInvariantUString_nid_postfix(&unit, 1) == member);
        Require(static_cast<std::uint8_t>(uprv_asciitolower_nid_postfix(byte)) == (i >= 65 && i <= 90 ? i + 32 : i));
        Require(static_cast<std::uint8_t>(uprv_toupper_nid_postfix(byte)) == (i >= 97 && i <= 122 ? i - 32 : i));
        Require(uprv_isASCIILetter_nid_postfix(byte) == ((i >= 65 && i <= 90) || (i >= 97 && i <= 122)));
        const bool ebcdicUpper = (i >= 0xc1 && i <= 0xc9) || (i >= 0xd1 && i <= 0xd9) || (i >= 0xe2 && i <= 0xe9);
        Require(static_cast<std::uint8_t>(uprv_ebcdictolower_nid_postfix(byte)) == (ebcdicUpper ? i - 64 : i));
    }
    for (std::uint32_t i = 256; i <= 65535; ++i) {
        const auto unit = static_cast<char16_t>(i);
        Require(!uprv_isInvariantUString_nid_postfix(&unit, 1));
    }
    Require(uprv_isInvariantString_nid_postfix(nullptr, 0));
    Require(uprv_isInvariantUString_nid_postfix(nullptr, 0));
    Require(uprv_isInvariantString_nid_postfix("a\0@", -128));
    Require(!uprv_isInvariantString_nid_postfix("a\0@", 3));
    Require(!uprv_isInvariantUString_nid_postfix(u"a\0@", 3));
    Invalid([] { uprv_isInvariantString_nid_postfix(nullptr, -1); });
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0);
    Require(uprv_strnicmp_nid_postfix(nullptr, "", 0) == -1);
    Require(uprv_stricmp_nid_postfix("", "z") == -1);
    Require(uprv_stricmp_nid_postfix("z", "") == 1);
    Require(uprv_stricmp_nid_postfix("ABC", "abc") == 0);
    Require(uprv_stricmp_nid_postfix("\xff", "A") == 158);
    Require(uprv_strnicmp_nid_postfix("abC", "ABd", 2) == 0);
    Require(uprv_strnicmp_nid_postfix("abC", "ABd", 3) == -1);
    Require(uprv_compareASCIIPropertyNames_nid_postfix("  A-_b\tC\r\n", "abc") == 0);
    Require(uprv_compareASCIIPropertyNames_nid_postfix("a", "C") == -2);
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("\xc1\x60\x40\x05\x15\x25\x0b\x0c\x0d\x6d\xc2", "\x81\x82") == 0);
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("\xc1", "\xc2") == -1);
    Invalid([] { uprv_compareASCIIPropertyNames_nid_postfix(nullptr, ""); });
}

void Ebcdic() {
    constexpr std::array<std::uint8_t, 256> expected{
        0,1,2,3,0,9,0,127,0,0,0,11,12,13,14,15,16,17,18,19,0,10,8,0,24,25,0,0,28,29,30,31,
        0,0,0,0,0,10,23,27,0,0,0,0,0,5,6,7,0,0,22,0,0,0,0,4,0,0,0,0,20,21,0,26,
        32,0,0,0,0,0,0,0,0,0,0,46,60,40,43,124,38,0,0,0,0,0,0,0,0,0,33,36,42,41,59,94,
        45,47,0,0,0,0,0,0,0,0,0,44,37,95,62,63,0,0,0,0,0,0,0,0,0,96,58,35,64,39,61,34,
        0,97,98,99,100,101,102,103,104,105,0,0,0,0,0,0,0,106,107,108,109,110,111,112,113,114,0,0,0,0,0,0,
        0,126,115,116,117,118,119,120,121,122,0,0,0,91,0,0,94,0,0,0,0,0,0,0,0,0,91,93,0,93,0,0,
        123,65,66,67,68,69,70,71,72,73,0,0,0,0,0,0,125,74,75,76,77,78,79,80,81,82,0,0,0,0,0,0,
        92,0,83,84,85,86,87,88,89,90,0,0,0,0,0,0,48,49,50,51,52,53,54,55,56,57,0,0,0,0,0,0};
    for (unsigned i = 0; i < 256; ++i) {
        Require(static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(i))) == expected[i]);
        const auto value = expected[i];
        const auto lower = i == 0xe0 ? 124 : value >= 65 && value <= 90 ? value + 32 : value;
        Require(static_cast<std::uint8_t>(uprv_ebcdicToLowercaseAscii_nid_postfix(static_cast<char>(i))) == lower);
    }
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1", "\x81") == -32);
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\x41", "\xc1") == -130);
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\x41", "\x41") == 0);
    std::array<std::uint8_t, 8> buffer;
    buffer.fill(0xee);
    const std::uint8_t bytes[]{0xc1, 0xc2, 0};
    Require(uprv_aestrncpy_nid_postfix(buffer.data(), bytes, 5) == buffer.data());
    Require(buffer == std::array<std::uint8_t, 8>{'A','B',0,0,0,0xee,0xee,0xee});
    const std::uint8_t chars[]{'A','!','B',0};
    buffer.fill(0xee);
    uprv_eastrncpy_nid_postfix(buffer.data(), chars, -1);
    Require(buffer == std::array<std::uint8_t, 8>{0xc1,0x6f,0xc2,0,0xee,0xee,0xee,0xee});
    Require(uprv_aestrncpy_nid_postfix(nullptr, bytes, 0) == nullptr);
    Require(uprv_eastrncpy_nid_postfix(nullptr, chars, std::numeric_limits<std::int32_t>::min()) == nullptr);
    Invalid([] { uprv_aestrncpy_nid_postfix(nullptr, nullptr, 0); });
    Icu::UDataSwapper swapper{};
    int context = 42;
    swapper.printErrorContext = &context;
    swapper.printError = Print_nid_no_patch;
    std::int32_t status = 0;
    buffer.fill(0xee);
    Require(uprv_ebcdicFromAscii_nid_postfix(&swapper, "AB", 3, buffer.data(), &status) == 3 && !status);
    Require(buffer[0] == 0xc1 && buffer[1] == 0xc2 && buffer[2] == 0);
    Require(!uprv_ebcdicFromAscii_nid_postfix(&swapper, chars, 4, buffer.data(), &status) && status == 10);
    Require(diagnostic == "uprv_ebcdicFromAscii() string[4] contains a variant character in position 1\n");
    status = 0;
    Require(!uprv_ebcdicFromAscii_nid_postfix(nullptr, chars, 0, nullptr, &status) && status == 1);
    Require(!uprv_ebcdicFromAscii_nid_postfix(nullptr, nullptr, -1, nullptr, nullptr));
}

void Sorting() {
    std::int32_t minimum = std::numeric_limits<std::int32_t>::min();
    std::int32_t maximum = std::numeric_limits<std::int32_t>::max();
    Require(uprv_int32Comparator_nid_postfix(nullptr, &minimum, &minimum) == 0);
    Invalid([&] { uprv_int32Comparator_nid_postfix(nullptr, &minimum, &maximum); });
    std::uint32_t high = 0xffffffff;
    std::uint32_t low = 0;
    Require(uprv_uint32Comparator_nid_postfix(nullptr, &high, &low) == 1);
    std::uint16_t high16 = 65535;
    std::uint16_t low16 = 0;
    Require(uprv_uint16Comparator_nid_postfix(nullptr, &high16, &low16) == 65535);
    int context = 42;
    for (const int count : {0, 1, 2, 8, 9, 10, 31, 257}) {
        for (const std::int8_t stable : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-128}}) {
            std::vector<Record> records(count);
            for (int i = 0; i < count; ++i) records[i] = {(count - i) % 5, i, {}};
            auto expected = records;
            std::stable_sort(expected.begin(), expected.end(), [](const Record& left, const Record& right) { return left.key < right.key; });
            std::int32_t status = -128;
            uprv_sortArray_nid_postfix(records.data(), count, sizeof(Record), Compare_nid_no_patch, &context, stable, &status);
            Require(status == -128);
            for (int i = 0; i < count; ++i) {
                Require(records[i].key == expected[i].key);
                if (stable) Require(records[i].order == expected[i].order);
            }
            for (int key = -1; key < 7; ++key) {
                Record item{key, 0, {}};
                const auto found = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(records.data()), count, &item, sizeof(Record), Compare_nid_no_patch, &context);
                int insertion = 0;
                while (insertion < count && records[insertion].key <= key) ++insertion;
                Require(found == (insertion > 0 && records[insertion - 1].key == key ? insertion - 1 : ~insertion));
            }
        }
    }
    std::array<std::int32_t, 10> integers{9,8,7,6,5,4,3,2,1,0};
    std::int32_t status = 0;
    uprv_sortArray_nid_postfix(integers.data(), integers.size(), 4, uprv_int32Comparator_nid_postfix, nullptr, 0, &status);
    Require(std::is_sorted(integers.begin(), integers.end()) && !status);
    status = 1;
    uprv_sortArray_nid_postfix(nullptr, -1, 0, nullptr, nullptr, 0, &status);
    Require(status == 1);
    status = 0;
    uprv_sortArray_nid_postfix(nullptr, 1, 4, uprv_int32Comparator_nid_postfix, nullptr, 0, &status);
    Require(status == 1);
    status = 0;
    uprv_sortArray_nid_postfix(nullptr, 0, 4, uprv_int32Comparator_nid_postfix, nullptr, 0, &status);
    Require(!status);
    uprv_sortArray_nid_postfix(nullptr, -1, 0, nullptr, nullptr, 0, nullptr);
    Require(uprv_stableBinarySearch_nid_postfix(nullptr, -1, nullptr, 0, nullptr, nullptr) == -1);
}

void FormattingAndTime() {
    std::array<char16_t, 40> buffer;
    buffer.fill(u'@');
    Require(uprv_itou_nid_postfix(buffer.data(), 40, 0xffffffff, 16, 10) == 10 && std::u16string(buffer.data()) == u"00FFFFFFFF");
    Require(uprv_itou_nid_postfix(buffer.data(), 2, 12345, 10, 0) == 2 && buffer[0] == u'4' && buffer[1] == u'5');
    Require(uprv_itou_nid_postfix(buffer.data(), 0, 42, 10, 0) == 1 && buffer[0] == u'2');
    Require(uprv_itou_nid_postfix(buffer.data(), 5, 42, 1, 0) == 5 && buffer[0] == u'0');
    Require(uprv_itou_nid_postfix(buffer.data(), 40, 35, 36, 0) == 1 && buffer[0] == u'Z');
    Require(uprv_itou_nid_postfix(buffer.data(), 40, 36, 37, 0) == 1 && buffer[0] == u'[');
    Require(uprv_itou_nid_postfix(buffer.data(), 40, 0xfffffffe, 0xffffffff, 0) == 1 && buffer[0] == u'.');
    Invalid([&] { uprv_itou_nid_postfix(buffer.data(), 40, 42, 0, 0); });
    Invalid([] { uprv_itou_nid_postfix(nullptr, 0, 0, 10, 0); });
    Require(!uprv_pathIsAbsolute_nid_postfix(nullptr));
    Require(!uprv_pathIsAbsolute_nid_postfix(""));
    Require(uprv_pathIsAbsolute_nid_postfix("/"));
    Require(!uprv_pathIsAbsolute_nid_postfix("C:/x"));
    Require(!uprv_pathIsAbsolute_nid_postfix("\\x"));
    const auto before = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto raw = uprv_getRawUTCtime_nid_postfix();
    const auto utc = uprv_getUTCtime_nid_postfix();
    const auto after = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    Require(raw >= before && raw <= after && utc >= before && utc <= after);
}

}

int main() {
    CharacterMappings();
    Ebcdic();
    Sorting();
    FormattingAndTime();
    std::puts("ICU platform helper tests passed");
}
