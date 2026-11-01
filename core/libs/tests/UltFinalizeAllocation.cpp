#include "prx/libSceUlt/libSceUlt.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

static int allocationBudget = -1;
static unsigned allocationAttempts = 0;

void* operator new(std::size_t size) {
    if (allocationBudget >= 0) {
        ++allocationAttempts;
        if (allocationBudget == 0) {
            allocationBudget = -1;
            throw std::bad_alloc();
        }
        --allocationBudget;
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }

struct alignas(8) Object { std::uint8_t bytes[256]{}; };

static void Require(bool passed, const char* message) {
    if (!passed) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    Require(sceUltInitialize() == ULT_OK, "initialization failed");
    unsigned failures = 0;
    unsigned rejectedCases = 0;
    for (int budget = 0; budget < 8; ++budget) {
        Object semaphore{};
        Object mutex{};
        Require(sceUltSemaphoreCreate(&semaphore, "semaphore", 1, nullptr, nullptr, 0) == ULT_OK, "semaphore creation failed");
        Require(sceUltMutexCreate(&mutex, "mutex", nullptr, nullptr, 0) == ULT_OK, "mutex creation failed");
        allocationAttempts = 0;
        allocationBudget = budget;
        bool rejected = false;
        int outcome = ULT_OK;
        try { outcome = sceUltFinalize(); }
        catch (const std::bad_alloc&) { rejected = true; }
        allocationBudget = -1;

        rejectedCases += rejected;
        const int semaphoreResult = sceUltSemaphoreTryAcquire(&semaphore, 1);
        const int mutexResult = sceUltMutexTryLock(&mutex);
        if (semaphoreResult == ULT_OK) Require(sceUltSemaphoreRelease(&semaphore, 1) == ULT_OK, "semaphore release failed");
        if (mutexResult == ULT_OK) Require(sceUltMutexUnlock(&mutex) == ULT_OK, "mutex unlock failed");
        const int expected = rejected ? ULT_OK : ULT_ERROR_STATE;
        if (outcome != ULT_OK || semaphoreResult != expected || mutexResult != expected) {
            std::fprintf(stderr, "budget=%d rejected=%d attempts=%u semaphore=%d mutex=%d\n",
                         budget, rejected, allocationAttempts, semaphoreResult, mutexResult);
            ++failures;
        }
        if (rejected) Require(sceUltFinalize() == ULT_OK, "finalization retry failed");
        Require(sceUltSemaphoreTryAcquire(&semaphore, 1) == ULT_ERROR_STATE, "finalized semaphore is still live");
        Require(sceUltMutexTryLock(&mutex) == ULT_ERROR_STATE, "finalized mutex is still live");
    }
    std::printf("finalization allocation budgets: 8 cases, %u rejections, %u incorrect lifecycles\n", rejectedCases, failures);
    Require(rejectedCases > 0, "no allocation failure was injected");
    Require(rejectedCases < 8, "no finalization succeeded without a retry");
    return failures == 0 ? 0 : 1;
}
