#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SecurityPolicy referrer header check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Make(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static std::string Narrow(const StringImpl* impl) {
    std::string text;
    if (!impl) return text;
    const auto view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned i = 0; i < view.length; ++i) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, i);
        text.push_back(unit > 0xFF ? '?' : static_cast<char>(unit));
    }
    return text;
}

static URL Parse(std::string_view text) {
    const String input = Make(text);
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

constexpr std::string_view kNone = "<null>";

static std::string Header(unsigned policy, std::string_view target, std::string_view referrerText) {
    URL url = Parse(target);
    const String referrer = Make(referrerText);
    const unsigned before = referrer.impl->refCount;
    std::string shown;
    {
        const String header = _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(static_cast<std::uint8_t>(policy), &url, &referrer);
        shown = header.impl ? Narrow(header.impl) : std::string(kNone);
        if (header.impl == referrer.impl)
            Require(referrer.impl->refCount == before + 2);
        else
            Require(referrer.impl->refCount == before);
        if (shown == referrerText && !referrerText.empty()) Require(header.impl == referrer.impl);
    }
    Require(referrer.impl->refCount == before);
    NKWebKit::WtfAbi::Release(url.impl);
    return shown;
}

constexpr std::string_view kReferrer = "https://a.example/path/page?q=1";
constexpr std::string_view kOrigin = "https://a.example/";
constexpr std::string_view kSame = "https://a.example/other";
constexpr std::string_view kCross = "https://b.example/x";
constexpr std::string_view kDowngrade = "http://b.example/x";
constexpr std::string_view kSameHostDowngrade = "http://a.example/x";

static void TestPolicies() {
    for (unsigned policy : {0u, 2u, 9u, 200u}) {
        Require(Header(policy, kSame, kReferrer) == kReferrer);
        Require(Header(policy, kCross, kReferrer) == kReferrer);
        Require(Header(policy, kDowngrade, kReferrer) == kNone);
        Require(Header(policy, kSameHostDowngrade, kReferrer) == kNone);
    }
    for (std::string_view target : {kSame, kCross, kDowngrade}) Require(Header(1, target, kReferrer) == kNone);
    Require(Header(3, kSame, kReferrer) == kReferrer);
    Require(Header(3, kCross, kReferrer) == kNone);
    Require(Header(3, kDowngrade, kReferrer) == kNone);
    Require(Header(3, kSameHostDowngrade, kReferrer) == kNone);
    Require(Header(3, "https://a.example:8443/x", kReferrer) == kNone);
    Require(Header(3, "https://A.EXAMPLE:443/x", kReferrer) == kReferrer);
    for (std::string_view target : {kSame, kCross, kDowngrade, kSameHostDowngrade}) Require(Header(4, target, kReferrer) == kOrigin);
    Require(Header(5, kSame, kReferrer) == kOrigin);
    Require(Header(5, kCross, kReferrer) == kOrigin);
    Require(Header(5, kDowngrade, kReferrer) == kNone);
    Require(Header(6, kSame, kReferrer) == kReferrer);
    Require(Header(6, kCross, kReferrer) == kOrigin);
    Require(Header(6, kDowngrade, kReferrer) == kOrigin);
    Require(Header(6, kSameHostDowngrade, kReferrer) == kOrigin);
    Require(Header(7, kSame, kReferrer) == kReferrer);
    Require(Header(7, kCross, kReferrer) == kOrigin);
    Require(Header(7, kDowngrade, kReferrer) == kNone);
    Require(Header(7, kSameHostDowngrade, kReferrer) == kNone);
    for (std::string_view target : {kSame, kCross, kDowngrade}) Require(Header(8, target, kReferrer) == kReferrer);
}

static void TestReferrers() {
    constexpr std::string_view plain = "http://a.example/p";
    Require(Header(2, "https://b.example/x", plain) == plain);
    Require(Header(2, "http://b.example/x", plain) == plain);
    Require(Header(5, "https://b.example/x", plain) == "http://a.example/");
    Require(Header(7, "http://a.example/x", plain) == plain);
    Require(Header(7, "https://a.example/x", plain) == "http://a.example/");
    Require(Header(3, "http://a.example:8080/x", plain) == kNone);
    Require(Header(3, "http://a.example:80/x", plain) == plain);
    Require(Header(4, kCross, "https://a.example:8443/p") == "https://a.example:8443/");
    Require(Header(4, kCross, "https://a.example:443/p") == "https://a.example/");
    Require(Header(4, kCross, "HTTPS://A.Example/p") == "https://a.example/");
    Require(Header(3, kSame, "HTTPS://A.Example/p") == "HTTPS://A.Example/p");
    Require(Header(4, kCross, "https://") == kNone);
    Require(Header(6, kCross, "https://") == kNone);
    Require(Header(2, kCross, "https://") == "https://");
    Require(Header(8, kCross, "https:") == "https:");
    Require(Header(8, kCross, "http") == kNone);
    Require(Header(8, kCross, "httpx://a.example/") == kNone);
    Require(Header(8, kCross, "httpsx://a.example/") == kNone);
    Require(Header(8, kCross, "https") == kNone);
    Require(Header(8, kCross, " https://a.example/") == kNone);
    Require(Header(8, kCross, "ftp://a.example/") == kNone);
    Require(Header(8, kCross, "about:blank") == kNone);
    Require(Header(4, kCross, "file:///p") == kNone);
    for (unsigned policy = 0; policy < 10; ++policy) {
        URL url = Parse(kSame);
        const String empty = Make("");
        const String none;
        Require(_ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(static_cast<std::uint8_t>(policy), &url, &empty).impl == nullptr);
        Require(_ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(static_cast<std::uint8_t>(policy), &url, &none).impl == nullptr);
        NKWebKit::WtfAbi::Release(url.impl);
    }
}

static void TestTargets() {
    Require(Header(6, "blob:https://a.example/1234-5678", kReferrer) == kNone);
    Require(Header(6, "blob:https://b.example/1234-5678", kReferrer) == kOrigin);
    Require(Header(3, "blob:https://b.example/1234-5678", kReferrer) == kNone);
    Require(Header(7, "blob:https://b.example/1234-5678", kReferrer) == kNone);
    Require(Header(6, "data:text/plain,x", kReferrer) == kOrigin);
    Require(Header(3, "data:text/plain,x", kReferrer) == kNone);
    Require(Header(6, "custom://a.example/x", kReferrer) == kOrigin);
    Require(Header(2, "wss://a.example/x", kReferrer) == kNone);
    {
        URL invalid{};
        const String referrer = Make(kReferrer);
        const String strict = _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(6, &invalid, &referrer);
        Require(Narrow(strict.impl) == kOrigin);
        const String same = _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(3, &invalid, &referrer);
        Require(same.impl == nullptr);
    }
    URL url = Parse(kSame);
    const String referrer = Make(kReferrer);
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(2, nullptr, &referrer); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(2, &url, nullptr); }));
    NKWebKit::WtfAbi::Release(url.impl);
}

int main() {
    TestPolicies();
    TestReferrers();
    TestTargets();
    std::puts("libSceNKWebKit SecurityPolicy referrer header tests passed");
    return 0;
}
