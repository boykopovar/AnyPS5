#include "prx/libkernel/System/include/MacActivity.hpp"
#include <dlfcn.h>
#include <exception>
#include <utility>

namespace HostPerformance {

MacActivity::MacActivity(ObjcLibraries libraries) : libraries(std::move(libraries)) {}

MacActivity::~MacActivity() {
    try {
        End();
    } catch (const std::exception&) {
    }
    if (foundation != nullptr) dlclose(foundation);
    if (runtime != nullptr) dlclose(runtime);
}

const char* MacActivity::Name() const {
    return "macos-activity";
}

BeginResult MacActivity::Load() {
    if (send != nullptr) return {Outcome::Started, {}};
    runtime = dlopen(libraries.Runtime.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (runtime == nullptr) {
        const char* reason = dlerror();
        return {Outcome::Unavailable, reason != nullptr ? reason : libraries.Runtime + ": unknown dlopen failure"};
    }
    foundation = dlopen(libraries.Foundation.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (foundation == nullptr) {
        const char* reason = dlerror();
        return {Outcome::Unavailable, reason != nullptr ? reason : libraries.Foundation + ": unknown dlopen failure"};
    }
    getClass = reinterpret_cast<GetClass>(dlsym(runtime, "objc_getClass"));
    registerSelector = reinterpret_cast<RegisterSelector>(dlsym(runtime, "sel_registerName"));
    poolPush = reinterpret_cast<PoolPush>(dlsym(runtime, "objc_autoreleasePoolPush"));
    poolPop = reinterpret_cast<PoolPop>(dlsym(runtime, "objc_autoreleasePoolPop"));
    void* const message = dlsym(runtime, "objc_msgSend");
    if (getClass == nullptr) return {Outcome::Unavailable, "the Objective-C runtime has no objc_getClass"};
    if (registerSelector == nullptr) return {Outcome::Unavailable, "the Objective-C runtime has no sel_registerName"};
    if (poolPush == nullptr || poolPop == nullptr) return {Outcome::Unavailable, "the Objective-C runtime has no autorelease pool functions"};
    if (message == nullptr) return {Outcome::Unavailable, "the Objective-C runtime has no objc_msgSend"};
    send = message;
    return {Outcome::Started, {}};
}

BeginResult MacActivity::Begin() {
    if (token != nullptr) return {Outcome::Started, {}};
    if (const auto loaded = Load(); loaded.Result != Outcome::Started) return loaded;
    void* const pool = poolPush();
    BeginResult result{Outcome::Started, {}};
    void* const processInfoClass = getClass("NSProcessInfo");
    void* const stringClass = getClass("NSString");
    if (processInfoClass == nullptr || stringClass == nullptr) {
        result = {Outcome::Unavailable, "NSProcessInfo is not loaded"};
    } else {
        void* const info = reinterpret_cast<SendObject>(send)(processInfoClass, registerSelector("processInfo"));
        void* const reason = reinterpret_cast<SendString>(send)(stringClass, registerSelector("stringWithUTF8String:"), "AnyPS5 title running");
        void* const activity = info != nullptr && reason != nullptr
            ? reinterpret_cast<SendBegin>(send)(info, registerSelector("beginActivityWithOptions:reason:"), ActivityUserInitiated | ActivityLatencyCritical, reason)
            : nullptr;
        if (activity == nullptr) {
            result = {Outcome::Refused, "beginActivityWithOptions:reason: returned no activity"};
        } else {
            reinterpret_cast<SendObject>(send)(activity, registerSelector("retain"));
            processInfo = info;
            token = activity;
        }
    }
    poolPop(pool);
    return result;
}

void MacActivity::End() {
    if (token == nullptr) return;
    void* const pool = poolPush();
    void* const activity = token;
    token = nullptr;
    reinterpret_cast<SendEnd>(send)(processInfo, registerSelector("endActivity:"), activity);
    reinterpret_cast<SendVoid>(send)(activity, registerSelector("release"));
    processInfo = nullptr;
    poolPop(pool);
}

}
