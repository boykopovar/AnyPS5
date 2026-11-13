#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EVP_PKEY_CTX_new_id check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (...) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    PkeyContext* context = static_cast<PkeyContext*>(EVP_PKEY_CTX_new_id_nid_postfix(6, nullptr));
    Require(context != nullptr && context->key == nullptr && context->method != nullptr && context->method->pkeyId == 6);
    PkeyContextFree_nid_no_patch(context);
    RequireThrows([] { EVP_PKEY_CTX_new_id_nid_postfix(408, nullptr); });
    std::puts("EVP_PKEY_CTX_new_id tests passed");
}
