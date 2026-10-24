#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslEvpCipherCtx {
    const void* cipher;
    void* engine;
    int encrypt;
    int bufLen;
    unsigned char oiv[16];
    unsigned char iv[16];
    unsigned char buf[32];
    int num;
    void* appData;
    int keyLen;
    std::uint64_t flags;
    void* cipherData;
    int finalUsed;
    int blockMask;
    unsigned char final[32];
};

static_assert(sizeof(LibreSslEvpCipherCtx) == 168);
static_assert(offsetof(LibreSslEvpCipherCtx, num) == 88);
static_assert(offsetof(LibreSslEvpCipherCtx, flags) == 112);
static_assert(offsetof(LibreSslEvpCipherCtx, final) == 136);

namespace {

constexpr std::uint64_t CipherFlagNoPadding = 0x100;

}

extern "C" {
void* APS5_VABI calloc_nid_postfix(std::size_t count, std::size_t size);

void APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    std::memset(ctx, 0, sizeof(LibreSslEvpCipherCtx));
}

LibreSslEvpCipherCtx* APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslEvpCipherCtx*>(calloc_nid_postfix(1, sizeof(LibreSslEvpCipherCtx)));
}

}
