#include "prx/libSceLibcInternal/include/FloatDigits.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal float digits: %s\n", message.c_str());
    std::abort();
}

struct Case {
    const char* text;
    int skip;
    int maxWords;
    int count;
    std::size_t consumed;
    std::vector<std::int64_t> words;
};

std::u16string Widen(const char* text) {
    std::u16string wide;
    for (; *text != '\0'; ++text) wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(*text)));
    return wide;
}

using Narrow = int (APS5_VABI *)(const char*, const char*, char**, std::int64_t*, int);
using Wide = int (APS5_VABI *)(const char16_t*, const char16_t*, char16_t**, std::int64_t*, int);

void Check(const char* name, Narrow narrow, Wide wide, std::initializer_list<Case> cases) {
    for (const Case& test : cases) {
        const std::string label = std::string(name) + "(\"" + test.text + "\")";
        std::int64_t words[8];
        for (auto& word : words) word = -7;
        char* end = nullptr;
        const int count = narrow(test.text, test.text + test.skip, &end, words, test.maxWords);
        Require(count == test.count, label + " returns the word count");
        Require(static_cast<std::size_t>(end - test.text) == test.consumed, label + " stops at the end of the field");
        for (std::size_t index = 0; index < test.words.size(); ++index) {
            Require(words[index] == test.words[index], label + " word " + std::to_string(index));
        }

        const std::u16string text = Widen(test.text);
        std::int64_t wideWords[8];
        for (auto& word : wideWords) word = -7;
        char16_t* wideEnd = nullptr;
        const int wideCount = wide(text.c_str(), text.c_str() + test.skip, &wideEnd, wideWords, test.maxWords);
        Require(wideCount == count, label + " wide count matches");
        Require(static_cast<std::size_t>(wideEnd - text.c_str()) == test.consumed, label + " wide end matches");
        for (std::size_t index = 0; index < test.words.size(); ++index) {
            Require(wideWords[index] == words[index], label + " wide word " + std::to_string(index));
        }
    }
}

}

int main() {
    Check("_Stoflt", _Stoflt_nid_postfix, _WStoflt_nid_postfix, {
        {"123.456e7", 0, 5, 1, 9, {4, 123456}},
        {"0.000125x", 0, 5, 1, 8, {-6, 125}},
        {"12345678901234567890", 0, 5, 3, 20, {1, 1, 234567890, 123456789}},
        {"1234567895", 0, 1, 1, 10, {2, 12345679}},
        {"999999999.5", 0, 1, 1, 11, {9, 1}},
        {"0.0", 0, 5, 1, 3, {0, 0}},
        {"0.000e5", 0, 5, 1, 7, {0, 0}},
        {"1e1234567890", 0, 5, 1, 12, {999999999, 1}},
        {"3e-0000000000012", 0, 5, 1, 16, {-12, 3}},
        {"12345678901234567890123456789012345678901234567890", 0, 9, 5, 50, {5, 123456789, 12345678, 901234567, 890123456, 789012346}},
        {"1234567894", 0, 1, 1, 10, {1, 123456789}},
        {"000", 0, 5, 1, 3, {0, 0}},
        {"5e+", 0, 5, 1, 1, {0, 5}},
        {"5E-3", 0, 5, 1, 4, {-3, 5}},
        {"1e999999999999", 0, 5, 1, 14, {999999999, 1}},
        {"2500", 0, 5, 1, 4, {2, 25}},
        {"-abc", 1, 5, 0, 0, {0}},
        {"+.", 1, 5, 0, 0, {0}},
        {".5", 0, 5, 1, 2, {-1, 5}},
    });

    Check("_Stoxflt", _Stoxflt_nid_postfix, _WStoxflt_nid_postfix, {
        {"1A.8p3", 0, 5, 1, 6, {-1, 0x1A8}},
        {"123456789ABCDEF", 0, 5, 3, 15, {0, 1, 0x2345678, 0x9ABCDEF}},
        {"ff", 0, 5, 1, 2, {0, 0xFF}},
        {"100", 0, 5, 1, 3, {8, 1}},
        {"0.01P-2", 0, 5, 1, 7, {-10, 1}},
        {"1234567F", 0, 1, 1, 8, {4, 0x1234568}},
        {"FFFFFFF8", 0, 1, 1, 8, {32, 1}},
        {"0.0p9", 0, 5, 1, 5, {0, 0}},
        {"1p", 0, 5, 1, 1, {0, 1}},
        {"0xg", 2, 5, 0, 0, {0}},
    });

    char16_t* end = nullptr;
    int error = -1;
    const std::u16string number = u"1.5e3";
    Require(_WStodx_nid_postfix(number.c_str(), &end, 2, &error) == 150000.0, "_WStodx scales by the power of ten");
    Require(end == number.c_str() + number.size() && error == 0, "_WStodx consumes the field and clears the error");

    const std::u16string accented = u"2.5é";
    Require(_WStodx_nid_postfix(accented.c_str(), &end, 0, nullptr) == 2.5 && end == accented.c_str() + 3, "_WStodx stops at a character outside ASCII");

    error = 0;
    Require(std::isinf(_WStodx_nid_postfix(u"1e400", &end, 0, &error)) && error == 1, "_WStodx reports overflow");
    error = 0;
    Require(_WStodx_nid_postfix(u"1e-400", &end, 0, &error) == 0.0 && error == 1, "_WStodx reports underflow");

    error = -1;
    Require(_WStofx_nid_postfix(u"0.25", &end, 1, &error) == 2.5f && error == 0, "_WStofx scales a float");
    error = 0;
    Require(std::isinf(_WStofx_nid_postfix(u"1e39", &end, 0, &error)) && error == 1, "_WStofx reports overflow");

    error = -1;
    Require(_WStoldx_nid_postfix(u"7e-1", &end, 1, &error) == 7.0L && error == 0, "_WStoldx scales a long double");
    error = 0;
    Require(std::isinf(_WStoldx_nid_postfix(u"1e5000", &end, 0, &error)) && error == 1, "_WStoldx reports overflow");

    const std::u16string empty = u"x";
    Require(_WStodx_nid_postfix(empty.c_str(), &end, 0, nullptr) == 0.0 && end == empty.c_str(), "_WStodx leaves a non-number unread");

    std::puts("libc internal float digit checks passed");
    return 0;
}
