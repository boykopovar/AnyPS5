#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::SHA1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "SHA1 check failed at line %u\n", location.line());
        std::abort();
    }
}

using Constructor = void (APS5_VABI *)(SHA1*);
using HexDigest = CString (APS5_VABI *)(SHA1*);
static_assert(std::is_same_v<decltype(&_ZN3WTF4SHA1C1Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN3WTF4SHA1C2Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN3WTF4SHA116computeHexDigestEv), HexDigest>);

constexpr std::uint32_t InitialHash[5] = {0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U};

struct Guarded {
    std::uint64_t before = 0x0123456789ABCDEFULL;
    SHA1 sha1;
    std::uint64_t after = 0xFEDCBA9876543210ULL;

    Guarded() {
        std::memset(&sha1, 0xCC, sizeof(sha1));
    }

    void CheckGuards() const {
        Require(before == 0x0123456789ABCDEFULL && after == 0xFEDCBA9876543210ULL);
    }
};

void RequireFresh(const SHA1& sha1) {
    Require(sha1.cursor == 0);
    Require(sha1.totalBytes == 0);
    Require(!std::memcmp(sha1.hash, InitialHash, sizeof(InitialHash)));
    for (auto byte : sha1.buffer) Require(byte == 0);
}

std::string Text(const CString& value) {
    Require(value.buffer != nullptr);
    Require(value.buffer->refCount == 1);
    const auto* data = reinterpret_cast<const char*>(value.buffer + 1);
    Require(data[value.buffer->length] == 0);
    return std::string(data, value.buffer->length);
}

std::string Digest(SHA1& sha1) {
    auto text = Text(_ZN3WTF4SHA116computeHexDigestEv(&sha1));
    Require(text.size() == 40);
    RequireFresh(sha1);
    return text;
}

void Fill(SHA1& sha1, const void* data, std::size_t length, std::uint64_t totalBytes, const std::uint32_t* hash) {
    std::memcpy(sha1.buffer, data, length);
    sha1.cursor = length;
    sha1.totalBytes = totalBytes;
    std::memcpy(sha1.hash, hash, sizeof(sha1.hash));
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestConstructors() {
    for (auto constructor : {&_ZN3WTF4SHA1C1Ev, &_ZN3WTF4SHA1C2Ev}) {
        Guarded value;
        constructor(&value.sha1);
        RequireFresh(value.sha1);
        value.CheckGuards();
        Require(Digest(value.sha1) == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
        value.CheckGuards();
        Require(Digest(value.sha1) == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
        value.CheckGuards();
    }
}

void TestGuestFilled() {
    struct Case {
        std::string input;
        const char* expected;
    };
    const Case cases[] = {
        {"abc", "A9993E364706816ABA3E25717850C26C9CD0D89D"},
        {std::string(55, 'a'), "C1C8BBDC22796E28C0E15163D20899B65621D65A"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983E441C3BD26EBAAE4AA1F95129E5E54670F1"},
        {std::string(63, 'a'), "03F09F5B158A7A8CDAD920BDDC29B81C18A551F5"},
    };
    for (const auto& test : cases) {
        for (std::uint8_t stale : {std::uint8_t{0}, std::uint8_t{0xA5}}) {
            Guarded value;
            _ZN3WTF4SHA1C1Ev(&value.sha1);
            std::memset(value.sha1.buffer, stale, sizeof(value.sha1.buffer));
            Fill(value.sha1, test.input.data(), test.input.size(), test.input.size(), InitialHash);
            Require(Digest(value.sha1) == test.expected);
            value.CheckGuards();
        }
    }

    const std::uint32_t afterFirstBlock[5] = {0xDA4968EBU, 0x2E377C1FU, 0x884E8F52U, 0x83524BEBU, 0xE74EBDBDU};
    Guarded continued;
    Fill(continued.sha1, "abc", 3, 67, afterFirstBlock);
    Require(Digest(continued.sha1) == "A5177E48D19A714D0463DBEAFAAB7F5C6D140FF3");
    continued.CheckGuards();

    const std::uint32_t wrappedHash[5] = {0x01234567U, 0x89ABCDEFU, 0xFEDCBA98U, 0x76543210U, 0xDEADBEEFU};
    Guarded wrapped;
    Fill(wrapped.sha1, "xyzzy", 5, 0x2000000000000001ULL, wrappedHash);
    Require(Digest(wrapped.sha1) == "B3F6A3705032D8D6BD0F1D3D102AE47B8E7B88D2");
    wrapped.CheckGuards();

    unsigned char counting[60];
    for (std::size_t i = 0; i < sizeof(counting); ++i) counting[i] = static_cast<unsigned char>(i);
    const std::uint32_t edgeHash[5] = {0, 0xFFFFFFFFU, 0x80000000U, 0x7FFFFFFFU, 0x5A5A5A5AU};
    Guarded maximum;
    Fill(maximum.sha1, counting, sizeof(counting), 0xFFFFFFFFFFFFFFFFULL, edgeHash);
    Require(Digest(maximum.sha1) == "F7AD580F14278F113BE4206BD89B8EB4F27BD4E0");
    maximum.CheckGuards();
}

void TestInvalid() {
    RequireThrows([] { _ZN3WTF4SHA1C1Ev(nullptr); });
    RequireThrows([] { _ZN3WTF4SHA1C2Ev(nullptr); });
    RequireThrows([] { _ZN3WTF4SHA116computeHexDigestEv(nullptr); });
    for (std::size_t cursor : {std::size_t{64}, std::size_t{65}, ~std::size_t{0}}) {
        Guarded value;
        _ZN3WTF4SHA1C1Ev(&value.sha1);
        value.sha1.cursor = cursor;
        value.sha1.totalBytes = 64;
        SHA1 before;
        std::memcpy(&before, &value.sha1, sizeof(before));
        RequireThrows([&] { _ZN3WTF4SHA116computeHexDigestEv(&value.sha1); });
        Require(!std::memcmp(&before, &value.sha1, sizeof(before)));
        value.CheckGuards();
    }
}

}  // namespace

int main() {
    TestConstructors();
    TestGuestFilled();
    TestInvalid();
    return 0;
}
