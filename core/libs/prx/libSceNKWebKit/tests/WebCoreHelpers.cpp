#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string_view>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore helpers check failed at line %u\n", location.line());
        std::abort();
    }
}

using String = NKWebKit::WtfAbi::String;
using AtomString = NKWebKit::WtfAbi::AtomString;
using FloatRect = WebCore::FloatRect;
using FloatRectVector = NKWebKit::WtfAbi::Vector<FloatRect>;

static_assert(std::is_same_v<decltype(&_ZN7WebCore9unionRectERKN3WTF6VectorINS_9FloatRectELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE), FloatRect (APS5_VABI *)(const FloatRectVector&)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_), unsigned (APS5_VABI *)(const FloatRect&, const FloatRect&)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore8SVGTests27hasFeatureForLegacyBindingsERKN3WTF6StringES4_), bool (APS5_VABI *)(const String&, const String&)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE), WebCore::AutofillFieldName (APS5_VABI *)(const AtomString&)>);

FloatRect Rect(float x, float y, float width, float height) {
    return FloatRect{{x, y}, {width, height}};
}

bool Same(const FloatRect& a, const FloatRect& b) {
    return a.x() == b.x() && a.y() == b.y() && a.width() == b.width() && a.height() == b.height();
}

String Make8(std::string_view text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

String Make16(std::string_view text) {
    char16_t buffer[128];
    for (std::size_t i = 0; i < text.size(); ++i) buffer[i] = static_cast<unsigned char>(text[i]);
    String result;
    _ZN3WTF6StringC1EPKDsj(&result, buffer, static_cast<unsigned>(text.size()));
    Require(text.empty() || (result.impl && !(result.impl->hashAndFlags & 4)));
    return result;
}

String Make16(const char16_t* characters, unsigned length) {
    String result;
    _ZN3WTF6StringC1EPKDsj(&result, characters, length);
    Require(result.impl && !(result.impl->hashAndFlags & 4));
    return result;
}

bool HasFeature(const String& feature, const String& version) {
    return _ZN7WebCore8SVGTests27hasFeatureForLegacyBindingsERKN3WTF6StringES4_(feature, version);
}

int Autofill(const String& value) {
    AtomString atom{value};
    return static_cast<int>(_ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(atom));
}

unsigned Edges(const FloatRect& total, const FloatRect& tile) {
    return _ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_(total, tile);
}

FloatRect Union(const FloatRectVector& rects) {
    return _ZN7WebCore9unionRectERKN3WTF6VectorINS_9FloatRectELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(rects);
}

void UnionRect() {
    FloatRectVector rects;
    Require(Same(Union(rects), Rect(0, 0, 0, 0)));
    rects.append(Rect(5, 5, 0, 10));
    Require(Same(Union(rects), Rect(0, 0, 0, 0)));
    rects.append(Rect(10, 20, 30, 40));
    Require(Same(Union(rects), Rect(10, 20, 30, 40)));
    rects.append(Rect(-5, 100, 10, 5));
    Require(Same(Union(rects), Rect(-5, 20, 45, 85)));
}

void ExposedTileEdges() {
    FloatRect total = Rect(0, 0, 512, 256);
    Require(Edges(total, Rect(0, 0, 256, 128)) == 5);
    Require(Edges(total, Rect(256, 128, 256, 128)) == 10);
    Require(Edges(total, Rect(128, 64, 128, 64)) == 0);
    Require(Edges(total, Rect(0, 0, 512, 256)) == 15);
    Require(Edges(total, Rect(300, 10, 300, 10)) == 2);
}

void SVGFeatures() {
    for (String (*make)(std::string_view) : {&Make8, static_cast<String (*)(std::string_view)>(&Make16)}) {
        Require(HasFeature(make("foo"), String{}));
        Require(HasFeature(make("foo"), make("2.0")));
        Require(HasFeature(make("org.w3c.svg"), String{}));
        Require(HasFeature(make("org.w3c.svg"), make("")));
        Require(HasFeature(make("ORG.W3C.SVG.static"), make("1.0")));
        Require(!HasFeature(make("org.w3c.svg"), make("1.1")));
        Require(HasFeature(make("http://www.w3.org/TR/SVG11/feature#shape"), make("1.1")));
        Require(!HasFeature(make("http://www.w3.org/TR/SVG11/feature#shape"), make("1.0")));
        Require(!HasFeature(make("http://www.w3.org/TR/SVG11/feature#bogus"), String{}));
        Require(!HasFeature(make("org.w3c.svg.bogus"), make("")));
        Require(HasFeature(make("org.w3c.dom"), make("1.0")));
        Require(!HasFeature(make("org.w3c.svg.static"), make(" 1.0")));
        Require(!HasFeature(make("org.w3c.svg.static"), make("1.0 ")));
    }
    const char16_t nonAscii[] = {u'o', u'r', u'g', u'.', u'w', u'3', u'c', u'.', u's', u'v', 0x0147};
    Require(HasFeature(Make16(nonAscii, 11), Make8("1.1")));
}

void AutofillFieldNames() {
    for (String (*make)(std::string_view) : {&Make8, static_cast<String (*)(std::string_view)>(&Make16)}) {
        Require(Autofill(make("email")) == 52);
        Require(Autofill(make("EMAIL")) == 52);
        Require(Autofill(make("off")) == 0);
        Require(Autofill(make("on")) == 0);
        Require(Autofill(make("unknown")) == 0);
        Require(Autofill(make("webauthn")) == 54);
        Require(Autofill(make("additional-name")) == 4);
        Require(Autofill(make("Tel-Local-Suffix")) == 50);
        Require(Autofill(make("cc-exp-month")) == 30);
        Require(Autofill(make("cc-exp")) == 29);
        Require(Autofill(make("cc-ex")) == 0);
        Require(Autofill(make("url")) == 42);
        Require(Autofill(make("emai")) == 0);
        Require(Autofill(make("emails")) == 0);
        Require(Autofill(make("")) == 0);
    }
    Require(Autofill(String{}) == 0);
    const char16_t nonAscii[] = {u'e', u'm', u'a', u'i', 0x016C};
    Require(Autofill(Make16(nonAscii, 5)) == 0);
}

}

int main() {
    UnionRect();
    ExposedTileEdges();
    SVGFeatures();
    AutofillFieldNames();
    std::printf("WebCore helpers: unionRect, exposed tile edges, SVG legacy features and autofill field names match WebCore\n");
}
