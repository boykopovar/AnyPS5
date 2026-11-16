#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MessageDigestIsNull = 159;
constexpr int MaxDigestSize = 64;

class HmacKey {
public:
    HmacKey() { HMAC_CTX_init_nid_postfix(&context_); }
    ~HmacKey() { HMAC_CTX_cleanup_nid_postfix(&context_); }
    HmacKey(const HmacKey&) = delete;
    HmacKey& operator=(const HmacKey&) = delete;

    bool Init(const char* password, int length, const EvpMd* digest) {
        return HMAC_Init_ex_nid_postfix(&context_, password, length, digest, nullptr) == 1;
    }

    bool Mac(const unsigned char* first, std::size_t firstLength, const unsigned char* second, std::size_t secondLength,
             unsigned char* output) {
        std::uint32_t length = 0;
        return HMAC_Init_ex_nid_postfix(&context_, nullptr, 0, nullptr, nullptr) == 1
            && HMAC_Update_nid_postfix(&context_, first, firstLength) == 1
            && (secondLength == 0 || HMAC_Update_nid_postfix(&context_, second, secondLength) == 1)
            && HMAC_Final_nid_postfix(&context_, output, &length) == 1;
    }

private:
    HmacContext context_;
};

}

extern "C" int APS5_VABI PKCS5_PBKDF2_HMAC_nid_postfix(const char* password, int passwordLength, const unsigned char* salt,
                                                       int saltLength, int iterations, const EvpMd* digest, int keyLength,
                                                       unsigned char* output) {
    if (!digest) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, MessageDigestIsNull);
        return 0;
    }
    const int digestLength = EVP_MD_size_nid_postfix(digest);
    if (digestLength <= 0 || digestLength > MaxDigestSize) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: invalid digest size");
    if (!password) {
        passwordLength = 0;
    } else if (passwordLength == -1) {
        passwordLength = static_cast<int>(std::strlen(password));
    }
    HmacKey key;
    if (!key.Init(password, passwordLength, digest)) return 0;
    if (keyLength == 0) return 1;
    if (keyLength < 0 || !output) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: invalid output");
    if (saltLength < 0 || (saltLength > 0 && !salt)) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: invalid salt");
    const auto size = static_cast<std::size_t>(digestLength);
    std::array<unsigned char, MaxDigestSize> block{};
    std::uint32_t index = 1;
    for (int remaining = keyLength; remaining > 0; ++index) {
        const auto part = static_cast<std::size_t>(remaining < digestLength ? remaining : digestLength);
        const std::array<unsigned char, 4> counter{static_cast<unsigned char>(index >> 24), static_cast<unsigned char>(index >> 16),
                                                   static_cast<unsigned char>(index >> 8), static_cast<unsigned char>(index)};
        if (!key.Mac(salt, static_cast<std::size_t>(saltLength), counter.data(), counter.size(), block.data())) return 0;
        std::memcpy(output, block.data(), part);
        for (int round = 1; round < iterations; ++round) {
            if (!key.Mac(block.data(), size, nullptr, 0, block.data())) return 0;
            for (std::size_t i = 0; i < part; ++i) output[i] ^= block[i];
        }
        output += part;
        remaining -= static_cast<int>(part);
    }
    return 1;
}
