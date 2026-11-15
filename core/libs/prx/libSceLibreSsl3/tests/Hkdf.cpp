#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 HKDF check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free(void* memory) {
    std::free(memory);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate(void* memory, std::size_t size) {
    return std::realloc(memory, size);
}

void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) {
    return std::realloc(memory, size);
}

int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

constexpr std::uint64_t TooLarge = 0x0ffff0a4;
constexpr std::uint64_t CryptoLibraryFailure = 0x0ffff00f;
constexpr unsigned char Guard = 0xa5;

enum class Hash {
    Sha1,
    Sha256,
};

struct Vector {
    Hash hash;
    const char* secret;
    const char* salt;
    const char* info;
    const char* key;
};

constexpr Vector Vectors[] {
    {Hash::Sha256, "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", "000102030405060708090a0b0c", "f0f1f2f3f4f5f6f7f8f9", "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"},
    {Hash::Sha256, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f", "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeaf", "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87"},
    {Hash::Sha256, "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", "", "", "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"},
    {Hash::Sha1, "0b0b0b0b0b0b0b0b0b0b0b", "000102030405060708090a0b0c", "f0f1f2f3f4f5f6f7f8f9", "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2c22e422478d305f3f896"},
    {Hash::Sha1, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f", "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeaf", "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", "0bd770a74d1160f7c9f12cd5912a06ebff6adcae899d92191fe4305673ba2ffe8fa3f1a4e5ad79f3f334b3b202b2173c486ea37ce3d397ed034c7f9dfeb15c5e927336d0441f4c4300e2cff0d0900b52d3b4"},
    {Hash::Sha1, "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", "", "", "0ac1af7002b3d761d1e55298da9d0506b9ae52057220a306e07b6b87e8df21d0ea00033de03984d34918"},
    {Hash::Sha1, "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c", "", "", "2c91117204d745f3500d636a62f64f0ab3bae548aa53d423b0d1f27ebba6f5e5673a081d70cce7acfc48"},
};

std::vector<std::uint8_t> Bytes(const char* hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; hex[i]; i += 2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(std::string(hex + i, 2), nullptr, 16)));
    return bytes;
}

const EvpMd* Digest(Hash hash) {
    if (hash == Hash::Sha256) return EVP_sha256_nid_postfix();
    return reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix());
}

int Derive(std::vector<std::uint8_t>& out, std::size_t length, const EvpMd* digest, const std::vector<std::uint8_t>& secret, const std::uint8_t* salt, std::size_t saltLength, const std::uint8_t* info, std::size_t infoLength) {
    out.assign(length + 1, Guard);
    const int result = HKDF_nid_postfix(out.data(), length, digest, secret.data(), secret.size(), salt, saltLength, info, infoLength);
    Require(out[length] == Guard);
    out.pop_back();
    return result;
}

void CheckVectors() {
    for (const auto& vector : Vectors) {
        const auto secret = Bytes(vector.secret);
        const auto salt = Bytes(vector.salt);
        const auto info = Bytes(vector.info);
        const auto key = Bytes(vector.key);
        const std::uint8_t empty = 0;
        const std::uint8_t* saltData = salt.empty() ? &empty : salt.data();
        std::vector<std::uint8_t> out;
        for (std::size_t length = 0; length <= key.size(); ++length) {
            ERR_clear_error_nid_postfix();
            Require(Derive(out, length, Digest(vector.hash), secret, saltData, salt.size(), info.data(), info.size()) == 1);
            Require(std::memcmp(out.data(), key.data(), length) == 0);
            Require(ERR_get_error_nid_postfix() == 0);
        }
        Require(HKDF_nid_postfix(nullptr, 0, Digest(vector.hash), secret.data(), secret.size(), saltData, salt.size(), info.data(), info.size()) == 1);
        Require(ERR_get_error_nid_postfix() == 0);
    }
}

void CheckEmptyInputs() {
    const auto secret = Bytes(Vectors[2].secret);
    const auto key = Bytes(Vectors[2].key);
    const std::uint8_t empty = 0;
    std::vector<std::uint8_t> out;
    ERR_clear_error_nid_postfix();
    Require(Derive(out, key.size(), Digest(Hash::Sha256), secret, &empty, 0, nullptr, 0) == 1);
    Require(out == key);
    std::vector<std::uint8_t> expected;
    Require(Derive(expected, 32, Digest(Hash::Sha256), {}, &empty, 0, &empty, 0) == 1);
    Require(HKDF_nid_postfix(out.data(), 32, Digest(Hash::Sha256), nullptr, 0, &empty, 0, &empty, 0) == 1);
    Require(std::memcmp(out.data(), expected.data(), 32) == 0);
    Require(ERR_get_error_nid_postfix() == 0);
}

void CheckLengthLimit(Hash hash, std::size_t digestLength) {
    const auto secret = Bytes(Vectors[0].secret);
    const auto salt = Bytes(Vectors[0].salt);
    const auto info = Bytes(Vectors[0].info);
    const std::size_t limit = digestLength * 255;
    std::vector<std::uint8_t> shorter;
    std::vector<std::uint8_t> full;
    ERR_clear_error_nid_postfix();
    Require(Derive(shorter, limit - digestLength + 1, Digest(hash), secret, salt.data(), salt.size(), info.data(), info.size()) == 1);
    Require(Derive(full, limit, Digest(hash), secret, salt.data(), salt.size(), info.data(), info.size()) == 1);
    Require(std::memcmp(full.data(), shorter.data(), shorter.size()) == 0);
    Require(ERR_get_error_nid_postfix() == 0);
    for (const std::size_t length : {limit + 1, limit + digestLength, std::numeric_limits<std::size_t>::max()}) {
        std::vector<std::uint8_t> out(limit + 16, Guard);
        Require(HKDF_nid_postfix(out.data(), length, Digest(hash), secret.data(), secret.size(), salt.data(), salt.size(), info.data(), info.size()) == 0);
        Require(std::memcmp(out.data(), full.data(), limit) == 0);
        Require(std::vector<std::uint8_t>(out.begin() + static_cast<std::ptrdiff_t>(limit), out.end()) == std::vector<std::uint8_t>(16, Guard));
        Require(ERR_get_error_nid_postfix() == TooLarge);
        Require(ERR_get_error_nid_postfix() == 0);
    }
}

void CheckSalt() {
    const auto secret = Bytes(Vectors[0].secret);
    const auto salt = Bytes(Vectors[0].salt);
    const auto info = Bytes(Vectors[0].info);
    const auto key = Bytes(Vectors[0].key);
    const std::uint8_t empty = 0;
    std::vector<std::uint8_t> unsalted;
    std::vector<std::uint8_t> out;
    ERR_clear_error_nid_postfix();
    Require(Derive(unsalted, 42, Digest(Hash::Sha256), secret, &empty, 0, info.data(), info.size()) == 1);
    Require(unsalted != key);
    for (const std::size_t saltLength : {std::size_t{0}, salt.size(), std::size_t{0x80000000}}) {
        Require(Derive(out, 42, Digest(Hash::Sha256), secret, nullptr, saltLength, info.data(), info.size()) == 1);
        Require(out == unsalted);
    }
    Require(Derive(out, 42, Digest(Hash::Sha256), secret, salt.data(), (std::size_t{1} << 32) + salt.size(), info.data(), info.size()) == 1);
    Require(out == key);
    Require(ERR_get_error_nid_postfix() == 0);
    for (const std::size_t saltLength : {std::size_t{0x80000000}, std::size_t{0xffffffff}}) {
        std::vector<std::uint8_t> untouched(42, Guard);
        Require(HKDF_nid_postfix(untouched.data(), untouched.size(), Digest(Hash::Sha256), secret.data(), secret.size(), salt.data(), saltLength, info.data(), info.size()) == 0);
        Require(untouched == std::vector<std::uint8_t>(42, Guard));
        Require(ERR_get_error_nid_postfix() == CryptoLibraryFailure);
        Require(ERR_get_error_nid_postfix() == 0);
    }
}

void CheckFailures() {
    const auto secret = Bytes(Vectors[0].secret);
    const auto salt = Bytes(Vectors[0].salt);
    const auto info = Bytes(Vectors[0].info);
    std::vector<std::uint8_t> out(42, Guard);
    ERR_clear_error_nid_postfix();
    Require(HKDF_nid_postfix(out.data(), out.size(), nullptr, secret.data(), secret.size(), salt.data(), salt.size(), info.data(), info.size()) == 0);
    Require(out == std::vector<std::uint8_t>(42, Guard));
    Require(ERR_get_error_nid_postfix() == CryptoLibraryFailure);
    Require(ERR_get_error_nid_postfix() == 0);
    bool threw = false;
    try {
        static_cast<void>(HKDF_nid_postfix(nullptr, 1, Digest(Hash::Sha256), secret.data(), secret.size(), salt.data(), salt.size(), info.data(), info.size()));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckVectors();
    CheckEmptyInputs();
    CheckLengthLimit(Hash::Sha256, 32);
    CheckLengthLimit(Hash::Sha1, 20);
    CheckSalt();
    CheckFailures();
    std::puts("LibreSSL3 HKDF: ok");
    return 0;
}
