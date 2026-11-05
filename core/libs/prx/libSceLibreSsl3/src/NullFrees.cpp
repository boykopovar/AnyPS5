#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI EC_POINT_clear_free_nid_postfix(void* point) {
    if (!point) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI EVP_PKEY_CTX_free_nid_postfix(void* context) {
    if (!context) return;
    NotImplemented_nid_no_patch(__func__);
}

}
