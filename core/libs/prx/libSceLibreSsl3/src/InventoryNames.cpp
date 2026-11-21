#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

template <typename Object, typename Field>
Field CopyFieldIn(const Object* object, Field Object::*field, const char* function) {
    Field value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, &(object->*field), sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

}

extern "C" {

int APS5_VABI RSA_bits_nid_postfix(const LibreSsl3Rsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null RSA key");
    const void* const modulus = CopyFieldIn(rsa, &LibreSsl3Rsa::n, __func__);
    if (!modulus) return 0;
    return BN_num_bits_nid_postfix(static_cast<const Bignum*>(modulus));
}

int APS5_VABI BN_is_negative_nid_postfix(const Bignum* value) {
    if (!value) throw std::invalid_argument(std::string(__func__) + ": null BIGNUM");
    return CopyFieldIn(value, &Bignum::neg, __func__) != 0;
}

int APS5_VABI EVP_CIPHER_nid_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher) throw std::invalid_argument(std::string(__func__) + ": null cipher");
    return CopyFieldIn(cipher, &LibreSsl3EvpCipher::nid, __func__);
}

void APS5_VABI ERR_load_ERR_strings_nid_postfix() {
}

}
