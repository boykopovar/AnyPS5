#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

static void CheckValueLimit() {
    constexpr unsigned int maximum = 0x7fffffffu;
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 7, nullptr) == SCE_OK);
    const PthreadSem original = sem;
    const unsigned int invalidValues[] = {maximum + 1u, std::numeric_limits<unsigned int>::max()};
    int value = -1;
    for (const unsigned int initial : invalidValues) {
        Require(scePthreadSemInit(&sem, 0, initial, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(sem == original);
        Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 7);
    }
    Require(scePthreadSemDestroy(&sem) == SCE_OK);

    Require(scePthreadSemInit(&sem, 0, maximum, nullptr) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum - 1));
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

struct Context {
    PthreadSem sem = nullptr;
};

static void* APS5_VABI Poster(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    return nullptr;
}

int main() {
    CheckValueLimit();

    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 2, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 2);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 0);
    Require(scePthreadSemTimedwait(&sem, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 1);

    Context context;
    Require(scePthreadSemInit(&context.sem, 0, 0, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Poster, &context, nullptr) == SCE_OK);
    Require(scePthreadSemTimedwait(&context.sem, 5000000) == SCE_OK);
    Require(scePthreadSemWait(&context.sem) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadSemDestroy(&context.sem) == SCE_OK);

    bool rejected = false;
    try { scePthreadSemWait(&context.sem); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}
