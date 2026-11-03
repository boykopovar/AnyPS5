#include "prx/libSceAgcDriver/Execution/include/Driver/DeviceAccess.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

void MutexCounts(int threads, int iterations, int holdSpins) {
    AgcDriver::Mutex mutex;
    std::uint64_t counter = 0;
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < iterations; ++i) {
                std::lock_guard lock(mutex);
                const auto seen = counter;
                for (volatile int spin = 0; spin < holdSpins; spin = spin + 1) {
                }
                counter = seen + 1;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Check(counter == static_cast<std::uint64_t>(threads) * static_cast<std::uint64_t>(iterations), "every increment under the mutex counted");
}

void MutexTryLock() {
    AgcDriver::Mutex mutex;
    Check(mutex.try_lock(), "try_lock takes a free mutex");
    bool taken = true;
    std::thread other([&] { taken = mutex.try_lock(); });
    other.join();
    Check(!taken, "try_lock fails while another thread holds the mutex");
    mutex.unlock();
    std::thread again([&] {
        taken = mutex.try_lock();
        if (taken) mutex.unlock();
    });
    again.join();
    Check(taken, "try_lock takes the mutex once released");
}

void MutexWakesSleeper() {
    AgcDriver::Mutex mutex;
    mutex.lock();
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        std::lock_guard lock(mutex);
        acquired = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Check(!acquired, "lock() waits while the mutex is held");
    mutex.unlock();
    waiter.join();
    Check(acquired, "the waiting thread took the mutex after the release");
}

void DeviceGate() {
    AgcDriver::DriverDetail::DeviceUseGate gate;
    std::atomic<int> users{0};
    std::atomic<int> replacements{0};
    std::atomic<bool> overlap{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> uses{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                std::shared_lock use(gate);
                users.fetch_add(1);
                if (replacements.load() != 0) overlap = true;
                users.fetch_sub(1);
                uses.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (int r = 0; r < 2; ++r) {
        threads.emplace_back([&] {
            for (int i = 0; i < 300; ++i) {
                std::unique_lock replacing(gate);
                if (replacements.fetch_add(1) != 0 || users.load() != 0) overlap = true;
                std::this_thread::yield();
                if (users.load() != 0) overlap = true;
                replacements.fetch_sub(1);
            }
        });
    }
    threads[6].join();
    threads[7].join();
    stop = true;
    for (int t = 0; t < 6; ++t) threads[static_cast<std::size_t>(t)].join();
    Check(!overlap, "no user inside the gate during a replacement, one replacement at a time");
    Check(uses.load() != 0, "users got through between replacements");
}

void GpuMutexRecursion() {
    auto& mutex = AgcDriver::GuestMemory::GpuMutex();
    Check(!mutex.HeldByThisThread(), "the GPU mutex starts free");
    mutex.lock();
    mutex.lock();
    Check(mutex.HeldByThisThread() && mutex.DepthOnThisThread() == 2, "a nested lock counts the depth");
    bool taken = true;
    std::thread other([&] {
        taken = mutex.try_lock();
        if (taken) mutex.unlock();
    });
    other.join();
    Check(!taken, "another thread cannot take the held GPU mutex");
    Check(mutex.try_lock() && mutex.DepthOnThisThread() == 3, "the holder's try_lock nests");
    mutex.unlock();
    mutex.unlock();
    other = std::thread([&] {
        taken = mutex.try_lock();
        if (taken) mutex.unlock();
    });
    other.join();
    Check(!taken, "the GPU mutex stays held until the outermost unlock");
    mutex.unlock();
    Check(!mutex.HeldByThisThread() && mutex.DepthOnThisThread() == 0, "the outermost unlock releases the GPU mutex");
    std::uint64_t counter = 0;
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                std::lock_guard outer(mutex);
                std::lock_guard inner(mutex);
                ++counter;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Check(counter == 80000, "every nested increment under the GPU mutex counted");
}

template<typename TTag>
void SlotIsolation(const char* what) {
    using Slot = HostThreadSlot<std::uint64_t, TTag>;
    bool ok = Slot::Get() == 0;
    Slot::Set(0x1234567890abcdefull);
    std::uint64_t seen = 1;
    std::thread other([&] {
        seen = Slot::Get();
        Slot::Set(99);
    });
    other.join();
    ok = ok && seen == 0 && Slot::Get() == 0x1234567890abcdefull;
    Slot::Set(0);
    Check(ok, what);
}

struct DirectSlot {};
struct ExpansionSlot {};
struct PointerSlot {};

void ThreadSlots() {
    SlotIsolation<DirectSlot>("a thread slot starts at zero on every thread and keeps its own value");
    int object = 0;
    HostThreadSlot<int*, PointerSlot>::Set(&object);
    Check(HostThreadSlot<int*, PointerSlot>::Get() == &object, "a pointer round-trips through a thread slot");
#ifdef _WIN32
    std::vector<DWORD> held;
    for (DWORD index = TlsAlloc(); index != TLS_OUT_OF_INDEXES; index = TlsAlloc()) {
        held.push_back(index);
        if (index >= 64) break;
    }
    SlotIsolation<ExpansionSlot>("an expansion thread slot starts at zero on every thread and keeps its own value");
    for (const auto index : held) TlsFree(index);
#endif
    AgcDriver::GuestMemory::PacketTag fresh{};
    AgcDriver::GuestMemory::ReadSite freshSite = AgcDriver::GuestMemory::ReadSite::Count;
    std::thread other([&] {
        fresh = AgcDriver::GuestMemory::CurrentPacket();
        freshSite = AgcDriver::GuestMemory::CurrentReadSite();
    });
    other.join();
    Check(fresh.opcode == AgcDriver::GuestMemory::NoPacket && fresh.queue == 0xffffffffu, "a thread that ran no packet reads NoPacket");
    Check(freshSite == AgcDriver::GuestMemory::ReadSite::Unknown, "a thread that named no read site reads Unknown");
    AgcDriver::GuestMemory::SetCurrentPacket(0xffffu, 0x2au);
    const auto packet = AgcDriver::GuestMemory::CurrentPacket();
    Check(packet.opcode == 0xffffu && packet.queue == 0x2au, "the packet tag reads back what was set");
    const auto previous = AgcDriver::GuestMemory::SetReadSite(AgcDriver::GuestMemory::ReadSite::Scanout);
    Check(previous == AgcDriver::GuestMemory::ReadSite::Unknown && AgcDriver::GuestMemory::CurrentReadSite() == AgcDriver::GuestMemory::ReadSite::Scanout, "SetReadSite returns the previous site and sets the new one");
    AgcDriver::GuestMemory::SetReadSite(previous);
}

}

int main() {
    MutexCounts(8, 100000, 0);
    MutexCounts(4, 2000, 20000);
    MutexTryLock();
    MutexWakesSleeper();
    DeviceGate();
    GpuMutexRecursion();
    ThreadSlots();
    if (failures != 0) {
        std::fprintf(stderr, "%d lock check(s) failed\n", failures);
        return 1;
    }
    std::printf("lock tests passed\n");
    return 0;
}
