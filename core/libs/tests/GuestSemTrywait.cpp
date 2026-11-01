#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>

static constexpr int GUEST_EBADF = 9;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EAGAIN = 35;
static constexpr int O_CREAT_FLAG = 0x200;
static constexpr int O_EXCL_FLAG = 0x800;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    std::uintptr_t uninitSem = 0;
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(&uninitSem) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    std::uintptr_t storage[2] = {};
    Require(sem_init_nid_postfix(storage, 0, 1) == 0);
    Require(sem_destroy_nid_postfix(storage) == 0);
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(storage) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    const char* semName = "/sem_trywait_test_descriptor";
    void* sem = sem_open_nid_postfix(semName, O_CREAT_FLAG | O_EXCL_FLAG, 0600, 1);
    Require(sem != nullptr);
    Require(sem_trywait_nid_postfix(sem) == 0);
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(sem) == -1);
    Require(*__error_nid_postfix() == GUEST_EAGAIN);
    Require(sem_close_nid_postfix(sem) == 0);
    Require(sem_unlink_nid_postfix(semName) == 0);

    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(-1) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(-100) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(static_cast<std::intptr_t>(INT_MIN)) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(static_cast<std::intptr_t>(INT_MAX) + 1) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(99999) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    std::intptr_t ksemId = -1;
    Require(ksem_init_nid_postfix(&ksemId, 0) == 0);
    Require(ksem_destroy_nid_postfix(ksemId) == 0);
    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(ksemId) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    std::intptr_t closedId = -1;
    Require(ksem_init_nid_postfix(&closedId, 1) == 0);
    Require(close_nid_postfix(static_cast<int>(closedId)) == 0);
    *__error_nid_postfix() = 0;
    Require(ksem_trywait_nid_postfix(closedId) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL || *__error_nid_postfix() == GUEST_EBADF);

    return 0;
}
