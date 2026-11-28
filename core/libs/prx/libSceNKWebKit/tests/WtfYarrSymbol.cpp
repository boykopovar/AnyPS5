#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <string>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Yarr errorMessage / SymbolImpl check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

using ErrorCode = NKWebKit::YarrAbi::ErrorCode;
using ErrorMessage = NKWebKit::HttpAbi::ASCIILiteral (APS5_VABI *)(ErrorCode);
using NextHashForSymbol = unsigned (APS5_VABI *)();
static_assert(std::is_same_v<decltype(&_ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE), ErrorMessage>);
static_assert(std::is_same_v<decltype(&_ZN3WTF10SymbolImpl17nextHashForSymbolEv), NextHashForSymbol>);

constexpr std::array<const char*, 31> Reasons{
    "regular expression too large",
    "numbers out of order in {} quantifier",
    "nothing to repeat",
    "number too large in {} quantifier",
    "incomplete {} quantifier for Unicode pattern",
    "invalid quantifier",
    "missing )",
    "unmatched ] or } bracket for Unicode pattern",
    "unmatched parentheses",
    "unrecognized character after (?",
    "invalid group specifier name",
    "duplicate group specifier name",
    "missing terminating ] for character class",
    "range out of order in character class",
    "invalid range in character class for Unicode pattern",
    "missing terminating } for class string disjunction",
    "\\ at end of pattern",
    "invalid Unicode \\u escape",
    "invalid Unicode code point \\u{} escape",
    "invalid backreference for Unicode pattern",
    "invalid \\k<> named backreference",
    "invalid escaped character for Unicode pattern",
    "invalid octal escape for Unicode pattern",
    "invalid \\c escape for Unicode pattern",
    "invalid property expression",
    "too many nested disjunctions",
    "pattern exceeds string length limits",
    "invalid flags",
    "invalid operation in class set",
    "negated class set may contain strings",
    "invalid class set character",
};

std::string Message(unsigned code) {
    NKWebKit::HttpAbi::ASCIILiteral literal = _ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(static_cast<ErrorCode>(code));
    Require(literal.characters != nullptr);
    return literal.characters;
}

void TestNoError() {
    Require(_ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(ErrorCode::NoError).characters == nullptr);
    std::printf("Yarr errorMessage: NoError returns a null literal\n");
}

void TestMessages() {
    for (unsigned code = 1; code <= Reasons.size(); ++code) {
        Require(Message(code) == std::string("Invalid regular expression: ") + Reasons[code - 1]);
    }
    Require(Message(1) == "Invalid regular expression: regular expression too large");
    Require(Message(3) == "Invalid regular expression: nothing to repeat");
    Require(Message(static_cast<unsigned>(ErrorCode::TooManyDisjunctions)) == "Invalid regular expression: too many nested disjunctions");
    Require(Message(static_cast<unsigned>(ErrorCode::InvalidClassSetCharacter)) == "Invalid regular expression: invalid class set character");
    Require(Message(17) == std::string("Invalid regular expression: ") + static_cast<char>(0x5c) + " at end of pattern");
    Require(_ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(ErrorCode::PatternTooLarge).characters == _ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(ErrorCode::PatternTooLarge).characters);
    std::printf("Yarr errorMessage: codes 1 to 31 match the pinned table\n");
}

void TestOutOfRange() {
    RequireThrows([] { _ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(static_cast<ErrorCode>(32)); });
    RequireThrows([] { _ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(static_cast<ErrorCode>(255)); });
    std::printf("Yarr errorMessage: codes 32 and 255 throw\n");
}

void TestNextHashForSymbol() {
    unsigned previous = _ZN3WTF10SymbolImpl17nextHashForSymbolEv();
    Require((previous & 0x80000000u) != 0);
    Require((previous & 0xffu) == 0);
    for (int i = 0; i < 1000; ++i) {
        unsigned next = _ZN3WTF10SymbolImpl17nextHashForSymbolEv();
        Require((next & 0x80000000u) != 0);
        Require((next & 0xffu) == 0);
        Require(((next - previous) & 0x7fffffffu) == 256u);
        previous = next;
    }
    std::printf("SymbolImpl nextHashForSymbol: bit 31 set, flag bits clear, steps of 256\n");
}

void TestFirstSymbolHash() {
    unsigned first = _ZN3WTF10SymbolImpl17nextHashForSymbolEv();
    Require(first == (0x80000000u | 256u));
    std::printf("SymbolImpl nextHashForSymbol: first call returns 0x80000100\n");
}

}

int main() {
    TestFirstSymbolHash();
    TestNoError();
    TestMessages();
    TestOutOfRange();
    TestNextHashForSymbol();
    std::printf("Yarr errorMessage and SymbolImpl nextHashForSymbol tests passed\n");
    return 0;
}
