#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

extern "C" {
int APS5_VABI sem_init_nid_postfix(void*, int, unsigned int);
int APS5_VABI sem_destroy_nid_postfix(void*);
int APS5_VABI sem_post_nid_postfix(void*);
int APS5_VABI sem_trywait_nid_postfix(void*);
int APS5_VABI sem_getvalue_nid_postfix(void*, int*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr unsigned int maximum = 0x7fffffffu;
constexpr int einval = 22;
constexpr int eagain = 35;
constexpr int eoverflow = 84;

class Semaphore {
public:
    explicit Semaphore(unsigned int initial) {
        RequireEqual(sem_init_nid_postfix(&storage, 0, initial), 0, "sem_init(" + std::to_string(initial) + ")");
        initialized = true;
    }
    ~Semaphore() {
        if (initialized) sem_destroy_nid_postfix(&storage);
    }
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    int Post() { return sem_post_nid_postfix(&storage); }
    int TryWait() { return sem_trywait_nid_postfix(&storage); }

    int Value() {
        int value = -1;
        RequireEqual(sem_getvalue_nid_postfix(&storage, &value), 0, "sem_getvalue");
        return value;
    }

    int Destroy() {
        initialized = false;
        return sem_destroy_nid_postfix(&storage);
    }

private:
    std::uintptr_t storage = 0;
    bool initialized = false;
};

void RequireFailure(int result, int expectedError, const std::string& message) {
    RequireEqual(result, -1, message + " result");
    RequireEqual(*__error_nid_postfix(), expectedError, message + " errno");
}

const Case initAboveMaximum{"SemInit_CountAboveMaximum_FailsWithEinvalAndLeavesStorage", [] {
    for (const unsigned int initial : {maximum + 1, 0xffffffffu}) {
        std::uintptr_t storage = 0x1234;
        *__error_nid_postfix() = 0;
        RequireFailure(sem_init_nid_postfix(&storage, 0, initial), einval, "sem_init(" + std::to_string(initial) + ")");
        RequireEqual(storage, std::uintptr_t{0x1234}, "storage after sem_init(" + std::to_string(initial) + ")");
    }
}};

const Case initMaximum{"SemInit_MaximumCount_ReportsMaximumValue", [] {
    Semaphore semaphore(maximum);
    RequireEqual(semaphore.Value(), static_cast<int>(maximum), "value");
}};

const Case postAtMaximum{"SemPost_AtMaximum_FailsWithEoverflowAndKeepsCount", [] {
    Semaphore semaphore(maximum);
    *__error_nid_postfix() = 0;
    RequireFailure(semaphore.Post(), eoverflow, "post at maximum");
    RequireEqual(semaphore.Value(), static_cast<int>(maximum), "value after overflow");
}};

const Case waitAtMaximum{"SemTrywait_AtMaximum_DecrementsAndPostRestoresMaximum", [] {
    Semaphore semaphore(maximum);
    RequireEqual(semaphore.TryWait(), 0, "trywait at maximum");
    RequireEqual(semaphore.Value(), static_cast<int>(maximum - 1), "value after trywait");
    RequireEqual(semaphore.Post(), 0, "post below maximum");
    RequireEqual(semaphore.Value(), static_cast<int>(maximum), "value after post");
}};

const Case repeatedOverflow{"SemPost_AtMaximumAgainAfterWaitAndPost_FailsWithEoverflow", [] {
    Semaphore semaphore(maximum);
    *__error_nid_postfix() = 0;
    RequireFailure(semaphore.Post(), eoverflow, "first overflow");
    RequireEqual(semaphore.TryWait(), 0, "trywait");
    RequireEqual(semaphore.Post(), 0, "post");
    RequireFailure(semaphore.Post(), eoverflow, "repeated overflow");
    RequireEqual(semaphore.Value(), static_cast<int>(maximum), "value after repeated overflow");
    RequireEqual(semaphore.Destroy(), 0, "destroy");
}};

const Case emptyWait{"SemTrywait_ZeroCount_FailsWithEagain", [] {
    Semaphore semaphore(0);
    RequireFailure(semaphore.TryWait(), eagain, "trywait on empty semaphore");
}};

const Case normalCount{"SemPostTrywait_BelowMaximum_IncrementAndDecrement", [] {
    Semaphore semaphore(0);
    RequireEqual(semaphore.Post(), 0, "post");
    RequireEqual(semaphore.Value(), 1, "value after post");
    RequireEqual(semaphore.TryWait(), 0, "trywait");
    RequireEqual(semaphore.Value(), 0, "value after trywait");
    RequireEqual(semaphore.Destroy(), 0, "destroy");
}};

} // namespace
