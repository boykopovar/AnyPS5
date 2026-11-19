#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/BackForwardItemIdentifierExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::BackForwardItemIdentifier;

constexpr std::uint64_t kDeleted = std::numeric_limits<std::uint64_t>::max();

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "BackForwardItemIdentifier check failed at line %u\n", location.line());
        std::abort();
    }
}

bool IsValid(std::uint64_t processIdentifier, std::uint64_t itemIdentifier) {
    BackForwardItemIdentifier identifier{processIdentifier, itemIdentifier};
    return _ZNK7WebCore25BackForwardItemIdentifier7isValidEv(&identifier);
}

void TestEmptyIsInvalid() {
    Check(!IsValid(0, 0));
}

void TestDeletedProcessIsInvalid() {
    Check(!IsValid(kDeleted, 0));
    Check(!IsValid(kDeleted, 1));
    Check(!IsValid(kDeleted, kDeleted));
}

void TestAnyOtherPairIsValid() {
    Check(IsValid(1, 1));
    Check(IsValid(7, 42));
    Check(IsValid(1, 0));
    Check(IsValid(0, 1));
    Check(IsValid(0, kDeleted));
    Check(IsValid(1, kDeleted));
    Check(IsValid(kDeleted - 1, 5));
}

void TestReadsThroughThePointer() {
    BackForwardItemIdentifier identifiers[2] = {{0, 0}, {3, 9}};
    Check(!_ZNK7WebCore25BackForwardItemIdentifier7isValidEv(&identifiers[0]));
    Check(_ZNK7WebCore25BackForwardItemIdentifier7isValidEv(&identifiers[1]));
}

}  // namespace

int main() {
    TestEmptyIsInvalid();
    TestDeletedProcessIsInvalid();
    TestAnyOtherPairIsValid();
    TestReadsThroughThePointer();
    std::puts("BackForwardItemIdentifier tests passed");
    return 0;
}
