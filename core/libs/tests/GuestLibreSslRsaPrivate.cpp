#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <utility>
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
constexpr int RsaFlagsDefault = 0x0006;
constexpr int RsaFlagSignVerify = 0x0040;
constexpr int RsaFlagNoBlinding = 0x0080;
constexpr int DigestRsaEncryption = 6;
constexpr int DigestMd5Sha1 = 114;
constexpr int DigestSha256 = 672;
constexpr int DigestSha512 = 674;
constexpr std::size_t KeyBytes = 64;
constexpr unsigned UntouchedLength = 0xDEADBEEF;

constexpr unsigned FieldN = 1;
constexpr unsigned FieldE = 2;
constexpr unsigned FieldD = 4;
constexpr unsigned FieldP = 8;
constexpr unsigned FieldQ = 16;
constexpr unsigned AllFields = 0xFF;
constexpr unsigned ExponentOnly = FieldN | FieldE | FieldD;

constexpr const char* KeyFields[] {
    "bcd38b7c3d7b903877b76c9fbf302647623e3f1da631dbca910c3d856449e80ad54c6ddd1985d3cd3d2a749f5855fd02e5a024452716531b181d7b47a9d15af1",
    "010001",
    "7a556789ff5f3d49882e1abec823ceb8f2aa3743e3376c1929adcf71cb4ea278bcca6a96ab64f66953160cfc23219810a33d21d41ed2dacac750012a36bdb4c1",
    "ea8f9bbce04d46486a1abf114eac902dc72178f52c29aa68a6d45a54cd80dc79",
    "ce15cfc935461eb8cb5f26c44877111cd931e41e90d50ce519b77caa6fc96439",
    "14476a2d802eee1ea09dc35b19b42d9ae7966b40b71abb12e52982cdc61e97f1",
    "c926c109c3d60d66921aeddbe85f4f74b90ad97e7c1f77d6a6a3522b89b12901",
    "70f6f9531edf27552a668dc25de53506bb7a0d9c2f3325eec6a26b1af4245466",
};
constexpr const char* Pkcs1Output = "98b9be8cd189fdbb72e581df83344bf3e89c4e2b2fbde0a67f39fd33999fc51f4f3e8522dcd978fa13bd89af6845ca4523cc0b0d5059ffb58c191406a960cae0";
constexpr const char* X931Output = "15142a299fec8220311b6a74654f50e897264d30b9ebcc1e258e5cf9c1142c26a8e2224987d05de2db01e9a041e8e228338f6c2d1c8f07744b8e0b50e2c25328";
constexpr const char* X931Complement = "0d0cc3d1c49d106afe19cea137f271af65e6e58ec23b2d244f2c54e3718d43fe929242940a32018d225628c2c64014ef105725d78d88edba16bea2954225703b";
constexpr const char* X931Header = "43b130ed6b108b52d6b10663a6e79eab2f9c28b10c3b960d12596eea522765574bcb97ec38fca851ec4a7ffc2667e6850261dfa0ae580afe163f59608fcea3a6";
constexpr const char* X931OnePad = "1009317e2e51e994d8fc08fd5559deab8fec7e31fd471a6a5f5b060f15d1ae34edbff1c4146d9b8589c63780a02c649824a7d4e8743ea03fe9d3c2b9a8c2a4db";
constexpr const char* NoneOutput = "6c908f3440e9f0b72c0988ca4d5dcb79a71bf96405b0e176a31e0b242a4558f62552d1fba084e7e09ad6639035a4ca75a4dae373e3eb2d814c38fcfa8cfe8c02";
constexpr const char* UnverifiedOutput = "60e6c10cc8c5a6bbed2fb2ecade6c89e47e9eeccbfdd16d1ef645544ee16256dde6f2f799abc7b385cba69c671953dee9229b6166a72d1cc32e13f81dbcc2597";
constexpr const char* Sha256Signature = "5c4d0125b472c0111145cc5073cb2d14fa0e9bac1944195a84b7137ced20ea08faa984301a88d534a0855e2896f1e9049e81e0fbb4049146ff39785efa192f5e";
constexpr const char* Md5Sha1Signature = "153cae5f125cd1cb490c28188271623fbc23c975fae9c44e523f9f14f3e9fb9bb4801459207a6acdba99e9a1930064c4fc12f673d8fd8629dd10192b5cc91f60";
constexpr const char* Sha256Longest = "5f3eac2d93721fb4d3240feb3495435fbceac0a49fe4cfbc7c3c4c3ac0e8461201b43207c3171ea553d2ca6a537f76442836b0d6e185ebfcfdffc0019e054d3d";
constexpr const char* Sha256Empty = "01b76892f23818d57c39d74eb4bc97b6865528f08286102f21117fc9c3fcd9d98c99284aa74368fcbe5a679155768df246fb2bf1f3422e83cecf03fa239f9a90";
constexpr const char* Pkcs1Longest = "5dd4bac4dd292e8452df5bf64624e521f7e7320f74c33c8f4274483ef8a24d975bb65cfca282f5f642a347595aeb32c7855c023844bf4b7e3149b2e40714ac8b";
constexpr const char* Pkcs1Empty = "b5d663f34b0ba5eb69328560e36b2c97c1275cd2f455abc76ad547ce9bf52277152a6ae7e5d08f2cd6246174ba93f5b2744a2f300b6000de2469e5410eee6607";
constexpr const char* OaepLongest = "2d47c3de8dd7c31ef56df82242a672bcaf5152987df855ff829cadf1d4a64361002647e396762c76317c8fe097162ee8dd21902c34d5557c8a5f2732edd9a226";
constexpr const char* Sslv23Cipher = "0f95299baabad71e842ef3d5f6bc0fee122fb5aa8252530eb14ee77133e95c086eb3648dc5d4a3e516891543a69283909f8eadce718f11d180715253b7099904";
constexpr const char* NoneCipher = "20d51896cae51be8e71a91e3ed7c66e7b50b53300cdfc11fb160e54b1462d5dd3c5f4bf7d3cd74177583ae886ef55d0159cca091c9afcde7d8e54312eefb245b";
constexpr const char* Pkcs1Type1 = "a19dcacb77ded7f5ba0b8dc0d327c85904816811241aafc5a209b3a463164982f38aee429f0a7e6138457c3053e4ffc1bbfadf86959f0df1fb60c6bd8f4ef07f";
constexpr const char* Pkcs1SevenPad = "a16e851c07aa9716955ac0c566fb54aad48b208ff764e93545b168bc4eaaf778c812b36e080a1bac203e1390bd663fad05b48c1c08338127bc3276099a7b1c12";
constexpr const char* OaepLabel = "9350dfbadf3a36cd8a41bf9934dd863b8a6383c13750045101c1317b4cff27c3bee88accb726300a9c9944befb1d8c44343ddd83762fddc896ad196090a9b2df";
constexpr const char* Sslv23Rollback = "bb4a9c484080f29f82217013924fd5258244489d21d6c44584c6dbcad3b977cb5b9d748d5e7571802383146f4faf273fe545680290866708cf9cd47222b6cbe2";

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL RSA private check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
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
    StaticNumber numbers[8];
    LibreSslRsa* rsa;

    explicit TestKey(unsigned fields, int flags = RsaFlagsDefault) : rsa(RSA_new_nid_postfix()) {
        void** slots[] {&rsa->n, &rsa->e, &rsa->d, &rsa->p, &rsa->q, &rsa->dmp1, &rsa->dmq1, &rsa->iqmp};
        for (unsigned field = 0; field < 8; ++field) {
            SetNumber(numbers[field], Hex(KeyFields[field]));
            if (fields & 1u << field) *slots[field] = &numbers[field].number;
        }
        rsa->flags = flags;
    }
    TestKey(const TestKey&) = delete;
    ~TestKey() { RSA_free_nid_postfix(rsa); }
};

static bool Written(const Bytes& out, const Bytes& expected) {
    if (!std::equal(expected.begin(), expected.end(), out.begin())) return false;
    return std::all_of(out.begin() + static_cast<long>(expected.size()), out.end(), [](unsigned char value) { return value == 0xEE; });
}

static int Encrypt(const TestKey& key, int length, const Bytes& from, Bytes& out, int padding) {
    out.assign(KeyBytes + 8, 0xEE);
    return RSA_private_encrypt_nid_postfix(length, from.data(), out.data(), key.rsa, padding);
}

static int Decrypt(const TestKey& key, int length, const Bytes& from, Bytes& out, int padding) {
    out.assign(KeyBytes + 8, 0xEE);
    return RSA_private_decrypt_nid_postfix(length, from.data(), out.data(), key.rsa, padding);
}

static int Decrypt(const TestKey& key, const char* cipher, Bytes& out, int padding) {
    return Decrypt(key, static_cast<int>(KeyBytes), Hex(cipher), out, padding);
}

static int Sign(const TestKey& key, int type, const Bytes& digest, Bytes& signature, unsigned& length) {
    signature.assign(KeyBytes + 8, 0xEE);
    length = UntouchedLength;
    return RSA_sign_nid_postfix(type, digest.empty() ? nullptr : digest.data(), static_cast<unsigned>(digest.size()), signature.data(), &length, key.rsa);
}

static void TestPrivateEncrypt() {
    TestKey key(AllFields);
    const auto message = Pattern(65, 1);
    Bytes out;
    Require(Encrypt(key, 20, message, out, PaddingPkcs1) == 64 && Written(out, Hex(Pkcs1Output)));
    Require(Encrypt(key, 20, Pattern(20, 100), out, PaddingX931) == 64 && Written(out, Hex(X931Output)));
    Require(Encrypt(key, 20, Pattern(20, 101), out, PaddingX931) == 64 && Written(out, Hex(X931Complement)));
    Require(Encrypt(key, 62, message, out, PaddingX931) == 64 && Written(out, Hex(X931Header)));
    Require(Encrypt(key, 61, Pattern(61, 3), out, PaddingX931) == 64 && Written(out, Hex(X931OnePad)));
    auto block = Pattern(65, 2);
    block[0] = 0;
    Require(Encrypt(key, 64, block, out, PaddingNone) == 64 && Written(out, Hex(NoneOutput)));
    const std::pair<int, int> refused[] {{54, PaddingPkcs1}, {65, PaddingPkcs1}, {63, PaddingX931}, {20, 0}, {20, PaddingSslv23}, {20, PaddingOaep}, {20, PaddingPss}, {20, 7}};
    for (auto [length, padding] : refused) Require(Encrypt(key, length, message, out, padding) == -1 && Written(out, {}));
    for (int length : {63, 65, 0, -1}) Require(Encrypt(key, length, block, out, PaddingNone) == -1 && Written(out, {}));
    Require(Encrypt(key, 64, Hex(KeyFields[0]), out, PaddingNone) == -1 && Written(out, {}));
    RequireThrows([&] { Encrypt(key, -1, message, out, PaddingPkcs1); });
    RequireThrows([&] { Encrypt(key, -1, message, out, PaddingX931); });
}

static void TestKeyShapes() {
    const auto message = Pattern(20, 1);
    Bytes out;
    const std::pair<unsigned, int> consistent[] {{ExponentOnly, RsaFlagsDefault}, {AllFields, RsaFlagsDefault | RsaFlagNoBlinding}, {AllFields, 0}, {ExponentOnly, 0},
        {AllFields & ~FieldD, RsaFlagsDefault}, {AllFields & ~FieldE, RsaFlagsDefault}, {FieldN | FieldD | FieldP | FieldQ, RsaFlagsDefault}, {FieldN | FieldD, RsaFlagsDefault | RsaFlagNoBlinding}};
    for (auto [fields, flags] : consistent) {
        TestKey key(fields, flags);
        Require(Encrypt(key, 20, message, out, PaddingPkcs1) == 64 && Written(out, Hex(Pkcs1Output)));
    }
    for (unsigned fields : {FieldN | FieldD, AllFields & ~(FieldE | FieldD)}) {
        TestKey key(fields);
        Require(Encrypt(key, 20, message, out, PaddingPkcs1) == -1 && Written(out, {}));
        unsigned length = 0;
        Require(Sign(key, DigestSha256, Pattern(32, 30), out, length) == 0 && length == UntouchedLength && Written(out, {}));
    }
    {
        TestKey key(AllFields);
        key.numbers[5].words[0] += 2;
        Require(Encrypt(key, 20, message, out, PaddingPkcs1) == 64 && Written(out, Hex(Pkcs1Output)));
        key.rsa->e = nullptr;
        key.rsa->flags |= RsaFlagNoBlinding;
        Require(Encrypt(key, 20, message, out, PaddingPkcs1) == 64 && Written(out, Hex(UnverifiedOutput)));
    }
    TestKey even(AllFields);
    even.numbers[0].words[0] -= 1;
    Require(Encrypt(even, 20, message, out, PaddingPkcs1) == -1 && Written(out, {}));
}

static void TestPrivateDecrypt() {
    Bytes out;
    auto block = Pattern(64, 90);
    block[0] = 0;
    for (unsigned fields : {AllFields, ExponentOnly}) {
        TestKey key(fields);
        Require(Decrypt(key, Pkcs1Longest, out, PaddingPkcs1) == 53 && Written(out, Pattern(53, 43)));
        Require(Decrypt(key, OaepLongest, out, PaddingOaep) == 22 && Written(out, Pattern(22, 62)));
        Require(Decrypt(key, Sslv23Cipher, out, PaddingSslv23) == 16 && Written(out, Pattern(16, 80)));
        Require(Decrypt(key, NoneCipher, out, PaddingNone) == 64 && Written(out, block));
    }
    TestKey key(AllFields);
    Require(Decrypt(key, Pkcs1Empty, out, PaddingPkcs1) == 0 && Written(out, {}));
    const std::pair<const char*, int> refused[] {{Pkcs1Type1, PaddingPkcs1}, {Pkcs1SevenPad, PaddingPkcs1}, {OaepLabel, PaddingOaep}, {Sslv23Rollback, PaddingSslv23},
        {Pkcs1Empty, 0}, {Pkcs1Empty, PaddingX931}, {Pkcs1Empty, PaddingPss}};
    for (auto [cipher, padding] : refused) Require(Decrypt(key, cipher, out, padding) == -1 && Written(out, {}));
    auto longer = Hex(Pkcs1Empty);
    longer.insert(longer.begin(), 0);
    Require(Decrypt(key, 65, longer, out, PaddingPkcs1) == -1 && Written(out, {}));
    Require(Decrypt(key, 64, Hex(KeyFields[0]), out, PaddingNone) == -1 && Written(out, {}));
    Require(Decrypt(key, -1, Hex(Pkcs1Empty), out, PaddingPkcs1) == -1 && Written(out, {}));
}

static void TestSign() {
    TestKey key(AllFields);
    Bytes signature;
    unsigned length = 0;
    Require(Sign(key, DigestSha256, Pattern(32, 18), signature, length) == 1 && length == 64 && Written(signature, Hex(Sha256Signature)));
    Require(Sign(key, DigestMd5Sha1, Pattern(36, 30), signature, length) == 1 && length == 64 && Written(signature, Hex(Md5Sha1Signature)));
    Require(Sign(key, DigestSha256, Pattern(34, 31), signature, length) == 1 && length == 64 && Written(signature, Hex(Sha256Longest)));
    Require(Sign(key, DigestSha256, {}, signature, length) == 1 && length == 64 && Written(signature, Hex(Sha256Empty)));
    const std::pair<int, Bytes> refused[] {{DigestMd5Sha1, Pattern(35, 30)}, {DigestMd5Sha1, {}}, {0, Pattern(32, 30)}, {-1, Pattern(32, 30)},
        {DigestSha512, Pattern(64, 30)}, {DigestSha256, Pattern(35, 31)}};
    for (const auto& [type, digest] : refused) Require(Sign(key, type, digest, signature, length) == 0 && length == UntouchedLength && Written(signature, {}));
    RequireThrows([&] { Sign(key, DigestRsaEncryption, Pattern(32, 30), signature, length); });
    const auto digest = Pattern(32, 30);
    RequireThrows([&] { RSA_sign_nid_postfix(DigestSha256, digest.data(), 0x80000000u, signature.data(), &length, key.rsa); });
}

static void TestSignAddedObject() {
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    const int nid = OBJ_create_nid_postfix("1.2.3.4", "testDigest", "Test Digest");
    Require(nid >= 956);
    TestKey key(AllFields);
    const auto digest = Pattern(32, 30);
    Bytes signature;
    unsigned length = 0;
    bool accepted = false;
    try {
        const int result = Sign(key, nid, digest, signature, length);
        if (result == 1) {
            Require(length == KeyBytes);
            auto expected = Hex("302b300706032a030405000420");
            expected.insert(expected.end(), digest.begin(), digest.end());
            Bytes recovered(KeyBytes + 8, 0xEE);
            Require(RSA_public_decrypt_nid_postfix(static_cast<int>(length), signature.data(), recovered.data(), key.rsa, PaddingPkcs1) == static_cast<int>(expected.size()));
            accepted = Written(recovered, expected);
        } else {
            std::fprintf(stderr, "added digest RSA_sign: expected valid signature or not implemented, got %d\n", result);
        }
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "RSA_sign") && std::strstr(error.what(), "not implemented"));
        accepted = true;
    }
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(accepted);
}

struct Crypts {
    int calls;
    int length;
    const unsigned char* from;
    unsigned char* to;
    LibreSslRsa* rsa;
    int padding;
};

static Crypts crypts;
struct Signs {
    int calls;
    int type;
    const unsigned char* digest;
    unsigned int digestLength;
    unsigned char* signature;
    unsigned int* signatureLength;
    const LibreSslRsa* rsa;
};

static Signs signs;

static int APS5_VABI RecordCrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    crypts = {crypts.calls + 1, length, from, to, rsa, padding};
    return 77;
}

static int APS5_VABI RecordSign(int type, const unsigned char* digest, unsigned int digestLength, unsigned char* signature, unsigned int* signatureLength, const LibreSslRsa* rsa) {
    signs = {signs.calls + 1, type, digest, digestLength, signature, signatureLength, rsa};
    return 66;
}

static void TestDispatch() {
    TestKey key(AllFields);
    const auto* defaultMethod = key.rsa->meth;
    LibreSslRsaMethod method = *defaultMethod;
    method.operations[2] = reinterpret_cast<void*>(RecordCrypt);
    method.operations[3] = reinterpret_cast<void*>(RecordCrypt);
    method.sign = reinterpret_cast<void*>(RecordSign);
    key.rsa->meth = &method;
    const auto digest = Pattern(36, 30);
    Bytes out(KeyBytes + 8, 0xEE);
    crypts = {};
    Require(RSA_private_encrypt_nid_postfix(5, digest.data(), out.data(), key.rsa, 9) == 77);
    Require(crypts.calls == 1 && crypts.length == 5 && crypts.from == digest.data() && crypts.to == out.data() && crypts.rsa == key.rsa && crypts.padding == 9);
    Require(RSA_private_decrypt_nid_postfix(6, digest.data(), out.data(), key.rsa, 8) == 77 && crypts.calls == 2 && crypts.length == 6 && crypts.padding == 8);
    unsigned length = UntouchedLength;
    Require(RSA_sign_nid_postfix(DigestSha256, digest.data(), 32, out.data(), &length, key.rsa) == 1 && length == 77);
    Require(crypts.calls == 3 && crypts.length == 51 && crypts.to == out.data() && crypts.rsa == key.rsa && crypts.padding == PaddingPkcs1);
    Require(RSA_sign_nid_postfix(DigestMd5Sha1, digest.data(), 36, out.data(), &length, key.rsa) == 1 && crypts.calls == 4 && crypts.length == 36 && crypts.from == digest.data());
    signs = {};
    key.rsa->flags |= RsaFlagSignVerify;
    Require(RSA_sign_nid_postfix(5, digest.data(), 3, out.data(), &length, key.rsa) == 66 && crypts.calls == 4);
    Require(signs.calls == 1 && signs.type == 5 && signs.digest == digest.data() && signs.digestLength == 3 && signs.signature == out.data() && signs.signatureLength == &length && signs.rsa == key.rsa);
    key.rsa->flags = RsaFlagsDefault;
    method = *defaultMethod;
    method.operations[2] = nullptr;
    method.operations[3] = nullptr;
    RequireThrows([&] { Encrypt(key, 20, digest, out, PaddingPkcs1); });
    RequireThrows([&] { Decrypt(key, Pkcs1Empty, out, PaddingPkcs1); });
    RequireThrows([&] { RSA_sign_nid_postfix(DigestSha256, digest.data(), 32, out.data(), &length, key.rsa); });
    key.rsa->meth = nullptr;
    RequireThrows([&] { Encrypt(key, 20, digest, out, PaddingPkcs1); });
    RequireThrows([&] { RSA_sign_nid_postfix(DigestSha256, digest.data(), 32, out.data(), &length, key.rsa); });
    key.rsa->meth = defaultMethod;
    RequireThrows([&] { RSA_private_encrypt_nid_postfix(20, digest.data(), out.data(), nullptr, PaddingPkcs1); });
    RequireThrows([&] { RSA_private_decrypt_nid_postfix(64, Hex(Pkcs1Empty).data(), out.data(), nullptr, PaddingPkcs1); });
    RequireThrows([&] { RSA_sign_nid_postfix(DigestSha256, digest.data(), 32, out.data(), &length, nullptr); });
    key.numbers[0].number.top = 9;
    RequireThrows([&] { Encrypt(key, 20, digest, out, PaddingPkcs1); });
    key.numbers[0].number.top = 8;
}

int main() {
    InitializeHeap();
    TestPrivateEncrypt();
    TestKeyShapes();
    TestPrivateDecrypt();
    TestSign();
    TestSignAddedObject();
    TestDispatch();
    std::puts("libSceLibreSsl RSA private tests passed");
}
