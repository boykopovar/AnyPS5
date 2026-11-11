//
// Copyright (C) 2014-2022 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//


#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

struct Common {
    int key;
    String value;
};

struct Uncommon {
    String key;
    String value;
};

template<class TEntry>
struct Vector {
    TEntry* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct Map {
    Vector<Common> common;
    Vector<Uncommon> uncommon;
};

struct Literal {
    const char* characters;
};

static_assert(sizeof(Map) == 32);
static_assert(offsetof(Map, uncommon) == 16);
static_assert(sizeof(Common) == 16 && offsetof(Common, value) == 8);
static_assert(sizeof(Uncommon) == 16 && offsetof(Uncommon, value) == 8);

using Construct = void (APS5_VABI *)(Map*);
using Write = void (APS5_VABI *)(Map*, const String&, const String&);
using RemoveEnum = bool (APS5_VABI *)(Map*, int);
using RemoveString = bool (APS5_VABI *)(Map*, const String&);
using Get = String (APS5_VABI *)(const Map*, int);
using ContainsEnum = bool (APS5_VABI *)(const Map*, int);
using ContainsString = bool (APS5_VABI *)(const Map*, const String&);
using NameString = Literal (APS5_VABI *)(int);

const auto construct1 = std::bit_cast<Construct>(&_ZN7WebCore13HTTPHeaderMapC1Ev);
const auto construct2 = std::bit_cast<Construct>(&_ZN7WebCore13HTTPHeaderMapC2Ev);
const auto add = std::bit_cast<Write>(&_ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_);
const auto set = std::bit_cast<Write>(&_ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_);
const auto append = std::bit_cast<Write>(&_ZN7WebCore13HTTPHeaderMap6appendERKN3WTF6StringES4_);
const auto removeEnum = std::bit_cast<RemoveEnum>(&_ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE);
const auto removeString = std::bit_cast<RemoveString>(&_ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE);
const auto get = std::bit_cast<Get>(&_ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE);
const auto containsEnum = std::bit_cast<ContainsEnum>(&_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE);
const auto containsString = std::bit_cast<ContainsString>(&_ZNK7WebCore13HTTPHeaderMap8containsERKN3WTF6StringE);
const auto nameString = std::bit_cast<NameString>(&_ZN7WebCore20httpHeaderNameStringENS_14HTTPHeaderNameE);

constexpr std::array<const char*, 95> expectedNames {
    "Accept",
    "Accept-Charset",
    "Accept-Encoding",
    "Accept-Language",
    "Accept-Ranges",
    "Access-Control-Allow-Credentials",
    "Access-Control-Allow-Headers",
    "Access-Control-Allow-Methods",
    "Access-Control-Allow-Origin",
    "Access-Control-Expose-Headers",
    "Access-Control-Max-Age",
    "Access-Control-Request-Headers",
    "Access-Control-Request-Method",
    "Age",
    "Authorization",
    "Cache-Control",
    "Clear-Site-Data",
    "Connection",
    "Content-Disposition",
    "Content-Encoding",
    "Content-Language",
    "Content-Length",
    "Content-Location",
    "Content-Range",
    "Content-Security-Policy",
    "Content-Security-Policy-Report-Only",
    "Content-Type",
    "Cookie",
    "Cookie2",
    "Cross-Origin-Embedder-Policy",
    "Cross-Origin-Embedder-Policy-Report-Only",
    "Cross-Origin-Opener-Policy",
    "Cross-Origin-Opener-Policy-Report-Only",
    "Cross-Origin-Resource-Policy",
    "DNT",
    "Date",
    "Default-Style",
    "ETag",
    "Expect",
    "Expires",
    "Host",
    "Icy-MetaInt",
    "Icy-Metadata",
    "If-Match",
    "If-Modified-Since",
    "If-None-Match",
    "If-Range",
    "If-Unmodified-Since",
    "Keep-Alive",
    "Last-Event-ID",
    "Last-Modified",
    "Link",
    "Location",
    "Origin",
    "Ping-From",
    "Ping-To",
    "Pragma",
    "Proxy-Authorization",
    "Purpose",
    "Range",
    "Referer",
    "Referrer-Policy",
    "Refresh",
    "Report-To",
    "Reporting-Endpoints",
    "Sec-Fetch-Dest",
    "Sec-Fetch-Mode",
    "Sec-Fetch-Site",
    "Sec-WebSocket-Accept",
    "Sec-WebSocket-Extensions",
    "Sec-WebSocket-Key",
    "Sec-WebSocket-Protocol",
    "Sec-WebSocket-Version",
    "Server-Timing",
    "Service-Worker",
    "Service-Worker-Allowed",
    "Service-Worker-Navigation-Preload",
    "Set-Cookie",
    "Set-Cookie2",
    "SourceMap",
    "TE",
    "Timing-Allow-Origin",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
    "Upgrade-Insecure-Requests",
    "User-Agent",
    "Vary",
    "Via",
    "X-Content-Type-Options",
    "X-DNS-Prefetch-Control",
    "X-Frame-Options",
    "X-SourceMap",
    "X-Temp-Tablet",
    "X-XSS-Protection",
};

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HTTPHeaderMap line %u: %s\n", location.line(), message);
        std::abort();
    }
}

template<class TFunction>
void RequireThrows(TFunction function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "expected exception");
}

String Text(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
}

String Wide(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), text.size());
}

std::u16string Units(const String& text) {
    std::u16string result;
    if (!text.impl) return result;
    for (unsigned i = 0; i < text.impl->length; ++i) {
        if (text.impl->hashAndFlags & 4) result.push_back(static_cast<const unsigned char*>(text.impl->data)[i]);
        else result.push_back(static_cast<const char16_t*>(text.impl->data)[i]);
    }
    return result;
}

void Expect(const String& actual, std::u16string_view expected) {
    Require(actual.impl && Units(actual) == expected, "header value mismatch");
}

template<class TEntry>
void Destroy(Vector<TEntry>& vector) {
    for (unsigned i = 0; i < vector.size; ++i) std::destroy_at(vector.buffer + i);
    _ZN3WTF8fastFreeEPv(vector.buffer);
}

struct Guarded {
    std::array<unsigned char, 16> before;
    Map map;
    std::array<unsigned char, 16> after;

    explicit Guarded(Construct construct = construct1) {
        before.fill(0xA5);
        after.fill(0x5A);
        std::memset(&map, 0xCC, sizeof(map));
        construct(&map);
        Require(!map.common.buffer && !map.common.capacity && !map.common.size, "common constructor layout");
        Require(!map.uncommon.buffer && !map.uncommon.capacity && !map.uncommon.size, "uncommon constructor layout");
        Check();
    }

    ~Guarded() {
        Destroy(map.common);
        Destroy(map.uncommon);
        Check();
    }

    void Check() const {
        Require(std::all_of(before.begin(), before.end(), [](auto x) { return x == 0xA5; }), "leading map canary");
        Require(std::all_of(after.begin(), after.end(), [](auto x) { return x == 0x5A; }), "trailing map canary");
    }
};

void TestNames() {
    Guarded guarded;
    auto& map = guarded.map;
    auto value = Text("value");
    for (int i = 0; i < 95; ++i) {
        const auto literal = nameString(i);
        Require(literal.characters && !std::strcmp(literal.characters, expectedNames[i]), "enum/name pair");
        Require(nameString(i).characters == literal.characters, "static literal lifetime");
        auto canonical = Text(expectedNames[i]);
        std::string mixed(expectedNames[i]);
        for (std::size_t j = 0; j < mixed.size(); ++j)
            if (mixed[j] >= 'A' && mixed[j] <= 'Z') mixed[j] += 'a' - 'A';
            else if (mixed[j] >= 'a' && mixed[j] <= 'z') mixed[j] -= 'a' - 'A';
        auto variant = Text(mixed.c_str());
        std::u16string wide(mixed.begin(), mixed.end());
        auto wideVariant = Wide(wide);
        set(&map, canonical, value);
        Require(map.common.size == 1 && map.uncommon.size == 0, "known header classification");
        Require(map.common.buffer[0].key == i, "sorted enum numbering");
        Require(containsEnum(&map, i), "enum contains");
        Require(containsString(&map, variant), "ASCII case equivalence");
        Require(containsString(&map, wideVariant), "UTF16 recognition");
        Expect(get(&map, i), u"value");
        set(&map, wideVariant, value);
        Require(map.common.size == 1, "wide set replaces known header");
        Require(removeString(&map, variant), "remove common by mixed-case string");
        Require(!containsEnum(&map, i) && !get(&map, i).impl, "missing common header");
        append(&map, wideVariant, value);
        Require(map.common.size == 1 && map.common.buffer[0].key == i, "wide name classification");
        Require(removeEnum(&map, i), "remove common by enum");
        Require(!removeEnum(&map, i) && !removeString(&map, canonical), "missing removal");
        guarded.Check();
    }
    RequireThrows([] { nameString(-1); });
    RequireThrows([] { nameString(95); });
    RequireThrows([] { nameString(std::numeric_limits<int>::max()); });
}

void TestValuesAndDuplicates() {
    Guarded guarded(construct2);
    auto& map = guarded.map;
    auto name = Text("aCcEpT");
    auto a = Text("a");
    auto b = Text("b");
    auto c = Wide(u"\u20AC");
    String null;
    auto empty = Text("");
    Require(empty.impl && !empty.impl->length, "non-null empty test input");
    set(&map, name, null);
    Require(containsEnum(&map, 0) && !get(&map, 0).impl, "present null value");
    set(&map, name, empty);
    Require(get(&map, 0).impl, "set preserves empty versus null");
    add(&map, name, null);
    Expect(get(&map, 0), u", ");
    set(&map, name, null);
    add(&map, name, empty);
    Expect(get(&map, 0), u", ");
    set(&map, name, a);
    Require(a.impl->refCount == 4, "set retains value");
    {
        auto result = get(&map, 0);
        Require(result.impl == a.impl && a.impl->refCount == 6, "get returns retained String");
    }
    Require(a.impl->refCount == 4, "get temporary releases value");
    add(&map, name, b);
    Expect(get(&map, 0), u"a, b");
    Require(a.impl->refCount == 2 && b.impl->refCount == 2, "join releases replaced value");
    add(&map, name, c);
    Expect(get(&map, 0), u"a, b, \u20AC");
    set(&map, name, c);
    Expect(get(&map, 0), u"\u20AC");
    append(&map, name, a);
    append(&map, name, b);
    Require(map.common.size == 3, "release append permits duplicates");
    Expect(get(&map, 0), u"\u20AC");
    set(&map, name, a);
    Expect(map.common.buffer[1].value, u"a");
    Expect(map.common.buffer[2].value, u"b");
    Require(removeString(&map, name), "first common removal");
    Expect(get(&map, 0), u"a");
    Require(removeEnum(&map, 0), "second common removal");
    Expect(get(&map, 0), u"b");
    Require(removeEnum(&map, 0), "third common removal");
    Require(!get(&map, 0).impl, "all common duplicates removed");
    Require(a.impl->refCount == 2 && b.impl->refCount == 2 && c.impl->refCount == 2, "common removal releases strings");
}

void TestNullValues() {
    for (auto write : {set, add, append}) {
        Guarded guarded;
        auto& map = guarded.map;
        auto known = Text("Accept");
        auto unknown = Text("X-Null");
        String null;
        auto empty = Text("");
        const auto emptyReferences = empty.impl->refCount;
        write(&map, known, null);
        write(&map, unknown, null);
        Require(containsEnum(&map, 0) && !get(&map, 0).impl, "insertion preserves null common value");
        Require(containsString(&map, unknown) && !map.uncommon.buffer[0].value.impl, "insertion preserves null uncommon value");
        Require(removeEnum(&map, 0) && removeString(&map, unknown), "remove null values");
        write(&map, known, empty);
        write(&map, unknown, empty);
        Require(get(&map, 0).impl == empty.impl, "insertion preserves empty common value");
        Require(map.uncommon.buffer[0].value.impl == empty.impl, "insertion preserves empty uncommon value");
        Require(empty.impl->refCount == emptyReferences + 4, "empty values retain references");
    }
    Guarded guarded;
    auto name = Text("Accept");
    auto empty = Text("");
    String null;
    append(&guarded.map, name, null);
    append(&guarded.map, name, empty);
    Require(!get(&guarded.map, 0).impl, "duplicate null first value");
    Require(removeEnum(&guarded.map, 0), "remove first null duplicate");
    Require(get(&guarded.map, 0).impl == empty.impl, "duplicate empty second value");
}

void TestUncommon() {
    Guarded guarded;
    auto& map = guarded.map;
    auto name = Text("X-Unknown");
    auto variant = Wide(u"x-uNkNoWn");
    auto first = Text("first");
    auto second = Text("second");
    auto nonASCII = Wide(u"X-\u00C4");
    auto lowerNonASCII = Wide(u"x-\u00E4");
    auto nonASCII8 = Text("X-\xC4");
    auto asciiPrefix = Wide(u"Accept\u00E9");
    auto embeddedNul = Wide(std::u16string(u"Accept\0", 7));
    auto invalidSyntax = Text(" : bad header\r\n");
    String null;
    auto empty = Text("");
    set(&map, name, first);
    Require(map.common.size == 0 && map.uncommon.size == 1, "unknown classification");
    Require(name.impl->refCount == 4 && first.impl->refCount == 4, "uncommon retains key/value");
    Require(containsString(&map, variant), "uncommon ASCII case equivalence");
    set(&map, variant, second);
    Require(map.uncommon.size == 1 && map.uncommon.buffer[0].key.impl == name.impl, "set preserves original uncommon key");
    Expect(map.uncommon.buffer[0].value, u"second");
    add(&map, variant, first);
    Expect(map.uncommon.buffer[0].value, u"second, first");
    append(&map, variant, second);
    append(&map, name, first);
    Require(map.uncommon.size == 3, "uncommon append duplicates");
    Require(removeString(&map, variant), "remove first uncommon duplicate");
    Expect(map.uncommon.buffer[0].value, u"second");
    Require(removeString(&map, name), "remove second uncommon duplicate");
    Expect(map.uncommon.buffer[0].value, u"first");
    Require(removeString(&map, variant) && !removeString(&map, name), "remove last uncommon duplicate");
    Require(name.impl->refCount == 2 && first.impl->refCount == 2 && second.impl->refCount == 2, "uncommon removal releases references");
    for (const auto* key : {&nonASCII, &lowerNonASCII, &asciiPrefix, &embeddedNul, &invalidSyntax}) set(&map, *key, first);
    Require(map.uncommon.size == 5 && map.common.size == 0, "non-ASCII and invalid syntax remain uncommon");
    Require(containsString(&map, nonASCII8), "Latin1 and UTF16 uncommon equivalence");
    Require(removeString(&map, nonASCII8), "remove Latin1 match");
    Require(containsString(&map, lowerNonASCII), "non-ASCII case is preserved");
    for (const auto* key : {&lowerNonASCII, &asciiPrefix, &embeddedNul, &invalidSyntax}) Require(removeString(&map, *key), "remove uncommon key");
    set(&map, name, null);
    Require(!map.uncommon.buffer[0].value.impl, "null uncommon value");
    set(&map, name, empty);
    Require(map.uncommon.buffer[0].value.impl, "empty uncommon value");
    add(&map, name, null);
    Expect(map.uncommon.buffer[0].value, u", ");
    set(&map, null, first);
    Require(!containsString(&map, empty), "null and empty uncommon keys stay distinct");
    Require(!removeString(&map, empty), "empty name does not remove null name");
    set(&map, empty, second);
    Require(map.uncommon.size == 3, "empty and null uncommon keys coexist");
    Require(removeString(&map, null), "remove null name");
    Require(containsString(&map, empty), "empty key survives null removal");
    Require(removeString(&map, empty), "remove empty name");
}

void TestGrowth() {
    Guarded guarded;
    auto& map = guarded.map;
    auto value = Text("retained");
    unsigned commonCapacity = 0;
    unsigned uncommonCapacity = 0;
    for (unsigned i = 0; i < 240; ++i) {
        auto known = Text(expectedNames[i % 95]);
        auto unknown = Text(("X-Growth-" + std::to_string(i)).c_str());
        if (i == commonCapacity) commonCapacity = std::max(6u, commonCapacity + commonCapacity / 4 + 1);
        if (i == uncommonCapacity) uncommonCapacity += uncommonCapacity / 4 + 1;
        append(&map, known, value);
        append(&map, unknown, value);
        Require(map.common.size == i + 1 && map.uncommon.size == i + 1, "vector size after append");
        Require(map.common.capacity == commonCapacity, "WTF common growth policy and minimum 6");
        Require(map.uncommon.capacity == uncommonCapacity, "WTF uncommon growth policy");
        Require(_ZN3WTF14fastMallocSizeEPKv(map.common.buffer) >= commonCapacity * sizeof(Common), "common uses WTF allocator");
        Require(_ZN3WTF14fastMallocSizeEPKv(map.uncommon.buffer) >= uncommonCapacity * sizeof(Uncommon), "uncommon uses WTF allocator");
        Require(value.impl->refCount == 2 + (i + 1) * 4, "growth preserves string references");
        for (unsigned j = 0; j <= i; ++j) {
            Require(map.common.buffer[j].key == int(j % 95), "growth preserves common entries");
            Require(Units(map.uncommon.buffer[j].key) == Units(Text(("X-Growth-" + std::to_string(j)).c_str())), "growth preserves uncommon entries");
        }
        guarded.Check();
    }
    const auto commonBuffer = map.common.buffer;
    const auto uncommonBuffer = map.uncommon.buffer;
    for (unsigned i = 0; i < 240; ++i) {
        Require(removeEnum(&map, i % 95), "remove growing common vector");
        auto unknown = Text(("X-Growth-" + std::to_string(i)).c_str());
        Require(removeString(&map, unknown), "remove growing uncommon vector");
    }
    Require(value.impl->refCount == 2, "all vector references released");
    Require(map.common.buffer == commonBuffer && map.uncommon.buffer == uncommonBuffer, "removal keeps buffers");
    Require(map.common.capacity == commonCapacity && map.uncommon.capacity == uncommonCapacity, "removal keeps capacity");
}

void TestAliasing() {
    Guarded guarded;
    auto& map = guarded.map;
    auto name = Text("X-Alias");
    auto value = Text("a");
    append(&map, name, value);
    append(&map, map.uncommon.buffer[0].key, map.uncommon.buffer[0].value);
    Require(map.uncommon.size == 2, "append retains aliased entries before reallocation");
    set(&map, map.uncommon.buffer[0].key, map.uncommon.buffer[0].value);
    add(&map, map.uncommon.buffer[0].key, map.uncommon.buffer[0].value);
    Expect(map.uncommon.buffer[0].value, u"a, a");
    auto known = Text("Accept");
    for (int i = 0; i < 6; ++i) append(&map, known, value);
    append(&map, known, map.common.buffer[0].value);
    Require(map.common.size == 7, "common aliased append across growth");
    add(&map, known, map.common.buffer[0].value);
    Expect(get(&map, 0), u"a, a");
}

void TestInvalidStorage() {
    auto name = Text("Accept");
    auto unknown = Text("X-Unknown");
    auto value = Text("value");
    RequireThrows([] { construct1(nullptr); });
    RequireThrows([] { construct2(nullptr); });
    RequireThrows([&] { add(nullptr, name, value); });
    RequireThrows([&] { set(nullptr, name, value); });
    RequireThrows([&] { append(nullptr, name, value); });
    RequireThrows([&] { removeString(nullptr, name); });
    RequireThrows([] { removeEnum(nullptr, 0); });
    RequireThrows([] { get(nullptr, 0); });
    RequireThrows([] { containsEnum(nullptr, 0); });
    RequireThrows([&] { containsString(nullptr, name); });
    Map broken {};
    broken.common = {nullptr, 1, 1};
    RequireThrows([&] { get(&broken, 0); });
    RequireThrows([&] { containsEnum(&broken, 0); });
    RequireThrows([&] { removeEnum(&broken, 0); });
    RequireThrows([&] { set(&broken, name, value); });
    RequireThrows([&] { append(&broken, name, value); });
    broken = {};
    broken.uncommon = {nullptr, 1, 1};
    RequireThrows([&] { containsString(&broken, unknown); });
    RequireThrows([&] { removeString(&broken, unknown); });
    RequireThrows([&] { add(&broken, unknown, value); });
    broken = {};
    broken.common = {nullptr, 3, 0};
    RequireThrows([&] { append(&broken, name, value); });
    broken = {};
    broken.common = {nullptr, 0, 1};
    RequireThrows([&] { get(&broken, 0); });
    StringImpl invalid {2, 6, nullptr, 4};
    String bad(&invalid);
    RequireThrows([&] { containsString(&broken, bad); });
    bad.impl = nullptr;
    Guarded guarded;
    set(&guarded.map, name, value);
    StringImpl huge {2, std::numeric_limits<unsigned>::max(), nullptr, 4};
    String overflow(&huge);
    RequireThrows([&] { add(&guarded.map, name, overflow); });
    overflow.impl = nullptr;
    Common dummy {0, String()};
    broken = {};
    const auto maximum = std::numeric_limits<unsigned>::max() / sizeof(Common);
    broken.common = {&dummy, static_cast<unsigned>(maximum), static_cast<unsigned>(maximum)};
    RequireThrows([&] { append(&broken, name, value); });
    broken.common.size = broken.common.capacity = static_cast<unsigned>(maximum - 1);
    RequireThrows([&] { append(&broken, name, value); });
    StringImpl noData {2, 1, nullptr, 4};
    String inaccessible(&noData);
    RequireThrows([&] { add(&guarded.map, name, inaccessible); });
    inaccessible.impl = nullptr;
}

bool ProbeExports() {
    Map map {};
    auto name = Text("Accept");
    auto value = Text("probe");
    unsigned failures = 0;
    auto probe = [&](const char* label, auto function) {
        try {
            function();
            std::printf("%s: callable\n", label);
        } catch (const std::exception& error) {
            ++failures;
            std::fprintf(stderr, "%s: %s\n", label, error.what());
        }
    };
    probe("C1", [&] { construct1(&map); });
    probe("C2", [&] { construct2(&map); });
    probe("add", [&] { add(&map, name, value); });
    probe("set", [&] { set(&map, name, value); });
    probe("append", [&] { append(&map, name, value); });
    probe("get(enum)", [&] { get(&map, 0); });
    probe("contains(enum)", [&] { containsEnum(&map, 0); });
    probe("contains(String)", [&] { containsString(&map, name); });
    probe("remove(enum)", [&] { removeEnum(&map, 0); });
    probe("remove(String)", [&] { removeString(&map, name); });
    probe("httpHeaderNameString", [&] { nameString(0); });
    Destroy(map.common);
    Destroy(map.uncommon);
    std::printf("HTTPHeaderMap export probes: %u/11 threw\n", failures);
    return !failures;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && !std::strcmp(argv[1], "--probe-exports")) return ProbeExports() ? 0 : 1;
        TestNames();
        TestValuesAndDuplicates();
        TestNullValues();
        TestUncommon();
        TestGrowth();
        TestAliasing();
        TestInvalidStorage();
        std::puts("HTTPHeaderMap: all 11 exports, 95 names, ABI, growth and lifetime checks passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HTTPHeaderMap unexpected exception: %s\n", error.what());
        return 1;
    }
}
