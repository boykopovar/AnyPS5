#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ProtectionSpaceCompareExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::ByteVector;
using NKWebKit::WebCoreAbi::ProtectionSpace;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore ProtectionSpace compare check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

namespace ProtectionDriver {

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

struct Space {
    Text host;
    Text realm;
    int port = 0;
    std::uint8_t serverType = 1;
    std::uint8_t authenticationScheme = 1;
    int verificationError = 0;
    std::vector<std::vector<std::uint8_t>> chain;
    std::vector<std::uint8_t> privateKey;
    Text password;
};

inline const char16_t Alphabet[] = {u'a', u'b', u'.', u'x', 0xe9, 0x4e2d};

template<typename Random> Text MakeText(Random& random) {
    Text text;
    int kind = random(6);
    if (kind == 0) {
        text.null = true;
        return text;
    }
    int length = kind == 1 ? 0 : random(4);
    bool wide = false;
    for (int i = 0; i < length; ++i) {
        char16_t unit = Alphabet[random(random(4) ? 4 : 6)];
        wide |= unit > 0xff;
        text.units.push_back(unit);
    }
    text.sixteen = wide || random(2);
    return text;
}

template<typename Random> std::vector<std::uint8_t> MakeBytes(Random& random) {
    std::vector<std::uint8_t> bytes(random(3));
    for (auto& byte : bytes) byte = static_cast<std::uint8_t>(random(3));
    return bytes;
}

template<typename Random> Space MakeSpace(Random& random) {
    static const std::uint8_t schemes[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 100};
    Space space;
    space.host = MakeText(random);
    space.realm = MakeText(random);
    space.port = random(3) ? 80 + random(3) : -random(3);
    space.serverType = static_cast<std::uint8_t>(1 + random(8));
    space.authenticationScheme = schemes[random(10)];
    space.verificationError = random(3);
    int certificates = random(3);
    for (int i = 0; i < certificates; ++i) space.chain.push_back(MakeBytes(random));
    space.privateKey = MakeBytes(random);
    space.password = MakeText(random);
    return space;
}

template<typename Random> Space Vary(Random& random, const Space& a) {
    Space b = a;
    switch (random(12)) {
    case 0: b.host = MakeText(random); break;
    case 1: b.realm = MakeText(random); break;
    case 2: b.port = random(3) ? 80 + random(3) : -random(3); break;
    case 3: b.serverType = static_cast<std::uint8_t>(1 + random(8)); break;
    case 4: b.authenticationScheme = static_cast<std::uint8_t>(1 + random(9)); break;
    case 5: b.verificationError = random(3); break;
    case 6: if (!b.chain.empty()) b.chain.back() = MakeBytes(random); else b.chain.push_back(MakeBytes(random)); break;
    case 7: b.privateKey = MakeBytes(random); break;
    case 8: b.password = MakeText(random); break;
    case 9:
        if (!b.host.null) {
            bool wide = false;
            for (char16_t unit : b.host.units) wide |= unit > 0xff;
            b.host.sixteen = wide || !b.host.sixteen;
        }
        break;
    default: break;
    }
    return b;
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
        Space a = MakeSpace(random);
        Space b = random(4) ? Vary(random, a) : MakeSpace(random);
        if (random(3) == 0) b = Vary(random, b);
        hash.Mix(adapter.Compare(a, b));
        hash.Mix(adapter.Compare(b, a));
        hash.Mix(adapter.Compare(a, a));
    }
    return hash.value;
}

} // namespace ProtectionDriver

String Make(const ProtectionDriver::Text& text) {
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

ByteVector View(std::vector<std::uint8_t>& bytes) {
    return {bytes.empty() ? nullptr : bytes.data(), static_cast<unsigned>(bytes.size()), static_cast<unsigned>(bytes.size())};
}

struct GuestSpace {
    ProtectionDriver::Space source;
    std::vector<ByteVector> chain;
    ProtectionSpace space;
    explicit GuestSpace(const ProtectionDriver::Space& fields) : source(fields) {
        space.host = Make(source.host);
        space.realm = Make(source.realm);
        space.port = source.port;
        space.serverType = source.serverType;
        space.authenticationScheme = source.authenticationScheme;
        space.isHashTableDeletedValue = false;
        space.certificateInfo.verificationError = source.verificationError;
        for (auto& certificate : source.chain) chain.push_back(View(certificate));
        space.certificateInfo.certificateChain = {chain.empty() ? nullptr : chain.data(), static_cast<unsigned>(chain.size()), static_cast<unsigned>(chain.size())};
        space.certificateInfo.privateKey = View(source.privateKey);
        space.certificateInfo.privateKeyPassword = Make(source.password);
    }
    GuestSpace(const GuestSpace&) = delete;
    GuestSpace& operator=(const GuestSpace&) = delete;
};

struct Guest {
    bool Compare(const ProtectionDriver::Space& a, const ProtectionDriver::Space& b) {
        GuestSpace x(a);
        GuestSpace y(b);
        return _ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_(&x.space, &y.space);
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0xc0566132a22536e3ull,
    0x0a47c284b0774403ull,
    0x11801504e0760c23ull,
    0x4f54959b57ade9e3ull,
    0x18b17c45cad78463ull,
    0x352567ab302ae6a3ull,
    0x7d00907244b908a3ull,
    0x1c1ea39cdf3535a3ull,
    0xba8d274106ac4243ull,
    0xd6aa234fba42e283ull,
    0x4177d4d0d6f6cb83ull,
    0xc25935d3294bf123ull,
    0x862f24b9f4303f83ull,
    0xa88dbaf244083183ull,
    0x46b2cc5762003b63ull,
    0x8b2a856d04f60543ull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        Guest guest;
        const std::uint64_t hash = ProtectionDriver::Run(guest, seed, 1000);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Require(hash == ExpectedHashes[seed - 1]);
    }
}

ProtectionDriver::Text Ascii(const char* text) {
    ProtectionDriver::Text result;
    for (const char* p = text; *p; ++p) result.units.push_back(static_cast<char16_t>(*p));
    return result;
}

void TestKnownCases() {
    Guest guest;
    ProtectionDriver::Space a;
    a.host = Ascii("example.com");
    a.realm = Ascii("realm");
    a.port = 443;
    a.serverType = 2;
    a.authenticationScheme = 2;
    a.chain = {{1, 2}, {3}};
    Require(guest.Compare(a, a));

    ProtectionDriver::Space b = a;
    b.realm = Ascii("other");
    Require(!guest.Compare(a, b));
    a.serverType = 5;
    b.serverType = 5;
    Require(guest.Compare(a, b));

    b = a;
    b.chain[1] = {4};
    Require(!guest.Compare(a, b));
    b = a;
    b.password = Ascii("secret");
    Require(!guest.Compare(a, b));
    b = a;
    b.host.null = true;
    b.host.units.clear();
    Require(!guest.Compare(a, b));
}

void TestNullArguments() {
    ProtectionDriver::Space fields;
    GuestSpace space(fields);
    bool threw = false;
    try {
        _ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_(nullptr, &space.space);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_(&space.space, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}  // namespace

int main() {
    TestTranscripts();
    TestKnownCases();
    TestNullArguments();
    return 0;
}
