#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;
namespace LeanFront = AgcDriver::DriverDetail::LeanFront;
using Range = DrawFinisher::Range;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::mutex orderMutex;
std::vector<std::uint32_t> order;

void Append(std::uint32_t value) {
    const std::lock_guard lock(orderMutex);
    order.push_back(value);
}

void RunPayload(const std::byte* payload) {
    std::uint32_t value = 0;
    std::memcpy(&value, payload, sizeof(value));
    Append(value);
}

void PublishValue(std::uint32_t value, std::span<const Range> writes = {}, std::uint64_t key = 0) {
    auto& finisher = DrawFinisher::Get();
    auto* payload = finisher.ReserveLean();
    Require(payload != nullptr, "the finisher ring refused a job while open");
    std::memcpy(payload, &value, sizeof(value));
    finisher.PublishLean(&RunPayload, writes, key, 0);
}

DrawFinisher::Pending Unknown() {
    return DrawFinisher::Pending{0, 0, true, {}, 0, {}};
}

void QueriesFromTheRing() {
    auto& finisher = DrawFinisher::Get();
    std::binary_semaphore release{0};
    DrawFinisher::Pending held;
    held.key = 41;
    held.shape = 42;
    held.unknown = false;
    held.writes = {{0x80000, 0x80100}};
    finisher.Enqueue([&] { release.acquire(); }, held);
    Require(finisher.RingStore(), "a job queued in ring mode switched the finisher store");
    Require(finisher.PendingOverlaps(0x800f0, 0x80110), "a pending ring write did not overlap");
    Require(!finisher.PendingOverlaps(0x80100, 0x80110), "a range after a pending ring write overlapped");
    const std::array<Range, 2> ranges{{{0x1, 0x2}, {0x80000, 0x80001}}};
    Require(finisher.PendingOverlapsAny(ranges), "a pending ring write did not overlap a list of ranges");
    Require(finisher.PendingKey(41, 0), "a pending ring key was not found");
    Require(finisher.PendingKey(1, 42), "a pending ring shape was not found");
    Require(!finisher.PendingKey(43, 0), "a key that is not in the ring was found");
    const std::array<Range, 1> lean{{{0x90000, 0x90010}}};
    PublishValue(7, lean, 51);
    Require(finisher.PendingOverlaps(0x90008, 0x90009), "a pending lean draw's write template did not overlap");
    Require(finisher.PendingKey(51, 0), "a pending lean draw's key was not found");
    std::uint64_t value = 0;
    Require(finisher.PendingLabel(0x90000, 4, value) == DrawFinisher::LabelLookup::Unknown, "a lean draw's held write answered as a label value");
    DrawFinisher::Pending label;
    label.unknown = false;
    label.writes = {{0xa0000, 0xa0004}};
    label.labelAddress = 0xa0000;
    label.labelBytes = {std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}};
    finisher.Enqueue([] {}, label);
    Require(finisher.PendingLabel(0xa0000, 4, value) == DrawFinisher::LabelLookup::Value && value == 0x44332211u, "a pending ring label did not answer its value");
    finisher.Enqueue([] {});
    Require(finisher.PendingOverlaps(0x1, 0x2), "a pending ring job with unknown writes did not overlap everything");
    Require(finisher.Issued() > finisher.Front(), "the ring issued nothing ahead of its front");
    release.release();
    finisher.Drain();
    Require(finisher.Front() == finisher.Issued(), "a drained ring left jobs before its front");
    Require(!finisher.PendingOverlaps(0x800f0, 0x80110), "a finished ring job's write still overlapped");
    Require(!finisher.PendingKey(41, 0) && !finisher.PendingKey(51, 0), "a finished ring job's key was still found");
    const std::lock_guard lock(orderMutex);
    order.clear();
}

void OrderOfJobs() {
    auto& finisher = DrawFinisher::Get();
    {
        const std::lock_guard lock(orderMutex);
        order.clear();
    }
    constexpr std::uint32_t Jobs = 300;
    for (std::uint32_t index = 0; index < Jobs; ++index) {
        if (index % 3 == 0) {
            finisher.Enqueue([index] {
                if (index % 7 == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
                Append(index);
            }, Unknown());
        } else {
            PublishValue(index);
        }
    }
    finisher.Drain();
    const std::lock_guard lock(orderMutex);
    Require(order.size() == Jobs, "the ring ran " + std::to_string(order.size()) + " of " + std::to_string(Jobs) + " jobs");
    for (std::uint32_t index = 0; index < Jobs; ++index) Require(order[index] == index, "the ring ran job " + std::to_string(order[index]) + " in turn " + std::to_string(index));
    Require(finisher.Front() == finisher.Issued() && !DrawFinisher::Busy(), "the drained ring is still busy");
}

void FailureRethrown() {
    auto& finisher = DrawFinisher::Get();
    {
        const std::lock_guard lock(orderMutex);
        order.clear();
    }
    finisher.Enqueue([] { throw std::runtime_error("ring job failure"); }, Unknown());
    bool thrown = false;
    try {
        finisher.Drain();
    } catch (const std::runtime_error& error) {
        thrown = std::string(error.what()) == "ring job failure";
    }
    Require(thrown, "a failed ring job was not rethrown by the next drain");
    PublishValue(5);
    finisher.Drain();
    const std::lock_guard lock(orderMutex);
    Require(order.size() == 1 && order[0] == 5, "a job queued after the failure was rethrown did not run");
}

void RoomWait() {
    auto& finisher = DrawFinisher::Get();
    std::binary_semaphore release{0};
    finisher.Enqueue([&] { release.acquire(); }, Unknown());
    {
        const std::lock_guard lock(orderMutex);
        order.clear();
    }
    for (std::uint32_t index = 1; index < 16; ++index) PublishValue(index);
    const auto waitsBefore = LeanFront::Total(LeanFront::Counter::RingRoomWaits);
    std::thread releaser([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        release.release();
    });
    const auto start = std::chrono::steady_clock::now();
    PublishValue(16);
    const auto waited = std::chrono::steady_clock::now() - start;
    releaser.join();
    finisher.Drain();
    Require(LeanFront::Total(LeanFront::Counter::RingRoomWaits) == waitsBefore + 1, "a job queued into a full ring did not wait for room");
    Require(waited >= std::chrono::milliseconds(20), "a job queued into a full ring did not wait for the front job");
    const std::lock_guard lock(orderMutex);
    Require(order.size() == 16, "the ring lost jobs queued behind a room wait");
}

void StoreSwitches() {
    auto& finisher = DrawFinisher::Get();
    finisher.Drain();
    finisher.Adopt(false);
    Require(!finisher.RingStore(), "an idle finisher did not switch back to its job queue");
    {
        const std::lock_guard lock(orderMutex);
        order.clear();
    }
    std::binary_semaphore release{0};
    finisher.Enqueue([&] {
        release.acquire();
        Append(1);
    }, Unknown());
    finisher.Adopt(true);
    Require(!finisher.RingStore(), "a busy finisher switched stores");
    release.release();
    finisher.Drain();
    Require(finisher.Front() == finisher.Issued(), "the job queue's front does not follow its jobs");
    finisher.Adopt(true);
    Require(finisher.RingStore(), "an idle finisher did not switch to its ring");
    PublishValue(2);
    finisher.Drain();
    const std::lock_guard lock(orderMutex);
    Require(order.size() == 2 && order[0] == 1 && order[1] == 2, "jobs across a store switch ran out of order");
}

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0 || !DrawFinisher::Enabled() || !DrawFinisher::Overlap()) return 77;
        auto& finisher = DrawFinisher::Get();
        finisher.Adopt(true);
        Require(finisher.RingStore(), "an idle finisher did not switch to its ring");
        QueriesFromTheRing();
        OrderOfJobs();
        FailureRethrown();
        RoomWait();
        StoreSwitches();
        finisher.Drain();
        finisher.Adopt(false);
        std::cout << "lean finisher ring tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
