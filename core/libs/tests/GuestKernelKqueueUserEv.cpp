#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

static void VerifyKernelKqueueUserEv() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "userev") == SCE_OK);
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    int payload = 0;

    Require(sceKernelAddUserEvent(eq, 100) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 100, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
    Require(count == 1);
    Require(sceKernelGetEventId(&events[0]) == 100);
    Require(sceKernelGetEventFilter(&events[0]) == EVFILT_USER);
    Require(sceKernelGetEventUserData(&events[0]) == &payload);
    Require(sceKernelGetEventFflags(&events[0]) == 0);

    Require(sceKernelDeleteUserEvent(eq, 100) == SCE_OK);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

int main() {
    VerifyKernelKqueueUserEv();
}
