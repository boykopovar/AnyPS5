#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <stdexcept>

namespace {

struct BoolCallableWrapper;

struct BoolCallableWrapperVtable {
    void (APS5_VABI* completeDestructor)(BoolCallableWrapper* self);
    void (APS5_VABI* deletingDestructor)(BoolCallableWrapper* self);
    void (APS5_VABI* call)(BoolCallableWrapper* self, bool value);
};

struct BoolCallableWrapper {
    const BoolCallableWrapperVtable* vtable;
};

struct BoolCompletionHandler {
    BoolCallableWrapper* callableWrapper;
};

struct CallableDeleter {
    BoolCallableWrapper* wrapper;

    ~CallableDeleter() {
        wrapper->vtable->deletingDestructor(wrapper);
    }
};

}

extern "C" {

void APS5_VABI _ZNK7WebCore21NetworkStorageSession10hasCookiesERKNS_17RegistrableDomainEON3WTF17CompletionHandlerIFvbEEE(const void*, const void*, void* completionHandler) {
    auto* handler = static_cast<BoolCompletionHandler*>(completionHandler);
    if (!handler || !handler->callableWrapper) throw std::invalid_argument("WebCore::NetworkStorageSession::hasCookies requires a completion handler");
    CallableDeleter callable{handler->callableWrapper};
    handler->callableWrapper = nullptr;
    callable.wrapper->vtable->call(callable.wrapper, false);
}

}
