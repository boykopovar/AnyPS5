#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF::Logger check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using LogArgumentToString = NKWebKit::WtfAbi::String (APS5_VABI *)(const void*);
using LogSiteIdentifierToString = NKWebKit::WtfAbi::String (APS5_VABI *)(const NKWebKit::WtfAbi::LogSiteIdentifier*);
static_assert(std::is_same_v<decltype(&_ZN3WTF11LogArgumentIPKvE8toStringES2_), LogArgumentToString>);
static_assert(std::is_same_v<decltype(&_ZNK3WTF6Logger17LogSiteIdentifier8toStringEv), LogSiteIdentifierToString>);

void RequireText(const NKWebKit::WtfAbi::String& value, const char* expected) {
    Require(value.impl != nullptr);
    Require((value.impl->hashAndFlags & 4) != 0);
    const std::size_t length = std::strlen(expected);
    Require(value.impl->length == length);
    Require(std::memcmp(value.impl->data, expected, length) == 0);
}

void TestLogArgument() {
    RequireText(_ZN3WTF11LogArgumentIPKvE8toStringES2_(nullptr), "(0)");
    RequireText(_ZN3WTF11LogArgumentIPKvE8toStringES2_(reinterpret_cast<const void*>(0x10)), "(10)");
    RequireText(_ZN3WTF11LogArgumentIPKvE8toStringES2_(reinterpret_cast<const void*>(0xABC)), "(ABC)");
    RequireText(_ZN3WTF11LogArgumentIPKvE8toStringES2_(reinterpret_cast<const void*>(~std::uintptr_t{0})), "(FFFFFFFFFFFFFFFF)");
    std::printf("WTF::LogArgument<void const*>::toString: hex forms passed\n");
}

void TestLogSiteIdentifier() {
    const NKWebKit::WtfAbi::LogSiteIdentifier named{"Cls", "run", 0x1F};
    const NKWebKit::WtfAbi::LogSiteIdentifier unnamed{nullptr, "run", 0x1F};
    const NKWebKit::WtfAbi::LogSiteIdentifier emptyClass{"", "run", 0};
    const NKWebKit::WtfAbi::LogSiteIdentifier highByte{"caf\xE9", "m", 1};
    const NKWebKit::WtfAbi::LogSiteIdentifier nullMethod{"Cls", nullptr, 1};
    RequireText(_ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(&named), "Cls::run(1F) ");
    RequireText(_ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(&unnamed), "run(1F) ");
    RequireText(_ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(&emptyClass), "::run(0) ");
    RequireText(_ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(&highByte), "caf\xE9::m(1) ");
    RequireThrows([&] { _ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(&nullMethod); });
    RequireThrows([] { _ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(nullptr); });
    std::printf("WTF::Logger::LogSiteIdentifier::toString: names and hex passed\n");
}

}

int main() {
    TestLogArgument();
    TestLogSiteIdentifier();
}
