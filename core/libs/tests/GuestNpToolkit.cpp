#include "prx/libSceNpToolkit/libSceNpToolkit.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpToolkit: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a declared toolkit function threw another error");
        return;
    }
    Require(false, "a declared toolkit function returned");
}

}

int main() {
    RequireNotImplemented([] { libSceNpToolkitUnknown_Pqt3YpFSHgI_nid_no_patch(nullptr); }, "libSceNpToolkitUnknown_Pqt3YpFSHgI_nid_no_patch");
    RequireNotImplemented([] { libSceNpToolkitUnknown_tbGlFkI2QxQ_nid_no_patch(nullptr); }, "libSceNpToolkitUnknown_tbGlFkI2QxQ_nid_no_patch");
    RequireNotImplemented([] { libSceNpToolkitUnknown_kn0_minus_k32rPJE_nid_no_patch(nullptr); }, "libSceNpToolkitUnknown_kn0_minus_k32rPJE_nid_no_patch");
    RequireNotImplemented([] { libSceNpToolkitUnknown_lqeZJNlw8C4_nid_no_patch(nullptr); }, "libSceNpToolkitUnknown_lqeZJNlw8C4_nid_no_patch");
    return 0;
}
