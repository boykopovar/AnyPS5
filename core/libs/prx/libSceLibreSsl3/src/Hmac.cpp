#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void* APS5_VABI HMAC_CTX_new_nid_postfix() {
    auto* const context = static_cast<HmacContext*>(calloc_nid_postfix(1, sizeof(HmacContext)));
    if (!context) return nullptr;
    HMAC_CTX_init_nid_postfix(context);
    return context;
}

void APS5_VABI HMAC_CTX_free_nid_postfix(void* context) {
    if (!context) return;
    HMAC_CTX_cleanup_nid_postfix(static_cast<HmacContext*>(context));
    free_nid_postfix(context);
}

}
