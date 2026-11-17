#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreCookieExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::Cookie;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore Cookie check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

namespace CookieDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
};

struct Text {
    bool null = false;
    bool sixteen = false;
    std::vector<char16_t> units;
};

struct Fields {
    Text name;
    Text value;
    Text domain;
    Text path;
    bool secure = false;
    bool httpOnly = false;
    bool session = false;
    double created = 0;
};

inline const char16_t Alphabet[] = {u'a', u'b', u'.', u'/', u'=', u'Z', u'0', 0xe9, 0xff, 0x100, 0x4e2d, 0xd83d};

template<typename Random> Text MakeText(Random& random, bool allowNull) {
    Text text;
    int kind = random(8);
    if (allowNull && kind == 0) {
        text.null = true;
        return text;
    }
    int length = kind == 1 ? 0 : random(12);
    bool wide = false;
    for (int i = 0; i < length; ++i) {
        int pick = random(random(3) ? 7 : 12);
        char16_t unit = Alphabet[pick];
        if (unit > 0xff) wide = true;
        text.units.push_back(unit);
    }
    text.sixteen = wide || random(2);
    return text;
}

template<typename Random> Text Vary(Random& random, const Text& text, bool allowNull) {
    int choice = random(5);
    if (choice == 0) return MakeText(random, allowNull);
    Text copy = text;
    if (choice == 1 && !copy.null) {
        bool wide = false;
        for (char16_t unit : copy.units) wide |= unit > 0xff;
        copy.sixteen = wide || !copy.sixteen;
    }
    if (choice == 2 && !copy.null && !copy.units.empty()) copy.units.back() = copy.units.back() == u'a' ? u'b' : u'a';
    return copy;
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Hash hash;
    std::uint32_t state = seed ? seed : 1;
    auto random = [&](std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    };
    for (int i = 0; i < count; ++i) {
        bool allowNull = random(2);
        Fields a;
        a.name = MakeText(random, allowNull);
        a.value = MakeText(random, true);
        a.domain = MakeText(random, allowNull);
        a.path = MakeText(random, allowNull);
        a.secure = random(2);
        a.httpOnly = random(2);
        a.session = random(2);
        a.created = random(1000) * 0.5;
        Fields b = a;
        if (random(2)) b.name = Vary(random, a.name, allowNull);
        if (random(2)) b.value = Vary(random, a.value, true);
        if (random(2)) b.domain = Vary(random, a.domain, allowNull);
        if (random(2)) b.path = Vary(random, a.path, allowNull);
        if (!random(4)) b.secure = !a.secure;
        if (random(2)) b.httpOnly = !a.httpOnly;
        if (random(2)) b.session = !a.session;
        b.created = random(1000) * 0.5;
        hash.Mix(adapter.Equal(a, b));
        hash.Mix(adapter.Equal(b, a));
        if (!allowNull) {
            hash.Mix(adapter.HashOf(a));
            hash.Mix(adapter.HashOf(b));
        }
    }
    return hash.value;
}

} // namespace CookieDriver

String Make(const CookieDriver::Text& text) {
    String result;
    if (text.null) return result;
    if (!text.sixteen) {
        static const unsigned char emptyNarrow[1] = {0};
        std::vector<unsigned char> narrow(text.units.begin(), text.units.end());
        _ZN3WTF6StringC1EPKhj(&result, narrow.empty() ? emptyNarrow : narrow.data(), static_cast<unsigned>(narrow.size()));
        return result;
    }
    static const char16_t emptyWide[1] = {0};
    _ZN3WTF6StringC1EPKDsj(&result, text.units.empty() ? emptyWide : text.units.data(), static_cast<unsigned>(text.units.size()));
    return result;
}

struct GuestCookie {
    Cookie cookie;
    explicit GuestCookie(const CookieDriver::Fields& fields) {
        cookie.expires = {};
        cookie.sameSite = 0;
        std::memset(cookie.commentURL, 0, sizeof(cookie.commentURL));
        std::memset(cookie.ports, 0, sizeof(cookie.ports));
        cookie.name = Make(fields.name);
        cookie.value = Make(fields.value);
        cookie.domain = Make(fields.domain);
        cookie.path = Make(fields.path);
        cookie.created = fields.created;
        cookie.httpOnly = fields.httpOnly;
        cookie.secure = fields.secure;
        cookie.session = fields.session;
    }
};

struct Guest {
    bool Equal(const CookieDriver::Fields& a, const CookieDriver::Fields& b) {
        GuestCookie left(a);
        GuestCookie right(b);
        return _ZNK7WebCore6CookieeqERKS0_(&left.cookie, &right.cookie);
    }
    std::uint64_t HashOf(const CookieDriver::Fields& a) {
        GuestCookie cookie(a);
        return _ZNK7WebCore6Cookie4hashEv(&cookie.cookie);
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0xe1d8bacddd41b952ull,
    0x4eb98b21289cd894ull,
    0xd867581e20aa9ea6ull,
    0xe2a6b3def545eba8ull,
    0xac09a8927d37ab75ull,
    0x5558642bc3ba6e27ull,
    0x289025ba13540ac2ull,
    0x54651b5abf67d00eull,
    0x9718100ef9f1efdaull,
    0x452c103ebab7908eull,
    0xb67b9c0ebaf1ccf8ull,
    0xa4eccc010dcfb823ull,
    0x4b6ab9ac493d4134ull,
    0x00527bd79772df1eull,
    0xda13e8c51af4b2aaull,
    0x162ab3501da1ea1aull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        Guest guest;
        const std::uint64_t hash = CookieDriver::Run(guest, seed, 500);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Require(hash == ExpectedHashes[seed - 1]);
    }
}

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestKnownCookie() {
    CookieDriver::Fields fields;
    fields.name.units = {u'n'};
    fields.domain.units = {u'e', u'x', u'a', u'm', u'p', u'l', u'e', u'.', u'c', u'o', u'm'};
    fields.path.units = {u'/'};
    fields.value.null = true;
    fields.secure = true;
    GuestCookie cookie(fields);
    Require(_ZNK7WebCore6Cookie4hashEv(&cookie.cookie) == 21286027u);
    Require(_ZNK7WebCore6CookieeqERKS0_(&cookie.cookie, &cookie.cookie));
}

void TestNullArguments() {
    CookieDriver::Fields fields;
    fields.name.null = true;
    GuestCookie cookie(fields);
    Require(Throws([&] { _ZNK7WebCore6Cookie4hashEv(&cookie.cookie); }));
    Require(Throws([] { _ZNK7WebCore6Cookie4hashEv(nullptr); }));
    Require(Throws([&] { _ZNK7WebCore6CookieeqERKS0_(nullptr, &cookie.cookie); }));
    Require(Throws([&] { _ZNK7WebCore6CookieeqERKS0_(&cookie.cookie, nullptr); }));
    Require(_ZNK7WebCore6CookieeqERKS0_(&cookie.cookie, &cookie.cookie));
}

}  // namespace

int main() {
    TestTranscripts();
    TestKnownCookie();
    TestNullArguments();
    return 0;
}
