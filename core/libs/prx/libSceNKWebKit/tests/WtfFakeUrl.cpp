#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::URL::fakeURLWithRelativePart check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::string Text(const URL& url) {
    std::string text;
    if (!url.impl) return text;
    const bool is8Bit = url.impl->hashAndFlags & 4;
    for (unsigned i = 0; i < url.impl->length; ++i) {
        const char16_t unit = is8Bit ? static_cast<const unsigned char*>(url.impl->data)[i] : static_cast<const char16_t*>(url.impl->data)[i];
        Require(unit < 0x80);
        text.push_back(static_cast<char>(unit));
    }
    return text;
}

URL Parse(const std::string& text) {
    const URL base{};
    const String input = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

bool SameFields(const URL& a, const URL& b) {
    return Text(a) == Text(b) && a.bits == b.bits && a.userStart == b.userStart && a.userEnd == b.userEnd && a.passwordEnd == b.passwordEnd
        && a.hostEnd == b.hostEnd && a.pathAfterLastSlash == b.pathAfterLastSlash && a.pathEnd == b.pathEnd && a.queryEnd == b.queryEnd;
}

std::string RequireFake(const URL& url, const std::string& suffix) {
    const std::string text = Text(url);
    const std::string prefix = "webkit-fake-url://";
    Require(url.bits & 1);
    Require(text.size() == prefix.size() + 36 + suffix.size());
    Require(text.compare(0, prefix.size(), prefix) == 0);
    const std::string uuid = text.substr(prefix.size(), 36);
    for (std::size_t i = 0; i < uuid.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) Require(uuid[i] == '-');
        else Require((uuid[i] >= '0' && uuid[i] <= '9') || (uuid[i] >= 'a' && uuid[i] <= 'f'));
    }
    Require(uuid[14] == '4');
    Require(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b');
    Require(text.substr(prefix.size() + 36) == suffix);
    Require(url.hostEnd == prefix.size() + 36);
    Require(SameFields(url, Parse(text)));
    return uuid;
}

StringView View(const char* text) {
    return {text, static_cast<unsigned>(std::char_traits<char>::length(text)), true};
}

}  // namespace

int main() {
    std::set<std::string> uuids;
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(View("index.html")), "/index.html"));
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(View("a/b/../c?q=1#frag")), "/a/c?q=1#frag"));
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(View("")), "/"));
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(StringView{nullptr, 0, true}), "/"));
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(View("sp ace")), "/sp%20ace"));

    const std::u16string wide = u"café/\U0001F600";
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(StringView{wide.data(), static_cast<unsigned>(wide.size()), false}),
        "/caf%C3%A9/%F0%9F%98%80"));
    const unsigned char latin1[] = {'n', 0xE9};
    uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(StringView{latin1, 2, true}), "/n%C3%A9"));
    for (int i = 0; i < 64; ++i) uuids.insert(RequireFake(_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(View("x")), "/x"));
    Require(uuids.size() == 71);

    bool threw = false;
    try {
        _ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE(StringView{nullptr, 3, true});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::puts("libSceNKWebKit WTF::URL::fakeURLWithRelativePart passed");
    return 0;
}
