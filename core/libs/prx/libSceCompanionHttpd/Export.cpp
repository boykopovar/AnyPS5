#include "prx/libSceCompanionHttpd/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int CompanionHttpdErrorInvalidParam = static_cast<int>(0x80E40004u);
constexpr int CompanionHttpdErrorNotInitialized = static_cast<int>(0x80E40006u);
constexpr int CompanionHttpdErrorAlreadyInitialized = static_cast<int>(0x80E40007u);
constexpr int CompanionHttpdErrorNoEvent = static_cast<int>(0x80E40008u);
constexpr int CompanionHttpdErrorAlreadyStarted = static_cast<int>(0x80E4000Au);
constexpr int CompanionHttpdErrorNotStarted = static_cast<int>(0x80E4000Bu);
constexpr int CompanionHttpdErrorAlreadyRegistered = static_cast<int>(0x80E4000Cu);

static_assert(sizeof(CompanionHttpdOptParam) == 0x60);

struct CompanionHttpdState {
    std::mutex mutex;
    bool initialized = false;
    bool started = false;
    CompanionHttpdRequestCallback callback = nullptr;
    void* callbackArgument = nullptr;
};

CompanionHttpdState& State() {
    static CompanionHttpdState state;
    return state;
}

}

extern "C" {

int APS5_VABI sceCompanionHttpdOptParamInitialize(CompanionHttpdOptParam* param) {
    if (param == nullptr) return CompanionHttpdErrorInvalidParam;
    std::memset(param, 0, sizeof(*param));
    return 0;
}

int APS5_VABI sceCompanionHttpdInitialize(const CompanionHttpdOptParam* param) {
    if (param == nullptr) return CompanionHttpdErrorInvalidParam;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initialized) return CompanionHttpdErrorAlreadyInitialized;
    state.initialized = true;
    return 0;
}

int APS5_VABI sceCompanionHttpdTerminate(void) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    if (state.started) NotImplemented_nid_no_patch(__func__);
    state.initialized = false;
    state.callback = nullptr;
    state.callbackArgument = nullptr;
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestCallback(CompanionHttpdRequestCallback function, void* argument) {
    if (function == nullptr) return CompanionHttpdErrorInvalidParam;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    if (state.callback != nullptr) return CompanionHttpdErrorAlreadyRegistered;
    state.callback = function;
    state.callbackArgument = argument;
    return 0;
}

int APS5_VABI sceCompanionHttpdUnregisterRequestCallback(void) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    if (state.callback == nullptr) NotImplemented_nid_no_patch(__func__);
    state.callback = nullptr;
    state.callbackArgument = nullptr;
    return 0;
}

int APS5_VABI sceCompanionHttpdStart(void) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    if (state.started) return CompanionHttpdErrorAlreadyStarted;
    state.started = true;
    return 0;
}

int APS5_VABI sceCompanionHttpdStop(void) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    if (!state.started) return CompanionHttpdErrorNotStarted;
    state.started = false;
    return 0;
}

int APS5_VABI sceCompanionHttpdGetEvent(void* event) {
    if (event == nullptr) return CompanionHttpdErrorInvalidParam;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return CompanionHttpdErrorNotInitialized;
    return CompanionHttpdErrorNoEvent;
}

int APS5_VABI sceCompanionHttpdAddHeader(const char* key, const char* value, void* response) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdSetStatus(int status, void* response) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdSetBody(const char* body, std::uint64_t size, void* response) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdGet2ndScreenStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdGetUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdInitialize2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestCallback2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
