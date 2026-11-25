#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <vector>

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

using Bytes = std::vector<unsigned char>;

constexpr int PaddingPkcs1 = 1;
constexpr int PaddingSslv23 = 2;
constexpr int PaddingNone = 3;
constexpr int PaddingOaep = 4;
constexpr int PaddingX931 = 5;
constexpr int PaddingPss = 6;
constexpr int BignumStaticData = 0x02;

constexpr int BadEValue = 101;
constexpr int BadPadByteCount = 103;
constexpr int ModulusTooLarge = 105;
constexpr int BlockTypeIsNot02 = 107;
constexpr int DataGreaterThanModLen = 108;
constexpr int DataTooLargeForKeySize = 110;
constexpr int DataTooSmall = 111;
constexpr int NullBeforeBlockMissing = 113;
constexpr int PaddingCheckFailed = 114;
constexpr int Sslv3RollbackAttack = 115;
constexpr int UnknownPaddingType = 118;
constexpr int KeySizeTooSmall = 120;
constexpr int OaepDecodingError = 121;
constexpr int DataTooSmallForKeySize = 122;
constexpr int DataTooLargeForModulus = 132;

constexpr const char* KeyN = "bcd38b7c3d7b903877b76c9fbf302647623e3f1da631dbca910c3d856449e80ad54c6ddd1985d3cd3d2a749f5855fd02e5a024452716531b181d7b47a9d15af1";
constexpr const char* KeyE = "010001";
constexpr const char* KeyD = "7a556789ff5f3d49882e1abec823ceb8f2aa3743e3376c1929adcf71cb4ea278bcca6a96ab64f66953160cfc23219810a33d21d41ed2dacac750012a36bdb4c1";
constexpr const char* SmallN = "df6b560591e783d15cfa2b06a03845e5ba458ad0c790ec0690486eba18fea0c06b6fc8e2b44f68f9";
constexpr const char* SmallD = "949fe5c8acc7f363ba535d7df9e59571fe5936bef01b948d4f0e277249097ea5347f32ae3a4f3955";
constexpr const char* OaepValid = "00a3cdcdc661f9015e325a696f0fe5b1d83f471977e24b2a3197fc76fa04923539ab236ca8bd4e32a3350c2a5c0a3b7bc63169a70503b7a8efca2a9e0e2ad00c";
constexpr const char* OaepNoMarker = "00d8c70438ca1c17aa4d8d7e28fce0ef4c5e7756d0e24b2a3197fc76fa04923539ab236ca8bd4e32a3350c2a5c0a3b7bc63169a70503b7a8efca2a9e0f4bb26f";
constexpr const char* OaepWrongMarker = "006792896d9840647ad03c75afde9a61354aa63554e24b2a3197fc76fa04923539ab236ca8bd4e32a3350c2a5c0a3b7bc63169a70503b7a8efca2a9e0d2ad00c";

static Bytes Output;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL RSA error queue check failed at line %u\n", location.line());
    std::abort();
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static Bytes Hex(const char* text) {
    const auto nibble = [](char digit) { return static_cast<unsigned char>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) bytes.push_back(static_cast<unsigned char>(nibble(text[0]) << 4 | nibble(text[1])));
    return bytes;
}

static Bytes Pattern(std::size_t length, unsigned seed) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(seed + 29 * index);
    return bytes;
}

static Bytes Concat(std::initializer_list<Bytes> parts) {
    Bytes bytes;
    for (const Bytes& part : parts) bytes.insert(bytes.end(), part.begin(), part.end());
    return bytes;
}

struct StaticNumber {
    std::vector<std::uint64_t> words;
    TestBignum number;
};

static void SetNumber(StaticNumber& storage, const Bytes& bigEndian) {
    storage.words.assign(bigEndian.size() / 8 + 1, 0);
    for (std::size_t position = 0; position < bigEndian.size(); ++position)
        storage.words[position / 8] |= std::uint64_t(bigEndian[bigEndian.size() - 1 - position]) << (8 * (position % 8));
    int top = static_cast<int>(storage.words.size());
    while (top > 0 && !storage.words[static_cast<std::size_t>(top) - 1]) --top;
    storage.number = {storage.words.data(), top, static_cast<int>(storage.words.size()), 0, BignumStaticData};
}

struct TestKey {
    StaticNumber modulus;
    StaticNumber publicExponent;
    StaticNumber privateExponent;
    LibreSslRsa* rsa;
    int size;

    TestKey(const Bytes& n, const Bytes& e, const Bytes& d) : rsa(RSA_new_nid_postfix()), size(static_cast<int>(n.size())) {
        Require(rsa != nullptr);
        SetNumber(modulus, n);
        SetNumber(publicExponent, e);
        rsa->n = &modulus.number;
        rsa->e = &publicExponent.number;
        if (d.empty()) return;
        SetNumber(privateExponent, d);
        rsa->d = &privateExponent.number;
    }
    TestKey(const TestKey&) = delete;
    ~TestKey() { RSA_free_nid_postfix(rsa); }
};

static void Prepare(const TestKey& key) {
    Output.assign(static_cast<std::size_t>(key.size) + 8, 0xEE);
}

static int PublicEncrypt(const TestKey& key, int length, const Bytes& from, int padding) {
    Prepare(key);
    return RSA_public_encrypt_nid_postfix(length, from.data(), Output.data(), key.rsa, padding);
}

static int PrivateEncrypt(const TestKey& key, int length, const Bytes& from, int padding) {
    Prepare(key);
    return RSA_private_encrypt_nid_postfix(length, from.data(), Output.data(), key.rsa, padding);
}

static int PrivateDecrypt(const TestKey& key, int length, const Bytes& from, int padding) {
    Prepare(key);
    return RSA_private_decrypt_nid_postfix(length, from.data(), Output.data(), key.rsa, padding);
}

static void Expect(int result, int expected, std::initializer_list<int> reasons, std::source_location location = std::source_location::current()) {
    Require(result == expected, location);
    for (int reason : reasons) Require(ERR_get_error_nid_postfix() == (0x04fff000u | static_cast<unsigned>(reason)), location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

static bool OutputStartsWith(const Bytes& expected) {
    for (std::size_t index = 0; index < expected.size(); ++index)
        if (Output[index] != expected[index]) return false;
    return Output[expected.size()] == 0xEE;
}

static Bytes Cipher(const TestKey& key, const Bytes& block, std::source_location location = std::source_location::current()) {
    Require(static_cast<int>(block.size()) == key.size, location);
    Expect(PublicEncrypt(key, key.size, block, PaddingNone), key.size, {}, location);
    return Bytes(Output.begin(), Output.begin() + key.size);
}

static void TestPublicEncrypt() {
    TestKey key(Hex(KeyN), Hex(KeyE), {});
    const auto message = Pattern(80, 1);
    for (int padding : {PaddingPkcs1, PaddingSslv23}) {
        Expect(PublicEncrypt(key, 53, message, padding), 64, {});
        Expect(PublicEncrypt(key, 54, message, padding), -1, {DataTooLargeForKeySize});
        Expect(PublicEncrypt(key, 80, message, padding), -1, {DataTooLargeForKeySize});
    }
    Expect(PublicEncrypt(key, 22, message, PaddingOaep), 64, {});
    Expect(PublicEncrypt(key, 23, message, PaddingOaep), -1, {DataTooLargeForKeySize});
    auto block = Pattern(80, 2);
    block[0] = 0;
    Expect(PublicEncrypt(key, 64, block, PaddingNone), 64, {});
    Expect(PublicEncrypt(key, 63, block, PaddingNone), -1, {DataTooSmallForKeySize});
    Expect(PublicEncrypt(key, 0, block, PaddingNone), -1, {DataTooSmallForKeySize});
    Expect(PublicEncrypt(key, 65, block, PaddingNone), -1, {DataTooLargeForKeySize});
    Expect(PublicEncrypt(key, 64, Hex(KeyN), PaddingNone), -1, {DataTooLargeForModulus});
    Expect(PublicEncrypt(key, 64, Bytes(64, 0xFF), PaddingNone), -1, {DataTooLargeForModulus});
    for (int padding : {0, PaddingX931, PaddingPss, 7}) Expect(PublicEncrypt(key, 20, message, padding), -1, {UnknownPaddingType});

    TestKey small(Hex(SmallN), Hex(KeyE), {});
    Expect(PublicEncrypt(small, 0, message, PaddingOaep), -1, {DataTooLargeForKeySize});
    Expect(PublicEncrypt(small, -1, message, PaddingOaep), -1, {DataTooLargeForKeySize});
    Expect(PublicEncrypt(small, -2, message, PaddingOaep), -1, {KeySizeTooSmall});
    Expect(PublicEncrypt(small, 29, message, PaddingPkcs1), 40, {});

    TestKey equalExponent(Hex(KeyN), Hex(KeyN), {});
    Expect(PublicEncrypt(equalExponent, 20, message, PaddingPkcs1), -1, {BadEValue});
    TestKey longExponent(Concat({{0x01}, Bytes(384, 0xFF)}), Concat({{0x01}, Bytes(7, 0x00), {0x01}}), {});
    Expect(PublicEncrypt(longExponent, 20, message, PaddingPkcs1), -1, {BadEValue});
    TestKey largeModulus(Concat({{0x01}, Bytes(2048, 0xFF)}), Hex(KeyE), {});
    Expect(PublicEncrypt(largeModulus, 20, message, PaddingPkcs1), -1, {ModulusTooLarge});
}

static void TestPrivateEncrypt() {
    TestKey key(Hex(KeyN), Hex(KeyE), Hex(KeyD));
    const auto message = Pattern(80, 3);
    Expect(PrivateEncrypt(key, 53, message, PaddingPkcs1), 64, {});
    Expect(PrivateEncrypt(key, 54, message, PaddingPkcs1), -1, {DataTooLargeForKeySize});
    Expect(PrivateEncrypt(key, 62, message, PaddingX931), 64, {});
    Expect(PrivateEncrypt(key, 63, message, PaddingX931), -1, {DataTooLargeForKeySize});
    auto block = Pattern(80, 4);
    block[0] = 0;
    Expect(PrivateEncrypt(key, 64, block, PaddingNone), 64, {});
    Expect(PrivateEncrypt(key, 63, block, PaddingNone), -1, {DataTooSmallForKeySize});
    Expect(PrivateEncrypt(key, -1, block, PaddingNone), -1, {DataTooSmallForKeySize});
    Expect(PrivateEncrypt(key, 65, block, PaddingNone), -1, {DataTooLargeForKeySize});
    Expect(PrivateEncrypt(key, 64, Hex(KeyN), PaddingNone), -1, {DataTooLargeForModulus});
    for (int padding : {0, PaddingSslv23, PaddingOaep, PaddingPss, 7}) Expect(PrivateEncrypt(key, 20, message, padding), -1, {UnknownPaddingType});
}

static void TestPrivateDecrypt() {
    TestKey key(Hex(KeyN), Hex(KeyE), Hex(KeyD));
    const Bytes payload = Pattern(53, 5);
    const Bytes valid = Cipher(key, Concat({{0x00, 0x02}, Bytes(8, 0x11), {0x00}, payload}));
    const Bytes typeOne = Cipher(key, Concat({{0x00, 0x01}, Bytes(8, 0xFF), {0x00}, payload}));
    const Bytes fullWidth = Cipher(key, Concat({{0x01, 0x02}, Bytes(8, 0x11), {0x00}, payload}));
    const Bytes shortLead = Cipher(key, Concat({{0x00, 0x00, 0x02}, Bytes(8, 0x11), {0x00}, Pattern(52, 5)}));
    const Bytes noSeparator = Cipher(key, Concat({{0x00, 0x02}, Bytes(62, 0x11)}));
    const Bytes sevenPadding = Cipher(key, Concat({{0x00, 0x02}, Bytes(7, 0x11), {0x00}, Pattern(54, 5)}));
    const Bytes rollback = Cipher(key, Concat({{0x00, 0x02}, Bytes(8, 0x03), {0x00}, payload}));
    const Bytes sevenThrees = Cipher(key, Concat({{0x00, 0x02, 0x11}, Bytes(7, 0x03), {0x00}, payload}));
    const Bytes nineBytes = Cipher(key, Concat({Bytes(55, 0x00), {0x02}, Bytes(8, 0x11)}));

    Expect(PrivateDecrypt(key, 64, valid, PaddingPkcs1), 53, {});
    Require(OutputStartsWith(payload));
    Expect(PrivateDecrypt(key, 64, typeOne, PaddingPkcs1), -1, {BlockTypeIsNot02, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, fullWidth, PaddingPkcs1), -1, {BlockTypeIsNot02, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, shortLead, PaddingPkcs1), -1, {BlockTypeIsNot02, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, noSeparator, PaddingPkcs1), -1, {NullBeforeBlockMissing, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, sevenPadding, PaddingPkcs1), -1, {BadPadByteCount, PaddingCheckFailed});

    Expect(PrivateDecrypt(key, 64, valid, PaddingSslv23), 53, {});
    Require(OutputStartsWith(payload));
    Expect(PrivateDecrypt(key, 64, sevenThrees, PaddingSslv23), 53, {});
    Expect(PrivateDecrypt(key, 64, nineBytes, PaddingSslv23), -1, {DataTooSmall, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, typeOne, PaddingSslv23), -1, {BlockTypeIsNot02, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, fullWidth, PaddingSslv23), -1, {BlockTypeIsNot02, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, noSeparator, PaddingSslv23), -1, {NullBeforeBlockMissing, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, sevenPadding, PaddingSslv23), -1, {NullBeforeBlockMissing, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, rollback, PaddingSslv23), -1, {Sslv3RollbackAttack, PaddingCheckFailed});

    Expect(PrivateDecrypt(key, 64, Cipher(key, Hex(OaepValid)), PaddingOaep), 3, {});
    Require(OutputStartsWith({'a', 'b', 'c'}));
    Expect(PrivateDecrypt(key, 64, Cipher(key, Hex(OaepNoMarker)), PaddingOaep), -1, {OaepDecodingError, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, Cipher(key, Hex(OaepWrongMarker)), PaddingOaep), -1, {OaepDecodingError, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, valid, PaddingOaep), -1, {OaepDecodingError, PaddingCheckFailed});
    Expect(PrivateDecrypt(key, 64, fullWidth, PaddingOaep), -1, {OaepDecodingError, PaddingCheckFailed});

    Expect(PrivateDecrypt(key, 64, valid, PaddingNone), 64, {});
    for (int padding : {0, PaddingX931, PaddingPss, 7}) Expect(PrivateDecrypt(key, 64, valid, padding), -1, {UnknownPaddingType});
    Expect(PrivateDecrypt(key, 65, Concat({{0x00}, valid}), PaddingPkcs1), -1, {DataGreaterThanModLen});
    Expect(PrivateDecrypt(key, 64, Hex(KeyN), PaddingPkcs1), -1, {DataTooLargeForModulus});
    Expect(PrivateDecrypt(key, 64, Bytes(64, 0xFF), PaddingNone), -1, {DataTooLargeForModulus});

    TestKey small(Hex(SmallN), Hex(KeyE), Hex(SmallD));
    Expect(PrivateDecrypt(small, 1, {0x02}, PaddingOaep), -1, {OaepDecodingError, PaddingCheckFailed});
    Expect(PrivateDecrypt(small, 1, {0x02}, PaddingNone), 40, {});
}

int main() {
    InitializeHeap();
    ERR_clear_error_nid_postfix();
    TestPublicEncrypt();
    TestPrivateEncrypt();
    TestPrivateDecrypt();
    std::puts("libSceLibreSsl RSA error queue tests passed");
}
