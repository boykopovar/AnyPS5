#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int GUEST_EINVAL = 22;
static constexpr int O_CREAT_FLAG = 0x200;
static constexpr int O_EXCL_FLAG = 0x800;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    const char* semName = "/sem_close_test_descriptor";
    void* sem = sem_open_nid_postfix(semName, O_CREAT_FLAG | O_EXCL_FLAG, 0600, 1);
    Require(sem != nullptr);

    Require(sem_close_nid_postfix(sem) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(sem) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(sem) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sem_unlink_nid_postfix(semName) == 0);

    void* multiSem = sem_open_nid_postfix("/sem_close_test_multi", O_CREAT_FLAG | O_EXCL_FLAG, 0600, 2);
    Require(multiSem != nullptr);
    Require(sem_open_nid_postfix("/sem_close_test_multi", 0) == multiSem);

    Require(sem_close_nid_postfix(multiSem) == 0);
    Require(sem_close_nid_postfix(multiSem) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(multiSem) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sem_unlink_nid_postfix("/sem_close_test_multi") == 0);

    std::uintptr_t storage[2] = {};
    Require(sem_init_nid_postfix(storage, 0, 1) == 0);
    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(storage) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);
    Require(sem_destroy_nid_postfix(storage) == 0);

    std::uintptr_t bogus = 0xdeadbeef;
    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(&bogus) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678))) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_close_nid_postfix(reinterpret_cast<void*>(static_cast<std::intptr_t>(-1))) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    return 0;
}
