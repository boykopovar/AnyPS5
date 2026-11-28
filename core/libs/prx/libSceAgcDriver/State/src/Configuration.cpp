#include "prx/libSceAgcDriver/State/include/Configuration.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"

#include <cstdint>
#include <cstddef>
#include <cerrno>
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

struct HsOffchipParam {
 uint16_t granularity = 0;
 uint16_t buffering = 0;
};

std::mutex hsOffchipParamMutex;
HsOffchipParam hsOffchipParam;

}

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint16_t granularity, uint16_t buffering) {
 if (granularity > 3 || (buffering > 0x1ff && buffering != 0x3ff)) throw std::runtime_error("sceAgcDriverSetHsOffchipParam: what the console keeps for a granularity above 3 or a buffering above 511 other than 1023 is not measured");
 std::lock_guard lock(hsOffchipParamMutex);
 hsOffchipParam = {granularity, static_cast<uint16_t>(buffering & 0x1ff)};
 return 0;
}

int APS5_VABI sceAgcDriverGetHsOffchipParam(uint16_t* granularity, uint16_t* buffering) {
 if (granularity == nullptr || buffering == nullptr) throw std::invalid_argument("sceAgcDriverGetHsOffchipParam: null output");
 std::lock_guard lock(hsOffchipParamMutex);
 const auto localParam = hsOffchipParam;
 const int savedErrno = errno;
 const bool copiedGranularity = GuestCopyOut_nid_no_patch(granularity, &localParam.granularity, sizeof(localParam.granularity));
 const bool copiedBuffering = GuestCopyOut_nid_no_patch(buffering, &localParam.buffering, sizeof(localParam.buffering));
 errno = savedErrno;
 if (!copiedGranularity || !copiedBuffering) throw std::invalid_argument("sceAgcDriverGetHsOffchipParam: invalid output pointer");
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
 const auto localRing = tessellationFactorRing;
 const int savedErrno = errno;
 const bool copiedBase = GuestCopyOut_nid_no_patch(base, &localRing.base, sizeof(localRing.base));
 const bool copiedSize = GuestCopyOut_nid_no_patch(size, &localRing.size, sizeof(localRing.size));
 errno = savedErrno;
 if (!copiedBase || !copiedSize) throw std::invalid_argument("sceAgcDriverGetTFRing: invalid output pointer");
 return 0;
}

}
