#include <atomic>
#include <cstddef>
#include <cstring>
#include <new>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t size);
void APS5_VABI free_nid_postfix(void* pointer);
}

namespace {
struct Pkey {
    int type;
    int saveType;
    int references;
    const void* ameth;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};
static_assert(sizeof(int) == 4 && sizeof(void*) == 8, "layout assumes LP64");
}

extern "C" {

void* APS5_VABI EVP_PKEY_new_nid_postfix(void) {
    auto* pkey = static_cast<Pkey*>(malloc_nid_postfix(sizeof(Pkey)));
    if (!pkey) return nullptr;
    std::memset(pkey, 0, sizeof(Pkey));
    pkey->references = 1;
    pkey->saveParameters = 1;
    return pkey;
}

void APS5_VABI EVP_PKEY_free_nid_postfix(void* value) {
    if (!value) return;
    auto* pkey = static_cast<Pkey*>(value);
    auto references = std::atomic_ref<int>(pkey->references);
    if (references.fetch_sub(1, std::memory_order_acq_rel) - 1 > 0) return;
    if (pkey->ameth || pkey->engine || pkey->key || pkey->attributes) {
        references.fetch_add(1, std::memory_order_relaxed);
        NotImplemented_nid_no_patch("EVP_PKEY_free of a key that holds algorithm data");
    }
    free_nid_postfix(pkey);
}

}
