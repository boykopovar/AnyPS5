#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslEvpCipher) == 88);
static_assert(offsetof(LibreSslEvpCipher, blockSize) == 4);
static_assert(offsetof(LibreSslEvpCipher, init) == 24);
static_assert(offsetof(LibreSslEvpCipher, contextSize) == 48);

extern "C" {

int APS5_VABI EVP_CIPHER_block_size_nid_postfix(const LibreSslEvpCipher* cipher) {
    if (!cipher) throw std::invalid_argument(std::string(__func__) + ": null cipher");
    int blockSize = 0;
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&blockSize, reinterpret_cast<const std::byte*>(cipher) + offsetof(LibreSslEvpCipher, blockSize), sizeof(blockSize));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return blockSize;
}

}
