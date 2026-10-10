#include <mutex>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"

namespace {

struct Helper {
    std::mutex mutex;
    bool initialized = false;
};

Helper& State() {
    static Helper helper;
    return helper;
}

[[noreturn]] void Fail(const char* function, const std::string& why) {
    throw std::logic_error(std::string(function) + ": " + why);
}

void RequireInitialized(const Helper& helper, const char* function) {
    if (!helper.initialized) Fail(function, "library not initialized");
}

}  // namespace

extern "C" {

int APS5_VABI sceProprietaryVoiceChatHelperInitialize(void) {
    auto& helper = State();
    std::lock_guard lock(helper.mutex);
    if (helper.initialized) Fail(__func__, "already initialized");
    helper.initialized = true;
    return 0;
}

int APS5_VABI sceProprietaryVoiceChatHelperTerminate(void) {
    auto& helper = State();
    std::lock_guard lock(helper.mutex);
    RequireInitialized(helper, __func__);
    helper.initialized = false;
    return 0;
}

int APS5_VABI sceProprietaryVoiceChatHelperSetVoiceChatState(void) {
    auto& helper = State();
    std::lock_guard lock(helper.mutex);
    RequireInitialized(helper, __func__);
    return 0;
}

int APS5_VABI sceProprietaryVoiceChatHelperGetVoiceChatUsageState(void) {
    auto& helper = State();
    std::lock_guard lock(helper.mutex);
    RequireInitialized(helper, __func__);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
