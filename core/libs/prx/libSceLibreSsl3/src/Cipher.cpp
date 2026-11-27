#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

template <typename T>
T ReadFieldAt(const void* base, std::size_t offset, const char* message) {
    const int savedErrno = errno;
    T value{};
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(base);
    const auto fieldAddress = baseAddress + offset;
    const bool copied = fieldAddress >= baseAddress &&
                        GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(fieldAddress), sizeof(T));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument(message);
    return value;
}

}

extern "C" {

std::uint64_t APS5_VABI SSL_CIPHER_get_id_nid_postfix(const LibreSsl3Cipher* cipher) {
    if (!cipher)
        return 0;
    return ReadFieldAt<std::uint64_t>(cipher, offsetof(LibreSsl3Cipher, id), "SSL_CIPHER_get_id requires a readable cipher");
}

int APS5_VABI EVP_CIPHER_iv_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_iv_length requires a cipher");
    return ReadFieldAt<int>(cipher, offsetof(LibreSsl3EvpCipher, ivLength), "EVP_CIPHER_iv_length requires a readable cipher");
}

int APS5_VABI EVP_CIPHER_key_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_key_length requires a cipher");
    return ReadFieldAt<int>(cipher, offsetof(LibreSsl3EvpCipher, keyLength), "EVP_CIPHER_key_length requires a readable cipher");
}

}
