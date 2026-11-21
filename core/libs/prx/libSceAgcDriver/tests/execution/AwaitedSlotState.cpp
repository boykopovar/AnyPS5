#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static constexpr std::uint32_t Queue = UnmeasuredSubmissionQueueEnd;
    static const Driver::AwaitedSlot& Slot() { return Driver::Get().awaitedSlots[Queue - 0x20 + 1]; }
    static void Run() { Driver::Get().runningWorkers.fetch_add(1, std::memory_order_seq_cst); }
    static void Rest() { Driver::Get().runningWorkers.fetch_sub(1, std::memory_order_seq_cst); }
    static void BlockOnMemory(const std::vector<std::uint32_t>& packet, const void* awaited) {
        Driver::Get().noteWaitFields(Queue, packet);
        Driver::Get().noteWaitBlocked(Queue, reinterpret_cast<std::uint64_t>(awaited), true);
    }
    static void BlockOnRewind(const void* control) {
        Driver::Get().noteRewindWaitFields(Queue);
        Driver::Get().noteWaitBlocked(Queue, reinterpret_cast<std::uint64_t>(control), true);
    }
    static void Unblock() { Driver::Get().noteWaitBlocked(Queue, 0, false); }
    static std::uint64_t HeldAddress() { return Slot().address.load(std::memory_order_acquire); }
    static bool StillAwaited(std::uint64_t held) { return Driver::stillAwaited(Slot(), held); }
    static std::optional<Driver::AwaitedFields> Read(std::uint64_t held) { return Driver::readAwaited(Slot(), held); }
    static bool Unchanged(const Driver::AwaitedFields& fields) { return Driver::awaitedUnchanged(Slot(), fields); }
    static std::optional<Driver::AwaitedFields> Blocked(std::uint64_t held) { return Driver::blockedOn(Slot(), held); }
    static bool HeldStill(const Driver::AwaitedFields& fields) {
        const std::array<Driver::HeldSlot, 1> held{{{&Slot(), fields}}};
        return Driver::Get().slotsHeldStill(held);
    }
    static void HoldCapture(bool held) { Driver::Get().captureHold.store(held, std::memory_order_seq_cst); }
    static std::uint32_t Running() { return Driver::Get().runningWorkers.load(std::memory_order_seq_cst); }
};

}

namespace {

using AgcDriver::DriverDetail::DriverTestAccess;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint32_t low(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t high(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); }

std::vector<std::uint32_t> waitEqual(const void* awaited, std::uint32_t reference) {
    return {0xc0000000u | (5u << 16u) | (0x3cu << 8u), 0x13u, low(awaited), high(awaited), reference, 0xffffffffu, 0x10u};
}

struct Worker {
    Worker() { DriverTestAccess::Run(); }
    ~Worker() { DriverTestAccess::Rest(); }
};

void satisfiedGateHeldAcrossUnblock() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    Worker worker;
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    const auto held = DriverTestAccess::HeldAddress();
    require(held == reinterpret_cast<std::uint64_t>(&gate), "the slot does not publish the awaited gate");
    require(DriverTestAccess::StillAwaited(held), "an unsatisfied gate is not awaited");
    gate.store(1, std::memory_order_release);
    require(!DriverTestAccess::StillAwaited(held), "a satisfied gate is still awaited while the worker is blocked");
    DriverTestAccess::Unblock();
    require(!DriverTestAccess::StillAwaited(held), "a gate read before the worker unblocked is still awaited after it unblocked");
}

void fieldsReadAcrossReblockAtTheSameAddress() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    Worker worker;
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    const auto read = DriverTestAccess::Read(reinterpret_cast<std::uint64_t>(&gate));
    require(read.has_value() && read->reference == 1, "the slot does not report the first wait");
    require(DriverTestAccess::Unchanged(*read), "an untouched slot reads as changed");
    DriverTestAccess::Unblock();
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 2), &gate);
    require(!DriverTestAccess::Unchanged(*read), "fields read before a reblock at the same address still read as current");
    const auto reread = DriverTestAccess::Read(reinterpret_cast<std::uint64_t>(&gate));
    require(reread.has_value() && reread->reference == 2 && DriverTestAccess::Unchanged(*reread), "the second wait does not read as current");
    DriverTestAccess::Unblock();
}

void rewindWaitAfterMemoryWait() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    alignas(8) std::atomic<std::uint32_t> control{0};
    Worker worker;
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    DriverTestAccess::Unblock();
    DriverTestAccess::BlockOnRewind(&control);
    const auto held = DriverTestAccess::HeldAddress();
    require(held == reinterpret_cast<std::uint64_t>(&control), "the slot does not publish the REWIND control word");
    require(DriverTestAccess::StillAwaited(held), "a REWIND wait whose control word is not valid is not awaited");
    control.store(AgcDriver::Pm4::RewindValid, std::memory_order_release);
    require(!DriverTestAccess::StillAwaited(held), "a REWIND wait whose control word is valid is still awaited");
    DriverTestAccess::Unblock();
}

void workerLeavingItsWaitDuringTheCheck() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    Worker worker;
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    const auto blocked = DriverTestAccess::Blocked(reinterpret_cast<std::uint64_t>(&gate));
    require(blocked.has_value() && DriverTestAccess::HeldStill(*blocked), "a blocked wait does not hold still");
    DriverTestAccess::HoldCapture(true);
    std::thread leaving([] { DriverTestAccess::Unblock(); });
    while (DriverTestAccess::Running() == 0) std::this_thread::yield();
    const bool heldStill = DriverTestAccess::HeldStill(*blocked);
    DriverTestAccess::HoldCapture(false);
    leaving.join();
    require(!heldStill, "a wait the worker is leaving holds still");
    require(!DriverTestAccess::HeldStill(*blocked), "a wait the worker left holds still");
}

void workerReblockedDuringTheCheck() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    Worker worker;
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    const auto blocked = DriverTestAccess::Blocked(reinterpret_cast<std::uint64_t>(&gate));
    require(blocked.has_value(), "a blocked wait is not blocked");
    DriverTestAccess::Unblock();
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 1), &gate);
    require(!DriverTestAccess::HeldStill(*blocked), "a wait the worker left and entered again holds still");
    DriverTestAccess::Unblock();
}

void awaitedWaitsFollowTheSlot() {
    alignas(8) std::atomic<std::uint32_t> gate{0};
    Worker worker;
    const auto listed = [&] {
        std::array<AgcDriver::AwaitedWait, 64> waits{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
        for (std::size_t i = 0; i < count && i < waits.size(); ++i) {
            if (waits[i].address == reinterpret_cast<std::uint64_t>(&gate)) return std::optional(waits[i]);
        }
        return std::optional<AgcDriver::AwaitedWait>();
    };
    DriverTestAccess::BlockOnMemory(waitEqual(&gate, 7), &gate);
    const auto blocked = listed();
    require(blocked.has_value() && blocked->reference == 7 && blocked->function == 3 && blocked->bytes == 4, "the awaited waits do not report the blocked wait");
    DriverTestAccess::Unblock();
    require(!listed().has_value(), "the awaited waits still report a wait the worker left");
}

}

int main() {
    const std::array<std::pair<const char*, void (*)()>, 6> cases{{
        {"satisfied gate held across unblock", satisfiedGateHeldAcrossUnblock},
        {"fields read across a reblock at the same address", fieldsReadAcrossReblockAtTheSameAddress},
        {"REWIND wait after a memory wait", rewindWaitAfterMemoryWait},
        {"awaited waits follow the slot", awaitedWaitsFollowTheSlot},
        {"worker leaving its wait during the check", workerLeavingItsWaitDuringTheCheck},
        {"worker reblocked during the check", workerReblockedDuringTheCheck},
    }};
    int failures = 0;
    for (const auto& [name, run] : cases) {
        try {
            run();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
