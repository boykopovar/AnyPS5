#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using Jsc::WtfAbi::SHA1;
using Jsc::WtfAbi::SHA1Digest;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF SHA1 check failed at line %d\n", line);
        std::abort();
    }
}

constexpr std::array<std::uint32_t, 5> InitialHash{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U};

struct Case {
    std::array<std::uint32_t, 5> hash;
    std::vector<std::uint8_t> pending;
    std::uint64_t totalBytes;
    const char* digest;
};

std::vector<std::uint8_t> Bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> Sequence(std::size_t begin, std::size_t end, std::size_t multiplier, std::size_t offset) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = begin; i < end; ++i) bytes.push_back(static_cast<std::uint8_t>(i * multiplier + offset));
    return bytes;
}

SHA1 Make(const Case& value) {
    SHA1 sha1;
    std::memset(&sha1, 0xA5, sizeof(sha1));
    std::memcpy(sha1.buffer, value.pending.data(), value.pending.size());
    sha1.cursor = value.pending.size();
    sha1.totalBytes = value.totalBytes;
    for (std::size_t i = 0; i < 5; ++i) sha1.hash[i] = value.hash[i];
    return sha1;
}

std::string Hex(const SHA1Digest& digest) {
    static constexpr char Digits[] = "0123456789ABCDEF";
    std::string text;
    for (auto byte : digest) {
        text += Digits[byte >> 4];
        text += Digits[byte & 15];
    }
    return text;
}

bool IsReset(const SHA1& sha1) {
    for (auto byte : sha1.buffer) if (byte) return false;
    for (std::size_t i = 0; i < 5; ++i) if (sha1.hash[i] != InitialHash[i]) return false;
    return sha1.cursor == 0 && sha1.totalBytes == 0;
}

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}

int main() {
    const std::array<Case, 8> cases{{
        {InitialHash, {}, 0, "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"},
        {InitialHash, Bytes("abc"), 3, "A9993E364706816ABA3E25717850C26C9CD0D89D"},
        {InitialHash, Bytes(std::string(55, 'a')), 55, "C1C8BBDC22796E28C0E15163D20899B65621D65A"},
        {InitialHash, Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), 56, "84983E441C3BD26EBAAE4AA1F95129E5E54670F1"},
        {InitialHash, Sequence(0, 63, 1, 0), 63, "6D942DA0C4392B123528F2905C713A3CE28364BD"},
        {{0x10802D0AU, 0x3AE914A9U, 0x2CF9155BU, 0xFF4CFF7DU, 0x7A598402U}, Sequence(960, 1000, 7, 3), 1000, "4231A8A50A10FA9758DB8EC71FDEF855B751048A"},
        {{0xA048FAD6U, 0x1AFDE325U, 0xFDF0CDD0U, 0x4E6302CFU, 0x6A2344DDU}, {}, 1000000, "34AA973CD4C4DAA4F61EEB2BDBAD27316534016F"},
        {InitialHash, {}, 0x2000000000000001ULL, "72DC4AAEBF1CE74F1159D176D15CE840E50EC0B5"},
    }};
    for (const auto& value : cases) {
        auto sha1 = Make(value);
        SHA1Digest digest;
        digest.fill(0xA5);
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest);
        Check(Hex(digest) == value.digest, __LINE__);
        Check(IsReset(sha1), __LINE__);
    }

    {
        auto sha1 = Make(cases[1]);
        SHA1Digest first;
        SHA1Digest second;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &first);
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &second);
        Check(Hex(first) == cases[1].digest, __LINE__);
        Check(Hex(second) == cases[0].digest, __LINE__);
    }

    {
        auto sha1 = Make(cases[0]);
        sha1.cursor = 64;
        SHA1Digest digest;
        digest.fill(0xA5);
        const auto before = sha1;
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest); }), __LINE__);
        Check(std::memcmp(&before, &sha1, sizeof(sha1)) == 0, __LINE__);
        for (auto byte : digest) Check(byte == 0xA5, __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(nullptr, &digest); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, nullptr); }), __LINE__);
    }
    return 0;
}
