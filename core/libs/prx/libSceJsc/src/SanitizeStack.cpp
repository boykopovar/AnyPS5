#include "prx/libSceJsc/include/SanitizeStackExports.hpp"

#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3JSC18sanitizeStackForVMERNS_2VME(void* vm) {
    if (!vm) throw std::invalid_argument("JSC::sanitizeStackForVM requires a VM");
}

}
