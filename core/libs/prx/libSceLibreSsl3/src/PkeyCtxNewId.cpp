#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>

extern "C" void* APS5_VABI EVP_PKEY_CTX_new_id_nid_postfix(int id, void* engine) {
    if (id != 6) NotImplemented_nid_no_patch("EVP_PKEY_CTX_new_id without an RSA key type");
    if (engine) NotImplemented_nid_no_patch("EVP_PKEY_CTX_new_id with an ENGINE");
    void* key = EVP_PKEY_new_nid_postfix();
    if (!key) return nullptr;
    EVP_PKEY_set1_RSA_nid_postfix(key, nullptr);
    PkeyContext* seed = PkeyContextNew_nid_no_patch(key, nullptr);
    EVP_PKEY_free_nid_postfix(key);
    if (!seed) return nullptr;
    const PkeyMethod* method = seed->method;
    PkeyContextFree_nid_no_patch(seed);
    auto* context = static_cast<PkeyContext*>(calloc_nid_postfix(1, sizeof(PkeyContext)));
    if (!context) return nullptr;
    context->method = method;
    if (!method->init || method->init(context) <= 0) {
        PkeyContextFree_nid_no_patch(context);
        return nullptr;
    }
    return context;
}
