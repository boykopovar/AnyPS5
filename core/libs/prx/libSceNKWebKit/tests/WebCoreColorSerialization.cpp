#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore color serialization check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(value) Require((value), __LINE__)

std::string Text(const NKWebKit::WtfAbi::String& value) {
    REQUIRE(value.impl != nullptr);
    REQUIRE((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

WebCore::Color Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    constexpr std::uint64_t colorSpaceSrgb = 17ULL << 56;
    constexpr std::uint64_t valid = 1ULL << 50;
    return WebCore::Color{colorSpaceSrgb | valid | (static_cast<std::uint64_t>(red) << 24)
                          | (static_cast<std::uint64_t>(green) << 16)
                          | (static_cast<std::uint64_t>(blue) << 8) | alpha};
}

} // namespace

int main() {
    const auto opaque = Color(12, 34, 56, 255);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&opaque)) == "#0c2238");

    const auto transparent = Color(12, 34, 56, 0);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&transparent)) == "rgba(12, 34, 56, 0)");

    const auto translucent = Color(12, 34, 56, 128);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&translucent)) == "rgba(12, 34, 56, 0.5)");

    const WebCore::Color invalid{0};
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&invalid)) == "rgba(0, 0, 0, 0)");
    return 0;
}
