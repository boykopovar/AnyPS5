#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include <stdexcept>

namespace {

constexpr int Allocated = 1;
constexpr int StaticData = 2;

void Validate_nid_no_patch(const LibreSsl3Bignum* number) {
    if (number && number->data && !(number->flags & StaticData) && number->capacity < 0)
        throw std::invalid_argument("LibreSSL3 BIGNUM has a negative limb capacity");
}

void Wipe_nid_no_patch(void* pointer, std::size_t bytes) {
    auto* output = static_cast<volatile unsigned char*>(pointer);
    for (std::size_t i = 0; i < bytes; ++i) output[i] = 0;
}

void ValidateReplacement_nid_no_patch(void* previous, const LibreSsl3Bignum* replacement) {
    if (replacement) Validate_nid_no_patch(static_cast<LibreSsl3Bignum*>(previous));
}

void Replace_nid_no_patch(void*& previous, LibreSsl3Bignum* replacement) {
    if (!replacement) return;
    BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(previous));
    previous = replacement;
}

}

extern "C" {

void APS5_VABI BN_clear_free_nid_postfix(LibreSsl3Bignum* number) {
    if (!number) return;
    Validate_nid_no_patch(number);
    const bool allocated = (number->flags & Allocated) != 0;
    if (number->data && !(number->flags & StaticData)) {
        Wipe_nid_no_patch(number->data, static_cast<std::size_t>(number->capacity) * sizeof(*number->data));
        free_nid_postfix(number->data);
    }
    Wipe_nid_no_patch(number, sizeof(*number));
    if (allocated) free_nid_postfix(number);
}

int APS5_VABI RSA_set0_key_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* n, LibreSsl3Bignum* e, LibreSsl3Bignum* d) {
    if (!rsa) throw std::invalid_argument("RSA_set0_key requires a key");
    if ((!rsa->n && !n) || (!rsa->e && !e)) return 0;
    ValidateReplacement_nid_no_patch(rsa->n, n);
    ValidateReplacement_nid_no_patch(rsa->e, e);
    ValidateReplacement_nid_no_patch(rsa->d, d);
    Replace_nid_no_patch(rsa->n, n);
    Replace_nid_no_patch(rsa->e, e);
    Replace_nid_no_patch(rsa->d, d);
    return 1;
}

int APS5_VABI RSA_set0_factors_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* p, LibreSsl3Bignum* q) {
    if (!rsa) throw std::invalid_argument("RSA_set0_factors requires a key");
    if ((!rsa->p && !p) || (!rsa->q && !q)) return 0;
    ValidateReplacement_nid_no_patch(rsa->p, p);
    ValidateReplacement_nid_no_patch(rsa->q, q);
    Replace_nid_no_patch(rsa->p, p);
    Replace_nid_no_patch(rsa->q, q);
    return 1;
}

int APS5_VABI RSA_set0_crt_params_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* dmp1, LibreSsl3Bignum* dmq1, LibreSsl3Bignum* iqmp) {
    if (!rsa) throw std::invalid_argument("RSA_set0_crt_params requires a key");
    if ((!rsa->dmp1 && !dmp1) || (!rsa->dmq1 && !dmq1) || (!rsa->iqmp && !iqmp)) return 0;
    ValidateReplacement_nid_no_patch(rsa->dmp1, dmp1);
    ValidateReplacement_nid_no_patch(rsa->dmq1, dmq1);
    ValidateReplacement_nid_no_patch(rsa->iqmp, iqmp);
    Replace_nid_no_patch(rsa->dmp1, dmp1);
    Replace_nid_no_patch(rsa->dmq1, dmq1);
    Replace_nid_no_patch(rsa->iqmp, iqmp);
    return 1;
}

}
