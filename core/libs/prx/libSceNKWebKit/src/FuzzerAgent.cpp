#include "prx/libSceNKWebKit/include/FuzzerAgentExports.hpp"

#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3JSC11FuzzerAgentD0Ev(void* self) {
    if (!self) throw std::invalid_argument("JSC::FuzzerAgent deleting destructor requires an object");
    _ZdlPv_nid_postfix(self);
}

std::uint64_t APS5_VABI _ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(void*, void*, const void*, std::uint64_t original) {
    return original;
}

}
