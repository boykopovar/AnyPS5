#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddUserEvent(KernelEqueue eq, int id);
int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
int APS5_VABI sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata);
int APS5_VABI sceKernelAddTimerEvent(KernelEqueue eq, int id, KernelUseconds usec, void* udata);
int APS5_VABI sceKernelDeleteTimerEvent(KernelEqueue eq, int id);
intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev);
intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev);
int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev);
uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev);
void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev);
}

namespace {

using namespace std::chrono_literals;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int sceOk = 0;
constexpr int sceKernelErrorEnoent = static_cast<int>(0x80020002);
constexpr int sceKernelErrorEbadf = static_cast<int>(0x80020009);
constexpr int sceKernelErrorEtimedout = static_cast<int>(0x8002003c);
constexpr int evfiltTimer = -7;
constexpr int evfiltUser = -11;
constexpr int evfiltHrtimer = -15;
constexpr KernelUseconds pollTimeout = 0;
constexpr KernelUseconds oneSecond = 1000000;

class Equeue {
public:
    explicit Equeue(const char* name) {
        RequireEqual(sceKernelCreateEqueue(&handle, name), sceOk, std::string("create equeue ") + name);
    }

    ~Equeue() {
        if (!deleted) sceKernelDeleteEqueue(handle);
    }

    Equeue(const Equeue&) = delete;
    Equeue& operator=(const Equeue&) = delete;

    KernelEqueue Handle() const noexcept { return handle; }

    int Delete() {
        deleted = true;
        return sceKernelDeleteEqueue(handle);
    }

private:
    KernelEqueue handle = 0;
    bool deleted = false;
};

void SleepAtLeast(std::chrono::steady_clock::duration duration) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < duration) {
        std::this_thread::sleep_for(duration);
    }
}

std::uintptr_t Id(const KernelEvent& event) {
    return sceKernelGetEventId(&event);
}

std::intptr_t Data(const KernelEvent& event) {
    return sceKernelGetEventData(&event);
}

void RequireDataAtLeast(const KernelEvent& event, std::intptr_t minimum, const std::string& context) {
    const std::intptr_t data = Data(event);
    Require(data >= minimum, context + " expirations at least " + std::to_string(minimum) + ", got " + std::to_string(data));
}

const Case timerBadQueue{"TimerEvent_InvalidEqueue_FailsWithEbadf", [] {
    int first = 0;
    RequireEqual(sceKernelAddTimerEvent(0, 10, 1000, &first), sceKernelErrorEbadf, "add timer to equeue 0");
    RequireEqual(sceKernelDeleteTimerEvent(0, 10), sceKernelErrorEbadf, "delete timer from equeue 0");
}};

const Case timerMissing{"DeleteTimerEvent_NotAdded_FailsWithEnoent", [] {
    Equeue queue("timer");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 10), sceKernelErrorEnoent, "delete timer 10");
}};

const Case periodicTimer{"TimerEvent_PeriodicTimer_ReportsAccumulatedExpirations", [] {
    Equeue queue("timer");
    int first = 0;
    KernelEvent events[2]{};
    int count = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 10, 1000, &first), sceOk, "add timer 10");
    SleepAtLeast(5ms);
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceOk, "poll");
    RequireEqual(count, 1, "polled event count");
    RequireEqual(Id(events[0]), std::uintptr_t{10}, "event id");
    RequireEqual(sceKernelGetEventFilter(&events[0]), evfiltTimer, "event filter");
    RequireDataAtLeast(events[0], 5, "polled");
    RequireEqual(sceKernelGetEventUserData(&events[0]), static_cast<void*>(&first), "event user data");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &oneSecond), sceOk, "wait");
    RequireEqual(count, 1, "waited event count");
    RequireDataAtLeast(events[0], 1, "waited");
}};

const Case timerReplaced{"AddTimerEvent_ExistingTimer_KeepsPendingExpirationsAndReplacesUserData", [] {
    Equeue queue("timer");
    int first = 0;
    int second = 0;
    KernelEvent events[2]{};
    int count = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 10, 1000, &first), sceOk, "add timer 10");
    SleepAtLeast(5ms);
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 10, 3000000000u, &second), sceOk, "re-add timer 10");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceOk, "poll");
    RequireEqual(count, 1, "polled event count");
    RequireDataAtLeast(events[0], 5, "polled");
    RequireEqual(sceKernelGetEventUserData(&events[0]), static_cast<void*>(&second), "event user data");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceKernelErrorEtimedout, "second poll");
}};

const Case timerDeletedTwice{"DeleteTimerEvent_AlreadyDeleted_FailsWithEnoent", [] {
    Equeue queue("timer");
    int first = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 10, 3000000000u, &first), sceOk, "add timer 10");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 10), sceOk, "first delete");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 10), sceKernelErrorEnoent, "second delete");
}};

const Case timerShortened{"AddTimerEvent_ShorterPeriodOnLongTimer_RestartsWithNewPeriod", [] {
    Equeue queue("timer");
    int first = 0;
    KernelEvent events[2]{};
    int count = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 13, 3000000000u, &first), sceOk, "add long timer 13");
    const auto readded = std::chrono::steady_clock::now();
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 13, 200000, &first), sceOk, "re-add timer 13 with 200 ms");
    SleepAtLeast(250ms);
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceOk, "poll");
    RequireEqual(count, 1, "polled event count");
    RequireDataAtLeast(events[0], 1, "polled");
    const std::intptr_t expirations = Data(events[0]);
    const int again = sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout);
    if (std::chrono::steady_clock::now() - readded < 400ms) {
        RequireEqual(expirations, std::intptr_t{1}, "expirations within 400 ms");
        RequireEqual(again, sceKernelErrorEtimedout, "second poll within 400 ms");
    }
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &oneSecond), sceOk, "wait");
    RequireEqual(count, 1, "waited event count");
    RequireEqual(Id(events[0]), std::uintptr_t{13}, "waited event id");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 13), sceOk, "delete timer 13");
}};

const Case timerInfiniteWait{"WaitEqueue_NullTimeoutWithTimer_ReturnsTimerEvent", [] {
    Equeue queue("timer");
    int first = 0;
    KernelEvent events[2]{};
    int count = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 14, 1000, &first), sceOk, "add timer 14");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, nullptr), sceOk, "wait");
    RequireEqual(count, 1, "event count");
    RequireEqual(Id(events[0]), std::uintptr_t{14}, "event id");
    RequireDataAtLeast(events[0], 1, "waited");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 14), sceOk, "delete timer 14");
}};

const Case hrTimerInfiniteWait{"WaitEqueue_NullTimeoutWithHrTimer_ReturnsHrTimerEvent", [] {
    Equeue queue("timer");
    int second = 0;
    KernelEvent events[2]{};
    int count = 0;
    const KernelTimespec soon{0, 1000000};
    RequireEqual(sceKernelAddHRTimerEvent(queue.Handle(), 15, &soon, &second), sceOk, "add hr timer 15");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, nullptr), sceOk, "wait");
    RequireEqual(count, 1, "event count");
    RequireEqual(Id(events[0]), std::uintptr_t{15}, "event id");
    RequireEqual(sceKernelGetEventFilter(&events[0]), evfiltHrtimer, "event filter");
}};

const Case zeroPeriodTimer{"AddTimerEvent_ZeroPeriodAddedTwice_FiresOncePerAddUntilDeleted", [] {
    Equeue queue("timer");
    int first = 0;
    KernelEvent events[2]{};
    int count = 0;
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 11, 0, &first), sceOk, "first add");
    RequireEqual(sceKernelAddTimerEvent(queue.Handle(), 11, 0, &first), sceOk, "second add");
    for (int i = 0; i < 2; ++i) {
        const std::string context = "poll " + std::to_string(i);
        RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceOk, context);
        RequireEqual(count, 1, context + " event count");
        RequireEqual(Data(events[0]), std::intptr_t{1}, context + " expirations");
    }
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 11), sceOk, "delete timer 11");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), events, 2, &count, &pollTimeout), sceKernelErrorEtimedout, "poll after delete");
}};

const Case hrTimerNotTimer{"DeleteTimerEvent_HrTimer_FailsWithEnoentAndEqueueStillDeletes", [] {
    Equeue queue("timer");
    int first = 0;
    const KernelTimespec delay{0, 1000000};
    RequireEqual(sceKernelAddHRTimerEvent(queue.Handle(), 12, &delay, &first), sceOk, "add hr timer 12");
    RequireEqual(sceKernelDeleteTimerEvent(queue.Handle(), 12), sceKernelErrorEnoent, "delete hr timer as timer");
    RequireEqual(queue.Delete(), sceOk, "delete equeue");
}};

const Case userEvent{"TriggerUserEvent_AddedEvent_DeliversUserEventFields", [] {
    Equeue queue("events");
    int payload = 0;
    int count = 0;
    KernelEvent event{};
    RequireEqual(sceKernelAddUserEvent(queue.Handle(), 7), sceOk, "add user event");
    RequireEqual(sceKernelTriggerUserEvent(queue.Handle(), 7, &payload), sceOk, "trigger user event");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &oneSecond), sceOk, "wait");
    RequireEqual(count, 1, "event count");
    RequireEqual(Id(event), std::uintptr_t{7}, "event id");
    RequireEqual(sceKernelGetEventFilter(&event), evfiltUser, "event filter");
    RequireEqual(Data(event), reinterpret_cast<std::intptr_t>(&payload), "event data");
    RequireEqual(sceKernelGetEventUserData(&event), static_cast<void*>(&payload), "event user data");
    RequireEqual(sceKernelGetEventFflags(&event), std::intptr_t{0}, "event fflags");
    RequireEqual(sceKernelDeleteUserEvent(queue.Handle(), 7), sceOk, "delete user event");
}};

const Case hrTimerEvent{"AddHRTimerEvent_OneMillisecond_DeliversHrTimerEventAndEqueueDeletes", [] {
    Equeue queue("events");
    int payload = 0;
    int count = 0;
    KernelEvent event{};
    const KernelTimespec delay{0, 1000000};
    RequireEqual(sceKernelAddHRTimerEvent(queue.Handle(), 9, &delay, &payload), sceOk, "add hr timer 9");
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &oneSecond), sceOk, "wait");
    RequireEqual(count, 1, "event count");
    RequireEqual(Id(event), std::uintptr_t{9}, "event id");
    RequireEqual(sceKernelGetEventFilter(&event), evfiltHrtimer, "event filter");
    RequireEqual(sceKernelGetEventUserData(&event), static_cast<void*>(&payload), "event user data");
    RequireEqual(queue.Delete(), sceOk, "delete equeue");
}};

const Case rawEvent{"EventAccessors_RawEvent_ReturnFieldsUnchanged", [] {
    KernelEvent event{};
    event.ident = UINTPTR_MAX;
    event.filter = INT16_MIN;
    event.fflags = 0x80000001u;
    event.data = -5;
    RequireEqual(Id(event), std::uintptr_t{UINTPTR_MAX}, "id");
    RequireEqual(sceKernelGetEventFilter(&event), int{INT16_MIN}, "filter");
    RequireEqual(sceKernelGetEventFflags(&event), static_cast<std::intptr_t>(0x80000001LL), "fflags");
    RequireEqual(Data(event), std::intptr_t{-5}, "data");
    Require(sceKernelGetEventUserData(&event) == nullptr, "user data is null");
}};

const Case nullEvent{"EventAccessors_NullEvent_ThrowRuntimeError", [] {
    RequireThrows<std::runtime_error>([] { sceKernelGetEventData(nullptr); }, "sceKernelGetEventData");
    RequireThrows<std::runtime_error>([] { sceKernelGetEventFflags(nullptr); }, "sceKernelGetEventFflags");
    RequireThrows<std::runtime_error>([] { sceKernelGetEventFilter(nullptr); }, "sceKernelGetEventFilter");
    RequireThrows<std::runtime_error>([] { sceKernelGetEventId(nullptr); }, "sceKernelGetEventId");
    RequireThrows<std::runtime_error>([] { sceKernelGetEventUserData(nullptr); }, "sceKernelGetEventUserData");
}};

} // namespace
