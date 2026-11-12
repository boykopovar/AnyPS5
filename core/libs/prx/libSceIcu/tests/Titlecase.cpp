#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t StringNotTerminated = -124;
constexpr std::int32_t UsingDefault = -127;
constexpr std::int32_t CharacterBreaks = 0;
constexpr std::int32_t SentenceBreaks = 3;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Expect(false, what);
}

std::u16string Title(const std::u16string& source, void* iterator, const char* locale) {
    char16_t buffer[64] = {};
    std::int32_t error = Success;
    const auto length = u_strToTitle_nid_postfix(buffer, 64, source.c_str(), -1, iterator, locale, &error);
    Expect(error <= Success && length >= 0 && length < 64 && buffer[length] == 0, "u_strToTitle status");
    return std::u16string(buffer, length);
}

void ExpectTitle(const std::u16string& source, void* iterator, const char* locale, const std::u16string& expected, const char* what) {
    Expect(Title(source, iterator, locale) == expected, what);
}

void* Open(std::int32_t type) {
    std::int32_t error = Success;
    auto* iterator = ubrk_open(type, "", nullptr, 0, &error);
    Expect(iterator && error == Success, "ubrk_open");
    return iterator;
}

void TestWords() {
    ExpectTitle(u"hello wORLD", nullptr, "", u"Hello World", "u_strToTitle(root)");
    ExpectTitle(u"don't stop", nullptr, "en", u"Don't Stop", "u_strToTitle(apostrophe)");
    ExpectTitle(u"ijssel igloo", nullptr, "nl", u"IJssel Igloo", "u_strToTitle(nl)");
    ExpectTitle(u"ijssel", nullptr, "en", u"Ijssel", "u_strToTitle(ij, en)");
    ExpectTitle(u"istanbul", nullptr, "tr", u"İstanbul", "u_strToTitle(tr)");
    ExpectTitle(u"ǆemal", nullptr, "", u"ǅemal", "u_strToTitle(U+01C6)");
    ExpectTitle(u"ßa", nullptr, "", u"Ssa", "u_strToTitle(U+00DF)");
    ExpectTitle(u"", nullptr, "", u"", "u_strToTitle(empty)");

    char16_t buffer[16];
    std::int32_t error = Success;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 16, u"a b", -1, nullptr, "", &error), 3, "u_strToTitle(root) length");
    ExpectInt(error, Success, "u_strToTitle(root) status");
    ExpectInt(u_strToTitle_nid_postfix(buffer, 16, u"a b", -1, nullptr, "en", &error), 3, "u_strToTitle(en) length");
    ExpectInt(error, UsingDefault, "u_strToTitle(en) status");
}

void TestIterator() {
    auto* characters = Open(CharacterBreaks);
    ExpectTitle(u"abc DEF", characters, "", u"ABC DEF", "u_strToTitle(character breaks)");
    ExpectTitle(u"xyz", characters, "", u"XYZ", "u_strToTitle(reused iterator)");
    ubrk_close(characters);
    auto* sentences = Open(SentenceBreaks);
    ExpectTitle(u"one TWO. THREE four.", sentences, "", u"One two. Three four.", "u_strToTitle(sentence breaks)");
    ExpectTitle(u"one TWO. three four.", sentences, "", u"One two. three four.", "u_strToTitle(no break before lowercase)");
    ubrk_close(sentences);
}

void TestBuffers() {
    char16_t buffer[8];
    std::int32_t error = Success;
    ExpectInt(u_strToTitle_nid_postfix(nullptr, 0, u"hello", -1, nullptr, "", &error), 5, "u_strToTitle preflight");
    ExpectInt(error, BufferOverflow, "u_strToTitle preflight status");
    error = Success;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 5, u"hello", -1, nullptr, "", &error), 5, "u_strToTitle exact");
    ExpectInt(error, StringNotTerminated, "u_strToTitle exact status");
    Expect(std::u16string(buffer, 5) == u"Hello", "u_strToTitle exact contents");
    error = Success;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 8, u"hello world", 5, nullptr, "", &error), 5, "u_strToTitle counted");
    Expect(error == Success && std::u16string(buffer, 5) == u"Hello" && buffer[5] == 0, "u_strToTitle counted contents");

    error = Success;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 8, nullptr, -1, nullptr, "", &error), 0, "u_strToTitle null source");
    ExpectInt(error, IllegalArgument, "u_strToTitle null source status");
    error = Success;
    u_strToTitle_nid_postfix(buffer, -1, u"a", -1, nullptr, "", &error);
    ExpectInt(error, IllegalArgument, "u_strToTitle negative capacity");
    error = Success;
    u_strToTitle_nid_postfix(nullptr, 4, u"a", -1, nullptr, "", &error);
    ExpectInt(error, IllegalArgument, "u_strToTitle null destination");
    error = BufferOverflow;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 8, u"a", -1, nullptr, nullptr, &error), 0, "u_strToTitle failed input");
    ExpectInt(error, BufferOverflow, "u_strToTitle failed input status");

    error = Success;
    ExpectInt(u_strToTitle_nid_postfix(buffer, 8, nullptr, -1, nullptr, nullptr, &error), 0, "u_strToTitle default locale, null source");
    ExpectInt(error, IllegalArgument, "u_strToTitle default locale, null source status");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { u_strToTitle_nid_postfix(buffer, 8, u"a", -1, nullptr, nullptr, &error); }, "u_strToTitle default locale");
    ExpectThrow<std::invalid_argument>([&] { u_strToTitle_nid_postfix(buffer, 8, u"a", -1, nullptr, "", nullptr); }, "u_strToTitle null error");
}

}

int main() {
    TestWords();
    TestIterator();
    TestBuffers();
    std::puts("ICU titlecase tests passed");
}
