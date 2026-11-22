#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int APS5_VABI X509_digest_nid_postfix(const LibreSsl3Certificate* certificate, const EvpMd* type, unsigned char* digest, std::uint32_t* size) {
    unsigned char* encoded = nullptr;
    const int length = i2d_X509_nid_postfix(certificate, &encoded);
    if (!encoded) return 0;
    int result = 0;
    try {
        result = EVP_Digest_nid_postfix(encoded, static_cast<std::size_t>(length), digest, size, type, nullptr);
    } catch (...) {
        free_nid_postfix(encoded);
        throw;
    }
    free_nid_postfix(encoded);
    return result ? 1 : 0;
}
