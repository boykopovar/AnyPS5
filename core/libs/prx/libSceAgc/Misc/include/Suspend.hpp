#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SUSPEND_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SUSPEND_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcSuspendPoint(void);
int APS5_VABI sceAgcSetSubmitMode(std::uint32_t mode);

}

#endif
