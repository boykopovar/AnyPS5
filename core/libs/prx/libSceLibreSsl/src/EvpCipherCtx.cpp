#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslEvpCipherCtx) == 168);
static_assert(offsetof(LibreSslEvpCipherCtx, num) == 88);
static_assert(offsetof(LibreSslEvpCipherCtx, flags) == 112);
static_assert(offsetof(LibreSslEvpCipherCtx, final) == 136);

namespace {

constexpr std::uint64_t CipherFlagNoPadding = 0x100;

}

extern "C" {
void APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    std::memset(ctx, 0, sizeof(LibreSslEvpCipherCtx));
}

LibreSslEvpCipherCtx* APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslEvpCipherCtx*>(calloc_nid_postfix(1, sizeof(LibreSslEvpCipherCtx)));
}

int APS5_VABI EVP_CIPHER_CTX_set_padding_nid_postfix(LibreSslEvpCipherCtx* ctx, int pad) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (pad) ctx->flags &= ~CipherFlagNoPadding;
    else ctx->flags |= CipherFlagNoPadding;
    return 1;
}

}
