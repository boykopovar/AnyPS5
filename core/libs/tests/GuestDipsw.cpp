#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceKernelCheckDipsw();
int APS5_VABI sceKernelInitializeDipsw();
int APS5_VABI sceKernelIsAssistMode();
int APS5_VABI sceKernelIsDevelopmentMode();
int APS5_VABI sceKernelIsDisableRazor();
int APS5_VABI sceKernelReadDipswData();
int APS5_VABI sceKernelSetDipsw();
int APS5_VABI sceKernelUnsetDipsw();
int APS5_VABI sceKernelWriteDipswData();
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool ReportsNotImplemented(const char* name, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

namespace {

void TestRetailDefaults() {
    Require(sceKernelInitializeDipsw() == 0);
    Require(sceKernelIsAssistMode() == 0);
    Require(sceKernelIsDevelopmentMode() == 0);
}

void TestUnimplementedThrow() {
    Require(ReportsNotImplemented("sceKernelCheckDipsw", [] { return sceKernelCheckDipsw(); }));
    Require(ReportsNotImplemented("sceKernelIsDisableRazor", [] { return sceKernelIsDisableRazor(); }));
    Require(ReportsNotImplemented("sceKernelReadDipswData", [] { return sceKernelReadDipswData(); }));
    Require(ReportsNotImplemented("sceKernelSetDipsw", [] { return sceKernelSetDipsw(); }));
    Require(ReportsNotImplemented("sceKernelUnsetDipsw", [] { return sceKernelUnsetDipsw(); }));
    Require(ReportsNotImplemented("sceKernelWriteDipswData", [] { return sceKernelWriteDipswData(); }));
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(std::strcmp(argv[1], "retail") == 0);
    TestRetailDefaults();
    TestUnimplementedThrow();
}
