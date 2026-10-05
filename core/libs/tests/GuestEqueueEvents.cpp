#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddUserEvent(KernelEqueue eq, int id);
int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
int APS5_VABI sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata);
intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev);
intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev);
int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev);
uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev);
void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev);
}

static constexpr int SCE_OK = 0;
static constexpr int EVFILT_USER = -11;
static constexpr int EVFILT_HRTIMER = -15;

static void Require(bool value) { if (!value) std::abort(); }

template <typename TResult>
static bool RejectsNull(TResult (APS5_VABI *accessor)(const KernelEvent*)) {
    try {
        accessor(nullptr);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "events") == SCE_OK);
    const KernelUseconds timeout = 1000000;
    int payload = 0;
    int count = 0;

    KernelEvent userEvent{};
    Require(sceKernelAddUserEvent(eq, 7) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 7, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, &userEvent, 1, &count, &timeout) == SCE_OK);
    Require(count == 1);
    Require(sceKernelGetEventId(&userEvent) == 7);
    Require(sceKernelGetEventFilter(&userEvent) == EVFILT_USER);
    Require(sceKernelGetEventData(&userEvent) == reinterpret_cast<intptr_t>(&payload));
    Require(sceKernelGetEventUserData(&userEvent) == &payload);
    Require(sceKernelGetEventFflags(&userEvent) == 0);
    Require(sceKernelDeleteUserEvent(eq, 7) == SCE_OK);

    KernelEvent timerEvent{};
    const KernelTimespec delay{0, 1000000};
    Require(sceKernelAddHRTimerEvent(eq, 9, &delay, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, &timerEvent, 1, &count, &timeout) == SCE_OK);
    Require(count == 1);
    Require(sceKernelGetEventId(&timerEvent) == 9);
    Require(sceKernelGetEventFilter(&timerEvent) == EVFILT_HRTIMER);
    Require(sceKernelGetEventUserData(&timerEvent) == &payload);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);

    KernelEvent rawEvent{};
    rawEvent.ident = UINTPTR_MAX;
    rawEvent.filter = INT16_MIN;
    rawEvent.fflags = 0x80000001u;
    rawEvent.data = -5;
    Require(sceKernelGetEventId(&rawEvent) == UINTPTR_MAX);
    Require(sceKernelGetEventFilter(&rawEvent) == INT16_MIN);
    Require(sceKernelGetEventFflags(&rawEvent) == static_cast<intptr_t>(0x80000001LL));
    Require(sceKernelGetEventData(&rawEvent) == -5);
    Require(sceKernelGetEventUserData(&rawEvent) == nullptr);

    Require(RejectsNull(sceKernelGetEventData));
    Require(RejectsNull(sceKernelGetEventFflags));
    Require(RejectsNull(sceKernelGetEventFilter));
    Require(RejectsNull(sceKernelGetEventId));
    Require(RejectsNull(sceKernelGetEventUserData));
}
