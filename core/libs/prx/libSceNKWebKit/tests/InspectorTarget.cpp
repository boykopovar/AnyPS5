#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector::InspectorTarget::resume check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct FakeWrapper {
    void* const* vtable;
};

struct FakeTarget {
    void* const* vtable;
    FakeWrapper* resumeCallback;
    std::uint8_t isPaused;
    std::uint8_t tail[7];
};

static_assert(sizeof(FakeTarget) == 24);

enum Event { Called = 1, Deleted = 2, Destroyed = 3 };

static FakeTarget* g_target = nullptr;
static int g_events[8] = {};
static int g_eventCount = 0;
static int g_pausedDuringCall = -1;
static FakeWrapper* g_callbackDuringCall = nullptr;
static FakeWrapper* g_lastThis = nullptr;

static void Record(Event event, FakeWrapper* self) {
    if (g_eventCount < 8) g_events[g_eventCount] = event;
    ++g_eventCount;
    g_lastThis = self;
}

static void APS5_VABI FakeDestroy(FakeWrapper* self) {
    Record(Destroyed, self);
}

static void APS5_VABI FakeDelete(FakeWrapper* self) {
    Record(Deleted, self);
}

static void APS5_VABI FakeCall(FakeWrapper* self) {
    g_pausedDuringCall = g_target->isPaused;
    g_callbackDuringCall = g_target->resumeCallback;
    Record(Called, self);
}

static void* const g_wrapperVtable[] = {
    reinterpret_cast<void*>(&FakeDestroy),
    reinterpret_cast<void*>(&FakeDelete),
    reinterpret_cast<void*>(&FakeCall),
};

static void* const g_targetVtable[] = {nullptr, nullptr};

static bool TailIntact(const FakeTarget& target) {
    for (std::uint8_t byte : target.tail) {
        if (byte != 0xA5) return false;
    }
    return true;
}

template <typename Function>
static bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    FakeTarget target{g_targetVtable, nullptr, 1, {0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5}};
    g_target = &target;

    _ZN9Inspector15InspectorTarget6resumeEv(&target);
    Require(target.isPaused == 0);
    Require(target.resumeCallback == nullptr);
    Require(target.vtable == g_targetVtable);
    Require(g_eventCount == 0);
    Require(TailIntact(target));

    FakeWrapper wrapper{g_wrapperVtable};
    target.isPaused = 1;
    target.resumeCallback = &wrapper;
    _ZN9Inspector15InspectorTarget6resumeEv(&target);
    Require(g_eventCount == 2);
    Require(g_events[0] == Called);
    Require(g_events[1] == Deleted);
    Require(g_lastThis == &wrapper);
    Require(g_pausedDuringCall == 0);
    Require(g_callbackDuringCall == &wrapper);
    Require(target.resumeCallback == nullptr);
    Require(target.isPaused == 0);
    Require(target.vtable == g_targetVtable);
    Require(TailIntact(target));

    _ZN9Inspector15InspectorTarget6resumeEv(&target);
    Require(g_eventCount == 2);
    Require(target.isPaused == 0);

    Require(ThrowsInvalidArgument([] { _ZN9Inspector15InspectorTarget6resumeEv(nullptr); }));
    FakeWrapper broken{nullptr};
    target.isPaused = 1;
    target.resumeCallback = &broken;
    Require(ThrowsInvalidArgument([&target] { _ZN9Inspector15InspectorTarget6resumeEv(&target); }));
    Require(target.resumeCallback == &broken);
    Require(g_eventCount == 2);

    std::puts("Inspector::InspectorTarget::resume tests passed");
    return 0;
}
