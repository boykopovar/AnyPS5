#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NetworkStorageSession cookies check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct Wrapper;

struct Vtable {
    void (APS5_VABI* completeDestructor)(Wrapper* self);
    void (APS5_VABI* deletingDestructor)(Wrapper* self);
    void (APS5_VABI* call)(Wrapper* self, bool value);
};

struct Wrapper {
    const Vtable* vtable;
    int calls;
    int lastValue;
    int deletes;
    bool throwOnCall;
};

void APS5_VABI CompleteDestructor(Wrapper*) {
    std::abort();
}

void APS5_VABI DeletingDestructor(Wrapper* self) {
    ++self->deletes;
}

void APS5_VABI Call(Wrapper* self, bool value) {
    ++self->calls;
    self->lastValue = value ? 1 : 0;
    if (self->throwOnCall) throw std::runtime_error("handler failed");
}

const Vtable WrapperVtable{CompleteDestructor, DeletingDestructor, Call};

void HasCookies(void* handler) {
    _ZNK7WebCore21NetworkStorageSession10hasCookiesERKNS_17RegistrableDomainEON3WTF17CompletionHandlerIFvbEEE(nullptr, nullptr, handler);
}

}

int main() {
    Wrapper wrapper{&WrapperVtable, 0, -1, 0, false};
    Wrapper* handler = &wrapper;
    HasCookies(&handler);
    Require(wrapper.calls == 1);
    Require(wrapper.lastValue == 0);
    Require(wrapper.deletes == 1);
    Require(handler == nullptr);

    bool threw = false;
    try {
        HasCookies(&handler);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        HasCookies(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Wrapper failing{&WrapperVtable, 0, -1, 0, true};
    handler = &failing;
    threw = false;
    try {
        HasCookies(&handler);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(failing.calls == 1);
    Require(failing.deletes == 1);
    Require(handler == nullptr);
    return 0;
}
