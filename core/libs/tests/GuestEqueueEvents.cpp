#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <thread>

static constexpr int SCE_OK = 0;

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

static bool RejectsWideFflags(const KernelEvent* ev) {
    try {
        sceKernelGetEventError(ev);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void SleepAtLeast(std::chrono::steady_clock::duration duration) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < duration) {
        std::this_thread::sleep_for(duration);
    }
}

static void VerifyPeriodicTimer() {
    using namespace std::chrono_literals;
    int first = 0;
    int second = 0;
    Require(sceKernelAddTimerEvent(0, 10, 1000, &first) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteTimerEvent(0, 10) == SCE_KERNEL_ERROR_EBADF);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "timer") == SCE_OK);
    Require(sceKernelDeleteTimerEvent(eq, 10) == SCE_KERNEL_ERROR_ENOENT);

    KernelEvent events[2]{};
    int count = 0;
    const KernelUseconds poll = 0;
    const KernelUseconds wait = 1000000;
    Require(sceKernelAddTimerEvent(eq, 10, 1000, &first) == SCE_OK);
    SleepAtLeast(5ms);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_OK);
    Require(count == 1);
    Require(sceKernelGetEventId(&events[0]) == 10);
    Require(sceKernelGetEventFilter(&events[0]) == EVFILT_TIMER);
    Require(sceKernelGetEventData(&events[0]) >= 5);
    Require(sceKernelGetEventUserData(&events[0]) == &first);
    Require(sceKernelGetEventError(&events[0]) == 0);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &wait) == SCE_OK);
    Require(count == 1 && sceKernelGetEventData(&events[0]) >= 1);

    SleepAtLeast(5ms);
    Require(sceKernelAddTimerEvent(eq, 10, 3000000000u, &second) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_OK);
    Require(count == 1);
    Require(sceKernelGetEventData(&events[0]) >= 5);
    Require(sceKernelGetEventUserData(&events[0]) == &second);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelDeleteTimerEvent(eq, 10) == SCE_OK);
    Require(sceKernelDeleteTimerEvent(eq, 10) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelAddTimerEvent(eq, 13, 3000000000u, &first) == SCE_OK);
    const auto readded = std::chrono::steady_clock::now();
    Require(sceKernelAddTimerEvent(eq, 13, 200000, &first) == SCE_OK);
    SleepAtLeast(250ms);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_OK);
    Require(count == 1 && sceKernelGetEventData(&events[0]) >= 1);
    const intptr_t expirations = sceKernelGetEventData(&events[0]);
    const int again = sceKernelWaitEqueue(eq, events, 2, &count, &poll);
    if (std::chrono::steady_clock::now() - readded < 400ms) {
        Require(expirations == 1 && again == SCE_KERNEL_ERROR_ETIMEDOUT);
    }
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &wait) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 13);
    Require(sceKernelDeleteTimerEvent(eq, 13) == SCE_OK);

    Require(sceKernelAddTimerEvent(eq, 14, 1000, &first) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, nullptr) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 14 && sceKernelGetEventData(&events[0]) >= 1);
    Require(sceKernelDeleteTimerEvent(eq, 14) == SCE_OK);
    const KernelTimespec soon{0, 1000000};
    Require(sceKernelAddHRTimerEvent(eq, 15, &soon, &second) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, nullptr) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 15 && sceKernelGetEventFilter(&events[0]) == EVFILT_HRTIMER);

    Require(sceKernelAddTimerEvent(eq, 11, 0, &first) == SCE_OK);
    Require(sceKernelAddTimerEvent(eq, 11, 0, &first) == SCE_OK);
    for (int i = 0; i < 2; ++i) {
        Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_OK);
        Require(count == 1 && sceKernelGetEventData(&events[0]) == 1);
    }
    Require(sceKernelDeleteTimerEvent(eq, 11) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 2, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);

    const KernelTimespec delay{0, 1000000};
    Require(sceKernelAddHRTimerEvent(eq, 12, &delay, &first) == SCE_OK);
    Require(sceKernelDeleteTimerEvent(eq, 12) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void VerifyUserEventReportedOncePerWait() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "user") == SCE_OK);
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    int payload = 0;

    Require(sceKernelAddUserEvent(eq, 5) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelTriggerUserEvent(eq, 5, &payload) == SCE_OK);
    for (int i = 0; i < 2; ++i) {
        count = 0;
        Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
        Require(count == 1 && sceKernelGetEventId(&events[0]) == 5);
    }
    Require(sceKernelDeleteUserEvent(eq, 5) == SCE_OK);

    Require(sceKernelAddUserEventEdge(eq, 6) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 6, &payload) == SCE_OK);
    Require(sceKernelAddUserEvent(eq, 8) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 8, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
    Require(count == 2);
    Require(sceKernelGetEventId(&events[0]) + sceKernelGetEventId(&events[1]) == 14);
    Require(sceKernelGetEventId(&events[0]) != sceKernelGetEventId(&events[1]));
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 8);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void VerifyBlockingWaitReportsLevelEventOnce() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "blocking") == SCE_OK);
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds wait = 1000000;
    int payload = 0;

    Require(sceKernelAddUserEvent(eq, 5) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 5, &payload) == SCE_OK);
    for (int i = 0; i < 2; ++i) {
        count = 0;
        Require(sceKernelWaitEqueue(eq, events, 4, &count, &wait) == SCE_OK);
        Require(count == 1 && sceKernelGetEventId(&events[0]) == 5);
        Require(sceKernelGetEventUserData(&events[0]) == &payload);
    }
    Require(sceKernelDeleteUserEvent(eq, 5) == SCE_OK);

    Require(sceKernelAddUserEvent(eq, 6) == SCE_OK);
    std::thread trigger([eq, &payload] {
        SleepAtLeast(std::chrono::milliseconds(10));
        Require(sceKernelTriggerUserEvent(eq, 6, &payload) == SCE_OK);
    });
    count = 0;
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &wait) == SCE_OK);
    trigger.join();
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 6);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void VerifyEdgeUserEventReportedOnce() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "edge") == SCE_OK);
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    const KernelUseconds wait = 1000000;
    int payload = 0;

    Require(sceKernelAddUserEventEdge(eq, 6) == SCE_OK);
    Require(sceKernelTriggerUserEvent(eq, 6, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &wait) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 6);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelTriggerUserEvent(eq, 6, &payload) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
    Require(count == 1 && sceKernelGetEventId(&events[0]) == 6);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void VerifyRepeatedAmprCompletionsAreNotLost() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "ampr") == SCE_OK);
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    int udata = 0;
    auto data = [](std::uintptr_t value) { return reinterpret_cast<void*>(value); };

    Require(sceKernelAddAmprEvent(eq, 3, &udata) == SCE_OK);
    Require(EqueueTriggerEvent_nid_postfix(eq, 3, EVFILT_AMPR, data(0x11)) == SCE_OK);
    Require(EqueueTriggerEvent_nid_postfix(eq, 3, EVFILT_AMPR, data(0x22)) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_OK);
    Require(count == 2);
    Require(sceKernelGetEventFilter(&events[0]) == EVFILT_AMPR && sceKernelGetEventFilter(&events[1]) == EVFILT_AMPR);
    Require(sceKernelGetEventId(&events[0]) == 3 && sceKernelGetEventId(&events[1]) == 3);
    Require(sceKernelGetEventData(&events[0]) == 0x11 && sceKernelGetEventData(&events[1]) == 0x22);
    Require(sceKernelGetEventUserData(&events[0]) == &udata && sceKernelGetEventUserData(&events[1]) == &udata);
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);

    Require(EqueueTriggerEvent_nid_postfix(eq, 3, EVFILT_AMPR, data(0x33)) == SCE_OK);
    Require(EqueueTriggerEvent_nid_postfix(eq, 3, EVFILT_AMPR, data(0x44)) == SCE_OK);
    Require(EqueueTriggerEvent_nid_postfix(eq, 3, EVFILT_AMPR, data(0x55)) == SCE_OK);
    for (const intptr_t expected : {0x33, 0x44, 0x55}) {
        Require(sceKernelWaitEqueue(eq, events, 1, &count, &poll) == SCE_OK);
        Require(count == 1 && sceKernelGetEventData(&events[0]) == expected);
    }
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelDeleteAmprEvent(eq, 3) == SCE_OK);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void RequireReported(KernelEqueue eq, int num, std::initializer_list<uintptr_t> expected) {
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    Require(sceKernelWaitEqueue(eq, events, num, &count, &poll) == SCE_OK);
    Require(count == static_cast<int>(expected.size()));
    int index = 0;
    for (const uintptr_t id : expected) {
        Require(sceKernelGetEventId(&events[index++]) == id);
    }
}

static void VerifyReportedEventsMoveBehindWaitingOnes() {
    KernelEqueue eq = 0;
    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds poll = 0;
    int payload = 0;
    Require(sceKernelCreateEqueue(&eq, "rotate") == SCE_OK);
    for (const int id : {1, 2}) {
        Require(sceKernelAddUserEvent(eq, id) == SCE_OK);
        Require(sceKernelTriggerUserEvent(eq, id, &payload) == SCE_OK);
    }
    RequireReported(eq, 1, {1});
    RequireReported(eq, 1, {2});
    RequireReported(eq, 1, {1});
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);

    Require(sceKernelCreateEqueue(&eq, "rotate three") == SCE_OK);
    for (const int id : {1, 2, 3}) {
        Require(sceKernelAddUserEvent(eq, id) == SCE_OK);
        Require(sceKernelTriggerUserEvent(eq, id, &payload) == SCE_OK);
    }
    RequireReported(eq, 2, {1, 2});
    RequireReported(eq, 2, {3, 1});
    RequireReported(eq, 2, {2, 3});
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);

    Require(sceKernelCreateEqueue(&eq, "rotate edge") == SCE_OK);
    for (const int id : {7, 8}) {
        Require(sceKernelAddUserEventEdge(eq, id) == SCE_OK);
        Require(sceKernelTriggerUserEvent(eq, id, &payload) == SCE_OK);
    }
    RequireReported(eq, 1, {7});
    Require(sceKernelTriggerUserEvent(eq, 7, &payload) == SCE_OK);
    RequireReported(eq, 1, {8});
    RequireReported(eq, 1, {7});
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &poll) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

int main() {
    VerifyPeriodicTimer();
    VerifyUserEventReportedOncePerWait();
    VerifyBlockingWaitReportsLevelEventOnce();
    VerifyEdgeUserEventReportedOnce();
    VerifyRepeatedAmprCompletionsAreNotLost();
    VerifyReportedEventsMoveBehindWaitingOnes();

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
    Require(RejectsWideFflags(&rawEvent));
    rawEvent.fflags = 0;
    rawEvent.flags = EV_ERROR;
    rawEvent.data = 22;
    Require(sceKernelGetEventError(&rawEvent) == 0);
    rawEvent.data = -1;
    Require(sceKernelGetEventError(&rawEvent) == 0);
    rawEvent.data = 0x80020016;
    Require(sceKernelGetEventError(&rawEvent) == 0);
    rawEvent.fflags = 5;
    rawEvent.data = 0;
    Require(sceKernelGetEventError(&rawEvent) == static_cast<int>(0x80020005));
    rawEvent.data = 22;
    Require(sceKernelGetEventError(&rawEvent) == static_cast<int>(0x80020005));
    rawEvent.flags = 0;
    rawEvent.fflags = 7;
    Require(sceKernelGetEventError(&rawEvent) == static_cast<int>(0x80020007));
    rawEvent.flags = EV_EOF;
    rawEvent.fflags = 3;
    rawEvent.data = 9;
    Require(sceKernelGetEventError(&rawEvent) == static_cast<int>(0x80020003));
    rawEvent.flags = static_cast<std::uint16_t>(~EV_ERROR);
    rawEvent.fflags = 0xFFFF;
    Require(sceKernelGetEventError(&rawEvent) == static_cast<int>(0x8002FFFF));

    Require(RejectsNull(sceKernelGetEventData));
    Require(RejectsNull(sceKernelGetEventError));
    Require(RejectsNull(sceKernelGetEventFflags));
    Require(RejectsNull(sceKernelGetEventFilter));
    Require(RejectsNull(sceKernelGetEventId));
    Require(RejectsNull(sceKernelGetEventUserData));
}
