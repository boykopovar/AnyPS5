#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "sanitizeStackForVM check failed at line %d\n", line);
        std::abort();
    }
}

}

int main() {
    unsigned char vm[64];
    std::memset(vm, 0xA5, sizeof(vm));
    _ZN3JSC18sanitizeStackForVMERNS_2VME(vm);
    _ZN3JSC18sanitizeStackForVMERNS_2VME(vm);
    for (unsigned char byte : vm) Check(byte == 0xA5, __LINE__);

    bool threw = false;
    try {
        _ZN3JSC18sanitizeStackForVMERNS_2VME(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw, __LINE__);

    std::puts("JSC sanitizeStackForVM tests passed");
    return 0;
}
