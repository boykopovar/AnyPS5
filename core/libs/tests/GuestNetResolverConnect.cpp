#include "prx/libSceNet/include/Net.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>

static bool ReportsNotImplemented(const std::string& name, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (error.what() == name + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name.c_str(), error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name.c_str());
    return false;
}

static bool FamilyReportsNotImplemented() {
    bool passed = ReportsNotImplemented("sceNetResolverConnectCreate", [] { return sceNetResolverConnectCreate("resolver", 1, nullptr, 0); });
    passed = ReportsNotImplemented("sceNetResolverConnectCreate", [] { return sceNetResolverConnectCreate(nullptr, 2, nullptr, 0x80000000); }) && passed;
    passed = ReportsNotImplemented("sceNetResolverConnect", [] { return sceNetResolverConnect(nullptr, "example.org", 53, nullptr); }) && passed;
    passed = ReportsNotImplemented("sceNetResolverConnectAbort", [] { return sceNetResolverConnectAbort(nullptr, 1); }) && passed;
    passed = ReportsNotImplemented("sceNetResolverConnectDestroy", [] { return sceNetResolverConnectDestroy(nullptr); }) && passed;
    return passed;
}

int main() {
    bool passed = FamilyReportsNotImplemented();
    if (sceNetInit_nid_postfix() != 0) {
        std::fprintf(stderr, "sceNetInit failed\n");
        return 1;
    }
    passed = FamilyReportsNotImplemented() && passed;
    return passed ? 0 : 1;
}
