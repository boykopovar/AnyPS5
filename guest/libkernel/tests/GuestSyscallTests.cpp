#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorInvalid = 22;
constexpr int ErrorAgain = 35;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSemGetvalueNullBounds() {
    int value = -1;
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(nullptr, &value) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t uninitDescriptor = 0;
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&uninitDescriptor, &value) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&uninitDescriptor, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t validDescriptor = 0;
    Require(sem_init_nid_postfix(&validDescriptor, 0, 5) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&validDescriptor, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    value = 0;
    Require(sem_getvalue_nid_postfix(&validDescriptor, &value) == 0);
    Require(value == 5);

    Require(sem_destroy_nid_postfix(&validDescriptor) == 0);

    for (const std::intptr_t badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(ksem_getvalue_nid_postfix(badDescriptor, &value) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);

        *__error_nid_postfix() = 0;
        Require(ksem_getvalue_nid_postfix(badDescriptor, nullptr) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }
}

void TestSemDestroyNullBounds() {
    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t uninitDescriptor = 0;
    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(&uninitDescriptor) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t validDescriptor = 0;
    Require(sem_init_nid_postfix(&validDescriptor, 0, 1) == 0);
    Require(sem_destroy_nid_postfix(&validDescriptor) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(&validDescriptor) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    for (const std::intptr_t badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(ksem_destroy_nid_postfix(badDescriptor) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }
}

void TestSemTrywaitInvalidDescriptors() {
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t uninitDescriptor = 0;
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(&uninitDescriptor) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    for (const std::intptr_t badDescriptor : {-1, -100, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(ksem_trywait_nid_postfix(badDescriptor) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    std::uintptr_t emptySem = 0;
    Require(sem_init_nid_postfix(&emptySem, 0, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(&emptySem) == -1);
    Require(*__error_nid_postfix() == ErrorAgain);

    Require(sem_post_nid_postfix(&emptySem) == 0);
    Require(sem_trywait_nid_postfix(&emptySem) == 0);
    Require(sem_destroy_nid_postfix(&emptySem) == 0);
}

}

int main() {
    TestSemGetvalueNullBounds();
    TestSemDestroyNullBounds();
    TestSemTrywaitInvalidDescriptors();
    return 0;
}
