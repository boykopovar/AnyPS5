#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

String MakeString(std::string_view text, bool wide) {
    if (wide) {
        std::u16string units(text.begin(), text.end());
        return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

void Check(const URL& url, const String& referrer, bool expected, std::string_view label) {
    bool actual = _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(&url, &referrer);
    if (actual != expected) {
        std::fprintf(stderr, "shouldHideReferrer mismatch: %.*s, expected %d got %d\n", static_cast<int>(label.size()), label.data(), expected, actual);
        std::abort();
    }
}

void Test() {
    struct ReferrerCase { std::string_view text; int scheme; };
    const ReferrerCase cases[] = {
        {"http://source/", 1}, {"https://source/", 2}, {"ftp://source/", 0},
        {"file:///source", 0}, {"data:text/plain,source", 0}, {"relative/path", 0},
        {"HtTp://source/", 1}, {"hTtPs://source/", 2},
        {" \t\r\nhttps://source/", 2}, {"\x01\x1fhttps://source/", 2},
        {std::string_view("\0https://source/", 16), 2},
        {"h\tt\rt\nps\t://source/", 2}, {"h\tt\rt\np\n://source/", 1},
        {"htt ps://source/", 0}, {"https ://source/", 0}, {"httpsx://source/", 0},
        {"https", 0}, {"http", 0}, {"https:", 2}, {"http:", 1},
        {"https\t\r\n:", 2}, {"https\v:", 0}, {"https\x01:", 0},
        {" \t\r\n", 0}, {"", 0}, {"\xa0https://source/", 0}
    };
    struct DestinationCase { std::string_view text; unsigned schemeLength; bool secure; };
    const DestinationCase destinations[] = {
        {"http://target/", 4, false}, {"https://target/", 5, true},
        {"ftp://target/", 3, false}, {"file:///target", 4, false},
        {"HtTpS://target/", 5, true}, {"httpsx://target/", 6, false},
        {" https://target/", 6, false}, {"ht\ttps://target/", 6, false}
    };
    for (bool destinationWide : {false, true}) {
        for (const auto& destination : destinations) {
            auto storage = MakeString(destination.text, destinationWide);
            URL url{storage.impl, 1U | (destination.schemeLength << 6), 0, 0, 0, 0, 0, 0, 0};
            for (bool referrerWide : {false, true}) {
                for (const auto& referrer : cases) {
                    auto text = MakeString(referrer.text, referrerWide);
                    Check(url, text, referrer.scheme == 0 || (referrer.scheme == 2 && !destination.secure), referrer.text);
                }
            }
            Check(url, String(), true, "null referrer string");
            url.bits &= ~1U;
            Check(url, MakeString("https://source/", false), true, "invalid destination, HTTPS referrer");
            Check(url, MakeString("http://source/", true), false, "invalid destination, HTTP referrer");
        }
    }
    URL empty{};
    Check(empty, MakeString("https://source/", true), true, "empty invalid destination");
    Check(empty, String(), true, "both null strings");
    auto unicode = _ZN3WTF10StringImpl6createEPKDsj(u"\u0100https://source/", 16);
    auto secureStorage = MakeString("https://target/", true);
    URL secure{secureStorage.impl, 1U | (5U << 6), 0, 0, 0, 0, 0, 0, 0};
    Check(secure, unicode, true, "non-ASCII UTF-16 prefix");
}

}

int main() {
    try {
        Test();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SecurityPolicy test exception: %s\n", error.what());
        return 1;
    }
}
