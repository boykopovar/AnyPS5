#include "prx/libScePsmlBcSisr/libScePsmlBcSisr.hpp"
#include <cstdio>

namespace {

int failures = 0;

template <typename TFn>
void RequireThrows(TFn fn, const char *name) {
    try {
        fn();
    } catch (...) {
        return;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    ++failures;
}

}

int main() {
    RequireThrows(scePsmlBcSisrGetInternalBufferRequirement_nid_no_patch, "scePsmlBcSisrGetInternalBufferRequirement");
    RequireThrows(scePsmlBcSisrGetPacketSizeInDwords_nid_no_patch, "scePsmlBcSisrGetPacketSizeInDwords");
    RequireThrows(scePsmlBcSisrInit_nid_no_patch, "scePsmlBcSisrInit");
    return failures == 0 ? 0 : 1;
}
