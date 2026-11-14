#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static constexpr int SigUsr1 = 30;
static constexpr int Raises = 100;
static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> deliveries{0};
static std::atomic<bool> stop{false};
static std::atomic<unsigned long long> naps{0};
static std::atomic<KernelUseconds> napMicros{0};

static void APS5_VABI Handler(int, void*) { deliveries.fetch_add(1); }

static void* APS5_VABI Napper(void*) {
    while (!stop.load()) {
        sceKernelUsleep_nid_postfix(napMicros.load());
        naps.fetch_add(1);
    }
    return nullptr;
}

static void RaiseAtNapper(KernelUseconds micros) {
    napMicros.store(micros);
    stop.store(false);
    naps.store(0);
    Pthread napper = nullptr;
    Require(scePthreadCreate(&napper, nullptr, Napper, nullptr, "napper") == 0);
    while (naps.load() == 0) std::this_thread::yield();
    for (int raise = 0; raise < Raises; ++raise) {
        const int before = deliveries.load();
        Require(sceKernelRaiseException(napper, SigUsr1) == 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (deliveries.load() == before) {
            if (std::chrono::steady_clock::now() >= deadline) {
                std::fprintf(stderr, "raise %d at a thread sleeping %u us in a loop was not delivered within 3 s (%llu sleeps since)\n", raise, micros, naps.load());
                std::abort();
            }
            std::this_thread::yield();
        }
    }
    stop.store(true);
    Require(scePthreadJoin(napper, nullptr) == 0);
}

int main() {
    std::atomic<bool> done{false};
    std::thread watchdog([&done] {
        for (int i = 0; i < 20000 && !done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!done.load()) std::abort();
    });
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    for (const KernelUseconds micros : {100u, 200u, 400u}) RaiseAtNapper(micros);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    std::printf("%d raises delivered\n", deliveries.load());
    done.store(true);
    watchdog.join();
}
