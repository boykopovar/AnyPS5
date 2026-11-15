#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU substring search check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::ptrdiff_t None = -1;

std::ptrdiff_t First(const std::u16string& text, const std::u16string& sub) {
    const auto* found = u_strstr_nid_postfix(text.c_str(), sub.c_str());
    return found ? found - text.c_str() : None;
}

std::ptrdiff_t Last(const std::u16string& text, const std::u16string& sub) {
    const auto* found = u_strrstr_nid_postfix(text.c_str(), sub.c_str());
    return found ? found - text.c_str() : None;
}

void Check(const std::u16string& text, const std::u16string& sub, std::ptrdiff_t first, std::ptrdiff_t last,
           std::source_location location = std::source_location::current()) {
    if (First(text, sub) == first && Last(text, sub) == last) return;
    std::fprintf(stderr, "ICU substring search check failed at line %u\n", location.line());
    std::abort();
}

void Plain() {
    Check(u"abcabc", u"bc", 1, 4);
    Check(u"abcabc", u"abc", 0, 3);
    Check(u"abcabc", u"c", 2, 5);
    Check(u"abcabc", u"abcabc", 0, 0);
    Check(u"abcabc", u"abcabcd", None, None);
    Check(u"abcabc", u"x", None, None);
    Check(u"abcabc", u"cb", None, None);
    Check(u"aaa", u"aa", 0, 1);
    Check(u"aaa", u"a", 0, 2);
    Check(u"ab", u"abc", None, None);
    Check(u"", u"a", None, None);
    Check(u"abc", u"", 0, 0);
    Check(u"", u"", 0, 0);
    Check(std::u16string({0x41, 0, 0x42}), u"B", None, None);
    Check(std::u16string({0x41, 0x42, 0x43}), std::u16string({0x42, 0x43}), 1, 1);

    const char16_t text[] = u"xyz";
    Require(u_strstr_nid_postfix(text, nullptr) == text);
    Require(u_strrstr_nid_postfix(text, nullptr) == text);
    Require(u_strstr_nid_postfix(nullptr, nullptr) == nullptr);
    Require(u_strrstr_nid_postfix(nullptr, nullptr) == nullptr);
    Require(u_strstr_nid_postfix(nullptr, text) == nullptr);
    Require(u_strrstr_nid_postfix(nullptr, text) == nullptr);
}

void Surrogates() {
    const std::u16string pair({0xd83d, 0xde00});
    Check(std::u16string({0x41, 0xd83d, 0xde00, 0x42}), pair, 1, 1);
    Check(std::u16string({0xd83d, 0xde00, 0xd83d, 0xde00}), pair, 0, 2);

    Check(std::u16string({0xd800, 0xdc00}), std::u16string({0xdc00}), None, None);
    Check(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800}), None, None);
    Check(std::u16string({0xd800, 0xdc00, 0xdc00}), std::u16string({0xdc00}), 2, 2);
    Check(std::u16string({0xdc00, 0xdc00, 0xd800, 0xdc00}), std::u16string({0xdc00}), 0, 1);
    Check(std::u16string({0xd800, 0xdc00, 0xd800}), std::u16string({0xd800}), 2, 2);
    Check(std::u16string({0xd800, 0xd800, 0xdc00}), std::u16string({0xd800}), 0, 0);
    Check(std::u16string({0x41, 0xd800, 0x42}), std::u16string({0x41, 0xd800}), 0, 0);
    Check(std::u16string({0x41, 0xd800, 0xdc00}), std::u16string({0x41, 0xd800}), None, None);
    Check(std::u16string({0xd800, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), None, None);
    Check(std::u16string({0xdc00, 0x41, 0xd800, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), 0, 0);
    Check(std::u16string({0xd800, 0xdc00, 0x41, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), 3, 3);
    Check(std::u16string({0xd800, 0xdc00, 0x41, 0xdc00, 0x41}), std::u16string({0xd800, 0xdc00, 0x41}), 0, 0);
    Check(std::u16string({0xdc00, 0xd800}), std::u16string({0xdc00}), 0, 0);
}

}

int main() {
    Plain();
    Surrogates();
    std::puts("ICU substring search tests passed");
}
