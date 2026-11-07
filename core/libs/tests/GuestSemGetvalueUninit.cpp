#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    std::uintptr_t sem = 0;
    Require(sem_init_nid_postfix(&sem, 0, 1) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&sem, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    int value = -1;
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0);
    Require(value == 1);

    Require(sem_destroy_nid_postfix(&sem) == 0);

    std::uintptr_t uninitSem = 0;
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&uninitSem, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    std::intptr_t ksemId = -1;
    Require(ksem_init_nid_postfix(&ksemId, 1) == 0);

    *__error_nid_postfix() = 0;
    Require(ksem_getvalue_nid_postfix(ksemId, nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(ksem_getvalue_nid_postfix(-1, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(ksem_destroy_nid_postfix(ksemId) == 0);
    return 0;
}
