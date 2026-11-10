#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SUSPEND_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SUSPEND_HPP

#include <cstdint>
#include "SceTypes.hpp"
#include "SceShaders.hpp"

// Suspend-disallowed graphics error for sceAgcSuspendPointAndCheckStatus: the value
// 0x8A6C002F names the driver-refused case and must not be confused with
// GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM (0x8A6C0005), which covers shader programs.
// The value is review-assigned; firmware behaviour is untraced, see debt libSceAgc.md.
constexpr int GRAPHICS5_ERROR_SUSPEND_DISALLOWED = static_cast<int>(0x8a6c002fu);

extern "C" {

int APS5_VABI sceAgcSuspendPoint(void);
int APS5_VABI sceAgcSuspendPointAndCheckStatus(void* suspendPoint);
int APS5_VABI sceAgcSetSubmitMode(std::uint32_t mode);

}

#endif
