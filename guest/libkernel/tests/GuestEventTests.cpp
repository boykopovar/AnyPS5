#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Func>
bool ExpectsException(Func&& func) {
    try {
        func();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestTriggerInvalidQueueHandles() {
    int payload = 42;
    for (const KernelEqueue badEq : {
        static_cast<KernelEqueue>(0),
        static_cast<KernelEqueue>(-1),
        static_cast<KernelEqueue>(-100),
        static_cast<KernelEqueue>(-9999),
        static_cast<KernelEqueue>(999999),
        static_cast<KernelEqueue>(~0ULL)
    }) {
        Require(sceKernelTriggerUserEvent(badEq, 0, nullptr) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelTriggerUserEvent(badEq, 1, nullptr) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelTriggerUserEvent(badEq, 1, &payload) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelTriggerUserEvent(badEq, -1, nullptr) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelTriggerUserEvent(badEq, 100, &payload) == SCE_KERNEL_ERROR_EBADF);
    }

    KernelEqueue closedEq = 0;
    Require(sceKernelCreateEqueue(&closedEq, "trigger_closed") == 0);
    Require(closedEq != 0);
    Require(sceKernelAddUserEvent(closedEq, 1) == 0);
    Require(sceKernelAddUserEventEdge(closedEq, 2) == 0);
    Require(sceKernelDeleteEqueue(closedEq) == 0);

    Require(sceKernelTriggerUserEvent(closedEq, 0, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(closedEq, 1, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(closedEq, 1, &payload) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(closedEq, 2, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(closedEq, 2, &payload) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(closedEq, 999, nullptr) == SCE_KERNEL_ERROR_EBADF);
}

void TestTriggerUnregisteredEventIds() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "trigger_unregistered") == 0);
    Require(eq != 0);

    int payload = 123;
    for (const int unregisteredId : {-9999, -1, 0, 1, 2, 10, 100, 99999, INT32_MIN, INT32_MAX}) {
        Require(sceKernelTriggerUserEvent(eq, unregisteredId, nullptr) == SCE_KERNEL_ERROR_ENOENT);
        Require(sceKernelTriggerUserEvent(eq, unregisteredId, &payload) == SCE_KERNEL_ERROR_ENOENT);
    }

    Require(sceKernelAddUserEvent(eq, 10) == 0);
    Require(sceKernelAddUserEventEdge(eq, 20) == 0);

    for (const int nonMatchingId : {-10, -1, 0, 9, 11, 19, 21, 30, 100, 9999}) {
        Require(sceKernelTriggerUserEvent(eq, nonMatchingId, nullptr) == SCE_KERNEL_ERROR_ENOENT);
        Require(sceKernelTriggerUserEvent(eq, nonMatchingId, &payload) == SCE_KERNEL_ERROR_ENOENT);
    }

    Require(sceKernelDeleteUserEvent(eq, 10) == 0);
    Require(sceKernelTriggerUserEvent(eq, 10, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 10, &payload) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteUserEvent(eq, 20) == 0);
    Require(sceKernelTriggerUserEvent(eq, 20, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 20, &payload) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteEqueue(eq) == 0);
}

void TestTriggerMismatchedFilterTypes() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "trigger_mismatched") == 0);
    Require(eq != 0);

    int payload = 55;
    Require(sceKernelAddTimerEvent(eq, 30, 1000000, nullptr) == 0);
    const KernelTimespec delay{1, 0};
    Require(sceKernelAddHRTimerEvent(eq, 31, &delay, nullptr) == 0);
    Require(sceKernelAddAmprEvent(eq, 32, nullptr) == 0);

    Require(sceKernelTriggerUserEvent(eq, 30, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 30, &payload) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 31, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 31, &payload) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 32, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 32, &payload) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteTimerEvent(eq, 30) == 0);
    Require(sceKernelDeleteHRTimerEvent(eq, 31) == 0);
    Require(sceKernelDeleteAmprEvent(eq, 32) == 0);

    Require(sceKernelTriggerUserEvent(eq, 30, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 31, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelTriggerUserEvent(eq, 32, nullptr) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteEqueue(eq) == 0);
}

void TestNullEventFilterAndRawTriggerBounds() {
    for (const KernelEqueue badEq : {static_cast<KernelEqueue>(0), static_cast<KernelEqueue>(-1)}) {
        Require(EqueueTriggerEvent_nid_postfix(badEq, 50, 0, nullptr) == SCE_KERNEL_ERROR_EBADF);
        Require(EqueueTriggerEvent_nid_postfix(badEq, 50, EVFILT_USER, nullptr) == SCE_KERNEL_ERROR_EBADF);
        Require(EqueueTriggerEvent_nid_postfix(badEq, 50, -99, nullptr) == SCE_KERNEL_ERROR_EBADF);
    }

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "trigger_raw_bounds") == 0);
    Require(eq != 0);

    Require(sceKernelAddUserEvent(eq, 50) == 0);

    Require(EqueueTriggerEvent_nid_postfix(eq, 50, 0, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(EqueueTriggerEvent_nid_postfix(eq, 50, -999, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(EqueueTriggerEvent_nid_postfix(eq, 50, 100, nullptr) == SCE_KERNEL_ERROR_ENOENT);
    Require(EqueueTriggerEvent_nid_postfix(eq, 51, EVFILT_USER, nullptr) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteUserEvent(eq, 50) == 0);
    Require(EqueueTriggerEvent_nid_postfix(eq, 50, EVFILT_USER, nullptr) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(EqueueTriggerEvent_nid_postfix(eq, 50, 0, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(EqueueTriggerEvent_nid_postfix(eq, 50, EVFILT_USER, nullptr) == SCE_KERNEL_ERROR_EBADF);
}

void TestTriggerNullPayloadDelivery() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "trigger_null_payload") == 0);
    Require(eq != 0);

    Require(sceKernelAddUserEvent(eq, 60) == 0);
    Require(sceKernelTriggerUserEvent(eq, 60, nullptr) == 0);

    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) == 0);
    Require(count == 1);
    Require(sceKernelGetEventId(&event) == 60);
    Require(sceKernelGetEventFilter(&event) == EVFILT_USER);
    Require(sceKernelGetEventUserData(&event) == nullptr);
    Require(sceKernelGetEventData(&event) == 0);
    Require(sceKernelDeleteUserEvent(eq, 60) == 0);

    Require(sceKernelAddUserEventEdge(eq, 61) == 0);
    Require(sceKernelTriggerUserEvent(eq, 61, nullptr) == 0);
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) == 0);
    Require(count == 1);
    Require(sceKernelGetEventId(&event) == 61);
    Require(sceKernelGetEventFilter(&event) == EVFILT_USER);
    Require(sceKernelGetEventUserData(&event) == nullptr);
    Require(sceKernelGetEventData(&event) == 0);
    Require(sceKernelDeleteUserEvent(eq, 61) == 0);

    Require(sceKernelDeleteEqueue(eq) == 0);
}

void TestNullEventAccessors() {
    Require(ExpectsException([] { sceKernelGetEventData(nullptr); }));
    Require(ExpectsException([] { sceKernelGetEventError(nullptr); }));
    Require(ExpectsException([] { sceKernelGetEventFflags(nullptr); }));
    Require(ExpectsException([] { sceKernelGetEventFilter(nullptr); }));
    Require(ExpectsException([] { sceKernelGetEventId(nullptr); }));
    Require(ExpectsException([] { sceKernelGetEventUserData(nullptr); }));
}

void TestUserEventRegistrationBounds() {
    for (const KernelEqueue badEq : {
        static_cast<KernelEqueue>(0),
        static_cast<KernelEqueue>(-1),
        static_cast<KernelEqueue>(999999)
    }) {
        Require(sceKernelAddUserEvent(badEq, 1) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelAddUserEventEdge(badEq, 1) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelDeleteUserEvent(badEq, 1) == SCE_KERNEL_ERROR_EBADF);
    }

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "reg_bounds") == 0);
    Require(eq != 0);

    for (const int nonExistent : {-1, 0, 1, 9999}) {
        Require(sceKernelDeleteUserEvent(eq, nonExistent) == SCE_KERNEL_ERROR_ENOENT);
    }

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelAddUserEvent(eq, 1) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddUserEventEdge(eq, 1) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteUserEvent(eq, 1) == SCE_KERNEL_ERROR_EBADF);
}

}

int main() {
    TestTriggerInvalidQueueHandles();
    TestTriggerUnregisteredEventIds();
    TestTriggerMismatchedFilterTypes();
    TestNullEventFilterAndRawTriggerBounds();
    TestTriggerNullPayloadDelivery();
    TestNullEventAccessors();
    TestUserEventRegistrationBounds();
    return 0;
}
