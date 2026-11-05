#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI OCSP_CERTID_free_nid_postfix(void* id) {
    if (!id) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI PKCS7_free_nid_postfix(void* pkcs7) {
    if (!pkcs7) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI PKCS8_PRIV_KEY_INFO_free_nid_postfix(void* info) {
    if (!info) return;
    NotImplemented_nid_no_patch(__func__);
}

}
