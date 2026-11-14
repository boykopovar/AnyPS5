#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/CipherList.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace LibreSslReference {
std::size_t AesKeySize();
int AesSetEncryptKey(const unsigned char* userKey, int bits, void* key);
int AesSetDecryptKey(const unsigned char* userKey, int bits, void* key);
void AesEncrypt(const unsigned char* in, unsigned char* out, const void* key);
void AesDecrypt(const unsigned char* in, unsigned char* out, const void* key);
void AesEcb(const unsigned char* in, unsigned char* out, const void* key, int encrypt);
void AesCfb128(const unsigned char* in, unsigned char* out, std::size_t length, const void* key, unsigned char* ivec, int* num, int encrypt);
void AesCtr128(const unsigned char* in, unsigned char* out, std::size_t length, const void* key, unsigned char* ivec, unsigned char* counter, unsigned* num);
std::size_t DigestContextSize(int kind);
int DigestInit(int kind, void* context);
int DigestUpdate(int kind, void* context, const void* data, std::size_t length);
int DigestFinal(int kind, unsigned char* digest, void* context);
unsigned char* Sha256(const unsigned char* data, std::size_t length, unsigned char* digest);
unsigned char* Md5(const unsigned char* data, std::size_t length, unsigned char* digest);
unsigned char* Sha1(const unsigned char* data, std::size_t length, unsigned char* digest);
void DesSetOddParity(unsigned char* key);
const char* X509VerifyCertErrorString(long error);
const char* SslAlertDescStringLong(int value);
const char* OcspCertStatusString(long status);
const char* OcspCrlReasonString(long reason);
const char* OcspResponseStatusString(long status);
void* D2iX509(void** certificate, const unsigned char** in, long length);
void X509Free(void* certificate);
int I2dX509Pubkey(void* key, unsigned char** out);
void Free(void* pointer);
int ObjCreate(const char* oid, const char* shortName, const char* longName);
void ObjCleanup();
void AddAllAlgorithms();
void LoadBuiltinModules();
void ConfModulesFree();
int I2tAsn1Object(char* buffer, int length, const void* object);
unsigned long ErrGetError();
void ErrClearError();
int CipherListNames(const char* rules, char* buffer, int length);
int CipherFields(int index, void* fields, int length);
std::size_t MdContextSize();
void MdContextInit(void* context);
int MdContextCleanup(void* context);
void* DigestMethod(int kind);
void* GenerateRsa(int bits);
void RsaFree(void* rsa);
int RsaSign(int type, const unsigned char* message, unsigned int messageLength, unsigned char* signature, unsigned int* signatureLength, void* rsa);
int I2dRsaPubKey(void* rsa, unsigned char** output);
void ReferenceFree(void* pointer);
void* PkeyNew();
int PkeySet1Rsa(void* pkey, void* rsa);
void PkeyFree(void* pkey);
int DigestUpdateContext(void* context, const void* data, std::size_t length);
int DigestVerifyInit(void* context, void** pctx, const void* digest, void* pkey);
int DigestVerifyFinal(void* context, unsigned char* signature, std::size_t signatureLength);
}

namespace {

struct Count {
    long cases = 0;
    long differing = 0;
    std::vector<std::string> samples;
};

std::map<std::string, Count> counts;
std::mt19937_64 generator(0x5eed);

void Record(const std::string& function, bool same, const std::function<std::string()>& detail) {
    auto& count = counts[function];
    ++count.cases;
    if (same) return;
    ++count.differing;
    if (count.samples.size() < 4) count.samples.push_back(detail());
}

std::vector<unsigned char> Bytes(std::size_t size) {
    std::vector<unsigned char> bytes(size);
    for (auto& byte : bytes) byte = static_cast<unsigned char>(generator());
    return bytes;
}

std::string Hex(const void* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    return text;
}

std::string Quoted(const char* text) { return text ? std::string("\"") + text + "\"" : std::string("null"); }

std::string Attempt(const std::function<void()>& action) {
    try {
        action();
        return {};
    } catch (const std::exception& error) {
        return std::string("threw: ") + error.what();
    }
}

void CompareAesKeys() {
    Record("AesKey size", sizeof(AesKey) == LibreSslReference::AesKeySize(), [] { return std::to_string(sizeof(AesKey)) + " vs " + std::to_string(LibreSslReference::AesKeySize()); });
    for (const int bits : {128, 192, 256, 0, 64, 127, 129, 255, 257, 512, -128}) {
        const auto userKey = Bytes(64);
        for (const bool decrypt : {false, true}) {
            AesKey mine;
            AesKey theirs;
            std::memset(&mine, 0xa5, sizeof mine);
            std::memset(&theirs, 0xa5, sizeof theirs);
            int mineResult = 0;
            const auto error = Attempt([&] { mineResult = decrypt ? AES_set_decrypt_key_nid_postfix(userKey.data(), bits, &mine) : AES_set_encrypt_key_nid_postfix(userKey.data(), bits, &mine); });
            const int theirResult = decrypt ? LibreSslReference::AesSetDecryptKey(userKey.data(), bits, &theirs) : LibreSslReference::AesSetEncryptKey(userKey.data(), bits, &theirs);
            Record(decrypt ? "AES_set_decrypt_key" : "AES_set_encrypt_key", error.empty() && mineResult == theirResult && std::memcmp(&mine, &theirs, sizeof mine) == 0,
                [&] { return "bits " + std::to_string(bits) + ": " + (error.empty() ? std::to_string(mineResult) : error) + " vs " + std::to_string(theirResult); });
        }
    }
    for (const int bits : {128, 192, 256}) {
        for (const bool decrypt : {false, true}) {
            const auto userKey = Bytes(32);
            AesKey key;
            int mineResult = 0;
            auto error = Attempt([&] { mineResult = decrypt ? AES_set_decrypt_key_nid_postfix(nullptr, bits, &key) : AES_set_encrypt_key_nid_postfix(nullptr, bits, &key); });
            int theirResult = decrypt ? LibreSslReference::AesSetDecryptKey(nullptr, bits, &key) : LibreSslReference::AesSetEncryptKey(nullptr, bits, &key);
            Record(decrypt ? "AES_set_decrypt_key" : "AES_set_encrypt_key", error.empty() && mineResult == theirResult, [&] { return "null key: " + (error.empty() ? std::to_string(mineResult) : error) + " vs " + std::to_string(theirResult); });
            error = Attempt([&] { mineResult = decrypt ? AES_set_decrypt_key_nid_postfix(userKey.data(), bits, nullptr) : AES_set_encrypt_key_nid_postfix(userKey.data(), bits, nullptr); });
            theirResult = decrypt ? LibreSslReference::AesSetDecryptKey(userKey.data(), bits, nullptr) : LibreSslReference::AesSetEncryptKey(userKey.data(), bits, nullptr);
            Record(decrypt ? "AES_set_decrypt_key" : "AES_set_encrypt_key", error.empty() && mineResult == theirResult, [&] { return "null schedule: " + (error.empty() ? std::to_string(mineResult) : error) + " vs " + std::to_string(theirResult); });
        }
    }
}

void CompareAesModes() {
    for (int round = 0; round < 1500; ++round) {
        const int bits = 128 + 64 * (round % 3);
        const auto userKey = Bytes(32);
        AesKey mineEncrypt;
        AesKey mineDecrypt;
        AesKey theirEncrypt;
        AesKey theirDecrypt;
        AES_set_encrypt_key_nid_postfix(userKey.data(), bits, &mineEncrypt);
        AES_set_decrypt_key_nid_postfix(userKey.data(), bits, &mineDecrypt);
        LibreSslReference::AesSetEncryptKey(userKey.data(), bits, &theirEncrypt);
        LibreSslReference::AesSetDecryptKey(userKey.data(), bits, &theirDecrypt);
        const auto block = Bytes(16);
        unsigned char mine[16];
        unsigned char theirs[16];
        AES_encrypt_nid_postfix(block.data(), mine, &mineEncrypt);
        LibreSslReference::AesEncrypt(block.data(), theirs, &theirEncrypt);
        Record("AES_encrypt", std::memcmp(mine, theirs, 16) == 0, [&] { return Hex(mine, 16) + " vs " + Hex(theirs, 16); });
        AES_decrypt_nid_postfix(block.data(), mine, &mineDecrypt);
        LibreSslReference::AesDecrypt(block.data(), theirs, &theirDecrypt);
        Record("AES_decrypt", std::memcmp(mine, theirs, 16) == 0, [&] { return Hex(mine, 16) + " vs " + Hex(theirs, 16); });
        for (const int encrypt : {0, 1}) {
            AES_ecb_encrypt_nid_postfix(block.data(), mine, encrypt ? &mineEncrypt : &mineDecrypt, encrypt);
            LibreSslReference::AesEcb(block.data(), theirs, encrypt ? &theirEncrypt : &theirDecrypt, encrypt);
            Record("AES_ecb_encrypt", std::memcmp(mine, theirs, 16) == 0, [&] { return Hex(mine, 16) + " vs " + Hex(theirs, 16); });

            auto mineIvec = Bytes(16);
            auto theirIvec = mineIvec;
            int mineNum = static_cast<int>(generator() % 16);
            int theirNum = mineNum;
            const std::size_t length = generator() % 200;
            const auto input = Bytes(length);
            std::vector<unsigned char> mineOut(length + 1);
            std::vector<unsigned char> theirOut(length + 1);
            for (std::size_t done = 0; done < length;) {
                const std::size_t chunk = std::min<std::size_t>(length - done, generator() % 40);
                AES_cfb128_encrypt_nid_postfix(input.data() + done, mineOut.data() + done, chunk, &mineEncrypt, mineIvec.data(), &mineNum, encrypt);
                LibreSslReference::AesCfb128(input.data() + done, theirOut.data() + done, chunk, &theirEncrypt, theirIvec.data(), &theirNum, encrypt);
                done += chunk;
            }
            Record("AES_cfb128_encrypt", mineOut == theirOut && mineIvec == theirIvec && mineNum == theirNum, [&] { return "length " + std::to_string(length) + " num " + std::to_string(mineNum) + " vs " + std::to_string(theirNum); });
        }
        auto mineIvec = Bytes(16);
        if (round % 7 == 0) std::fill(mineIvec.begin() + 8, mineIvec.end(), 0xff);
        auto theirIvec = mineIvec;
        unsigned char mineCounter[16] = {};
        unsigned char theirCounter[16] = {};
        unsigned mineNum = 0;
        unsigned theirNum = 0;
        if (round % 2) {
            const auto counter = Bytes(16);
            std::memcpy(mineCounter, counter.data(), 16);
            std::memcpy(theirCounter, counter.data(), 16);
            mineNum = theirNum = static_cast<unsigned>(generator() % 16);
        }
        const std::size_t length = generator() % 300;
        const auto input = Bytes(length);
        std::vector<unsigned char> mineOut(length + 1);
        std::vector<unsigned char> theirOut(length + 1);
        for (std::size_t done = 0; done < length;) {
            const std::size_t chunk = std::min<std::size_t>(length - done, generator() % 50);
            AES_ctr128_encrypt_nid_postfix(input.data() + done, mineOut.data() + done, chunk, &mineEncrypt, mineIvec.data(), mineCounter, &mineNum);
            LibreSslReference::AesCtr128(input.data() + done, theirOut.data() + done, chunk, &theirEncrypt, theirIvec.data(), theirCounter, &theirNum);
            done += chunk;
        }
        Record("AES_ctr128_encrypt", mineOut == theirOut && mineIvec == theirIvec && mineNum == theirNum && std::memcmp(mineCounter, theirCounter, 16) == 0,
            [&] { return "length " + std::to_string(length) + " ivec " + Hex(mineIvec.data(), 16) + " vs " + Hex(theirIvec.data(), 16); });
    }
}

struct Digest {
    const char* name;
    std::size_t length;
    std::size_t contextSize;
    std::function<int(void*)> init;
    std::function<int(void*, const void*, std::size_t)> update;
    std::function<int(unsigned char*, void*)> final;
};

void CompareDigests() {
    const Digest digests[] = {
        {"MD4", 16, sizeof(Md4Context), [](void* c) { return MD4_Init_nid_postfix(static_cast<Md4Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return MD4_Update_nid_postfix(static_cast<Md4Context*>(c), d, n); }, [](unsigned char* m, void* c) { return MD4_Final_nid_postfix(m, static_cast<Md4Context*>(c)); }},
        {"MD5", 16, sizeof(Md5Context), [](void* c) { return MD5_Init_nid_postfix(static_cast<Md5Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return MD5_Update_nid_postfix(static_cast<Md5Context*>(c), d, n); }, [](unsigned char* m, void* c) { return MD5_Final_nid_postfix(m, static_cast<Md5Context*>(c)); }},
        {"SHA1", 20, sizeof(Sha1Context), [](void* c) { return SHA1_Init_nid_postfix(static_cast<Sha1Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return SHA1_Update_nid_postfix(static_cast<Sha1Context*>(c), d, n); }, [](unsigned char* m, void* c) { return SHA1_Final_nid_postfix(m, static_cast<Sha1Context*>(c)); }},
        {"SHA224", 28, sizeof(Sha256Context), [](void* c) { return SHA224_Init_nid_postfix(static_cast<Sha256Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return SHA224_Update_nid_postfix(static_cast<Sha256Context*>(c), d, n); }, [](unsigned char* m, void* c) { return SHA224_Final_nid_postfix(m, static_cast<Sha256Context*>(c)); }},
        {"SHA256", 32, sizeof(Sha256Context), [](void* c) { return SHA256_Init_nid_postfix(static_cast<Sha256Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return SHA256_Update_nid_postfix(static_cast<Sha256Context*>(c), d, n); }, [](unsigned char* m, void* c) { return SHA256_Final_nid_postfix(m, static_cast<Sha256Context*>(c)); }},
        {"SHA384", 48, sizeof(Sha512Context), [](void* c) { return SHA384_Init_nid_postfix(static_cast<Sha512Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return SHA384_Update_nid_postfix(static_cast<Sha512Context*>(c), d, n); }, [](unsigned char* m, void* c) { return SHA384_Final_nid_postfix(m, static_cast<Sha512Context*>(c)); }},
        {"SHA512", 64, sizeof(Sha512Context), [](void* c) { return SHA512_Init_nid_postfix(static_cast<Sha512Context*>(c)); }, [](void* c, const void* d, std::size_t n) { return SHA512_Update_nid_postfix(static_cast<Sha512Context*>(c), d, n); }, [](unsigned char* m, void* c) { return SHA512_Final_nid_postfix(m, static_cast<Sha512Context*>(c)); }},
    };
    for (int kind = 0; kind < 7; ++kind) {
        const auto& digest = digests[kind];
        const std::string name = digest.name;
        Record(name + " context size", digest.contextSize == LibreSslReference::DigestContextSize(kind), [&] { return std::to_string(digest.contextSize) + " vs " + std::to_string(LibreSslReference::DigestContextSize(kind)); });
        for (int round = 0; round < 600; ++round) {
            std::vector<unsigned char> mine(digest.contextSize, 0xa5);
            std::vector<unsigned char> theirs(digest.contextSize, 0xa5);
            int mineResult = 0;
            const auto error = Attempt([&] { mineResult = digest.init(mine.data()); });
            int theirResult = LibreSslReference::DigestInit(kind, theirs.data());
            Record(name + "_Init", error.empty() && mineResult == theirResult && mine == theirs, [&] { return (error.empty() ? std::to_string(mineResult) : error) + " vs " + std::to_string(theirResult) + " context " + Hex(mine.data(), 32) + " vs " + Hex(theirs.data(), 32); });
            if (!error.empty()) continue;
            const std::size_t length = round < 300 ? static_cast<std::size_t>(round) : generator() % 5000;
            const auto input = Bytes(length);
            bool same = true;
            std::string first;
            for (std::size_t done = 0; done <= length;) {
                const std::size_t chunk = done == length ? 0 : std::min<std::size_t>(length - done, generator() % 300 + (generator() % 4 ? 1 : 0));
                mineResult = digest.update(mine.data(), input.data() + done, chunk);
                theirResult = LibreSslReference::DigestUpdate(kind, theirs.data(), input.data() + done, chunk);
                done += chunk;
                if (same && (mineResult != theirResult || mine != theirs)) {
                    same = false;
                    first = "after " + std::to_string(done) + " of " + std::to_string(length) + " bytes";
                }
                if (chunk == 0 && done == length) break;
            }
            Record(name + "_Update", same, [&] { return first; });
            unsigned char mineDigest[64];
            unsigned char theirDigest[64];
            mineResult = digest.final(mineDigest, mine.data());
            theirResult = LibreSslReference::DigestFinal(kind, theirDigest, theirs.data());
            Record(name + "_Final", mineResult == theirResult && std::memcmp(mineDigest, theirDigest, digest.length) == 0 && mine == theirs, [&] { return "length " + std::to_string(length) + " " + Hex(mineDigest, digest.length) + " vs " + Hex(theirDigest, digest.length); });
        }
    }
    for (int round = 0; round < 300; ++round) {
        const auto input = Bytes(round < 150 ? static_cast<std::size_t>(round) : generator() % 4000);
        unsigned char mine[64];
        unsigned char theirs[64];
        SHA256_nid_postfix(input.data(), input.size(), mine);
        LibreSslReference::Sha256(input.data(), input.size(), theirs);
        Record("SHA256", std::memcmp(mine, theirs, 32) == 0, [&] { return Hex(mine, 32) + " vs " + Hex(theirs, 32); });
        MD5_nid_postfix(input.data(), input.size(), mine);
        LibreSslReference::Md5(input.data(), input.size(), theirs);
        Record("MD5 (libSceLibreSsl3)", std::memcmp(mine, theirs, 16) == 0, [&] { return Hex(mine, 16) + " vs " + Hex(theirs, 16); });
        SHA1_nid_postfix(input.data(), input.size(), mine);
        LibreSslReference::Sha1(input.data(), input.size(), theirs);
        Record("SHA1 (libSceLibreSsl3)", std::memcmp(mine, theirs, 20) == 0, [&] { return Hex(mine, 20) + " vs " + Hex(theirs, 20); });
    }
}

void CompareDesParity() {
    for (int value = 0; value < 256; ++value) {
        for (int at = 0; at < 8; ++at) {
            auto mine = Bytes(8);
            mine[at] = static_cast<unsigned char>(value);
            auto theirs = mine;
            DES_set_odd_parity_nid_postfix(mine.data());
            LibreSslReference::DesSetOddParity(theirs.data());
            Record("DES_set_odd_parity", mine == theirs, [&] { return Hex(mine.data(), 8) + " vs " + Hex(theirs.data(), 8); });
        }
    }
}

void CompareStrings(const char* function, const std::vector<long>& values, const std::function<const char*(long)>& mine, const std::function<const char*(long)>& theirs) {
    for (const long value : values) {
        const char* mineText = nullptr;
        const auto error = Attempt([&] { mineText = mine(value); });
        const char* theirText = theirs(value);
        const bool same = error.empty() && (mineText == nullptr) == (theirText == nullptr) && (mineText == nullptr || std::strcmp(mineText, theirText) == 0);
        Record(function, same, [&] { return std::to_string(value) + ": " + (error.empty() ? Quoted(mineText) : error) + " vs " + Quoted(theirText); });
    }
}

std::vector<long> Range(long first, long last, std::initializer_list<long> extra = {}) {
    std::vector<long> values;
    for (long value = first; value <= last; ++value) values.push_back(value);
    values.insert(values.end(), extra);
    return values;
}

void CompareStrings() {
    CompareStrings("X509_verify_cert_error_string", Range(-3, 120, {1000, 0x7fffffff, -0x7fffffff - 1}), [](long v) { return X509_verify_cert_error_string_nid_postfix(v); }, LibreSslReference::X509VerifyCertErrorString);
    CompareStrings("SSL_alert_desc_string_long", Range(-2, 260), [](long v) { return SSL_alert_desc_string_long_nid_postfix(static_cast<int>(v)); }, [](long v) { return LibreSslReference::SslAlertDescStringLong(static_cast<int>(v)); });
    CompareStrings("OCSP_cert_status_str", Range(-2, 8), [](long v) { return OCSP_cert_status_str_nid_postfix(v); }, LibreSslReference::OcspCertStatusString);
    CompareStrings("OCSP_crl_reason_str", Range(-2, 14), [](long v) { return OCSP_crl_reason_str_nid_postfix(v); }, LibreSslReference::OcspCrlReasonString);
    CompareStrings("OCSP_response_status_str", Range(-2, 10), [](long v) { return OCSP_response_status_str_nid_postfix(v); }, LibreSslReference::OcspResponseStatusString);
}

using Der = std::vector<unsigned char>;

unsigned Pick(unsigned count) { return static_cast<unsigned>(generator() % count); }
bool Chance(unsigned percent) { return Pick(100) < percent; }

bool faulty = false;

bool Fault(unsigned percent) { return faulty && Chance(percent); }

Der Join(std::initializer_list<Der> parts) {
    Der joined;
    for (const auto& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Der Wrap(Der tag, const Der& content) {
    Der out = std::move(tag);
    if ((out[0] & 0x20) && Chance(8)) {
        out.push_back(0x80);
        out.insert(out.end(), content.begin(), content.end());
        out.push_back(0);
        out.push_back(0);
        return out;
    }
    const std::size_t size = content.size();
    if (size < 128 && !Chance(4)) {
        out.push_back(static_cast<unsigned char>(size));
    } else {
        Der length;
        for (std::size_t rest = size; rest; rest >>= 8) length.insert(length.begin(), static_cast<unsigned char>(rest));
        if (length.empty() || Chance(20)) length.insert(length.begin(), 0);
        out.push_back(static_cast<unsigned char>(0x80 | length.size()));
        out.insert(out.end(), length.begin(), length.end());
    }
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

Der Wrap(unsigned char tag, const Der& content) { return Wrap(Der{tag}, content); }

Der Characters(std::size_t count) {
    static constexpr unsigned char pool[] = {' ', ' ', 'A', 'b', 'Z', 'q', '\t', '\n', '\r', '0', '~', 'M', 0x80, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xff, 0x00, 0xd8, 0xdc, 0x10, 0x7f};
    Der out;
    for (std::size_t index = 0; index < count; ++index) out.push_back(!faulty ? pool[Pick(12)] : Chance(80) ? pool[Pick(sizeof pool)] : static_cast<unsigned char>(generator()));
    return out;
}

Der Integer() {
    static const Der patterns[] = {{}, {0x00}, {0x00, 0x80}, {0x00, 0x00, 0x01}, {0xff}, {0xff, 0x00}, {0xff, 0x00, 0x00}, {0x80}, {0x80, 0x00}, {0xff, 0x7f}, {0x01, 0x00}};
    if (Chance(60)) return patterns[Pick(std::size(patterns))];
    return Characters(Pick(10));
}

Der ObjectContent() {
    static const Der valid[] = {{0x55, 0x04, 0x03}, {0x55, 0x04, 0x0a}, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}, {0x55, 0x1d, 0x13}, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01}};
    static const Der invalid[] = {{}, {0x80, 0x01}, {0x55, 0x84}, {0x2a, 0x80, 0x80, 0x01}, {0x81, 0x80, 0x01}};
    if (!Fault(20)) return valid[Pick(std::size(valid))];
    if (Chance(50)) return invalid[Pick(std::size(invalid))];
    Der out = Characters(1 + Pick(6));
    out.back() &= 0x7f;
    return out;
}

Der BitContent() {
    if (Fault(5)) return {};
    Der out{static_cast<unsigned char>(Fault(20) ? Pick(64) : Pick(8))};
    const Der data = Characters(Pick(6));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

Der Split(unsigned char tag, const Der& content) {
    if (!Chance(12)) return Wrap(tag, content);
    Der chunks;
    for (std::size_t at = 0; at < content.size();) {
        const std::size_t size = std::min<std::size_t>(content.size() - at, 1 + Pick(4));
        const Der part(content.begin() + static_cast<std::ptrdiff_t>(at), content.begin() + static_cast<std::ptrdiff_t>(at + size));
        const unsigned char inner = !Fault(30) ? 0x04 : static_cast<unsigned char>(Chance(50) ? tag : Pick(31));
        chunks = Join({chunks, Chance(10) ? Split(static_cast<unsigned char>(inner | 0x20), part) : Wrap(inner, part)});
        at += size;
    }
    return Wrap(static_cast<unsigned char>(tag | 0x20), chunks);
}

Der Value(unsigned char tag) {
    switch (tag) {
    case 0x01: return Wrap(tag, !Fault(15) ? Der{static_cast<unsigned char>(Chance(50) ? 0xff : Pick(3))} : Characters(Pick(3)));
    case 0x02:
    case 0x0a: return Split(tag, Integer());
    case 0x03: return Split(tag, BitContent());
    case 0x05: return Wrap(tag, !Fault(10) ? Der{} : Der{0x00});
    case 0x06: return Wrap(tag, ObjectContent());
    case 0x1e: return Split(tag, Characters(!Fault(15) ? 2 * Pick(6) : Pick(9)));
    case 0x1c: return Split(tag, Characters(!Fault(15) ? 4 * Pick(4) : Pick(9)));
    case 0x30:
    case 0x31: return Wrap(tag, Chance(50) ? Join({Wrap(0x02, {0x01}), Wrap(0x0c, Characters(Pick(4)))}) : Der{});
    case 0x1f: return Wrap(Der{0x1f, static_cast<unsigned char>(31 + Pick(60))}, Characters(Pick(4)));
    default: return Split(tag, Characters(Pick(14)));
    }
}

Der NameValue() {
    static constexpr unsigned char common[] = {0x0c, 0x0c, 0x13, 0x13, 0x14, 0x16, 0x12, 0x1e, 0x1c, 0x03, 0x30};
    static constexpr unsigned char rare[] = {0x0a, 0x07, 0x08, 0x09, 0x0b, 0x0d, 0x0e, 0x0f, 0x1d, 0x1a, 0x02, 0x04, 0x31, 0x05, 0x01, 0x06, 0x17, 0x18, 0x15, 0x19, 0x1b, 0x80, 0x1f};
    return Value(!Fault(15) ? common[Pick(sizeof common)] : rare[Pick(sizeof rare)]);
}

Der Name() {
    Der sets;
    const unsigned count = Chance(10) ? 0 : 1 + Pick(3);
    for (unsigned set = 0; set < count; ++set) {
        Der entries;
        const unsigned size = Chance(75) ? 1 : 2 + Pick(2);
        for (unsigned entry = 0; entry < size; ++entry) entries = Join({entries, Wrap(0x30, Join({Wrap(0x06, ObjectContent()), NameValue()}))});
        sets = Join({sets, Wrap(0x31, entries)});
    }
    return Wrap(0x30, sets);
}

Der Parameter() {
    static constexpr unsigned char tags[] = {0x05, 0x05, 0x05, 0x01, 0x02, 0x03, 0x04, 0x06, 0x0a, 0x0c, 0x13, 0x1e, 0x1c, 0x30, 0x31, 0x17, 0x1f, 0x80, 0xa0, 0x40, 0x09};
    const unsigned char tag = tags[Pick(sizeof tags)];
    if (tag == 0x80 || tag == 0x40) return Wrap(tag, Characters(Pick(4)));
    if (tag == 0xa0) return Wrap(tag, Wrap(0x05, {}));
    return Value(tag);
}

Der Algorithm() {
    Der fields = Wrap(0x06, ObjectContent());
    if (Chance(85)) fields = Join({fields, Parameter()});
    return Wrap(0x30, fields);
}

Der Time() {
    static constexpr unsigned char tags[] = {0x17, 0x18, 0x13, 0x0c, 0x1a};
    return Split(tags[Fault(30) ? Pick(sizeof tags) : Pick(2)], !Fault(20) ? Der{'2', '0', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z'} : Characters(Pick(16)));
}

Der Extensions() {
    Der list;
    const unsigned count = Pick(4);
    for (unsigned index = 0; index < count; ++index) {
        Der fields = Wrap(0x06, ObjectContent());
        if (Chance(40)) fields = Join({fields, Value(0x01)});
        fields = Join({fields, !Fault(5) ? Split(0x04, Characters(Pick(10))) : NameValue()});
        list = Join({list, Wrap(0x30, fields)});
    }
    return Wrap(0xa3, Wrap(0x30, list));
}

Der Certificate() {
    Der tbs;
    if (Chance(70)) tbs = Wrap(0xa0, Wrap(0x02, Integer()));
    tbs = Join({tbs, Wrap(0x02, Integer()), Algorithm(), Name(), Wrap(0x30, Join({Time(), Time()})), Name(),
        Wrap(0x30, Join({Algorithm(), Split(0x03, BitContent())}))});
    if (Chance(15)) tbs = Join({tbs, !Fault(20) ? Wrap(0x81, BitContent()) : Wrap(0xa1, Wrap(0x04, BitContent()))});
    if (Chance(15)) tbs = Join({tbs, Wrap(0x82, BitContent())});
    if (Chance(60)) tbs = Join({tbs, Extensions()});
    return Wrap(0x30, Join({Wrap(0x30, tbs), Algorithm(), Split(0x03, BitContent())}));
}

Der Mutate(Der der) {
    const unsigned count = 1 + Pick(3);
    for (unsigned round = 0; round < count && !der.empty(); ++round) {
        const std::size_t at = Pick(static_cast<unsigned>(der.size()));
        switch (Pick(7)) {
        case 0: der[at] ^= static_cast<unsigned char>(1u << Pick(8)); break;
        case 1: der[at] = static_cast<unsigned char>(generator()); break;
        case 2: der.resize(at); break;
        case 3: der.insert(der.begin() + static_cast<std::ptrdiff_t>(at), static_cast<unsigned char>(generator())); break;
        case 4: der.erase(der.begin() + static_cast<std::ptrdiff_t>(at)); break;
        case 5: der[at] = 0x80; break;
        default: der[at] = static_cast<unsigned char>(Chance(50) ? 0x00 : 0xff); break;
        }
    }
    return der;
}

std::string Describe(const Asn1String* string) {
    if (!string) return "null";
    return "S(" + std::to_string(string->type) + "," + std::to_string(string->length) + "," + std::to_string(string->flags) + "," +
        (string->data ? Hex(string->data, static_cast<std::size_t>(std::max(string->length, 0))) : std::string("null")) + ")";
}

std::string Describe(const LibreSslAsn1Object* object) {
    if (!object) return "null";
    return "O(" + std::to_string(object->nid) + "," + std::to_string(object->flags) + "," + std::to_string(object->length) + "," + Quoted(object->shortName) + "," +
        Quoted(object->longName) + "," + (object->data ? Hex(object->data, static_cast<std::size_t>(std::max(object->length, 0))) : std::string("null")) + ")";
}

std::string Describe(const LibreSslAsn1Type* any) {
    if (!any) return "null";
    std::string text = "T(" + std::to_string(any->type) + ",";
    if (any->type == 1) return text + std::to_string(*reinterpret_cast<const std::int64_t*>(&any->value)) + ")";
    if (any->type == 5) return text + (any->value ? "set" : "null") + ")";
    if (any->type == 6) return text + Describe(static_cast<const LibreSslAsn1Object*>(any->value)) + ")";
    return text + Describe(static_cast<const Asn1String*>(any->value)) + ")";
}

std::string Describe(const LibreSslX509Algorithm* algorithm) {
    if (!algorithm) return "null";
    return "A(" + Describe(algorithm->algorithm) + "," + Describe(algorithm->parameter) + ")";
}

std::string DescribeStack(const LibreSslStack* stack) {
    if (!stack) return "null";
    return "K(" + std::to_string(stack->num) + "," + std::to_string(stack->numAlloc) + "," + std::to_string(stack->sorted) + "," + (stack->comp ? "comp" : "nocomp") + ")";
}

std::string Describe(const LibreSslX509Name* name) {
    if (!name) return "null";
    const auto* bytes = static_cast<const std::uint64_t*>(name->bytes);
    const auto* data = *reinterpret_cast<const unsigned char* const*>(bytes + 1);
    std::string text = "N(" + std::to_string(name->modified) + "," + std::to_string(bytes[0]) + "," + std::to_string(bytes[2]) + "," + Hex(data, bytes[0]) + "," + DescribeStack(name->entries);
    for (int index = 0; index < name->entries->num; ++index) {
        const auto* entry = reinterpret_cast<const LibreSslX509NameEntry*>(name->entries->data[index]);
        text += ",E(" + std::to_string(entry->set) + "," + std::to_string(entry->size) + "," + Describe(static_cast<const LibreSslAsn1Object*>(entry->object)) + "," +
            Describe(static_cast<const Asn1String*>(entry->value)) + ")";
    }
    return text + "," + std::to_string(name->canonEnclen) + "," + (name->canonEnc ? Hex(name->canonEnc, static_cast<std::size_t>(name->canonEnclen)) : std::string("null")) + ")";
}

std::string Describe(const LibreSslX509* certificate) {
    if (!certificate) return "null";
    const auto* info = certificate->certificateInfo;
    std::string text = "X(" + Describe(info->version) + "," + Describe(info->serialNumber) + "," + Describe(info->signature) + "," + Describe(info->issuer) + "," +
        Describe(info->validity->notBefore) + "," + Describe(info->validity->notAfter) + "," + Describe(info->subject) + "," + Describe(info->key->algorithm) + "," +
        Describe(info->key->publicKey) + "," + (info->key->key ? "key" : "nokey") + "," + Describe(info->issuerUid) + "," + Describe(info->subjectUid) + "," + DescribeStack(info->extensions);
    for (int index = 0; info->extensions && index < info->extensions->num; ++index) {
        const auto* extension = reinterpret_cast<const LibreSslX509Extension*>(info->extensions->data[index]);
        text += ",E(" + Describe(extension->object) + "," + std::to_string(extension->critical) + "," + Describe(extension->value) + ")";
    }
    text += "," + std::to_string(info->encoding.len) + "," + std::to_string(info->encoding.modified) + "," + Hex(info->encoding.enc, static_cast<std::size_t>(info->encoding.len));
    text += "," + Describe(certificate->signatureAlgorithm) + "," + Describe(certificate->signature) + "," + std::to_string(certificate->valid) + "," + std::to_string(certificate->references) +
        "," + Quoted(certificate->name) + "," + (certificate->exData.stack ? "exdata" : "noexdata") + "," + std::to_string(certificate->pathLength) + "," + std::to_string(certificate->proxyPathLength) +
        "," + std::to_string(certificate->extensionFlags) + "," + std::to_string(certificate->keyUsage) + "," + std::to_string(certificate->extendedKeyUsage) + "," +
        std::to_string(certificate->netscapeCertType) + "," + Hex(certificate->sha1Hash, sizeof certificate->sha1Hash);
    const void* const pointers[] = {certificate->subjectKeyId, certificate->authorityKeyId, certificate->policyCache, certificate->crlDistributionPoints, certificate->alternativeNames,
        certificate->nameConstraints, certificate->auxiliary};
    for (const void* pointer : pointers) text += pointer ? ",set" : ",null";
    return text + ")";
}

std::string Errors(const std::function<unsigned long()>& next) {
    std::string text;
    while (const unsigned long code = next()) text += " " + std::to_string(code);
    return text;
}

void CompareCertificates() {
    for (int round = 0; round < 60000; ++round) {
        faulty = Chance(60);
        Der der = Certificate();
        if (faulty && Chance(70)) der = Mutate(der);
        std::size_t length = der.size();
        if (Fault(6)) length = Pick(static_cast<unsigned>(der.size() + 1));
        if (Chance(3)) der.push_back(static_cast<unsigned char>(generator()));
        der.resize(der.size() + 32);
        const bool reuse = Chance(10);
        LibreSslX509* mine = nullptr;
        void* theirs = nullptr;
        if (reuse) {
            const bool keep = faulty;
            faulty = false;
            Der base;
            for (int attempt = 0; attempt < 20 && (!mine || !theirs); ++attempt) {
                if (mine) X509_free_nid_postfix(mine);
                if (theirs) LibreSslReference::X509Free(theirs);
                mine = nullptr;
                theirs = nullptr;
                base = Certificate();
                const unsigned char* in = base.data();
                d2i_X509_nid_postfix(&mine, &in, static_cast<std::int64_t>(base.size()));
                in = base.data();
                LibreSslReference::D2iX509(&theirs, &in, static_cast<long>(base.size()));
            }
            faulty = keep;
            ERR_clear_error_nid_postfix();
        }
        LibreSslReference::ErrClearError();
        LibreSslX509* const mineBefore = mine;
        void* const theirsBefore = theirs;
        const unsigned char* mineIn = der.data();
        const unsigned char* theirIn = der.data();
        LibreSslX509* mineResult = nullptr;
        const auto error = Attempt([&] { mineResult = d2i_X509_nid_postfix(reuse ? &mine : nullptr, &mineIn, static_cast<std::int64_t>(length)); });
        void* theirResult = LibreSslReference::D2iX509(reuse ? &theirs : nullptr, &theirIn, static_cast<long>(length));
        const std::string mineText = error.empty() ? Describe(mineResult) + " consumed " + std::to_string(mineIn - der.data()) + Errors(ERR_get_error_nid_postfix) +
            (reuse ? std::string(mine == mineBefore ? " same" : mine ? " other" : " cleared") : std::string()) : error;
        const std::string theirText = Describe(static_cast<const LibreSslX509*>(theirResult)) + " consumed " + std::to_string(theirIn - der.data()) + Errors(LibreSslReference::ErrGetError) +
            (reuse ? std::string(theirs == theirsBefore ? " same" : theirs ? " other" : " cleared") : std::string());
        const char* function = reuse ? "d2i_X509 (reused)" : mineResult ? "d2i_X509 (decoded)" : "d2i_X509 (rejected)";
        Record(function, mineText == theirText, [&] { return Hex(der.data(), length) + "\n      mine   " + mineText + "\n      theirs " + theirText; });
        if (error.empty() && mineResult) X509_free_nid_postfix(mineResult);
        if (theirResult) LibreSslReference::X509Free(theirResult);
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
    }
}

struct Saved {
    LibreSslX509Algorithm* algorithm;
    LibreSslAsn1Object* object;
    LibreSslAsn1Type* parameter;
    std::int32_t type;
    void* value;
    Asn1String* bits;
    std::int64_t flags;
    std::int32_t stringType;
};

bool StringParameter(const LibreSslAsn1Type* parameter) {
    return parameter && parameter->type != 1 && parameter->type != 5 && parameter->type != 6 && parameter->value;
}

Saved Save(LibreSslX509PublicKey* key) {
    Saved saved{key->algorithm, key->algorithm->algorithm, key->algorithm->parameter, 0, nullptr, key->publicKey, key->publicKey->flags, 0};
    if (saved.parameter) {
        saved.type = saved.parameter->type;
        saved.value = saved.parameter->value;
        if (StringParameter(saved.parameter)) saved.stringType = static_cast<Asn1String*>(saved.value)->type;
    }
    return saved;
}

void Restore(LibreSslX509PublicKey* key, const Saved& saved) {
    key->algorithm = saved.algorithm;
    key->algorithm->algorithm = saved.object;
    key->algorithm->parameter = saved.parameter;
    key->publicKey = saved.bits;
    key->publicKey->flags = saved.flags;
    if (!saved.parameter) return;
    saved.parameter->type = saved.type;
    saved.parameter->value = saved.value;
    if (saved.stringType) static_cast<Asn1String*>(saved.value)->type = saved.stringType;
}

bool Zero(const Asn1String* string) {
    return std::all_of(string->data, string->data + string->length, [](unsigned char byte) { return byte == 0; });
}

bool Unencodable(const LibreSslAsn1Type* parameter) {
    if (!StringParameter(parameter)) return false;
    const auto* string = static_cast<const Asn1String*>(parameter->value);
    if (!Zero(string)) return false;
    if (parameter->type == 2 || parameter->type == 10) return string->type & 0x100;
    return parameter->type == 3 && string->length && !(string->flags & 0x08);
}

std::string MutateKey(LibreSslX509PublicKey* key, unsigned kind, unsigned choice, std::int64_t number) {
    static constexpr std::int32_t stringTypes[] = {2, 10, 3, 4, 16, 17, -3, 12, 19, 30, 31, 40, 200, 0x1f};
    auto* parameter = key->algorithm->parameter;
    switch (kind) {
    case 1:
        if (!StringParameter(parameter)) return "unchanged";
        parameter->type = stringTypes[choice % std::size(stringTypes)];
        return "type " + std::to_string(parameter->type);
    case 2:
        if (!parameter) return "unchanged";
        parameter->type = choice % 3 == 0 ? 5 : 1;
        parameter->value = reinterpret_cast<void*>(choice % 3 == 2 ? static_cast<std::int64_t>(0xffffffff) : number);
        return "type " + std::to_string(parameter->type) + " value " + std::to_string(reinterpret_cast<std::int64_t>(parameter->value));
    case 3:
        if (!StringParameter(parameter)) return "unchanged";
        static_cast<Asn1String*>(parameter->value)->type ^= 0x100;
        parameter->type = choice % 2 ? 2 : 10;
        return "negated as " + std::to_string(parameter->type);
    case 4:
        key->algorithm->parameter = nullptr;
        return "no parameter";
    case 5:
        key->algorithm->algorithm = nullptr;
        return "no object";
    case 6:
        key->algorithm = nullptr;
        return "no algorithm";
    case 7:
        key->publicKey = nullptr;
        return "no bits";
    case 8:
        key->publicKey->flags = choice % 3 ? 0x08 | (number & 7) : 0;
        return "flags " + std::to_string(key->publicKey->flags);
    default:
        return "unchanged";
    }
}

bool Encodable(const LibreSslX509PublicKey* key) {
    if (key->algorithm && Unencodable(key->algorithm->parameter)) return false;
    return !key->publicKey || key->publicKey->flags & 0x08 || !key->publicKey->length || !Zero(key->publicKey);
}

std::string EncodeKey(void* key, unsigned mode, const std::function<int(void*, unsigned char**)>& encode, const std::function<void(void*)>& release) {
    if (mode == 0) return std::to_string(encode(key, nullptr));
    if (mode == 1) {
        unsigned char* out = nullptr;
        const int length = encode(key, &out);
        std::string text = std::to_string(length) + (out ? " " + Hex(out, static_cast<std::size_t>(std::max(length, 0))) : std::string(" null"));
        if (out) release(out);
        return text;
    }
    Der buffer(4096, 0xcc);
    unsigned char* out = buffer.data() + 3;
    const int length = encode(key, &out);
    return std::to_string(length) + " advanced " + std::to_string(out - buffer.data() - 3) + " " + Hex(buffer.data(), 1096);
}

void CompareKeyEncodings() {
    for (int round = 0; round < 40000; ++round) {
        faulty = Chance(50);
        const Der der = Certificate();
        const unsigned char* in = der.data();
        LibreSslX509* mine = d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(der.size()));
        in = der.data();
        auto* theirs = static_cast<LibreSslX509*>(LibreSslReference::D2iX509(nullptr, &in, static_cast<long>(der.size())));
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
        if (!mine || !theirs) {
            if (mine) X509_free_nid_postfix(mine);
            if (theirs) LibreSslReference::X509Free(theirs);
            continue;
        }
        auto* mineKey = mine->certificateInfo->key;
        auto* theirKey = theirs->certificateInfo->key;
        const Saved mineSaved = Save(mineKey);
        const Saved theirSaved = Save(theirKey);
        const unsigned kind = Chance(40) ? 0 : 1 + Pick(8);
        const unsigned choice = Pick(1000);
        const std::int64_t number = Chance(50) ? Pick(0x300) : static_cast<std::int64_t>(generator() & 0xffffffff);
        const std::string mutation = MutateKey(mineKey, kind, choice, number);
        MutateKey(theirKey, kind, choice, number);
        const unsigned mode = Pick(3);
        const bool none = Chance(3);
        if (Encodable(mineKey)) {
            std::string mineText;
            const auto error = Attempt([&] {
                mineText = EncodeKey(none ? nullptr : mineKey, mode, [](void* key, unsigned char** out) { return i2d_X509_PUBKEY_nid_postfix(static_cast<LibreSslX509PublicKey*>(key), out); },
                    [](void* pointer) { free_nid_postfix(pointer); });
            });
            if (!error.empty()) mineText = error;
            mineText += Errors(ERR_get_error_nid_postfix);
            const std::string theirText = EncodeKey(none ? nullptr : theirKey, mode, LibreSslReference::I2dX509Pubkey, LibreSslReference::Free) + Errors(LibreSslReference::ErrGetError);
            Record("i2d_X509_PUBKEY", mineText == theirText,
                [&] { return Hex(der.data(), der.size()) + " " + mutation + " mode " + std::to_string(mode) + "\n      mine   " + mineText + "\n      theirs " + theirText; });
        }
        Restore(mineKey, mineSaved);
        Restore(theirKey, theirSaved);
        X509_free_nid_postfix(mine);
        LibreSslReference::X509Free(theirs);
    }
}

struct Oid {
    std::string text;
    std::vector<unsigned char> content;
};

Oid RandomOid() {
    static const char* const huge[] = {"18446744073709551535", "18446744073709551616", "1844674407370955161", "99999999999999999999999999", "340282366920938463463374607431768211455"};
    Oid oid;
    const unsigned first = static_cast<unsigned>(generator() % 3);
    oid.text = std::to_string(first);
    const unsigned arcs = 1 + static_cast<unsigned>(generator() % 5);
    bool exact = true;
    for (unsigned index = 0; index < arcs; ++index) {
        oid.text += generator() % 5 ? '.' : ' ';
        if (generator() % 12 == 0) {
            oid.text += huge[generator() % std::size(huge)];
            exact = false;
            continue;
        }
        std::uint64_t value = index == 0 && first < 2 ? generator() % 40 : generator() % 3 ? generator() % 300 : generator() >> (generator() % 64);
        if (index == 0) value += first * 40;
        oid.text += std::to_string(index == 0 ? value - first * 40 : value);
        std::vector<unsigned char> groups;
        do {
            groups.push_back(static_cast<unsigned char>(value & 0x7f));
            value >>= 7;
        } while (value);
        for (std::size_t at = groups.size(); at-- > 1;) oid.content.push_back(groups[at] | 0x80);
        oid.content.push_back(groups[0]);
    }
    if (!exact) oid.content.clear();
    if (generator() % 4 == 0) {
        static constexpr char junk[] = {'.', ' ', 'x', '3', '-', '\x80', '0', '4'};
        const std::size_t at = generator() % (oid.text.size() + 1);
        if (generator() % 2 && at < oid.text.size()) oid.text[at] = junk[generator() % sizeof junk];
        else oid.text.insert(at, 1, junk[generator() % sizeof junk]);
        oid.content.clear();
    }
    if (generator() % 30 == 0) oid.text = std::string("012").substr(0, generator() % 3);
    return oid;
}

std::string Describe(int result, const std::function<unsigned long()>& errors) {
    return std::to_string(result) + Errors(errors);
}

void CompareCreatedObjects() {
    static const char* const names[] = {"alpha", "beta", "CN", "commonName", "", "x"};
    std::vector<std::vector<unsigned char>> probes = {{0x55, 0x04, 0x03}, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}, {0x2a, 0x03, 0x04}, {0x2b}, {0x88, 0x37, 0x05}};
    std::string mineLog;
    std::string theirLog;
    for (int round = 0; round < 20000; ++round) {
        const Oid oid = RandomOid();
        const char* shortName = generator() % 5 ? names[generator() % std::size(names)] : nullptr;
        const char* longName = generator() % 3 ? names[generator() % std::size(names)] : nullptr;
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
        switch (generator() % 60) {
        case 0:
        case 1:
            OBJ_cleanup_nid_postfix();
            LibreSslReference::ObjCleanup();
            break;
        case 2:
            OPENSSL_add_all_algorithms_noconf_nid_postfix();
            LibreSslReference::AddAllAlgorithms();
            break;
        case 3:
            OPENSSL_load_builtin_modules_nid_postfix();
            LibreSslReference::LoadBuiltinModules();
            break;
        case 4:
            CONF_modules_free_nid_postfix();
            LibreSslReference::ConfModulesFree();
            break;
        }
        int mineResult = 0;
        const auto error = Attempt([&] { mineResult = OBJ_create_nid_postfix(oid.text.c_str(), shortName, longName); });
        std::string mineText = error.empty() ? Describe(mineResult, ERR_get_error_nid_postfix) : error;
        std::string theirText = Describe(LibreSslReference::ObjCreate(oid.text.c_str(), shortName, longName), LibreSslReference::ErrGetError);
        if (!oid.content.empty() && probes.size() < 64) probes.push_back(oid.content);
        for (int probe = 0; probe < 4; ++probe) {
            std::vector<unsigned char> data = probes[generator() % probes.size()];
            LibreSslAsn1Object object{nullptr, nullptr, generator() % 4 ? 0 : static_cast<int>(940 + generator() % 60), static_cast<int>(data.size()), data.data(), 0};
            const int space = static_cast<int>(generator() % 40);
            char mine[48];
            char theirs[48];
            std::memset(mine, 0x5a, sizeof mine);
            std::memset(theirs, 0x5a, sizeof theirs);
            const int mineLength = i2t_ASN1_OBJECT_nid_postfix(mine, space, &object);
            mineText += " | " + std::to_string(mineLength) + " " + Hex(mine, sizeof mine) + Errors(ERR_get_error_nid_postfix);
            const int theirLength = LibreSslReference::I2tAsn1Object(theirs, space, &object);
            theirText += " | " + std::to_string(theirLength) + " " + Hex(theirs, sizeof theirs) + Errors(LibreSslReference::ErrGetError);
        }
        Record("OBJ_create", mineText == theirText, [&] { return Quoted(oid.text.c_str()) + " " + Quoted(shortName) + " " + Quoted(longName) + "\n      mine   " + mineText + "\n      theirs " + theirText; });
    }
    OBJ_cleanup_nid_postfix();
    LibreSslReference::ObjCleanup();
}

struct VerifyKey {
    std::vector<unsigned char> der;
    void* referenceKey;
    void* referencePkey;
    void* minePkey;
};

int DigestNid(int kind) {
    switch (kind) {
    case 1: return 4;
    case 2: return 64;
    case 3: return 675;
    case 4: return 672;
    case 5: return 673;
    default: return 674;
    }
}

std::size_t DigestBytes(int kind) {
    switch (kind) {
    case 1: return 16;
    case 2: return 20;
    case 3: return 28;
    case 4: return 32;
    case 5: return 48;
    default: return 64;
    }
}

const EvpMd* EmulatorDigest(int kind) {
    switch (kind) {
    case 1: return EVP_md5_nid_postfix();
    case 4: return EVP_sha256_nid_postfix();
    case 5: return EVP_sha384_nid_postfix();
    case 6: return EVP_sha512_nid_postfix();
    default: return nullptr;
    }
}

std::string CryptoErrors(const std::function<unsigned long()>& next) {
    std::string text;
    while (const unsigned long code = next()) {
        const std::uint64_t library = (code >> 24) & 0xff;
        if (library != 4 && library != 6) continue;
        text += " " + std::to_string(code);
    }
    return text;
}

void CompareDigestVerify() {
    LibreSslReference::AddAllAlgorithms();
    std::vector<VerifyKey> keys;
    for (const int bits : {512, 1024, 2048}) {
        void* rsa = LibreSslReference::GenerateRsa(bits);
        if (!rsa) continue;
        void* referencePkey = LibreSslReference::PkeyNew();
        if (!referencePkey || LibreSslReference::PkeySet1Rsa(referencePkey, rsa) != 1) {
            Record("EVP_DigestVerifyInit", false, [&] { return std::to_string(bits) + " bit key: the reference did not keep the RSA key"; });
            LibreSslReference::RsaFree(rsa);
            continue;
        }
        unsigned char* der = nullptr;
        const int length = LibreSslReference::I2dRsaPubKey(rsa, &der);
        if (length <= 0 || !der) {
            Record("EVP_DigestVerifyInit", false, [&] { return std::to_string(bits) + " bit key: the reference wrote no public key"; });
            LibreSslReference::RsaFree(rsa);
            LibreSslReference::PkeyFree(referencePkey);
            continue;
        }
        VerifyKey key {{der, der + length}, rsa, referencePkey, nullptr};
        LibreSslReference::ReferenceFree(der);
        const unsigned char* cursor = key.der.data();
        LibreSslRsa* mineRsa = nullptr;
        const auto error = Attempt([&] { mineRsa = d2i_RSA_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<long>(key.der.size())); });
        if (!error.empty() || !mineRsa) {
            Record("EVP_DigestVerifyInit", false, [&] { return std::to_string(bits) + " bit key: " + (error.empty() ? std::string("the public key was not read") : error); });
            LibreSslReference::RsaFree(rsa);
            LibreSslReference::PkeyFree(referencePkey);
            continue;
        }
        key.minePkey = EVP_PKEY_new_nid_postfix();
        const int taken = key.minePkey ? EVP_PKEY_set1_RSA_nid_postfix(key.minePkey, mineRsa) : 0;
        RSA_free_nid_postfix(mineRsa);
        if (taken != 1) {
            Record("EVP_DigestVerifyInit", false, [&] { return std::to_string(bits) + " bit key: the emulator did not keep the RSA key"; });
            LibreSslReference::RsaFree(rsa);
            LibreSslReference::PkeyFree(referencePkey);
            continue;
        }
        keys.push_back(key);
    }
    if (keys.empty()) {
        Record("EVP_DigestVerifyInit", false, [] { return std::string("no reference RSA key was generated"); });
        return;
    }
    static const int verifiable[] = {1, 4, 5, 6, 7};
    std::array<unsigned char, 256> hashState {};
    std::array<unsigned char, 64> hash {};
    std::array<unsigned char, 512> signature {};
    std::array<unsigned char, 512> attempt {};
    std::vector<std::uint64_t> theirs((LibreSslReference::MdContextSize() + 7) / 8);
    for (int round = 0; round < 900; ++round) {
        const VerifyKey& key = keys[static_cast<std::size_t>(round) % keys.size()];
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
        const int signKind = 1 + static_cast<int>(generator() % 6);
        const int verifyKind = verifiable[generator() % (sizeof verifiable / sizeof verifiable[0])];
        const auto message = Bytes(1 + static_cast<std::size_t>(generator() % 120));
        auto signedBytes = message;
        if (generator() % 3 == 0) signedBytes = Bytes(signedBytes.size());
        LibreSslReference::DigestInit(signKind, hashState.data());
        LibreSslReference::DigestUpdate(signKind, hashState.data(), signedBytes.data(), signedBytes.size());
        LibreSslReference::DigestFinal(signKind, hash.data(), hashState.data());
        unsigned int signatureLength = 0;
        if (LibreSslReference::RsaSign(DigestNid(signKind), hash.data(), static_cast<unsigned int>(DigestBytes(signKind)), signature.data(), &signatureLength,
                key.referenceKey) != 1 || signatureLength == 0 || static_cast<std::size_t>(signatureLength) > signature.size())
            continue;
        attempt = signature;
        std::size_t attemptLength = signatureLength;
        switch (static_cast<int>(generator() % 8)) {
        case 0:
            break;
        case 1:
            attempt[generator() % attemptLength] ^= static_cast<unsigned char>(1ull << (generator() % 8));
            break;
        case 2:
            attemptLength = generator() % attemptLength;
            break;
        case 3:
            attemptLength += 1 + generator() % 4;
            break;
        case 4:
            attemptLength = 0;
            break;
        case 5:
            attempt[attemptLength - 1] = static_cast<unsigned char>(generator());
            break;
        case 6:
            std::swap(attempt[0], attempt[attemptLength - 1]);
            break;
        default:
            for (std::size_t at = 0; at + 8 < attemptLength; at += 8) std::swap(attempt[at], attempt[at + 8]);
            break;
        }
        const std::string caseText = "sign " + std::to_string(signKind) + " verify " + std::to_string(verifyKind) + " length " + std::to_string(attemptLength)
            + " of " + std::to_string(signatureLength) + " signature " + Hex(attempt.data(), std::min<std::size_t>(12, attemptLength));
        EvpMdContext mine {};
        void* minePctx = nullptr;
        void* theirPctx = nullptr;
        std::string mineText;
        std::string theirText;
        int mineResult = 0;
        auto mineError = Attempt([&] { mineResult = EVP_DigestVerifyInit_nid_postfix(&mine, &minePctx, EmulatorDigest(verifyKind), nullptr, key.minePkey); });
        int theirResult = LibreSslReference::DigestVerifyInit(theirs.data(), &theirPctx,
            verifyKind == 7 ? nullptr : LibreSslReference::DigestMethod(verifyKind), key.referencePkey);
        mineText = (mineError.empty() ? std::to_string(mineResult) : mineError) + (minePctx ? " pctx" : " none") + CryptoErrors(ERR_get_error_nid_postfix);
        theirText = std::to_string(theirResult) + (theirPctx ? " pctx" : " none") + CryptoErrors(LibreSslReference::ErrGetError);
        Record("EVP_DigestVerifyInit", mineError.empty() && mineText == theirText,
            [&] { return caseText + " message " + std::to_string(message.size()) + "\n      mine   " + mineText + "\n      theirs " + theirText; });
        if (round % 5 == 0 && mineError.empty() && mineResult == 1 && theirResult == 1) {
            mineResult = 0;
            mineError = Attempt([&] { mineResult = EVP_DigestVerifyInit_nid_postfix(&mine, &minePctx, EmulatorDigest(verifyKind), nullptr, key.minePkey); });
            theirResult = LibreSslReference::DigestVerifyInit(theirs.data(), &theirPctx,
                verifyKind == 7 ? nullptr : LibreSslReference::DigestMethod(verifyKind), key.referencePkey);
            mineText = (mineError.empty() ? std::to_string(mineResult) : mineError) + (minePctx ? " pctx" : " none") + CryptoErrors(ERR_get_error_nid_postfix);
            theirText = std::to_string(theirResult) + (theirPctx ? " pctx" : " none") + CryptoErrors(LibreSslReference::ErrGetError);
            Record("EVP_DigestVerifyInit again", mineError.empty() && mineText == theirText,
                [&] { return caseText + "\n      mine   " + mineText + "\n      theirs " + theirText; });
        }
        if (mineError.empty() && mineResult == 1 && theirResult == 1) {
            const std::size_t split = generator() % (message.size() + 1);
            mineResult = 0;
            mineError = Attempt([&] {
                mineResult = EVP_DigestUpdate_nid_postfix(&mine, message.data(), split) &&
                    EVP_DigestUpdate_nid_postfix(&mine, message.data() + split, message.size() - split);
            });
            theirResult = LibreSslReference::DigestUpdateContext(theirs.data(), message.data(), split) &&
                LibreSslReference::DigestUpdateContext(theirs.data(), message.data() + split, message.size() - split);
            mineText = (mineError.empty() ? std::to_string(mineResult) : mineError) + CryptoErrors(ERR_get_error_nid_postfix);
            theirText = std::to_string(theirResult) + CryptoErrors(LibreSslReference::ErrGetError);
            Record("EVP_DigestVerifyUpdate", mineError.empty() && mineText == theirText,
                [&] { return caseText + " split " + std::to_string(split) + "\n      mine   " + mineText + "\n      theirs " + theirText; });
            if (mineError.empty() && mineResult == 1 && theirResult == 1) {
                mineResult = 0;
                mineError = Attempt([&] { mineResult = EVP_DigestVerifyFinal_nid_postfix(&mine, attempt.data(), attemptLength); });
                theirResult = LibreSslReference::DigestVerifyFinal(theirs.data(), attempt.data(), attemptLength);
                mineText = (mineError.empty() ? std::to_string(mineResult) : mineError) + CryptoErrors(ERR_get_error_nid_postfix);
                theirText = std::to_string(theirResult) + CryptoErrors(LibreSslReference::ErrGetError);
                Record("EVP_DigestVerifyFinal", mineError.empty() && mineText == theirText,
                    [&] { return caseText + "\n      mine   " + mineText + "\n      theirs " + theirText; });
            }
        }
        mineResult = 0;
        mineError = Attempt([&] { mineResult = EVP_MD_CTX_cleanup_nid_postfix(&mine); });
        theirResult = LibreSslReference::MdContextCleanup(theirs.data());
        mineText = (mineError.empty() ? std::to_string(mineResult) : mineError) + CryptoErrors(ERR_get_error_nid_postfix);
        theirText = std::to_string(theirResult) + CryptoErrors(LibreSslReference::ErrGetError);
        Record("EVP_MD_CTX_cleanup after verify", mineError.empty() && mineText == theirText,
            [&] { return caseText + "\n      mine   " + mineText + "\n      theirs " + theirText; });
    }
    for (const auto& key : keys) {
        LibreSslReference::PkeyFree(key.referencePkey);
        LibreSslReference::RsaFree(key.referenceKey);
        EVP_PKEY_free_nid_postfix(key.minePkey);
    }
}

struct CipherListContextInternal {
    unsigned char beforeCipherListById[256];
    void* cipherListById;
    unsigned char rest[200];
};

struct CipherListContext {
    const void* method;
    void* cipherList;
    unsigned char beforeInternal[96];
    CipherListContextInternal* internal;
};

static_assert(sizeof(CipherListContextInternal) == sizeof(LibreSslCtxInternal) && offsetof(CipherListContextInternal, cipherListById) == offsetof(LibreSslCtxInternal, cipherListById));
static_assert(sizeof(CipherListContext) == sizeof(LibreSslCtx) && offsetof(CipherListContext, cipherList) == offsetof(LibreSslCtx, cipherList) && offsetof(CipherListContext, internal) == offsetof(LibreSslCtx, internal));

void FreeCipherStack(void* pointer) {
    auto* stack = static_cast<LibreSslStack*>(pointer);
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

std::string JoinedCipherNames(const void* list) {
    const auto* stack = static_cast<const LibreSslStack*>(list);
    if (!stack) return "(null)";
    std::string names;
    for (int index = 0; index < sk_num_nid_postfix(stack); ++index) {
        if (index) names += ':';
        names += SSL_CIPHER_get_name_nid_postfix(sk_value_nid_postfix(stack, index));
    }
    return names;
}

bool ByIdStackConsistent(const CipherListContext& ctx) {
    const auto* ordered = static_cast<const LibreSslStack*>(ctx.cipherList);
    const auto* byId = static_cast<const LibreSslStack*>(ctx.internal->cipherListById);
    if (!ordered || !byId || ordered == byId || ordered->num != byId->num || byId->sorted != 1 || !byId->comp || ordered->sorted != 0 || ordered->comp) return false;
    for (int index = 0; index < ordered->num; ++index) {
        if (std::find(byId->data, byId->data + byId->num, ordered->data[index]) == byId->data + byId->num) return false;
        if (index && byId->comp(&byId->data[index - 1], &byId->data[index]) >= 0) return false;
    }
    return true;
}

std::string PrintableRules(const std::string& rules) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text = "\"";
    for (std::size_t index = 0; index < rules.size() && index < 160; ++index) {
        const auto byte = static_cast<unsigned char>(rules[index]);
        if (byte >= 0x20 && byte < 0x7f) {
            text += static_cast<char>(byte);
        } else {
            text += "\\x";
            text += digits[byte >> 4];
            text += digits[byte & 15];
        }
    }
    if (rules.size() > 160) text += "...(" + std::to_string(rules.size()) + " bytes)";
    return text + "\"";
}

void CompareCipherList(const std::string* rules) {
    std::string padded;
    if (rules) {
        padded = *rules;
        padded.append(16, '\0');
    }
    const char* text = rules ? padded.c_str() : nullptr;
    CipherListContextInternal internal{};
    CipherListContext ctx{};
    ctx.internal = &internal;
    auto* fake = reinterpret_cast<LibreSslCtx*>(&ctx);
    ERR_clear_error_nid_postfix();
    LibreSslReference::ErrClearError();
    int initial = 0;
    const auto initialError = Attempt([&] { initial = SSL_CTX_set_cipher_list_nid_postfix(fake, "ALL:!aNULL:!eNULL:!SSLv2"); });
    ERR_clear_error_nid_postfix();
    int mineResult = 0;
    const auto error = Attempt([&] { mineResult = SSL_CTX_set_cipher_list_nid_postfix(fake, text); });
    std::string mineText = error.empty() ? Describe(mineResult, ERR_get_error_nid_postfix) + " " + JoinedCipherNames(ctx.cipherList) : error;
    char buffer[16384];
    const int theirResult = LibreSslReference::CipherListNames(text, buffer, sizeof buffer);
    const std::string theirText = Describe(theirResult, LibreSslReference::ErrGetError) + " " + buffer;
    const bool consistent = !error.empty() || ByIdStackConsistent(ctx);
    Record("SSL_CTX_set_cipher_list", initialError.empty() && initial == 1 && consistent && mineText == theirText,
        [&] { return (rules ? PrintableRules(*rules) : std::string("null")) + (initialError.empty() && initial == 1 ? "" : " (default list failed)") + (consistent ? "" : " (by-id stack inconsistent)") + "\n      mine   " + mineText + "\n      theirs " + theirText; });
    FreeCipherStack(ctx.cipherList);
    FreeCipherStack(internal.cipherListById);
    ERR_clear_error_nid_postfix();
    LibreSslReference::ErrClearError();
}

std::string Repeated(const std::string& unit, int count) {
    std::string text;
    for (int index = 0; index < count; ++index) text += unit;
    return text;
}

std::string RandomCipherRules(const std::vector<std::string>& aliases, const std::vector<std::string>& ciphers) {
    static const char* const prefixes[] = {"", "", "", "", "!", "-", "+"};
    static const char* const separators[] = {":", ":", ":", ":", " ", ",", ";", "::", ": "};
    static const char junk[] = {'$', '#', '*', '&', '%', '/', '\\', '"', '\'', '(', ')', '[', '{', '<', '=', '?', '~', '`', '^', '|', '_', '\x01', '\x7f', '\x80', '\xff', '@', '+', '-', '!', '.'};
    const auto alias = [&]() -> const std::string& { return aliases[generator() % aliases.size()]; };
    const auto cipher = [&]() -> const std::string& { return ciphers[generator() % ciphers.size()]; };
    std::string rules;
    if (generator() % 12 == 0) {
        rules = "DEFAULT";
        if (generator() % 4) rules += separators[generator() % std::size(separators)];
    }
    const int tokens = 1 + static_cast<int>(generator() % 8);
    for (int index = 0; index < tokens; ++index) {
        if (index && !rules.empty()) rules += separators[generator() % std::size(separators)];
        const unsigned kind = static_cast<unsigned>(generator() % 24);
        if (kind == 0) {
            rules += "@STRENGTH";
            continue;
        }
        rules += prefixes[generator() % std::size(prefixes)];
        if (kind < 8) {
            rules += alias();
        } else if (kind < 15) {
            rules += cipher();
        } else if (kind < 19) {
            const int parts = 2 + static_cast<int>(generator() % 3);
            for (int part = 0; part < parts; ++part) {
                if (part) rules += '+';
                rules += alias();
            }
        } else if (kind == 19) {
            rules += alias() + "+" + cipher();
        } else if (kind == 20) {
            const std::string& name = cipher();
            rules += name.substr(0, 1 + generator() % name.size());
        } else if (kind == 21) {
            std::string name = alias();
            for (char& letter : name) if (generator() % 3 == 0 && letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter + 32);
            rules += name;
        } else if (kind == 22) {
            rules += "NOSUCH" + std::to_string(generator() % 10);
        } else {
            rules += alias() + "@STRENGTH";
        }
    }
    if (generator() % 6 == 0) {
        rules += separators[generator() % std::size(separators)];
        rules += "@STRENGTH";
    }
    if (generator() % 40 == 0) rules += prefixes[4 + generator() % 3];
    if (generator() % 40 == 0) rules += '@';
    if (generator() % 50 == 0) rules.insert(generator() % (rules.size() + 1), 1, junk[generator() % sizeof junk]);
    return rules;
}

void CompareCipherTable() {
    int theirCount = 0;
    for (;; ++theirCount) {
        unsigned char theirs[88];
        const int copied = LibreSslReference::CipherFields(theirCount, theirs, sizeof theirs);
        if (copied <= 0) break;
        std::uint64_t id = 0;
        std::memcpy(&id, theirs + 16, sizeof id);
        const auto* mine = static_cast<const unsigned char*>(LibreSslCipherById_nid_no_patch(id));
        const char* theirName = nullptr;
        std::memcpy(&theirName, theirs + 8, sizeof theirName);
        Record("SSL_CIPHER table", copied == 88 && mine && std::memcmp(mine, theirs, 8) == 0 && std::memcmp(mine + 16, theirs + 16, 72) == 0 &&
            std::strcmp(SSL_CIPHER_get_name_nid_postfix(mine), theirName) == 0,
            [&] { return std::string(theirName ? theirName : "null") + ": " + (mine ? Hex(mine, 88) : std::string("missing")) + " vs " + Hex(theirs, 88); });
    }
    Record("SSL_CIPHER table size", theirCount == LibreSslCipherCount_nid_no_patch(),
        [&] { return std::to_string(LibreSslCipherCount_nid_no_patch()) + " vs " + std::to_string(theirCount); });
}

void CompareCipherLists() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const std::vector<std::string> aliases = {"ALL", "COMPLEMENTOFALL", "COMPLEMENTOFDEFAULT", "kRSA", "kEDH", "DH", "kEECDH", "ECDH", "kGOST", "aRSA", "aDSS", "DSS", "aNULL", "aECDSA", "ECDSA",
        "aGOST01", "aGOST", "DHE", "EDH", "ECDHE", "EECDH", "NULL", "RSA", "ADH", "AECDH", "DES", "3DES", "RC4", "IDEA", "eNULL", "AES128", "AES256", "AES", "AESGCM", "CAMELLIA128", "CAMELLIA256",
        "CAMELLIA", "CHACHA20", "AEAD", "MD5", "SHA1", "SHA", "GOST94", "GOST89MAC", "SHA256", "SHA384", "STREEBOG256", "SSLv3", "TLSv1", "TLSv1.2", "LOW", "MEDIUM", "HIGH", "SSLv2", "EXPORT",
        "EXP", "GOST", "PSK", "SRP", "kPSK", "kSRP", "SEED", "RC2", "DEFAULT", "TLSv1.1", "DTLSv1", "FIPS", "kDHE", "kECDHE", "aDH", "kDH", "STRENGTH"};
    std::vector<std::string> ciphers;
    for (int index = 0; index < LibreSslCipherCount_nid_no_patch(); ++index) ciphers.emplace_back(SSL_CIPHER_get_name_nid_postfix(LibreSslCipherAt_nid_no_patch(static_cast<unsigned int>(index))));
    const std::string everyCipher = [&] {
        std::string joined;
        for (const auto& name : ciphers) joined += (joined.empty() ? "" : ":") + name;
        return joined;
    }();
    std::string reversedCiphers;
    for (auto name = ciphers.rbegin(); name != ciphers.rend(); ++name) reversedCiphers += (reversedCiphers.empty() ? "" : ":") + *name;
    std::vector<std::string> fixed = {
        "DEFAULT", "ALL", "HIGH", "MEDIUM", "LOW", "EXPORT", "EXP", "COMPLEMENTOFALL", "COMPLEMENTOFDEFAULT", "ALL:!aNULL:!eNULL:!SSLv2", "HIGH:!aNULL:!MD5",
        "kEECDH+AESGCM:kEDH+AESGCM", "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256", "EECDH+AES:EDH+AES:!SHA1", "AESGCM", "CHACHA20", "GOST", "CAMELLIA", "3DES", "RC4",
        "SHA256", "SHA384", "TLSv1.2", "TLSv1", "SSLv3", "SSLv2", "aRSA", "aECDSA", "kRSA", "kDHE", "kEDH", "kECDHE", "kEECDH", "ECDH", "DH", "PSK", "SRP", "NULL", "eNULL", "aNULL", "ADH", "AECDH",
        "IDEA", "DES", "SEED", "AES", "AES128", "AES256", "aDSS", "DSS", "kGOST", "aGOST", "aGOST01", "GOST94", "GOST89MAC", "STREEBOG256", "AEAD", "MD5", "SHA1", "SHA", "RSA", "ECDHE", "EECDH", "DHE",
        "EDH", "ECDSA", "CAMELLIA128", "CAMELLIA256", "@STRENGTH", "ALL:@STRENGTH", "DEFAULT:@STRENGTH", "HIGH:@STRENGTH", "ALL:!aNULL:@STRENGTH", "HIGH:MEDIUM:LOW:@STRENGTH", "LOW:MEDIUM:HIGH",
        "AES128-SHA:AES256-SHA:@STRENGTH", "AES256-SHA:AES128-SHA:@STRENGTH", "ALL:+RC4", "ALL:-RC4", "ALL:!RC4", "ALL:-ALL", "ALL:!ALL", "-ALL:AES128-SHA", "AES128-SHA:-ALL", "+ALL", "!ALL", "-ALL",
        "!", "+", "-", "@", "@FOO", "@STRENGTHX", "@STRENGTH+AES", "@STRENGTH:ALL", "@STRENGTH@FOO", "@STRENGTH.X", "ALL:@STRENGTH:AES256-SHA", "", " ", "  ", ":::", ";;;", ",,,", " : ; , ",
        "AES128-SHA AES256-SHA", "AES128-SHA,AES256-SHA", "AES128-SHA;AES256-SHA", "AES128-SHA:AES256-SHA", "AES256-SHA:AES128-SHA", "AES128-SHA:AES256-SHA:-AES128-SHA",
        "AES128-SHA:AES256-SHA:!AES128-SHA:AES128-SHA", "AES128-SHA:AES256-SHA:+AES128-SHA", "AES128-SHA:AES256-SHA:-AES128-SHA:AES128-SHA", "AES128-SHA:AES256-SHA:-AES128-SHA:+AES128-SHA",
        "aes128-sha", "Aes128-Sha", "all", "default", "NOSUCHCIPHER", "AES128-SH", "AES128-SHAX", "AES128-SHA-", "AES128-SHA.", "AES128-SHA\x01X", "AES128-SHA\xff", "AES128-SHA$", "$", "$$", "AES128-SHA:$:AES256-SHA",
        "AES128-SHA:$:@FOO", "..", ".", "A.B", "-.", "TLSv1.2:TLSv1.1", "DEFAULT:AES128-SHA", "DEFAULT:-AES128-SHA", "DEFAULT:!AES128-SHA", "DEFAULT:+AES128-SHA", "DEFAULTX", "DEFAULT:", "DEFAULT@FOO",
        "DEFAULT:@FOO", "DEFAULT ALL", "DEFAULT,AES128-SHA", "DEFAULT:ECDHE-RSA-NULL-SHA", "DEFAULT:eNULL", "DEFAULT:ADH", "DEFAULT:-ALL", "DEFAULT:!ALL", "DEFAULT:DEFAULT", "DEFAULT:ALL:@STRENGTH",
        "ECDHE-RSA-AES256-GCM-SHA384", "ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-CHACHA20-POLY1305", "AES128+kRSA", "kRSA+AES128", "AES+RC4", "AES128+AES256", "AES+", "+AES", "kEECDH+aRSA+AES256",
        "kEECDH+aRSA+AES256+SHA384", "ALL+aNULL", "ALL+eNULL", "ALL:!aNULL+kRSA", "HIGH+AES", "HIGH+MEDIUM", "LOW+MEDIUM", "MEDIUM+HIGH", "TLSv1.2+AES", "SSLv3+TLSv1.2", "TLSv1+TLSv1.2", "DES-CBC3-SHA",
        "RC4-SHA:RC4-MD5", "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256",
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384",
        "ECDHE-RSA-AES128-SHA256:!aNULL:!MD5:!RC4", "!aNULL:!eNULL:!LOW:!EXP:!RC4:!MD5:!SSLv2:!3DES:@STRENGTH", "ALL:!aNULL:!eNULL:!SSLv2:!LOW:!EXP:!RC4:!MD5:@STRENGTH", "kEECDH:kEDH:!aNULL:!eNULL",
        "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!SRP:!CAMELLIA", "AES256+EECDH:AES256+EDH:!aNULL", "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS",
        "RSA+3DES:!aNULL", "TLSv1.2+kEECDH+AESGCM", "ALL:-TLSv1.2", "ALL:!TLSv1", "ALL:!SSLv3", "ALL:!AEAD", "ALL:!SHA1:!SHA256", "ALL:+AEAD", "ALL:-AES:+AES128",
        everyCipher, reversedCiphers, Repeated("ALL:", 400), Repeated("AES128-SHA:", 1000), Repeated("ALL+", 300) + "ALL", std::string(5000, 'A'), Repeated("A", 100) + ":AES128-SHA", Repeated(":", 3000) + "AES128-SHA",
        everyCipher + ":" + everyCipher, "-" + Repeated("ALL:-", 200) + "ALL", Repeated("!AES128-SHA:", 50) + "ALL"};
    CompareCipherList(nullptr);
    for (const auto& rules : fixed) CompareCipherList(&rules);
    for (const auto& cipher : ciphers) {
        for (const char* prefix : {"", "!", "-", "+"}) {
            const std::string rules = std::string(prefix) + cipher;
            CompareCipherList(&rules);
        }
        const std::string pair = cipher + ":" + ciphers[generator() % ciphers.size()];
        CompareCipherList(&pair);
    }
    for (const auto& alias : aliases) {
        for (const char* prefix : {"", "!", "-", "+"}) {
            const std::string rules = std::string(prefix) + alias;
            CompareCipherList(&rules);
        }
        const std::string strength = "ALL:" + alias + ":@STRENGTH";
        CompareCipherList(&strength);
    }
    for (int round = 0; round < 2500; ++round) {
        const std::string rules = RandomCipherRules(aliases, ciphers);
        CompareCipherList(&rules);
    }
}

}

int main() {
    std::array<void*, 10> heap {};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    CompareAesKeys();
    CompareAesModes();
    CompareDigests();
    CompareDigestVerify();
    CompareDesParity();
    CompareStrings();
    CompareCertificates();
    CompareKeyEncodings();
    CompareCreatedObjects();
    CompareCipherLists();
    CompareCipherTable();
    long cases = 0;
    long differing = 0;
    for (const auto& [function, count] : counts) {
        cases += count.cases;
        differing += count.differing;
        std::printf("%-36s %6ld cases %6ld differ\n", function.c_str(), count.cases, count.differing);
        for (const auto& sample : count.samples) std::printf("    %s\n", sample.c_str());
    }
    std::printf("LibreSSL 2.5.4 oracle: %ld cases, %ld differ\n", cases, differing);
    return differing == 0 ? 0 : 1;
}
