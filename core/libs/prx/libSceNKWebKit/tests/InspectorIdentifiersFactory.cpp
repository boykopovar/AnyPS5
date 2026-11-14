#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Inspector::IdentifiersFactory check failed at line %u\n", location.line());
        std::abort();
    }
}

void RequireString(const NKWebKit::WtfAbi::String& value, const char* expected) {
    const auto length = static_cast<unsigned>(std::strlen(expected));
    Require(value.impl != nullptr);
    Require(value.impl->length == length);
    Require((value.impl->hashAndFlags & 4) != 0);
    Require(std::memcmp(value.impl->data, expected, length) == 0);
}

using CreateIdentifier = NKWebKit::WtfAbi::String (APS5_VABI *)();
using RequestId = NKWebKit::WtfAbi::String (APS5_VABI *)(std::uint64_t);
static_assert(std::is_same_v<decltype(&_ZN9Inspector18IdentifiersFactory16createIdentifierEv), CreateIdentifier>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector18IdentifiersFactory9requestIdEm), RequestId>);

void TestCreateIdentifier() {
    RequireString(_ZN9Inspector18IdentifiersFactory16createIdentifierEv(), "0.1");
    RequireString(_ZN9Inspector18IdentifiersFactory16createIdentifierEv(), "0.2");
    std::printf("Inspector::IdentifiersFactory::createIdentifier: first identifiers passed\n");
}

void TestRequestId() {
    const auto zero = _ZN9Inspector18IdentifiersFactory9requestIdEm(0);
    Require(zero.impl == nullptr);
    RequireString(_ZN9Inspector18IdentifiersFactory9requestIdEm(42), "0.42");
    RequireString(_ZN9Inspector18IdentifiersFactory9requestIdEm(1), "0.1");
    RequireString(_ZN9Inspector18IdentifiersFactory9requestIdEm(18446744073709551615ULL),
        "0.18446744073709551615");
    std::printf("Inspector::IdentifiersFactory::requestId: zero, decimal and largest value passed\n");
}

void TestRequestIdDoesNotAdvance() {
    RequireString(_ZN9Inspector18IdentifiersFactory16createIdentifierEv(), "0.3");
    std::printf("Inspector::IdentifiersFactory: requestId leaves the counter alone\n");
}

}

int main() {
    TestCreateIdentifier();
    TestRequestId();
    TestRequestIdDoesNotAdvance();
}
