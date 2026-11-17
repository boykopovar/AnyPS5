#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI AUTHORITY_KEYID_free_nid_postfix(void* authorityKeyId) {
    if (!authorityKeyId) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI X509_CRL_free_nid_postfix(void* crl) {
    if (!crl) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI PKCS7_free_nid_postfix(void* pkcs7) {
    if (!pkcs7) return;
    NotImplemented_nid_no_patch(__func__);
}

}
