#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int CryptoLibraryFailure = 15;
constexpr int TooLarge = 164;
constexpr std::size_t MaxDigestSize = 64;

void Wipe(std::uint8_t* data, std::size_t length) {
    volatile std::uint8_t* bytes = data;
    for (std::size_t i = 0; i < length; ++i) bytes[i] = 0;
}

class Hmac {
public:
    Hmac() { HMAC_CTX_init_nid_postfix(&context); }
    ~Hmac() { HMAC_CTX_cleanup_nid_postfix(&context); }
    Hmac(const Hmac&) = delete;
    Hmac& operator=(const Hmac&) = delete;

    bool Init(const void* key, int keyLength, const EvpMd* digest) {
        return HMAC_Init_ex_nid_postfix(&context, key, keyLength, digest, nullptr) != 0;
    }

    bool Update(const std::uint8_t* data, std::size_t length) {
        if (!data && length) throw std::invalid_argument("HKDF: null input with a nonzero length");
        return HMAC_Update_nid_postfix(&context, data, length) != 0;
    }

    bool Final(std::uint8_t* digest) {
        return HMAC_Final_nid_postfix(&context, digest, nullptr) != 0;
    }

private:
    HmacContext context;
};

bool Extract(std::uint8_t* prk, const EvpMd* digest, const std::uint8_t* secret, std::size_t secretLength, const std::uint8_t* salt, std::size_t saltLength) {
    static constexpr std::uint8_t emptyKey[1] {};
    const void* key = salt;
    int keyLength = static_cast<int>(saltLength);
    if (!key) {
        key = emptyKey;
        keyLength = 0;
    }
    Hmac hmac;
    return hmac.Init(key, keyLength, digest) && hmac.Update(secret, secretLength) && hmac.Final(prk);
}

bool Expand(std::uint8_t* outKey, std::size_t outLength, const EvpMd* digest, const std::uint8_t* prk, std::size_t prkLength, const std::uint8_t* info, std::size_t infoLength) {
    const auto digestLength = static_cast<std::size_t>(EVP_MD_size_nid_postfix(digest));
    std::uint8_t block[MaxDigestSize];
    Hmac hmac;
    bool ok = hmac.Init(prk, static_cast<int>(prkLength), digest);
    std::size_t done = 0;
    std::uint8_t counter = 0;
    while (ok && done < outLength) {
        if (++counter == 0) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, TooLarge);
            ok = false;
            break;
        }
        ok = hmac.Update(info, infoLength) && hmac.Update(&counter, 1) && hmac.Final(block);
        if (!ok) break;
        if (!outKey) throw std::invalid_argument("HKDF: null output buffer with a nonzero length");
        const std::size_t length = std::min(outLength - done, digestLength);
        std::memcpy(outKey + done, block, length);
        done += length;
        if (done < outLength) ok = hmac.Init(nullptr, 0, nullptr) && hmac.Update(block, digestLength);
    }
    Wipe(block, sizeof(block));
    return ok;
}

}

extern "C" {

int APS5_VABI HKDF_nid_postfix(std::uint8_t* outKey, std::size_t outLength, const EvpMd* digest, const std::uint8_t* secret, std::size_t secretLength, const std::uint8_t* salt, std::size_t saltLength, const std::uint8_t* info, std::size_t infoLength) {
    std::uint8_t prk[MaxDigestSize];
    if (!Extract(prk, digest, secret, secretLength, salt, saltLength)) {
        LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, CryptoLibraryFailure);
        return 0;
    }
    const bool ok = Expand(outKey, outLength, digest, prk, static_cast<std::size_t>(EVP_MD_size_nid_postfix(digest)), info, infoLength);
    Wipe(prk, sizeof(prk));
    return ok ? 1 : 0;
}

}
