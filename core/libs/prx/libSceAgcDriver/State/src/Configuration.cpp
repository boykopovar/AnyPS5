#include "prx/libSceAgcDriver/State/include/Configuration.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// Tessellation runs as Vulkan tessellation stages, which keep hull-shader outputs and tessellation
// factors on the host GPU: the off-chip hull memory and the tessellation-factor ring are never read.
int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
 (void)value0;
 (void)value1;
 (void)value2;
 return 0;
}

int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
 if (base == nullptr || size == 0) throw std::invalid_argument("sceAgcDriverSetTFRing: empty ring");
 return 0;
}

}
