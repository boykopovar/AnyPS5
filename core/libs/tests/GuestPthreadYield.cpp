#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int LONE_YIELDS = 4096;
static constexpr std::uint64_t YIELD_LIMIT = 100000ull;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<bool> workerPinned{false};
static std::atomic<bool> workerDone{false};

static void* APS5_VABI SpinWork(void* arg) {
    const auto cpu = *static_cast<const KernelCpumask*>(arg);
    workerPinned.store(scePthreadSetaffinity(scePthreadSelf(), cpu) == SCE_OK);
    volatile std::uint64_t sum = 0;
    for (std::uint64_t i = 0; i < 2000000; ++i) sum = sum + i;
    workerDone.store(true);
    return nullptr;
}

int main() {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    Require(available != 0);
    const KernelCpumask cpu = KernelCpumask{1} << std::countr_zero(available);
    Require(scePthreadSetaffinity(scePthreadSelf(), cpu) == SCE_OK);

    for (int call = 0; call < LONE_YIELDS; ++call) scePthreadYield();

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, SpinWork, const_cast<KernelCpumask*>(&cpu), nullptr) == SCE_OK);
    std::uint64_t yields = 0;
    while (!workerDone.load()) {
        scePthreadYield();
        Require(++yields < YIELD_LIMIT);
    }
    Require(scePthreadJoin(worker, nullptr) == SCE_OK);
    Require(workerPinned.load());
    return 0;
}
