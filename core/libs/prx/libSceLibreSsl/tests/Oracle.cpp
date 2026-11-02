#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <algorithm>
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


std::string Errors(const std::function<unsigned long()>& next) {
    std::string text;
    while (const unsigned long code = next()) text += " " + std::to_string(code);
    return text;
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

}

int main() {
    CompareAesKeys();
    CompareAesModes();
    CompareDigests();
    CompareDesParity();
    CompareStrings();
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
