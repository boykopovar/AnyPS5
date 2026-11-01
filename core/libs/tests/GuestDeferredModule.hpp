#ifndef CORE_LIBS_TESTS_GUESTDEFERREDMODULE_HPP
#define CORE_LIBS_TESTS_GUESTDEFERREDMODULE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"
#include <windows.h>
#include <cstdint>

struct GuestDeferredModuleArgs {
    unsigned Mode = 0;
    HANDLE Entered = nullptr;
    HANDLE Release = nullptr;
    void (APS5_VABI* Reenter)(const char*) = nullptr;
    const char* Path = nullptr;
};

inline constexpr int GuestDeferredModuleResult = 0x5a17;

#endif
