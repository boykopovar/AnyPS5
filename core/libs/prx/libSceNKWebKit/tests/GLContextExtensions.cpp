#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GLContext extension check failed at line %u\n", location.line());
        std::abort();
    }
}

using Query = bool (APS5_VABI *)(const char*, const char*);
static_assert(std::is_same_v<decltype(&_ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_), Query>);
constexpr Query IsSupported = &_ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_;

void RequireThrows(const char* list, const char* extension) {
    bool threw = false;
    try {
        IsSupported(list, extension);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    try {
        Require(IsSupported("EXT_x", "EXT_x"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_x"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_a"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_z"));
        Require(!IsSupported("EXT_xy", "EXT_x"));
        Require(!IsSupported("EXT_xy EXT_z", "EXT_x"));
        Require(IsSupported("EXT_xy EXT_x", "EXT_x"));
        Require(IsSupported("PREFIX_EXT_x", "EXT_x"));
        Require(!IsSupported("EXT_X", "EXT_x"));
        Require(!IsSupported("", "EXT_x"));
        Require(!IsSupported("EXT_a EXT_z", "EXT_x"));
        Require(!IsSupported("EXT_x\tEXT_z", "EXT_x"));
        Require(IsSupported("  EXT_x  EXT_z ", "EXT_x"));
        Require(IsSupported("EXT_a EXT_z", "EXT_a EXT_z"));
        Require(!IsSupported(nullptr, "EXT_x"));
        Require(!IsSupported(nullptr, nullptr));
        Require(!IsSupported(nullptr, ""));
        Require(IsSupported("", ""));
        Require(IsSupported(" ", ""));
        Require(IsSupported(" EXT_x", ""));
        RequireThrows("EXT_x", nullptr);
        RequireThrows("", nullptr);
        RequireThrows("EXT_x", "");
        RequireThrows("EXT_x ", "");
        RequireThrows("\tEXT_x", "");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GLContext extension query unexpectedly threw: %s\n", error.what());
        return 1;
    }
}
