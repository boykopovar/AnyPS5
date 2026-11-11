#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <mutex>

namespace {

std::recursive_mutex& OnceMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}

}

extern "C" void APS5_VABI _Call_onceEx_nid_postfix(std::int32_t* flag,
                                                  void (APS5_VABI *callback)(void*),
                                                  void* argument) {
    std::lock_guard lock(OnceMutex());
    std::int32_t value;
    if (!GuestCopyIn_nid_no_patch(&value, flag, sizeof(value))) APS5_INVALID_ARG_EX;
    if (value != 0) return;
    if (!callback) APS5_INVALID_ARG_EX;
    callback(argument);
    value = 1;
    if (!GuestCopyOut_nid_no_patch(flag, &value, sizeof(value))) APS5_INVALID_ARG_EX;
}
