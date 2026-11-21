#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::IdentifierRep;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::IdentifierRep check failed at line %u\n", location.line());
        std::abort();
    }
}

static_assert(std::is_same_v<decltype(&_ZN7WebCore13IdentifierRep3getEi), IdentifierRep* (APS5_VABI *)(int)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore13IdentifierRep3getEPKc), IdentifierRep* (APS5_VABI *)(const char*)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore13IdentifierRep7isValidEPS0_), bool (APS5_VABI *)(IdentifierRep*)>);
static_assert(sizeof(IdentifierRep) == 16);
static_assert(offsetof(IdentifierRep, value) == 0);
static_assert(offsetof(IdentifierRep, isString) == 8);

void TestNumbers() {
    auto* five = _ZN7WebCore13IdentifierRep3getEi(5);
    Require(five != nullptr);
    Require(_ZN7WebCore13IdentifierRep3getEi(5) == five);
    Require(_ZN7WebCore13IdentifierRep3getEi(6) != five);
    Require(!five->isString);
    Require(five->value.number == 5);
    Require(_ZN7WebCore13IdentifierRep7isValidEPS0_(five));
    auto* negative = _ZN7WebCore13IdentifierRep3getEi(-7);
    Require(negative->value.number == -7);
    Require(_ZN7WebCore13IdentifierRep7isValidEPS0_(negative));
    std::printf("IdentifierRep numbers: interned and valid\n");
}

void TestZeroAndNegativeOne() {
    auto* zero = _ZN7WebCore13IdentifierRep3getEi(0);
    auto* negativeOne = _ZN7WebCore13IdentifierRep3getEi(-1);
    Require(zero != nullptr && negativeOne != nullptr && zero != negativeOne);
    Require(_ZN7WebCore13IdentifierRep3getEi(0) == zero);
    Require(_ZN7WebCore13IdentifierRep3getEi(-1) == negativeOne);
    Require(!zero->isString && zero->value.number == 0);
    Require(!negativeOne->isString && negativeOne->value.number == -1);
    Require(!_ZN7WebCore13IdentifierRep7isValidEPS0_(zero));
    Require(!_ZN7WebCore13IdentifierRep7isValidEPS0_(negativeOne));
    std::printf("IdentifierRep 0 and -1: stable singletons outside the valid set\n");
}

void TestInvalid() {
    IdentifierRep local{};
    Require(!_ZN7WebCore13IdentifierRep7isValidEPS0_(nullptr));
    Require(!_ZN7WebCore13IdentifierRep7isValidEPS0_(&local));
    Require(_ZN7WebCore13IdentifierRep3getEPKc(nullptr) == nullptr);
    std::printf("IdentifierRep null and foreign pointers: rejected\n");
}

void TestStrings() {
    char name[] = "foo";
    auto* foo = _ZN7WebCore13IdentifierRep3getEPKc(name);
    Require(foo != nullptr);
    Require(foo->isString);
    Require(foo->value.string != name);
    Require(std::strcmp(foo->value.string, "foo") == 0);
    Require(_ZN7WebCore13IdentifierRep3getEPKc("foo") == foo);
    Require(_ZN7WebCore13IdentifierRep3getEPKc("bar") != foo);
    Require(_ZN7WebCore13IdentifierRep3getEi(5) != foo);
    Require(_ZN7WebCore13IdentifierRep7isValidEPS0_(foo));
    auto* empty = _ZN7WebCore13IdentifierRep3getEPKc("");
    Require(empty != nullptr && empty->isString && empty->value.string[0] == '\0');
    Require(_ZN7WebCore13IdentifierRep3getEPKc("") == empty);
    std::printf("IdentifierRep strings: interned copies separate from numbers\n");
}

void TestDecodedKey() {
    auto* latin1 = _ZN7WebCore13IdentifierRep3getEPKc("\xE9");
    auto* utf8 = _ZN7WebCore13IdentifierRep3getEPKc("\xC3\xA9");
    Require(latin1 == utf8);
    Require(std::strcmp(latin1->value.string, "\xE9") == 0);
    auto* wide = _ZN7WebCore13IdentifierRep3getEPKc("\xE2\x82\xAC");
    Require(wide != latin1);
    Require(_ZN7WebCore13IdentifierRep3getEPKc("\xE2\x82\xAC") == wide);
    std::printf("IdentifierRep string keys: decoded text, first caller's bytes\n");
}

}

int main() {
    TestNumbers();
    TestZeroAndNegativeOne();
    TestInvalid();
    TestStrings();
    TestDecodedKey();
    return 0;
}
