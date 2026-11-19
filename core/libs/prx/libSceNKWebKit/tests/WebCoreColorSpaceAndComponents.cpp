#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Color::colorSpaceAndComponents check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

WebCore::ColorSpaceAndComponents Components(const WebCore::Color& color) {
    WebCore::ColorSpaceAndComponents result{0xff, {-1.0f, -1.0f, -1.0f, -1.0f}};
    Require(_ZNK7WebCore5Color23colorSpaceAndComponentsEv(&result, &color) == &result);
    return result;
}

void RequireComponents(const WebCore::ColorSpaceAndComponents& value, std::uint8_t colorSpace, float red, float green, float blue, float alpha, int line) {
    Check(value.colorSpace == colorSpace, line);
    Check(value.components[0] == red, line);
    Check(value.components[1] == green, line);
    Check(value.components[2] == blue, line);
    Check(value.components[3] == alpha, line);
}

bool Throws(WebCore::ColorSpaceAndComponents* result, const WebCore::Color* color) {
    try {
        _ZNK7WebCore5Color23colorSpaceAndComponentsEv(result, color);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestInlineColors() {
    RequireComponents(Components(WebCore::Color{(std::uint64_t{0x11ff8000} << 32) | 0x3}), 0, 0x11 / 255.0f, 1.0f, 0x80 / 255.0f, 0.0f, __LINE__);
    RequireComponents(Components(WebCore::Color{(std::uint64_t{0x000000ff} << 32) | 0x7}), 0, 0.0f, 0.0f, 0.0f, 1.0f, __LINE__);
    RequireComponents(Components(WebCore::Color{0x1}), 0, 0.0f, 0.0f, 0.0f, 0.0f, __LINE__);
}

void TestExtendedColors() {
    WebCore::ExtendedColor extended{1, {0.25f, -0.5f, 1.5f, 0.75f}, 2};
    RequireComponents(Components(WebCore::Color{reinterpret_cast<std::uint64_t>(&extended)}), 2, 0.25f, -0.5f, 1.5f, 0.75f, __LINE__);
    Require(extended.m_refCount == 1);
}

void TestUnreadableColors() {
    WebCore::ColorSpaceAndComponents result{};
    const WebCore::Color nullExtended{0};
    Require(Throws(&result, nullptr));
    Require(Throws(&result, &nullExtended));
}

void TestUnwritableResult() {
    const WebCore::Color color{(std::uint64_t{0x11223344} << 32) | 0x3};
    Require(Throws(nullptr, &color));
}

}  // namespace

int main() {
    TestInlineColors();
    TestExtendedColors();
    TestUnreadableColors();
    TestUnwritableResult();
    return 0;
}
