#include "prx/libSceAgcDriver/State/include/Configuration.hpp"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct TessellationFactorRing {
 uintptr_t base = 0xff0000000;
 uint32_t size = 0x20000;
};

std::mutex tessellationFactorRingMutex;
TessellationFactorRing tessellationFactorRing;

}

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
 (void)value0;
 (void)value1;
 (void)value2;
 return 0;
}

int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
 if (base == nullptr || size == 0) throw std::invalid_argument("sceAgcDriverSetTFRing: empty ring");
 std::lock_guard lock(tessellationFactorRingMutex);
 tessellationFactorRing = {reinterpret_cast<uintptr_t>(base), size};
 return 0;
}

int APS5_VABI sceAgcDriverGetTFRing(uintptr_t* base, uint32_t* size) {
 if (base == nullptr || size == nullptr) throw std::invalid_argument("sceAgcDriverGetTFRing: null output");
 std::lock_guard lock(tessellationFactorRingMutex);
 *base = tessellationFactorRing.base;
 *size = tessellationFactorRing.size;
 return 0;
}

}
