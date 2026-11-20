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
    Require(sem_post_nid_postfix(&sem) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 2);
    Require(sem_trywait_nid_postfix(&sem) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 1);
    Require(sem_trywait_nid_postfix(&sem) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(&sem) == -1 && *__error_nid_postfix() == 35);

    Require(sem_destroy_nid_postfix(&sem) == 0);
    value = 99;
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&sem, &value) == -1 && *__error_nid_postfix() == 22);
    Require(value == 99);

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
    Require(ksem_getvalue_nid_postfix(ksemId, reinterpret_cast<int*>(0x10)) == -1);
    Require(*__error_nid_postfix() == 14);

    int kvalue = -1;
    Require(ksem_getvalue_nid_postfix(ksemId, &kvalue) == 0 && kvalue == 1);
    Require(ksem_post_nid_postfix(ksemId) == 0);
    Require(ksem_getvalue_nid_postfix(ksemId, &kvalue) == 0 && kvalue == 2);
    Require(ksem_trywait_nid_postfix(ksemId) == 0);
    Require(ksem_getvalue_nid_postfix(ksemId, &kvalue) == 0 && kvalue == 1);
    Require(ksem_trywait_nid_postfix(ksemId) == 0);
    Require(ksem_getvalue_nid_postfix(ksemId, &kvalue) == 0 && kvalue == 0);

    *__error_nid_postfix() = 0;
    Require(ksem_getvalue_nid_postfix(-1, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(ksem_destroy_nid_postfix(ksemId) == 0);
    return 0;
}
