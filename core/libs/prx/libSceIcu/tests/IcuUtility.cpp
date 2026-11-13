#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

int main() {
    const auto check = [](bool value) { if (!value) std::abort(); };
    for (std::int32_t point = -1; point <= 0x110000; ++point)
        check(_ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(point) == !(point >= 32 && point <= 126));
    check(_ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(std::numeric_limits<std::int32_t>::min()));
    check(_ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(std::numeric_limits<std::int32_t>::max()));
    struct Case {
        const char16_t* text;
        std::int32_t length;
        std::int32_t start;
        std::int32_t result;
        std::int32_t limit;
    };
    for (const auto& test : {Case{u"123!", 4, 0, 123, 3}, Case{u"-123", 4, 0, 0, 0}, Case{u"00042", 5, 0, 42, 5},
             Case{u"2147483647x", 11, 0, 2147483647, 10}, Case{u"123", 3, -1, 0, -1}, Case{u"", 0, 0, 0, 0},
             Case{u"a42", 3, 1, 42, 3}, Case{u"123", 3, 4, 0, 4}, Case{u"\uff11", 1, 0, 0, 0}, Case{u"12\0" u"3", 4, 0, 12, 2}}) {
        Icu::Abi::UnicodeString string{};
        string.storage.fields = {static_cast<std::int16_t>((test.length << 5) | 8), 0, test.length, const_cast<char16_t*>(test.text)};
        auto position = test.start;
        check(_ZN6icu_6811ICU_Utility17parseAsciiIntegerERKNS_13UnicodeStringERi_nid_postfix(&string, &position) == test.result);
        check(position == test.limit);
    }
    Icu::Abi::UnicodeString overflow{};
    overflow.storage.fields = {static_cast<std::int16_t>((10 << 5) | 8), 0, 10, const_cast<char16_t*>(u"2147483648")};
    std::int32_t position = 0;
    bool threw = false;
    try { _ZN6icu_6811ICU_Utility17parseAsciiIntegerERKNS_13UnicodeStringERi_nid_postfix(&overflow, &position); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw && position == 9);
    threw = false;
    try { _ZN6icu_6811ICU_Utility17parseAsciiIntegerERKNS_13UnicodeStringERi_nid_postfix(&overflow, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw);
    std::puts("ICU utility inspection tests passed");
}
