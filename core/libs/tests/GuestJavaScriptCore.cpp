#include "prx/libJavaScriptCore/libJavaScriptCore.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "JavaScriptCore: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a declared engine function threw another error");
        return;
    }
    Require(false, "a declared engine function returned");
}

}

int main() {
    RequireNotImplemented([] { libJavaScriptCoreUnknown_7pPMWplBcGQ_nid_no_patch(nullptr); }, "libJavaScriptCoreUnknown_7pPMWplBcGQ_nid_no_patch");
    RequireNotImplemented([] { libJavaScriptCoreUnknown_L_plus_uHfIz5ao8_nid_no_patch(nullptr); }, "libJavaScriptCoreUnknown_L_plus_uHfIz5ao8_nid_no_patch");
    RequireNotImplemented([] { libJavaScriptCoreUnknown_O8DunroJtKc_nid_no_patch(nullptr); }, "libJavaScriptCoreUnknown_O8DunroJtKc_nid_no_patch");
    RequireNotImplemented([] { libJavaScriptCoreUnknown_mgjWUOKHkJg_nid_no_patch(nullptr); }, "libJavaScriptCoreUnknown_mgjWUOKHkJg_nid_no_patch");
    return 0;
}
