#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <stdexcept>
#include <string>

extern "C" {

int APS5_VABI RSA_bits_nid_postfix(const LibreSsl3Rsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null RSA key");
    if (!rsa->n) return 0;
    return BN_num_bits_nid_postfix(static_cast<const Bignum*>(rsa->n));
}

int APS5_VABI BN_is_negative_nid_postfix(const Bignum* value) {
    if (!value) throw std::invalid_argument(std::string(__func__) + ": null BIGNUM");
    return value->neg != 0;
}

int APS5_VABI EVP_CIPHER_nid_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher) throw std::invalid_argument(std::string(__func__) + ": null cipher");
    return cipher->nid;
}

void APS5_VABI ERR_load_ERR_strings_nid_postfix() {
}

}
