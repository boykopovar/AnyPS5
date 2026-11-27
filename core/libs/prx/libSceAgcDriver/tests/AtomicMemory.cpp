#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void ExecuteAfterDeferredWait(std::span<const std::uint32_t> wait,
                                         std::span<const std::uint32_t> command) {
        Driver::notePendingWait(wait, ComputeQueue, 0);
        Execute(command);
    }

    static void ExecuteAfterDeferredLabel(std::uint64_t address, std::uint64_t value,
                                          std::span<const std::uint32_t> command) {
        auto& labels = deferredLabels();
        auto& label = labels.labels.emplace_back();
        label.address = address;
        label.size = sizeof(value);
        std::memcpy(label.bytes.data(), &value, sizeof(value));
        labels.since = std::chrono::steady_clock::now();
        Execute(command);
    }

private:
    static constexpr std::uint32_t ComputeQueue = 0x20;

    static void Execute(std::span<const std::uint32_t> command) {
        Submission submission{};
        submission.queue = ComputeQueue;
        submission.commands.assign(command.begin(), command.end());
        Driver::Get().execute(submission);
    }
};

}

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename F>
void refused(F action) {
    bool failed = false;
    try { action(); }
    catch (const std::runtime_error&) { failed = true; }
    check(failed, "unsupported atomic command was accepted");
}

template <typename T>
struct MemoryStorage {
    alignas(64) std::array<T, 8> words{};
    MemoryStorage() { GuestAllocations::Mutation().Add(words.data(), sizeof(words), true, true, true); }
    ~MemoryStorage() { GuestAllocations::Mutation().Remove(words.data()); }
};

using Storage = MemoryStorage<std::uint64_t>;

std::array<std::uint32_t, 9> packet(void* target, std::uint32_t operation, std::uint64_t source, std::uint64_t comparison = 0, std::uint32_t flags = 0) {
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    return {0xc0071e00u, operation | flags, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(comparison), static_cast<std::uint32_t>(comparison >> 32u), 6};
}

struct Example {
    std::uint32_t operation;
    std::uint64_t old;
    std::uint64_t source;
    std::uint64_t comparison;
    std::uint64_t expected;
};

void integers() {
    const std::array<Example, 21> examples{{
        {0x07, 8, 19, 0, 19},
        {0x08, 8, 19, 8, 19},
        {0x08, 8, 19, 7, 8},
        {0x0f, 8, 19, 0, 27},
        {0x10, 19, 8, 0, 11},
        {0x11, 8, 19, 0, 8},
        {0x12, 8, 19, 0, 8},
        {0x13, 8, 19, 0, 19},
        {0x14, 8, 19, 0, 19},
        {0x15, 0xa5, 0x3c, 0, 0x24},
        {0x16, 0xa5, 0x3c, 0, 0xbd},
        {0x17, 0xa5, 0x3c, 0, 0x99},
        {0x18, 11, 12, 0, 12},
        {0x18, 12, 12, 0, 0},
        {0x18, 13, 12, 0, 0},
        {0x18, 0, 0, 0, 0},
        {0x19, 5, 12, 0, 4},
        {0x19, 0, 12, 0, 12},
        {0x19, 13, 12, 0, 12},
        {0x19, 12, 12, 0, 11},
        {0x19, 0, 0, 0, 0},
    }};
    Storage storage;
    AgcDriver::QueueState queue;
    std::uint32_t checks = 0;
    const auto run = [&](const Example& item, bool wide, std::uint32_t noReturn, std::uint32_t control) {
        const auto operation = item.operation | (wide ? 0x20u : 0u) | noReturn;
        storage.words.fill(0x5566778811223344ull);
        storage.words[1] = wide ? item.old : 0xdeadbeef00000000ull | static_cast<std::uint32_t>(item.old);
        const auto command = packet(&storage.words[1], operation, wide ? item.source : item.source | 0xabcdef0000000000ull, wide ? item.comparison : item.comparison | 0x1122330000000000ull, control);
        AgcDriver::Pm4::Validate(command, 0);
        AgcDriver::Pm4::Execute(command, queue);
        const auto expected = wide ? item.expected : 0xdeadbeef00000000ull | static_cast<std::uint32_t>(item.expected);
        check(storage.words[1] == expected, "atomic operation produced the wrong value or touched the adjacent 32 bits");
        for (std::size_t i = 0; i < storage.words.size(); ++i) if (i != 1) check(storage.words[i] == 0x5566778811223344ull, "atomic operation changed its neighboring memory");
        ++checks;
    };
    for (const bool wide : {false, true}) {
        for (const auto noReturn : {0u, 0x40u}) {
            for (const auto engine : {0u, 1u}) {
                for (const auto mode : {0u, 2u, 3u}) {
                    for (std::uint32_t policy = 0; policy < 4; ++policy) {
                        const auto control = engine << 30u | mode << 8u | policy << 25u;
                        for (const auto& item : examples) run(item, wide, noReturn, control);
                        const auto all = wide ? 0xffffffffffffffffull : 0xffffffffull;
                        const auto sign = wide ? 0x8000000000000000ull : 0x80000000ull;
                        const auto positive = sign - 1;
                        for (const auto& item : std::array<Example, 8>{{
                            {0x0f, all, 1, 0, 0}, {0x10, 0, 1, 0, all},
                            {0x11, sign, positive, 0, sign}, {0x12, sign, positive, 0, positive},
                            {0x13, sign, positive, 0, positive}, {0x14, sign, positive, 0, sign},
                            {0x18, all, all, 0, 0}, {0x19, 0, all, 0, all},
                        }}) run(item, wide, noReturn, control);
                        if (wide) run({0x07, 0, 0x1122334455667788ull, 0, 0x1122334455667788ull}, wide, noReturn, control);
                    }
                }
            }
        }
    }
    std::printf("atomic integer examples passed: %u\n", checks);
}

void protocol() {
    Storage storage;
    AgcDriver::QueueState queue;
    auto valid = packet(&storage.words[1], 0x67, 1, 0, 0x40000200u);
    check(AgcDriver::Pm4::KnownName(valid[0]) == "ATOMIC_MEM" && AgcDriver::Pm4::UnsupportedReason(valid[0]).empty(), "atomic packet is not supported in the opcode catalog");
    check(AgcDriver::Pm4::AccessesMemory(valid[0]), "atomic packet is missing from memory-access classification");
    const auto stores = AgcDriver::Pm4::DecodeStores(valid);
    check(stores.size() == 1 && stores[0].address == reinterpret_cast<std::uintptr_t>(&storage.words[1]) && stores[0].bytes == 8 && stores[0].value == AgcDriver::Pm4::PacketStore::Value::RunTime, "atomic destination is missing from runtime write tracking");
    for (std::size_t length = 0; length < valid.size(); ++length) refused([&] { AgcDriver::Pm4::Validate(std::span(valid).first(length), 0); });
    for (const auto control : {0x40000167u, 0x40000467u, 0x80000067u, 0xc0000067u, 0x400000e7u, 0x40001067u}) {
        auto invalid = valid;
        invalid[1] = control;
        refused([&] { AgcDriver::Pm4::Validate(invalid, 0); });
        refused([&] { AgcDriver::Pm4::Execute(invalid, queue); });
    }
    for (std::uint32_t operation = 0; operation < 128; ++operation) {
        const auto base = operation & 0x1fu;
        if (base == 7 || base == 8 || (base >= 15 && base <= 25)) continue;
        auto invalid = valid;
        invalid[1] = operation;
        refused([&] { AgcDriver::Pm4::Validate(invalid, 0); });
        refused([&] { AgcDriver::Pm4::Execute(invalid, queue); });
    }
    for (const auto low : {0u, valid[2] + 1u, valid[2] + 4u}) {
        auto invalid = valid;
        invalid[2] = low;
        if (low == 0) invalid[3] = 0;
        refused([&] { AgcDriver::Pm4::Validate(invalid, 0); });
    }
    auto badInterval = valid;
    badInterval[8] = 0x2000u;
    refused([&] { AgcDriver::Pm4::Validate(badInterval, 0); });
    check(storage.words == std::array<std::uint64_t, 8>{}, "a refused atomic changed memory");
}

template <typename T>
void concurrentTyped() {
    MemoryStorage<T> storage;
    const auto command = packet(&storage.words[1], sizeof(T) == 8 ? 0x6fu : 0x4fu, 1);
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([&, i] {
        try {
            AgcDriver::QueueState queue;
            for (int iteration = 0; iteration < 250; ++iteration) {
                if (i < 2) AgcDriver::Pm4::Execute(command, queue);
                else std::atomic_ref<T>(storage.words[1]).fetch_add(1, std::memory_order_acq_rel);
            }
        } catch (...) { failed.store(true); }
    });
    for (auto& worker : workers) worker.join();
    check(!failed.load() && storage.words[1] == 1000, "concurrent atomic increments were lost");
}

void concurrent(bool wide) {
    if (wide) concurrentTyped<std::uint64_t>();
    else concurrentTyped<std::uint32_t>();
}

void exclusion() {
    Storage storage;
    const auto command = packet(&storage.words[1], 0x6f, 1);
    std::atomic<bool> started{false}, done{false}, failed{false};
    auto& mutex = AgcDriver::GuestMemory::GpuMutex();
    std::unique_lock lock(mutex);
    std::thread worker([&] {
        started.store(true, std::memory_order_release);
        try {
            AgcDriver::QueueState queue;
            AgcDriver::Pm4::Execute(command, queue);
        } catch (...) { failed.store(true); }
        done.store(true, std::memory_order_release);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((!started.load(std::memory_order_acquire) || (!done.load(std::memory_order_acquire) && mutex.Waiters() == 0)) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    const bool excluded = !done.load(std::memory_order_acquire) && mutex.Waiters() != 0;
    lock.unlock();
    worker.join();
    check(excluded, "CP atomic ran while another GPU transaction held the device mutex");
    check(!failed.load() && storage.words[1] == 1, "excluded atomic did not finish after the GPU transaction released");
}

struct FlushObservation {
    std::uint64_t* target = nullptr;
    std::uint64_t address = 0;
    std::size_t bytes = 0;
    unsigned calls = 0;
    bool heldDeviceLock = false;
};

thread_local FlushObservation atomicFlush;

class ScopedFlushHook {
public:
    explicit ScopedFlushHook(decltype(AgcDriver::GuestMemory::CurrentFlushHook()) hook)
        : previous(AgcDriver::GuestMemory::CurrentFlushHook()) {
        AgcDriver::GuestMemory::SetFlushHook(hook);
    }
    ~ScopedFlushHook() { AgcDriver::GuestMemory::SetFlushHook(previous); }

private:
    decltype(AgcDriver::GuestMemory::CurrentFlushHook()) previous;
};

void publishAtomicProducer(std::uint64_t address, std::size_t bytes) {
    ++atomicFlush.calls;
    atomicFlush.address = address;
    atomicFlush.bytes = bytes;
    atomicFlush.heldDeviceLock = AgcDriver::GuestMemory::GpuMutex().HeldByThisThread();
    if (address == reinterpret_cast<std::uintptr_t>(atomicFlush.target) &&
        bytes == sizeof(*atomicFlush.target)) {
        *atomicFlush.target = 37;
    }
}

void rangeFlush() {
    Storage storage;
    storage.words = {9, 11, 13};
    const auto command = packet(&storage.words[1], 0x6f, 5);
    const auto address = reinterpret_cast<std::uintptr_t>(&storage.words[1]);
    atomicFlush = {&storage.words[1]};
    const ScopedFlushHook hook(&publishAtomicProducer);

    AgcDriver::QueueState queue;
    AgcDriver::Pm4::Execute(command, queue);

    check(atomicFlush.calls == 1 && atomicFlush.address == address &&
              atomicFlush.bytes == sizeof(storage.words[1]),
          "CP atomic did not flush its exact destination range once");
    check(atomicFlush.heldDeviceLock, "CP atomic flushed outside the GPU transaction");
    check(storage.words == std::array<std::uint64_t, 8>{9, 42, 13},
          "CP atomic did not use the value published by the flush or changed an adjacent word");
}

struct PendingProducer {
    std::uint32_t* gate = nullptr;
    std::uint64_t* target = nullptr;
};

thread_local PendingProducer pendingProducer;

void publishWaitedProducer(std::uint64_t address, std::size_t bytes) {
    if (address == reinterpret_cast<std::uintptr_t>(pendingProducer.gate) &&
        bytes == sizeof(*pendingProducer.gate)) {
        *pendingProducer.target = 37;
        std::atomic_ref(*pendingProducer.gate).store(1, std::memory_order_release);
    }
}

void pendingWait() {
    Storage storage;
    MemoryStorage<std::uint32_t> gate;
    storage.words[1] = 11;
    const auto address = reinterpret_cast<std::uintptr_t>(&gate.words[0]);
    const std::array<std::uint32_t, 7> wait{
        0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>(address >> 32u), 1u, 0xffffffffu, 0x19u,
    };
    const auto command = packet(&storage.words[1], 0x6f, 5);
    pendingProducer = {&gate.words[0], &storage.words[1]};
    const ScopedFlushHook hook(&publishWaitedProducer);

    AgcDriver::DriverDetail::DriverTestAccess::ExecuteAfterDeferredWait(wait, command);

    check(gate.words[0] == 1 && storage.words[1] == 42,
          "CP atomic ran before the deferred wait published its producer's value");
}

void deferredLabel() {
    static_cast<void>(AgcDriver::DriverDetail::Driver::Get());
    Storage storage;
    storage.words[1] = 11;
    const auto address = reinterpret_cast<std::uintptr_t>(&storage.words[1]);
    const auto command = packet(&storage.words[1], 0x6f, 5);
    auto& gpu = AgcDriver::GuestMemory::GpuMutex();
    std::atomic<bool> finished{false};
    bool failed = false;
    std::unique_lock lock(gpu);
    std::thread worker([&] {
        try {
            AgcDriver::DriverDetail::DriverTestAccess::ExecuteAfterDeferredLabel(address, 37, command);
        } catch (...) {
            failed = true;
        }
        finished.store(true, std::memory_order_release);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (gpu.Waiters() == 0 && !finished.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const bool contended = gpu.Waiters() != 0;
    lock.unlock();
    worker.join();

    check(contended && !failed, "deferred-label fixture did not complete under GPU lock contention");
    check(storage.words[1] == 42, "CP atomic ran before the deferred label published its value");
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected atomic test mode");
        const std::string mode = argv[1];
        if (mode == "integers") integers();
        else if (mode == "protocol") protocol();
        else if (mode == "concurrent32") concurrent(false);
        else if (mode == "concurrent64") concurrent(true);
        else if (mode == "exclusion") exclusion();
        else if (mode == "range_flush") rangeFlush();
        else if (mode == "pending_wait") pendingWait();
        else if (mode == "deferred_label") deferredLabel();
        else throw std::runtime_error("unknown atomic test mode");
        LibcRunShutdown_nid_postfix();
        std::puts("atomic memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
