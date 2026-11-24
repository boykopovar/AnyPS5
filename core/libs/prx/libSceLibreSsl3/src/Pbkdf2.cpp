#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MessageDigestIsNull = 159;
constexpr int MaxDigestSize = 64;

template <typename T>
bool CopyInChunks(std::vector<T>& host, const void* source, std::size_t bytes) {
    constexpr std::size_t Chunk = 64 * 1024;
    host.clear();
    for (std::size_t done = 0; done < bytes;) {
        const std::size_t size = std::min(Chunk, bytes - done);
        host.resize(done + size);
        if (!GuestCopyIn_nid_no_patch(host.data() + done, static_cast<const unsigned char*>(source) + done, size)) return false;
        done += size;
    }
    return true;
}

bool PasswordLength(const char* password, int& length) {
    char chunk[4096];
    for (std::size_t offset = 0; offset <= static_cast<std::size_t>(INT32_MAX);) {
        const auto address = reinterpret_cast<std::uintptr_t>(password) + offset;
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        if (!GuestCopyIn_nid_no_patch(chunk, password + offset, count)) return false;
        if (const void* end = std::memchr(chunk, 0, count)) {
            const std::size_t total = offset + static_cast<std::size_t>(static_cast<const char*>(end) - chunk);
            if (total > static_cast<std::size_t>(INT32_MAX)) return false;
            length = static_cast<int>(total);
            return true;
        }
        offset += count;
    }
    return false;
}

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
    } else if (passwordLength == -1 && !PasswordLength(password, passwordLength)) {
        throw std::invalid_argument("PKCS5_PBKDF2_HMAC: unreadable password");
    }
    const std::size_t passwordBytes = passwordLength > 0 ? static_cast<std::size_t>(passwordLength) : 0;
    std::vector<char> passwordCopy;
    if (!CopyInChunks(passwordCopy, password, passwordBytes)) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: unreadable password");
    passwordCopy.push_back('\0');
    HmacKey key;
    if (!key.Init(password ? passwordCopy.data() : nullptr, passwordLength, digest)) return 0;
    if (keyLength == 0) return 1;
    if (keyLength < 0 || !output) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: invalid output");
    if (saltLength < 0 || (saltLength > 0 && !salt)) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: invalid salt");
    std::vector<unsigned char> saltCopy;
    if (!CopyInChunks(saltCopy, salt, static_cast<std::size_t>(saltLength))) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: unreadable salt");
    const auto size = static_cast<std::size_t>(digestLength);
    std::array<unsigned char, MaxDigestSize> block{};
    std::array<unsigned char, MaxDigestSize> derived{};
    std::uint32_t index = 1;
    for (int remaining = keyLength; remaining > 0; ++index) {
        const auto part = static_cast<std::size_t>(remaining < digestLength ? remaining : digestLength);
        const std::array<unsigned char, 4> counter{static_cast<unsigned char>(index >> 24), static_cast<unsigned char>(index >> 16),
                                                   static_cast<unsigned char>(index >> 8), static_cast<unsigned char>(index)};
        if (!key.Mac(saltCopy.data(), saltCopy.size(), counter.data(), counter.size(), block.data())) return 0;
        std::memcpy(derived.data(), block.data(), part);
        for (int round = 1; round < iterations; ++round) {
            if (!key.Mac(block.data(), size, nullptr, 0, block.data())) return 0;
            for (std::size_t i = 0; i < part; ++i) derived[i] ^= block[i];
        }
        if (!GuestCopyOut_nid_no_patch(output, derived.data(), part)) throw std::invalid_argument("PKCS5_PBKDF2_HMAC: unwritable output");
        output += part;
        remaining -= static_cast<int>(part);
    }
    return 1;
}
