#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

static constexpr int SigUsr1 = 30;
static constexpr int Raises = 100;
static void Require(bool value) { if (!value) std::abort(); }

static std::FILE* shared = nullptr;
static std::atomic<int> deliveries{0};
static std::atomic<int> released{0};
static std::atomic<bool> stop{false};
static std::atomic<bool> started{false};

static void APS5_VABI Handler(int, void*) {
    const auto delivery = deliveries.fetch_add(1) + 1;
    while (released.load() < delivery) std::this_thread::yield();
}

static void* APS5_VABI Writer(void*) {
    const std::string block(1u << 20u, 'x');
    started.store(true);
    while (!stop.load()) {
        Require(std::fwrite(block.data(), 1, block.size(), shared) == block.size());
        Require(std::fseek(shared, 0, SEEK_SET) == 0);
        volatile std::uint64_t spin = 0;
        for (std::uint64_t i = 0; i < 200000; ++i) spin = spin + i;
    }
    return nullptr;
}

int main() {
    std::atomic<bool> done{false};
    std::thread watchdog([&done] {
        for (int i = 0; i < 20000 && !done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!done.load()) std::abort();
    });
    shared = TemporaryFile::Open();
    Require(shared != nullptr);
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, nullptr, "host writer") == 0);
    while (!started.load()) std::this_thread::yield();
    auto longest = std::chrono::steady_clock::duration::zero();
    for (int raise = 1; raise <= Raises; ++raise) {
        const auto raisedAt = std::chrono::steady_clock::now();
        Require(sceKernelRaiseException(writer, SigUsr1) == 0);
        longest = std::max(longest, std::chrono::steady_clock::now() - raisedAt);
        while (deliveries.load() < raise) std::this_thread::yield();
        Require(std::fputc('y', shared) == 'y');
        released.store(raise);
    }
    stop.store(true);
    Require(scePthreadJoin(writer, nullptr) == 0);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    std::fclose(shared);
    std::printf("longest raise: %.2f ms\n", std::chrono::duration<double, std::milli>(longest).count());
    done.store(true);
    watchdog.join();
}
