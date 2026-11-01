#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestDeleteInvalidQueueHandles() {
    for (const KernelEqueue badEq : {
        static_cast<KernelEqueue>(0),
        static_cast<KernelEqueue>(-1),
        static_cast<KernelEqueue>(-2),
        static_cast<KernelEqueue>(-100),
        static_cast<KernelEqueue>(-9999),
        static_cast<KernelEqueue>(100000),
        static_cast<KernelEqueue>(999999),
        static_cast<KernelEqueue>(INT64_MIN),
        static_cast<KernelEqueue>(INT64_MAX),
        static_cast<KernelEqueue>(~0ULL)
    }) {
        Require(sceKernelDeleteEqueue(badEq) == SCE_KERNEL_ERROR_EBADF);
    }
}

void TestDoubleDeleteQueue() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "double_delete") == 0);
    Require(eq != 0);

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelDeleteEqueue(eq) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteEqueue(eq) == SCE_KERNEL_ERROR_EBADF);
}

void TestInterleavedQueueDeletions() {
    KernelEqueue eq1 = 0;
    KernelEqueue eq2 = 0;
    KernelEqueue eq3 = 0;

    Require(sceKernelCreateEqueue(&eq1, "queue_one") == 0);
    Require(sceKernelCreateEqueue(&eq2, "queue_two") == 0);
    Require(sceKernelCreateEqueue(&eq3, "queue_three") == 0);

    Require(eq1 != 0 && eq2 != 0 && eq3 != 0);
    Require(eq1 != eq2 && eq2 != eq3 && eq1 != eq3);

    Require(sceKernelDeleteEqueue(eq2) == 0);
    Require(sceKernelDeleteEqueue(eq2) == SCE_KERNEL_ERROR_EBADF);

    Require(sceKernelAddUserEvent(eq1, 1) == 0);
    Require(sceKernelAddUserEvent(eq3, 1) == 0);

    Require(sceKernelDeleteEqueue(eq1) == 0);
    Require(sceKernelDeleteEqueue(eq1) == SCE_KERNEL_ERROR_EBADF);

    Require(sceKernelDeleteEqueue(eq3) == 0);
    Require(sceKernelDeleteEqueue(eq3) == SCE_KERNEL_ERROR_EBADF);
}

void TestOperationsOnDeletedQueueHandle() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "deleted_ops") == 0);
    Require(eq != 0);
    Require(sceKernelDeleteEqueue(eq) == 0);

    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    const KernelTimespec delay{1, 0};
    int payload = 0;

    Require(sceKernelDeleteEqueue(eq) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddUserEvent(eq, 1) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddUserEventEdge(eq, 2) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(eq, 1, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelTriggerUserEvent(eq, 1, &payload) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteUserEvent(eq, 1) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddTimerEvent(eq, 3, 1000, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteTimerEvent(eq, 3) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddHRTimerEvent(eq, 4, &delay, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteHRTimerEvent(eq, 4) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelAddAmprEvent(eq, 5, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteAmprEvent(eq, 5) == SCE_KERNEL_ERROR_EBADF);
}

void TestDeleteQueueWithActiveEvents() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "delete_with_events") == 0);
    Require(eq != 0);

    int payload = 77;
    Require(sceKernelAddUserEvent(eq, 10) == 0);
    Require(sceKernelAddUserEventEdge(eq, 11) == 0);
    Require(sceKernelAddTimerEvent(eq, 12, 1000000, &payload) == 0);
    const KernelTimespec delay{2, 0};
    Require(sceKernelAddHRTimerEvent(eq, 13, &delay, &payload) == 0);
    Require(sceKernelAddAmprEvent(eq, 14, &payload) == 0);

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelDeleteEqueue(eq) == SCE_KERNEL_ERROR_EBADF);

    Require(sceKernelDeleteUserEvent(eq, 10) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteTimerEvent(eq, 12) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteHRTimerEvent(eq, 13) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelDeleteAmprEvent(eq, 14) == SCE_KERNEL_ERROR_EBADF);
}

void TestQueueCreationHandleBounds() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(nullptr, "bounds") == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEqueue(&eq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEqueue(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

}

int main() {
    TestDeleteInvalidQueueHandles();
    TestDoubleDeleteQueue();
    TestInterleavedQueueDeletions();
    TestOperationsOnDeletedQueueHandle();
    TestDeleteQueueWithActiveEvents();
    TestQueueCreationHandleBounds();
    return 0;
}
