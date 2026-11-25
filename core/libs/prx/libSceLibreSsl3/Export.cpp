#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI BIO_vfree_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_memcmp_nid_postfix(const void* first, const void* second, std::size_t length) {
    const auto* firstBytes = static_cast<const volatile unsigned char*>(first);
    const auto* secondBytes = static_cast<const volatile unsigned char*>(second);
    unsigned int difference = 0;
    for (std::size_t index = 0; index < length; ++index)
        difference |= firstBytes[index] ^ secondBytes[index];
    return static_cast<int>(difference);
}

void APS5_VABI ERR_load_crypto_strings_nid_postfix() {
}

std::uint64_t APS5_VABI ERR_peek_last_error_nid_postfix() {
    return LibreSslPeekLastError_nid_no_patch();
}

int APS5_VABI EVP_CIPHER_CTX_set_flags_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

Asn1Object_nid_no_patch* APS5_VABI OBJ_txt2obj_nid_postfix(const char* text, int noName) {
    return LibreSsl3::ObjectFromText(text, noName);
}

const char* APS5_VABI OpenSSL_version_nid_postfix(int type) {
    switch (type) {
    case 0: return "LibreSSL 4.0.0";
    case 1: return "compiler: information not available";
    case 2: return "built on: date not available";
    case 3: return "platform: information not available";
    case 4: return "OPENSSLDIR: \"/etc/ssl\"";
    case 5: return "ENGINESDIR: N/A";
    default: return "not available";
    }
}

int APS5_VABI SSL_CTX_add0_chain_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_clear_chain_certs_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI d2i_X509_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
