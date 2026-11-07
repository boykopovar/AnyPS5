#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::Process check failed at line %u\n", location.line());
        std::abort();
    }
}

using Identifier = std::uint64_t (APS5_VABI *)();
using SetIdentifier = void (APS5_VABI *)(std::uint64_t);
static_assert(std::is_same_v<decltype(&_ZN7WebCore7Process10identifierEv), Identifier>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore7Process13setIdentifierEN3WTF23ObjectIdentifierGenericINS_21ProcessIdentifierTypeENS1_38ObjectIdentifierMainThreadAccessTraitsEEE), SetIdentifier>);

void SetIdentifierTo(std::uint64_t identifier) {
    _ZN7WebCore7Process13setIdentifierEN3WTF23ObjectIdentifierGenericINS_21ProcessIdentifierTypeENS1_38ObjectIdentifierMainThreadAccessTraitsEEE(identifier);
}

}

int main() {
    SetIdentifierTo(0x123456789abcdef0);
    Require(_ZN7WebCore7Process10identifierEv() == 0x123456789abcdef0);
    Require(_ZN7WebCore7Process10identifierEv() == 0x123456789abcdef0);
    SetIdentifierTo(7);
    Require(_ZN7WebCore7Process10identifierEv() == 7);
    std::printf("WebCore::Process: an identifier set before the first query is kept and a later set replaces it\n");
}
