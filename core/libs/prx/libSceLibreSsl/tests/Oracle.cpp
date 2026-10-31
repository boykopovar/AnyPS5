#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
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
void* X509GetExtD2i(void* certificate, int nid, int* critical, int* index);
std::string ExtensionDer(int nid, void* value);
void ExtensionFree(int nid, void* value);
int ObjCreate(const char* oid, const char* shortName, const char* longName);
void ObjCleanup();
int I2tAsn1Object(char* buffer, int length, const void* object);
unsigned long ErrGetError();
void ErrClearError();
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

bool SameContext(int kind, const std::vector<unsigned char>& mine, const std::vector<unsigned char>& theirs) {
    if (kind < 5) return mine == theirs;
    const auto num = offsetof(Sha512Context, num);
    const auto data = offsetof(Sha512Context, data);
    std::uint32_t used = 0;
    std::memcpy(&used, theirs.data() + num, sizeof used);
    const std::size_t end = data + std::min<std::size_t>(used, sizeof(Sha512Context::data));
    return std::memcmp(mine.data(), theirs.data(), end) == 0 && std::memcmp(mine.data() + num, theirs.data() + num, mine.size() - num) == 0;
}

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
            Record(name + "_Init", error.empty() && mineResult == theirResult && SameContext(kind, mine, theirs), [&] { return (error.empty() ? std::to_string(mineResult) : error) + " vs " + std::to_string(theirResult) + " context " + Hex(mine.data(), 32) + " vs " + Hex(theirs.data(), 32); });
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
                if (same && (mineResult != theirResult || !SameContext(kind, mine, theirs))) {
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
    text += "," + std::to_string(info->encoding.length) + "," + std::to_string(info->encoding.modified) + "," + Hex(info->encoding.data, static_cast<std::size_t>(info->encoding.length));
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
        if (generator() % 25 == 0) {
            OBJ_cleanup_nid_postfix();
            LibreSslReference::ObjCleanup();
        }
        const Oid oid = RandomOid();
        const char* shortName = generator() % 5 ? names[generator() % std::size(names)] : nullptr;
        const char* longName = generator() % 3 ? names[generator() % std::size(names)] : nullptr;
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
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

Der Ia5() { return Characters(Pick(9)); }

Der GeneralizedTimeValue(unsigned char tag) {
    return Split(tag, !Fault(20) ? Der{'2', '0', '2', '0', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z'} : Characters(Pick(16)));
}

Der Optional(unsigned percent, const Der& value) { return Chance(percent) ? value : Der{}; }

Der GeneralName() {
    switch (Pick(Fault(25) ? 12 : 9)) {
    case 0: return Wrap(0xa0, Join({Wrap(0x06, ObjectContent()), Wrap(0xa0, NameValue())}));
    case 1: return Split(0x81, Ia5());
    case 2: return Split(0x82, Ia5());
    case 3: return Wrap(0xa3, Optional(70, Wrap(0x02, Integer())));
    case 4: return Wrap(0xa4, Name());
    case 5: return Wrap(0xa5, Join({Optional(30, Wrap(0xa0, NameValue())), Optional(30, Wrap(0x81, Ia5()))}));
    case 6: return Split(0x86, Ia5());
    case 7: return Split(0x87, Characters(Chance(50) ? 4 : 16));
    case 8: return Wrap(0x88, ObjectContent());
    case 9: return Wrap(0x89, Characters(Pick(3)));
    case 10: return NameValue();
    default: return Wrap(0xa4, NameValue());
    }
}

Der GeneralNameList() {
    Der list;
    const unsigned count = Fault(10) ? 0 : 1 + Pick(3);
    for (unsigned index = 0; index < count; ++index) list = Join({list, GeneralName()});
    return list;
}

Der DistPointName() {
    if (Chance(70)) return Wrap(0xa0, GeneralNameList());
    Der entries;
    const unsigned count = Pick(3);
    for (unsigned index = 0; index < count; ++index) entries = Join({entries, Wrap(0x30, Join({Wrap(0x06, ObjectContent()), NameValue()}))});
    return Wrap(0xa1, entries);
}

Der Repeat(Der (*element)(), unsigned most) {
    Der list;
    const unsigned count = Pick(most + 1);
    for (unsigned index = 0; index < count; ++index) list = Join({list, element()});
    return Wrap(0x30, list);
}

Der AccessDescription() { return Wrap(0x30, Join({Wrap(0x06, ObjectContent()), GeneralName()})); }
Der Subtree() { return Wrap(0x30, Join({GeneralName(), Optional(30, Split(0x80, Integer())), Optional(30, Split(0x81, Integer()))})); }
Der PolicyMapping() { return Wrap(0x30, Join({Wrap(0x06, ObjectContent()), Wrap(0x06, ObjectContent())})); }
Der SxnetId() { return Wrap(0x30, Join({Split(0x02, Integer()), Split(0x04, Characters(Pick(5)))})); }
Der Usage() { return Wrap(0x06, ObjectContent()); }
Der DistPoint() {
    return Wrap(0x30, Join({Optional(70, Wrap(0xa0, DistPointName())), Optional(30, Split(0x81, BitContent())), Optional(30, Wrap(0xa2, GeneralNameList()))}));
}

Der Flag(unsigned char tag) { return Wrap(tag, !Fault(15) ? Der{static_cast<unsigned char>(Chance(50) ? 0xff : 0x00)} : Characters(Pick(3))); }

Der ExtensionContent(int nid) {
    switch (nid) {
    case 71:
    case 83: return Split(0x03, BitContent());
    case 82: return Split(0x04, Characters(Pick(9)));
    case 84: return Wrap(0x30, Join({Optional(60, GeneralizedTimeValue(0x80)), Optional(60, GeneralizedTimeValue(0x81))}));
    case 85:
    case 86:
    case 771: return Wrap(0x30, GeneralNameList());
    case 87: return Wrap(0x30, Join({Optional(60, Value(0x01)), Optional(60, Split(0x02, Integer()))}));
    case 88:
    case 140:
    case 748: return Split(0x02, Integer());
    case 90: return Wrap(0x30, Join({Optional(60, Split(0x80, Characters(Pick(5)))), Optional(50, Wrap(0xa1, GeneralNameList())), Optional(50, Split(0x82, Integer()))}));
    case 103:
    case 857: return Repeat(DistPoint, 2);
    case 126:
    case 368: return Repeat(Usage, 3);
    case 141: return Split(0x0a, Integer());
    case 142:
    case 370: return GeneralizedTimeValue(0x18);
    case 143: return Wrap(0x30, Join({Split(0x02, Integer()), Repeat(SxnetId, 2)}));
    case 177:
    case 398: return Repeat(AccessDescription, 2);
    case 366: return Characters(Pick(10));
    case 367: return Wrap(0x30, Join({Optional(50, Wrap(0xa0, Split(0x16, Ia5()))), Optional(50, Wrap(0xa1, Split(0x02, Integer()))), Optional(50, Wrap(0xa2, GeneralizedTimeValue(0x18)))}));
    case 369: return Wrap(0x05, !Fault(20) ? Der{} : Der{0x00});
    case 371: return Wrap(0x30, Join({Name(), Optional(60, Repeat(AccessDescription, 2))}));
    case 401: return Wrap(0x30, Join({Optional(60, Split(0x80, Integer())), Optional(60, Split(0x81, Integer()))}));
    case 430: return Wrap(0x06, ObjectContent());
    case 663: return Wrap(0x30, Join({Optional(50, Split(0x02, Integer())), Wrap(0x30, Join({Wrap(0x06, ObjectContent()), Optional(50, Split(0x04, Characters(Pick(5))))}))}));
    case 666: return Wrap(0x30, Join({Optional(60, Wrap(0xa0, Repeat(Subtree, 2))), Optional(60, Wrap(0xa1, Repeat(Subtree, 2)))}));
    case 747: return Repeat(PolicyMapping, 2);
    case 770:
        return Wrap(0x30, Join({Optional(60, Wrap(0xa0, DistPointName())), Optional(30, Flag(0x81)), Optional(30, Flag(0x82)), Optional(30, Split(0x83, BitContent())),
            Optional(30, Flag(0x84)), Optional(30, Flag(0x85))}));
    default: return Split(0x16, Ia5());
    }
}

constexpr int ExtensionNids[] = {71, 72, 73, 74, 75, 76, 77, 78, 82, 83, 84, 85, 86, 87, 88, 90, 103, 126, 140, 141, 142, 143, 177, 366, 367, 368, 369, 370, 371, 398, 401,
    430, 663, 666, 747, 748, 770, 771, 857, 769, 13};

Der ExtensionObject(int nid) {
    const auto& object = LibreSslObjects_nid_no_patch[nid];
    return Der(object.data, object.data + object.length);
}

Der CertificateWithExtensions(const std::vector<int>& nids) {
    faulty = false;
    const Der base = Join({Wrap(0xa0, Wrap(0x02, {0x02})), Wrap(0x02, {0x01}), Algorithm(), Name(), Wrap(0x30, Join({Time(), Time()})), Name(),
        Wrap(0x30, Join({Algorithm(), Wrap(0x03, {0x00, 0x01})}))});
    faulty = Chance(40);
    Der list;
    for (const int nid : nids) {
        Der content = ExtensionContent(nid);
        if (faulty && Chance(30)) content = Mutate(content);
        Der fields = Wrap(0x06, ExtensionObject(nid));
        if (Chance(40)) fields = Join({fields, Wrap(0x01, {static_cast<unsigned char>(Chance(50) ? 0xff : Pick(2))})});
        list = Join({list, Wrap(0x30, Join({fields, Wrap(0x04, content)}))});
    }
    return Wrap(0x30, Join({Wrap(0x30, Join({base, Wrap(0xa3, Wrap(0x30, list))})), Algorithm(), Wrap(0x03, {0x00, 0x01})}));
}

std::string DescribeExtension(int nid, void* value, int critical, int index, bool withCritical, bool withIndex) {
    std::string text = withCritical ? " crit " + std::to_string(critical) : std::string(" nocrit");
    text += withIndex ? " idx " + std::to_string(index) : std::string(" noidx");
    if (!value) return text + " null";
    const std::string encoded = LibreSslReference::ExtensionDer(nid, value);
    return text + " " + Hex(encoded.data(), encoded.size());
}

void CompareExtensions() {
    for (int round = 0; round < 30000; ++round) {
        std::vector<int> nids;
        const unsigned count = 1 + Pick(4);
        for (unsigned index = 0; index < count; ++index) nids.push_back(ExtensionNids[Pick(std::size(ExtensionNids))]);
        if (Chance(20)) nids.push_back(nids[Pick(static_cast<unsigned>(nids.size()))]);
        const Der der = CertificateWithExtensions(nids);
        const unsigned char* in = der.data();
        LibreSslX509* mine = d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(der.size()));
        in = der.data();
        void* theirs = LibreSslReference::D2iX509(nullptr, &in, static_cast<long>(der.size()));
        ERR_clear_error_nid_postfix();
        LibreSslReference::ErrClearError();
        if (!mine || !theirs) {
            Record("X509_get_ext_d2i (certificate)", !mine == !theirs, [&] { return Hex(der.data(), der.size()); });
            if (mine) X509_free_nid_postfix(mine);
            if (theirs) LibreSslReference::X509Free(theirs);
            continue;
        }
        const int nid = Chance(85) ? nids[Pick(static_cast<unsigned>(nids.size()))] : ExtensionNids[Pick(std::size(ExtensionNids))];
        const bool withCritical = Chance(70);
        const bool withIndex = Chance(50);
        static constexpr int starts[] = {-1, -1, -1, -5, 0, 1, 2};
        int mineCritical = 9;
        int theirCritical = 9;
        int mineIndex = starts[Pick(std::size(starts))];
        int theirIndex = mineIndex;
        const unsigned calls = withIndex ? 1 + Pick(3) : 1;
        std::string mineText;
        std::string theirText;
        for (unsigned call = 0; call < calls; ++call) {
            void* mineValue = nullptr;
            const auto error = Attempt([&] { mineValue = X509_get_ext_d2i_nid_postfix(mine, nid, withCritical ? &mineCritical : nullptr, withIndex ? &mineIndex : nullptr); });
            mineText += error.empty() ? DescribeExtension(nid, mineValue, mineCritical, mineIndex, withCritical, withIndex) + Errors(ERR_get_error_nid_postfix) : " " + error;
            void* theirValue = LibreSslReference::X509GetExtD2i(theirs, nid, withCritical ? &theirCritical : nullptr, withIndex ? &theirIndex : nullptr);
            theirText += DescribeExtension(nid, theirValue, theirCritical, theirIndex, withCritical, withIndex) + Errors(LibreSslReference::ErrGetError);
            if (theirValue) LibreSslReference::ExtensionFree(nid, theirValue);
        }
        Record("X509_get_ext_d2i", mineText == theirText, [&] { return std::to_string(nid) + " " + Hex(der.data(), der.size()) + "\n      mine  " + mineText + "\n      theirs" + theirText; });
        X509_free_nid_postfix(mine);
        LibreSslReference::X509Free(theirs);
    }
}
}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CompareAesKeys();
    CompareAesModes();
    CompareDigests();
    CompareDesParity();
    CompareStrings();
    CompareCertificates();
    CompareExtensions();
    CompareCreatedObjects();
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
