#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/UserContentURLPatternExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::UserContentURLPattern;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore UserContentURLPattern check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

namespace PatternDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
};

inline const char* const Schemes[] = {"http", "HTTP", "https", "file", "FILE", "*", "ftp", "", "h*", "about"};
inline const char* const Hosts[] = {"*", "*.example.com", "example.com", "EXAMPLE.com", "sub.example.com", "a*b", "", "*.com", "*.", "xn--caf-dma.example", "127.0.0.1", "[::1]"};
inline const char* const Paths[] = {"/", "/*", "/a/*/c", "/a", "/A", "/*/*", "/a*", "/%E9", "/caf\xc3\xa9", "", "/a/b/c?x", "*"};

inline std::string BuildPattern(std::uint32_t& state) {
    auto random = [&](std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    };
    std::string pattern = Schemes[random(10)];
    int separator = random(8);
    pattern += separator == 0 ? ":/" : separator == 1 ? "" : "://";
    if (random(6)) pattern += Hosts[random(12)];
    if (random(8)) pattern += Paths[random(12)];
    if (!random(5)) pattern += Paths[random(12)];
    return pattern;
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
        adapter.Reset();
        int parses = random(4) ? 1 : 2;
        for (int p = 0; p < parses; ++p) {
            std::string pattern = BuildPattern(state);
            bool sixteen = random(3) == 0;
            bool null = random(40) == 0;
            hash.Mix(adapter.Parse(null ? nullptr : &pattern, sixteen));
            adapter.MixFields(hash);
        }
        for (int u = 0; u < 6; ++u) hash.Mix(adapter.Matches(random(adapter.UrlCount())));
    }
    return hash.value;
}

} // namespace PatternDriver

struct UrlEntry {
    const char* string;
    bool null;
    unsigned words[8];
};

const UrlEntry UrlTable[] = {
    {"http://example.com/", false, {259, 7, 7, 7, 18, 19, 19, 19}},
    {"http://example.com/a/b/c", false, {259, 7, 7, 7, 18, 23, 24, 24}},
    {"http://example.com/A", false, {259, 7, 7, 7, 18, 19, 20, 20}},
    {"https://sub.example.com/a/x/c", false, {323, 8, 8, 8, 23, 28, 29, 29}},
    {"https://deep.sub.example.com/", false, {323, 8, 8, 8, 28, 29, 29, 29}},
    {"http://notexample.com/", false, {259, 7, 7, 7, 21, 22, 22, 22}},
    {"http://example.com.evil.com/", false, {259, 7, 7, 7, 27, 28, 28, 28}},
    {"file:///a/b/c", false, {257, 7, 7, 7, 7, 12, 13, 13}},
    {"file:///", false, {257, 7, 7, 7, 7, 8, 8, 8}},
    {"file:///A", false, {257, 7, 7, 7, 7, 8, 9, 9}},
    {"ftp://example.com/a", false, {193, 6, 6, 6, 17, 18, 19, 19}},
    {"http://example.com/caf%C3%A9", false, {259, 7, 7, 7, 18, 19, 28, 28}},
    {"http://xn--caf-dma.example/", false, {259, 7, 7, 7, 26, 27, 27, 27}},
    {"http://127.0.0.1/a", false, {259, 7, 7, 7, 16, 17, 18, 18}},
    {"http://[::1]/", false, {259, 7, 7, 7, 12, 13, 13, 13}},
    {"about:blank", false, {325, 6, 6, 6, 6, 6, 11, 11}},
    {"http://example.com/a?x=1#y", false, {259, 7, 7, 7, 18, 19, 20, 24}},
    {"http://user:pw@example.com:8080/a", false, {299, 7, 11, 14, 26, 32, 33, 33}},
    {"http://a.com/", false, {259, 7, 7, 7, 12, 13, 13, 13}},
    {"http://com/", false, {259, 7, 7, 7, 10, 11, 11, 11}},
    {"http://example.com/", false, {259, 7, 7, 7, 18, 19, 19, 19}},
    {"data:text/plain,hi", false, {261, 5, 5, 5, 5, 10, 18, 18}},
    {"http://xn--caf-dma.example/a*b", false, {259, 7, 7, 7, 26, 27, 30, 30}},
    {"https://example.com/a/b/c?x", false, {323, 8, 8, 8, 19, 24, 25, 27}},
    {"not a url", false, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"http://example.com/%E9", false, {259, 7, 7, 7, 18, 19, 22, 22}},
};

String Latin1(const char* data, std::size_t length) {
    static const unsigned char emptyNarrow[1] = {0};
    String result;
    _ZN3WTF6StringC1EPKhj(&result, length ? reinterpret_cast<const unsigned char*>(data) : emptyNarrow, static_cast<unsigned>(length));
    return result;
}

struct UrlHolder {
    String string;
    URL url;
    explicit UrlHolder(const UrlEntry& entry) : string(Latin1(entry.string, std::strlen(entry.string))) {
        url.impl = string.impl;
        std::memcpy(&url.bits, entry.words, sizeof(entry.words));
    }
};

void MixString(PatternDriver::Hash& hash, const String& s) {
    if (!s.impl) {
        hash.Mix(~0ull);
        return;
    }
    auto view = NKWebKit::WtfAbi::ViewOf(s.impl);
    hash.Mix(view.length);
    for (unsigned i = 0; i < view.length; ++i) hash.Mix(NKWebKit::WtfAbi::CodeUnitAt(view, i));
}

struct Guest {
    UserContentURLPattern pattern;
    std::vector<UrlHolder*> urls;
    Guest() {
        Reset();
        for (const auto& entry : UrlTable) urls.push_back(new UrlHolder(entry));
    }
    ~Guest() {
        for (auto* url : urls) delete url;
    }
    void Reset() {
        pattern.invalid = true;
        pattern.scheme = String();
        pattern.host = String();
        pattern.path = String();
        pattern.matchSubdomains = false;
    }
    int UrlCount() const { return static_cast<int>(urls.size()); }
    bool Parse(const std::string* text, bool sixteen) {
        String s;
        if (text) {
            if (sixteen) {
                static const char16_t emptyWide[1] = {0};
                std::vector<char16_t> wide(text->begin(), text->end());
                for (auto& unit : wide) unit = static_cast<unsigned char>(unit);
                _ZN3WTF6StringC1EPKDsj(&s, wide.empty() ? emptyWide : wide.data(), static_cast<unsigned>(wide.size()));
            } else {
                s = Latin1(text->data(), text->size());
            }
        }
        bool result = _ZN7WebCore21UserContentURLPattern5parseERKN3WTF6StringE(&pattern, &s);
        pattern.invalid = !result;
        return result;
    }
    void MixFields(PatternDriver::Hash& hash) {
        hash.Mix(pattern.invalid);
        MixString(hash, pattern.scheme);
        MixString(hash, pattern.host);
        MixString(hash, pattern.path);
        hash.Mix(pattern.matchSubdomains);
    }
    bool Matches(int index) {
        return _ZNK7WebCore21UserContentURLPattern7matchesERKN3WTF3URLE(&pattern, &urls[index]->url);
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0xd8a2086b2c7e398full,
    0xbc81fc0d78f98498ull,
    0xca45465ce87f3bf8ull,
    0xb0c389bf823492cfull,
    0xde2f2c04f12dab9cull,
    0xebea7884f96b1717ull,
    0x6a94a3e20ff748feull,
    0xb1349daa57805be9ull,
    0x8972d2f31ff17d3bull,
    0x66f251ea48de1370ull,
    0x35cd900c1267f24full,
    0xa8e27a14184485a8ull,
    0x91fce338bca9e2a7ull,
    0x9d5fb0b934a47a5dull,
    0x0f041e4e7bfee790ull,
    0x1d16ff6f02008471ull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        Guest guest;
        const std::uint64_t hash = PatternDriver::Run(guest, seed, 1000);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Require(hash == ExpectedHashes[seed - 1]);
    }
}

int Index(const char* url) {
    for (std::size_t i = 0; i < std::size(UrlTable); ++i) {
        if (!std::strcmp(UrlTable[i].string, url)) return static_cast<int>(i);
    }
    std::abort();
}

void TestKnownPatterns() {
    Guest guest;
    std::string text = "http://*.example.com/a/*/c";
    Require(guest.Parse(&text, false));
    Require(guest.Matches(Index("https://sub.example.com/a/x/c")) == false);
    Require(guest.Matches(Index("http://example.com/a/b/c")));
    text = "https://*.example.com/a/*/c";
    Require(guest.Parse(&text, true));
    Require(guest.Matches(Index("https://sub.example.com/a/x/c")));
    Require(!guest.Matches(Index("http://example.com/a/b/c")));
    text = "*://*/*";
    Require(guest.Parse(&text, false));
    Require(guest.Matches(Index("ftp://example.com/a")));
    text = "file:///a/*";
    Require(guest.Parse(&text, false));
    Require(guest.Matches(Index("file:///a/b/c")));
    Require(!guest.Matches(Index("file:///A")));
    text = "http://a*b/";
    Require(!guest.Parse(&text, false));
    Require(!guest.Matches(Index("http://example.com/")));
}

void TestNullArguments() {
    Guest guest;
    String s;
    URL url{};
    bool threw = false;
    try { _ZN7WebCore21UserContentURLPattern5parseERKN3WTF6StringE(nullptr, &s); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { _ZN7WebCore21UserContentURLPattern5parseERKN3WTF6StringE(&guest.pattern, nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { _ZNK7WebCore21UserContentURLPattern7matchesERKN3WTF3URLE(nullptr, &url); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { _ZNK7WebCore21UserContentURLPattern7matchesERKN3WTF3URLE(&guest.pattern, nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

}  // namespace

int main() {
    TestTranscripts();
    TestKnownPatterns();
    TestNullArguments();
    return 0;
}
