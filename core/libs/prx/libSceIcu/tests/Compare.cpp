#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU string comparison check failed at line %u\n", location.line());
    std::abort();
}

std::int32_t Compare(const std::u16string& first, std::int32_t firstLength, const std::u16string& second, std::int32_t secondLength, bool codePointOrder) {
    return u_strCompare_nid_postfix(first.c_str(), firstLength, second.c_str(), secondLength, codePointOrder);
}

std::int32_t Terminated(const std::u16string& first, const std::u16string& second, bool codePointOrder) {
    return Compare(first, -1, second, -1, codePointOrder);
}

std::int32_t Counted(const std::u16string& first, const std::u16string& second, bool codePointOrder) {
    return Compare(first, static_cast<std::int32_t>(first.size()), second, static_cast<std::int32_t>(second.size()), codePointOrder);
}

void Plain() {
    for (bool order : {false, true}) {
        Require(Terminated(u"abc", u"abc", order) == 0);
        Require(Counted(u"abc", u"abc", order) == 0);
        Require(Terminated(u"abc", u"abd", order) == -1);
        Require(Terminated(u"abd", u"abc", order) == 1);
        Require(Terminated(u"ab", u"abc", order) == -u'c');
        Require(Terminated(u"abc", u"ab", order) == u'c');
        Require(Terminated(u"", u"", order) == 0);
        Require(Terminated(u"a", u"", order) == u'a');
        Require(Counted(u"ab", u"abc", order) == -1);
        Require(Counted(u"abc", u"ab", order) == 1);
        Require(Counted(u"", u"a", order) == -1);
        Require(Counted(u"x", u"abc", order) == u'x' - u'a');
        Require(Compare(u"ab", -1, u"abc", 3, order) == -1);
        Require(Compare(u"abc", 3, u"ab", -1, order) == 1);
        Require(Compare(u"abc", 2, u"abd", -1, order) == -1);
        Require(Compare(u"abc", 2, u"ab", -1, order) == 0);
        Require(Compare(u"abcd", 3, u"abce", 3, order) == 0);
        Require(Compare(u"abcd", 4, u"abce", 3, order) == 1);
        const std::u16string first({0x41, 0, 0x42});
        const std::u16string second({0x41, 0, 0x43});
        Require(Compare(first, 3, second, 3, order) == -1);
        Require(Compare(first, 1, second, 1, order) == 0);
        Require(Terminated(first, second, order) == 0);
        Require(u_strCompare_nid_postfix(first.c_str(), -1, first.c_str(), -1, order) == 0);
        Require(u_strCompare_nid_postfix(first.c_str(), 1, first.c_str(), 3, order) == -1);
        Require(u_strCompare_nid_postfix(first.c_str(), 3, first.c_str(), 1, order) == 1);
        Require(u_strCompare_nid_postfix(first.c_str(), 3, first.c_str(), 3, order) == 0);
    }
}

void Surrogates() {
    const std::u16string bmp({0xe000});
    const std::u16string supplementary({0xd800, 0xdc00});
    for (bool terminated : {false, true}) {
        const auto compare = [&](const std::u16string& first, const std::u16string& second, bool order) {
            return terminated ? Terminated(first, second, order) : Counted(first, second, order);
        };
        Require(compare(bmp, supplementary, false) == 0xe000 - 0xd800);
        Require(compare(bmp, supplementary, true) == 0xe000 - 0x2800 - 0xd800);
        Require(compare(supplementary, bmp, true) == 0xd800 - (0xe000 - 0x2800));

        const std::u16string loneLead({0xd900});
        Require(compare(loneLead, supplementary, false) == 0xd900 - 0xd800);
        Require(compare(loneLead, supplementary, true) == 0xd900 - 0x2800 - 0xd800);

        const std::u16string loneTrail({0xdc00});
        Require(compare(loneTrail, supplementary, false) == 0xdc00 - 0xd800);
        Require(compare(loneTrail, supplementary, true) == 0xdc00 - 0x2800 - 0xd800);

        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800, 0xdc01}), true) == -1);
        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xd801, 0xdc00}), true) == -1);
        Require(compare(std::u16string({0xd800, 0x41}), std::u16string({0xd800, 0xdc00}), true) == 0x41 - 0xdc00);
        Require(compare(std::u16string({0xdc00, 0xdc00}), std::u16string({0xdc00, 0xe000}), true) == 0xdc00 - 0x2800 - (0xe000 - 0x2800));
        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xdbff, 0xdfff}), true) == 0xd800 - 0xdbff);
        Require(compare(std::u16string({0x41, 0xd7ff}), std::u16string({0x41, 0xe000}), true) == 0xd7ff - 0xe000);
        Require(compare(std::u16string({0xe000}), std::u16string({0xffff}), true) == 0xe000 - 0xffff);
        Require(compare(std::u16string({0xd800}), std::u16string({0xdfff}), true) == 0xd800 - 0xdfff);
        Require(compare(std::u16string({0xdfff}), std::u16string({0xd800}), false) == 0xdfff - 0xd800);
    }
    Require(Counted(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800}), true) == 1);
    Require(Counted(std::u16string({0xd800}), std::u16string({0xd800, 0xdc00}), true) == -1);
    Require(Compare(std::u16string({0xd900, 0xdc00}), 1, std::u16string({0xd800, 0xdc00}), 2, true) == 0xd900 - 0x2800 - 0xd800);
    Require(Compare(std::u16string({0xd900, 0xdc00}), 2, std::u16string({0xd800, 0xdc00}), 2, true) == 0xd900 - 0xd800);
}

void Arguments() {
    const std::u16string text(u"a");
    Require(u_strCompare_nid_postfix(nullptr, 1, text.c_str(), 1, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), 1, nullptr, 1, 0) == 0);
    Require(u_strCompare_nid_postfix(nullptr, 0, nullptr, 0, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), -2, text.c_str(), 1, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), 1, text.c_str(), std::numeric_limits<std::int32_t>::min(), 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), -2, u"b", 1, 1) == 0);
}

}

int main() {
    Plain();
    Surrogates();
    Arguments();
    std::puts("ICU string comparison tests passed");
}
