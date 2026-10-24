#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceSystemLoggerInitialize();
int APS5_VABI sceSystemLoggerSetLogData();
int APS5_VABI sceSystemLoggerTerminate();
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

void TestLifecycle() {
    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerTerminate() == 0);
    Require(sceSystemLoggerTerminate() == 0);
    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerTerminate() == 0);
}

void TestUnimplementedThrow() {
    Require(ReportsNotImplemented("sceSystemLoggerSetLogData", [] { return sceSystemLoggerSetLogData(); }));
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(std::strcmp(argv[1], "lifecycle") == 0);
    TestLifecycle();
    TestUnimplementedThrow();
}
