#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int PosixEnoent = 2;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int PosixEtimedout = 60;
constexpr int PosixEoverflow = 84;
constexpr unsigned int GuestSemValueMax = 0x7fffffffu;
constexpr int GuestOCreat = 0x200;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCreateSemaParameterBounds() {
    bool threw = false;
    try {
        sceKernelCreateSema(nullptr, "test", 0, 0, 1, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, nullptr, 0, 0, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sem == nullptr);

    const uint32_t validAttrs[] = {0u, 1u, 2u, 0x100u, 0x101u, 0x102u};
    for (const auto attr : validAttrs) {
        KernelSema s = nullptr;
        Require(sceKernelCreateSema(&s, "valid_attr", attr, 0, 1, nullptr) == KERNEL_SEMA_OK);
        Require(s != nullptr);
        Require(sceKernelDeleteSema(s) == KERNEL_SEMA_OK);
    }

    const uint32_t invalidAttrs[] = {
        3u, 0x103u, 4u, 8u, 0x10u, 0x20u, 0x40u, 0x80u,
        0x200u, 0x400u, 0x800u, 0x1000u, 0x10000u, 0x80000000u, 0xffffffffu
    };
    for (const auto attr : invalidAttrs) {
        KernelSema s = nullptr;
        Require(sceKernelCreateSema(&s, "invalid_attr", attr, 0, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(s == nullptr);
    }

    Require(sceKernelCreateSema(&sem, "neg_init", 0, -1, 5, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "neg_init", 0, -100, 5, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "neg_init", 0, INT_MIN, 5, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelCreateSema(&sem, "zero_max", 0, 0, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "neg_max", 0, 0, -1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "neg_max", 0, 0, -100, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "neg_max", 0, 0, INT_MIN, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelCreateSema(&sem, "init_gt_max", 0, 2, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "init_gt_max", 0, 10, 5, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&sem, "init_gt_max", 0, INT_MAX, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelCreateSema(&sem, "valid_bounds", 0, 0, 1, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
    Require(sceKernelCreateSema(&sem, "valid_bounds", 0, 1, 1, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
    Require(sceKernelCreateSema(&sem, "valid_bounds", 0, 5, 5, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

void TestPollSemaParameterBounds() {
    Require(sceKernelPollSema(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "poll_bounds", 0, 2, 5, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, -50) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, INT_MIN) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 6) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 100) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, INT_MAX) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelPollSema(sem, 3) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelPollSema(sem, 2) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

void TestSignalSemaParameterBounds() {
    Require(sceKernelSignalSema(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "signal_bounds", 0, 1, 5, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelSignalSema(sem, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, -100) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, INT_MIN) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelSignalSema(sem, 5) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, INT_MAX) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelSignalSema(sem, 4) == KERNEL_SEMA_OK);
    Require(sceKernelSignalSema(sem, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 5) == KERNEL_SEMA_OK);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

void TestWaitSemaParameterBounds() {
    KernelUseconds timeout = 1000;
    Require(sceKernelWaitSema(nullptr, 1, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitSema(nullptr, 1, &timeout) == SCE_KERNEL_ERROR_ESRCH);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "wait_bounds", 0, 2, 5, nullptr) == KERNEL_SEMA_OK);

    bool threw0 = false;
    try {
        sceKernelWaitSema(sem, 0, nullptr);
    } catch (const std::invalid_argument&) {
        threw0 = true;
    }
    Require(threw0);

    bool threwNeg = false;
    try {
        sceKernelWaitSema(sem, -1, nullptr);
    } catch (const std::invalid_argument&) {
        threwNeg = true;
    }
    Require(threwNeg);

    Require(sceKernelWaitSema(sem, 6, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWaitSema(sem, 100, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWaitSema(sem, INT_MAX, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    KernelUseconds zeroTimeout = 0;
    Require(sceKernelWaitSema(sem, 3, &zeroTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);

    Require(sceKernelWaitSema(sem, 2, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

void TestCancelSemaParameterBounds() {
    int waiters = -1;
    Require(sceKernelCancelSema(nullptr, 0, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCancelSema(nullptr, 0, &waiters) == SCE_KERNEL_ERROR_ESRCH);
    Require(waiters == -1);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "cancel_bounds", 0, 1, 4, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelCancelSema(sem, 5, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCancelSema(sem, 100, &waiters) == SCE_KERNEL_ERROR_EINVAL);
    Require(waiters == -1);

    Require(sceKernelPollSema(sem, 1) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelCancelSema(sem, -1, &waiters) == KERNEL_SEMA_OK);
    Require(waiters == 0);
    Require(sceKernelPollSema(sem, 1) == KERNEL_SEMA_OK);

    Require(sceKernelCancelSema(sem, 3, nullptr) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 3) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

void TestDeleteSemaParameterBounds() {
    Require(sceKernelDeleteSema(nullptr) == SCE_KERNEL_ERROR_ESRCH);
}

void TestPosixSemaphoreParameterBounds() {
    Require(sem_init_nid_postfix(nullptr, 0, 0) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_init_nid_postfix(nullptr, 0, 1) == -1 && *__error_nid_postfix() == PosixEinval);

    std::uintptr_t semSlot = 0;
    Require(sem_init_nid_postfix(&semSlot, 0, 0x80000000u) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_init_nid_postfix(&semSlot, 0, 0xffffffffu) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_init_nid_postfix(&semSlot, 0, GuestSemValueMax) == 0);

    Require(sem_post_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEoverflow);
    Require(sem_destroy_nid_postfix(&semSlot) == 0);

    std::uintptr_t uninitSlot = 0;
    int val = -1;
    Require(sem_post_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_post_nid_postfix(&uninitSlot) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_trywait_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_trywait_nid_postfix(&uninitSlot) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_wait_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_wait_nid_postfix(&uninitSlot) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_getvalue_nid_postfix(nullptr, &val) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_getvalue_nid_postfix(&uninitSlot, &val) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_destroy_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_destroy_nid_postfix(&uninitSlot) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_init_nid_postfix(&semSlot, 0, 2) == 0);
    Require(sem_getvalue_nid_postfix(&semSlot, nullptr) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_getvalue_nid_postfix(&semSlot, &val) == 0 && val == 2);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEagain);

    KernelTimespec invalidSpec{0, -1};
    Require(sem_timedwait_nid_postfix(&semSlot, &invalidSpec) == -1 && *__error_nid_postfix() == PosixEinval);
    invalidSpec = KernelTimespec{0, 1000000000};
    Require(sem_timedwait_nid_postfix(&semSlot, &invalidSpec) == -1 && *__error_nid_postfix() == PosixEinval);
    invalidSpec = KernelTimespec{0, 2000000000};
    Require(sem_timedwait_nid_postfix(&semSlot, &invalidSpec) == -1 && *__error_nid_postfix() == PosixEinval);
    KernelTimespec pastSpec{1, 0};
    Require(sem_timedwait_nid_postfix(&semSlot, &pastSpec) == -1 && *__error_nid_postfix() == PosixEtimedout);

    Require(sem_reltimedwait_np_nid_postfix(nullptr, 1000) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_reltimedwait_np_nid_postfix(&uninitSlot, 1000) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(sem_reltimedwait_np_nid_postfix(&semSlot, 0) == -1 && *__error_nid_postfix() == PosixEtimedout);
    Require(sem_destroy_nid_postfix(&semSlot) == 0);

    Require(sem_destroy_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEinval);
}

void TestPosixNamedSemaphoreParameterBounds() {
    bool threwName = false;
    try {
        sem_open_nid_postfix(nullptr, 0);
    } catch (const std::invalid_argument&) {
        threwName = true;
    }
    Require(threwName);

    Require(sem_open_nid_postfix("no_slash_sem", 0) == nullptr && *__error_nid_postfix() == PosixEinval);
    Require(sem_open_nid_postfix("/test_sem", 0x1) == nullptr && *__error_nid_postfix() == PosixEinval);
    Require(sem_open_nid_postfix("/test_sem", 0x4) == nullptr && *__error_nid_postfix() == PosixEinval);
    Require(sem_open_nid_postfix("/test_sem", 0x100) == nullptr && *__error_nid_postfix() == PosixEinval);
    Require(sem_open_nid_postfix("/non_existent_sem", 0) == nullptr && *__error_nid_postfix() == PosixEnoent);

    bool threwNested = false;
    try {
        sem_open_nid_postfix("/subdir/named_sem", GuestOCreat, 0644, 1);
    } catch (const std::runtime_error&) {
        threwNested = true;
    }
    Require(threwNested);

    bool threwClose = false;
    try {
        sem_close_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threwClose = true;
    }
    Require(threwClose);

    std::uintptr_t bogus = 0x12345678;
    Require(sem_close_nid_postfix(&bogus) == -1 && *__error_nid_postfix() == PosixEinval);

    bool threwUnlink = false;
    try {
        sem_unlink_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threwUnlink = true;
    }
    Require(threwUnlink);

    Require(sem_unlink_nid_postfix("no_slash") == -1 && *__error_nid_postfix() == PosixEnoent);
    Require(sem_unlink_nid_postfix("/no_such_sem") == -1 && *__error_nid_postfix() == PosixEnoent);
}

} // namespace

int main() {
    TestCreateSemaParameterBounds();
    TestPollSemaParameterBounds();
    TestSignalSemaParameterBounds();
    TestWaitSemaParameterBounds();
    TestCancelSemaParameterBounds();
    TestDeleteSemaParameterBounds();
    TestPosixSemaphoreParameterBounds();
    TestPosixNamedSemaphoreParameterBounds();
    return 0;
}
