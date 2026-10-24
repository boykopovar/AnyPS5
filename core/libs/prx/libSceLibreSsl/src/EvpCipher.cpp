#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslEvpCipher {
    int nid;
    int blockSize;
    int keyLength;
    int ivLength;
    std::uint64_t flags;
    void* init;
    void* doCipher;
    void* cleanup;
    int contextSize;
    void* setAsn1Parameters;
    void* getAsn1Parameters;
    void* control;
    void* applicationData;
};

static_assert(sizeof(LibreSslEvpCipher) == 88);
static_assert(offsetof(LibreSslEvpCipher, blockSize) == 4);
static_assert(offsetof(LibreSslEvpCipher, init) == 24);
static_assert(offsetof(LibreSslEvpCipher, contextSize) == 48);

extern "C" {

int APS5_VABI EVP_CIPHER_block_size_nid_postfix(const LibreSslEvpCipher* cipher) {
    if (!cipher) throw std::invalid_argument(std::string(__func__) + ": null cipher");
    return cipher->blockSize;
}

}
