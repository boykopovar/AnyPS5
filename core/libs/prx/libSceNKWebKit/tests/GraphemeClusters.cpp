#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::StringView;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "grapheme cluster check failed at line %u\n", location.line());
        std::abort();
    }
}

unsigned Clusters(StringView string) {
    return _ZN3WTF19numGraphemeClustersENS_10StringViewE(string);
}

unsigned CodeUnits(StringView string, unsigned clusters) {
    return _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(string, clusters);
}

void Both(std::string_view text, unsigned clusters) {
    const StringView narrow {text.data(), static_cast<unsigned>(text.size()), true};
    std::u16string wideText;
    for (char character : text) wideText.push_back(static_cast<unsigned char>(character));
    const StringView wide {wideText.data(), static_cast<unsigned>(wideText.size()), false};
    Require(Clusters(narrow) == clusters);
    Require(Clusters(wide) == clusters);
}

void BothUnits(std::string_view text, unsigned clusters, unsigned units) {
    const StringView narrow {text.data(), static_cast<unsigned>(text.size()), true};
    std::u16string wideText;
    for (char character : text) wideText.push_back(static_cast<unsigned char>(character));
    const StringView wide {wideText.data(), static_cast<unsigned>(wideText.size()), false};
    Require(CodeUnits(narrow, clusters) == units);
    Require(CodeUnits(wide, clusters) == units);
}

void Wide(std::u16string_view text, unsigned clusters, unsigned units) {
    const StringView string {text.data(), static_cast<unsigned>(text.size()), false};
    Require(Clusters(string) == clusters);
    Require(CodeUnits(string, clusters) == units);
}

template<typename Function> bool Throws(Function function, std::string_view message) {
    try {
        function();
    } catch (const std::exception& error) {
        return std::string_view(error.what()).find(message) != std::string_view::npos;
    }
    return false;
}

void Run() {
    Both("", 0);
    Both("a", 1);
    Both("abc", 3);
    Both("\r\n", 1);
    Both("a\r\nb", 3);
    Both("\r\r\n", 2);
    Both("\n\r", 2);
    Both("\r\n\r\n", 2);
    Both("\r", 1);
    Both("\n", 1);
    Both(std::string_view("a\0b", 3), 3);
    Both("\xe9\xff\x80\xad", 4);
    Both("\r\n\xe9\r\n", 3);

    BothUnits("abc", 0, 0);
    BothUnits("abc", 1, 1);
    BothUnits("abc", 2, 2);
    BothUnits("abc", 3, 3);
    BothUnits("abc", 9, 3);
    BothUnits("", 0, 0);
    BothUnits("\r\n", 1, 2);
    BothUnits("\r\nab", 1, 2);
    BothUnits("\r\nab", 2, 3);
    BothUnits("a\r\n", 2, 3);
    BothUnits("a\r\nb", 2, 3);
    BothUnits("a\r\nb", 3, 4);
    BothUnits("\r\r\n", 1, 1);
    BothUnits("\r\r\n", 2, 3);
    BothUnits("\n\r\n", 2, 3);

    Require(Clusters({nullptr, 0, true}) == 0);
    Require(Clusters({nullptr, 0, false}) == 0);
    Require(CodeUnits({nullptr, 0, false}, 0) == 0);
    Require(CodeUnits({nullptr, 3, true}, 3) == 3);
    Require(Throws([] { Clusters({nullptr, 2, true}); }, "null WTF character buffer"));
    Require(Throws([] { CodeUnits({nullptr, 2, false}, 1); }, "null WTF character buffer"));

    const std::u16string combining = u"é";
    const StringView combiningView {combining.data(), static_cast<unsigned>(combining.size()), false};
    Require(Clusters(combiningView) == 1);
    Require(CodeUnits(combiningView, 1) == 2);
    Require(CodeUnits(combiningView, 2) == 2);

    const std::u16string surrogate = u"a\U0001F600";
    const StringView surrogateView {surrogate.data(), static_cast<unsigned>(surrogate.size()), false};
    Require(Clusters(surrogateView) == 2);
    Require(CodeUnits(surrogateView, 1) == 1);

    Wide(u"\u1100\u1161\u11a8", 1, 3);
    Wide(u"\u0600a", 1, 2);
    Wide(u"\u0915\u093e", 1, 2);
    Wide(u"\u0915\u094d\u0937", 1, 3);
    Wide(u"\U0001f469\u200d\U0001f469", 1, 5);

    const std::u16string regional = u"\U0001f1fa\U0001f1f8\U0001f1e8";
    const StringView regionalView {regional.data(), static_cast<unsigned>(regional.size()), false};
    Require(Clusters(regionalView) == 2);
    Require(CodeUnits(regionalView, 1) == 4);
    Require(CodeUnits(regionalView, 2) == 6);
}

}

int main() {
    Run();
    return 0;
}
