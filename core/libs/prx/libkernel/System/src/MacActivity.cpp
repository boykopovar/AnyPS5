#include "prx/libkernel/System/include/MacActivity.hpp"
#include <dlfcn.h>
#include <exception>
#include <utility>

namespace HostPerformance {

namespace {

class AutoreleasePoolGuard {
public:
    using Pop = void (*)(void*);

    AutoreleasePoolGuard(void* pool, Pop pop) : pool(pool), pop(pop) {}
    ~AutoreleasePoolGuard() {
        if (pop != nullptr) pop(pool);
    }

    AutoreleasePoolGuard(const AutoreleasePoolGuard&) = delete;
    AutoreleasePoolGuard& operator=(const AutoreleasePoolGuard&) = delete;

private:
    void* pool;
    Pop pop;
};

}

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
    auto fail = [this](std::string detail) -> BeginResult {
        if (foundation != nullptr) {
            dlclose(foundation);
            foundation = nullptr;
        }
        if (runtime != nullptr) {
            dlclose(runtime);
            runtime = nullptr;
        }
        getClass = nullptr;
        registerSelector = nullptr;
        poolPush = nullptr;
        poolPop = nullptr;
        send = nullptr;
        return {Outcome::Unavailable, std::move(detail)};
    };
    runtime = dlopen(libraries.Runtime.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (runtime == nullptr) {
        const char* reason = dlerror();
        return fail(reason != nullptr ? reason : libraries.Runtime + ": unknown dlopen failure");
    }
    foundation = dlopen(libraries.Foundation.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (foundation == nullptr) {
        const char* reason = dlerror();
        return fail(reason != nullptr ? reason : libraries.Foundation + ": unknown dlopen failure");
    }
    getClass = reinterpret_cast<GetClass>(dlsym(runtime, "objc_getClass"));
    registerSelector = reinterpret_cast<RegisterSelector>(dlsym(runtime, "sel_registerName"));
    poolPush = reinterpret_cast<PoolPush>(dlsym(runtime, "objc_autoreleasePoolPush"));
    poolPop = reinterpret_cast<PoolPop>(dlsym(runtime, "objc_autoreleasePoolPop"));
    void* const message = dlsym(runtime, "objc_msgSend");
    if (getClass == nullptr) return fail("the Objective-C runtime has no objc_getClass");
    if (registerSelector == nullptr) return fail("the Objective-C runtime has no sel_registerName");
    if (poolPush == nullptr || poolPop == nullptr) return fail("the Objective-C runtime has no autorelease pool functions");
    if (message == nullptr) return fail("the Objective-C runtime has no objc_msgSend");
    send = message;
    return {Outcome::Started, {}};
}

BeginResult MacActivity::Begin() {
    if (token != nullptr) return {Outcome::Started, {}};
    if (const auto loaded = Load(); loaded.Result != Outcome::Started) return loaded;
    AutoreleasePoolGuard poolGuard(poolPush(), poolPop);
    void* const processInfoClass = getClass("NSProcessInfo");
    void* const stringClass = getClass("NSString");
    if (processInfoClass == nullptr || stringClass == nullptr) {
        return {Outcome::Unavailable, "NSProcessInfo is not loaded"};
    }
    void* const processInfoSelector = registerSelector("processInfo");
    void* const stringSelector = registerSelector("stringWithUTF8String:");
    void* const beginSelector = registerSelector("beginActivityWithOptions:reason:");
    void* const retainSelector = registerSelector("retain");
    if (processInfoSelector == nullptr || stringSelector == nullptr || beginSelector == nullptr || retainSelector == nullptr) {
        return {Outcome::Unavailable, "required Objective-C selectors are unavailable"};
    }
    void* const info = reinterpret_cast<SendObject>(send)(processInfoClass, processInfoSelector);
    void* const reason = reinterpret_cast<SendString>(send)(stringClass, stringSelector, "AnyPS5 title running");
    void* const activity = info != nullptr && reason != nullptr
        ? reinterpret_cast<SendBegin>(send)(info, beginSelector, ActivityUserInitiated | ActivityLatencyCritical, reason)
        : nullptr;
    if (activity == nullptr) {
        return {Outcome::Refused, "beginActivityWithOptions:reason: returned no activity"};
    }
    reinterpret_cast<SendObject>(send)(activity, retainSelector);
    processInfo = info;
    token = activity;
    return {Outcome::Started, {}};
}

void MacActivity::End() {
    if (token == nullptr) return;
    AutoreleasePoolGuard poolGuard(poolPush(), poolPop);
    void* const info = processInfo;
    void* const activity = token;
    processInfo = nullptr;
    token = nullptr;
    void* const endSelector = registerSelector("endActivity:");
    void* const releaseSelector = registerSelector("release");
    if (info != nullptr && endSelector != nullptr) {
        reinterpret_cast<SendEnd>(send)(info, endSelector, activity);
    }
    if (releaseSelector != nullptr) {
        reinterpret_cast<SendVoid>(send)(activity, releaseSelector);
    }
}

}
