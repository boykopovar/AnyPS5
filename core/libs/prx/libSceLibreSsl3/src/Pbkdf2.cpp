#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MessageDigestIsNull = 159;
constexpr int MaxDigestSize = 64;

class HmacTemplate {
public:
    HmacTemplate() { HMAC_CTX_init_nid_postfix(&context); }
    ~HmacTemplate() { HMAC_CTX_cleanup_nid_postfix(&context); }
    HmacTemplate(const HmacTemplate&) = delete;
    HmacTemplate& operator=(const HmacTemplate&) = delete;

    HmacContext* Get() { return &context; }

private:
    HmacContext context;
};

bool Mac(HmacContext* context, const unsigned char* first, std::size_t firstLength, const unsigned char* second, std::size_t secondLength, unsigned char* digest) {
    return HMAC_Init_ex_nid_postfix(context, nullptr, 0, nullptr, nullptr)
        && HMAC_Update_nid_postfix(context, first, firstLength)
        && HMAC_Update_nid_postfix(context, second, secondLength)
        && HMAC_Final_nid_postfix(context, digest, nullptr);
}

}

extern "C" {

int APS5_VABI PKCS5_PBKDF2_HMAC_nid_postfix(const char* password, int passwordLength, const unsigned char* salt, int saltLength, int iterations, const EvpMd* digest, int keyLength, unsigned char* output) {
    if (!digest) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, MessageDigestIsNull);
        return 0;
    }
    const int digestLength = EVP_MD_size_nid_postfix(digest);
    if (digestLength < 0) return 0;
    if (!password) passwordLength = 0;
    else if (passwordLength == -1) passwordLength = static_cast<int>(std::strlen(password));
    HmacTemplate base;
    if (!HMAC_Init_ex_nid_postfix(base.Get(), password, passwordLength, digest, nullptr)) return 0;
    if (keyLength == 0) return 1;
    if (keyLength < 0) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: negative key length");
    if (!output) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: null output");
    if (saltLength < 0) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: negative salt length");
    if (!salt && saltLength > 0) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: null salt");
    if (digestLength == 0 || digestLength > MaxDigestSize) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: unsupported digest size");
    unsigned char block[MaxDigestSize];
    unsigned char* position = output;
    int remaining = keyLength;
    for (std::uint64_t index = 1; remaining > 0; ++index) {
        const int take = std::min(remaining, digestLength);
        const unsigned char counter[4] = {
            static_cast<unsigned char>(index >> 24),
            static_cast<unsigned char>(index >> 16),
            static_cast<unsigned char>(index >> 8),
            static_cast<unsigned char>(index),
        };
        if (!Mac(base.Get(), salt, static_cast<std::size_t>(saltLength), counter, sizeof(counter), block)) return 0;
        std::memcpy(position, block, static_cast<std::size_t>(take));
        for (int round = 1; round < iterations; ++round) {
            if (!Mac(base.Get(), block, static_cast<std::size_t>(digestLength), nullptr, 0, block)) return 0;
            for (int i = 0; i < take; ++i) position[i] ^= block[i];
        }
        remaining -= take;
        position += take;
    }
    return 1;
}

}
