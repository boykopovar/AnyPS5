#include "prx/libc/include/general/AtomicSharedPtr.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

template <typename AtomicPtr>
bool test_ownership_and_overloads() {
    int target = 42;
    int replacement = 99;
    auto ownerA = std::make_shared<int>(1);
    auto ownerB = std::make_shared<int>(2);
    auto ownerC = std::make_shared<int>(3);

    AtomicPtr atomicPtr(std::shared_ptr<int>(ownerA, &target));

    std::shared_ptr<int> wrongOwner(ownerB, &target);
    std::shared_ptr<int> desired(ownerC, &replacement);
    if (atomicPtr.compare_exchange_strong(wrongOwner, desired)) {
        return false;
    }
    if (wrongOwner.get() != &target || wrongOwner.owner_before(ownerA) || ownerA.owner_before(wrongOwner)) {
        return false;
    }

    std::shared_ptr<int> sameOwner(ownerA, &target);
    if (!atomicPtr.compare_exchange_strong(sameOwner, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }
    std::shared_ptr<int> current = atomicPtr.load();
    if (current.get() != &replacement || current.owner_before(ownerC) || ownerC.owner_before(current)) {
        return false;
    }

    atomicPtr.store(std::shared_ptr<int>(ownerA, nullptr));
    std::shared_ptr<int> emptyPtr{};
    if (atomicPtr.compare_exchange_strong(emptyPtr, desired)) {
        return false;
    }
    if (emptyPtr.get() != nullptr || emptyPtr.owner_before(ownerA) || ownerA.owner_before(emptyPtr)) {
        return false;
    }

    std::shared_ptr<int> weakExpected(ownerA, nullptr);
    while (!atomicPtr.compare_exchange_weak(weakExpected, std::shared_ptr<int>(ownerB, &target))) {
        if (weakExpected.get() != nullptr || weakExpected.owner_before(ownerA) || ownerA.owner_before(weakExpected)) {
            return false;
        }
    }
    current = atomicPtr.load();
    if (current.get() != &target || current.owner_before(ownerB) || ownerB.owner_before(current)) {
        return false;
    }

    std::shared_ptr<int> weakExpectedFourArg(ownerB, &target);
    while (!atomicPtr.compare_exchange_weak(
        weakExpectedFourArg,
        std::shared_ptr<int>(ownerC, &replacement),
        std::memory_order_acq_rel,
        std::memory_order_acquire)) {
        if (weakExpectedFourArg.get() != &target || weakExpectedFourArg.owner_before(ownerB) || ownerB.owner_before(weakExpectedFourArg)) {
            return false;
        }
    }
    current = atomicPtr.load();
    if (current.get() != &replacement || current.owner_before(ownerC) || ownerC.owner_before(current)) {
        return false;
    }

    return true;
}

bool test_unlock_before_destroy_on_cas_failure() {
    auto storedValue = std::make_shared<int>(40);
    AtomicSharedPtrFallback<int> atomicPtr(storedValue);

    int deleterCalls = 0;
    int* observedDuringDeleter = nullptr;
    std::shared_ptr<int> failureExpected(new int(50), [&](int* p) {
        ++deleterCalls;
        observedDuringDeleter = atomicPtr.load().get();
        delete p;
    });
    if (atomicPtr.compare_exchange_strong(failureExpected, std::make_shared<int>(60))) {
        return false;
    }
    if (deleterCalls != 1 || observedDuringDeleter != storedValue.get() || failureExpected.get() != storedValue.get()) {
        return false;
    }

    return true;
}

}

int main() {
    if (!test_ownership_and_overloads<AtomicSharedPtrFallback<int>>() || !test_ownership_and_overloads<AtomicSharedPtr<int>>()) {
        std::fputs("AtomicSharedPtr ownership equivalence or CAS overload check failed\n", stderr);
        return EXIT_FAILURE;
    }
    if (!test_unlock_before_destroy_on_cas_failure()) {
        std::fputs("AtomicSharedPtrFallback unlock-before-destroy CAS failure check failed\n", stderr);
        return EXIT_FAILURE;
    }
    std::puts("libc AtomicSharedPtr tests PASS");
    return EXIT_SUCCESS;
}
