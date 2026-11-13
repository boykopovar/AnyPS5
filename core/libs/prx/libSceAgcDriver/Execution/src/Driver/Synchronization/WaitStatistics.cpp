#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>

namespace AgcDriver::DriverDetail {

PollStats& Driver::pollStats() {
    static thread_local PollStats stats;
    return stats;
}

WaitOutcomes& Driver::waitOutcomes() {
    static thread_local WaitOutcomes outcomes;
    return outcomes;
}

Graphics::Recorder::LateStatistics& Driver::lateCountsSeen() {
    static thread_local Graphics::Recorder::LateStatistics seen{};
    return seen;
}

EpochBumps& Driver::epochBumps() {
    static thread_local EpochBumps bumps;
    return bumps;
}

std::chrono::steady_clock::time_point& Driver::lastEpochBump() {
    static thread_local std::chrono::steady_clock::time_point at{};
    return at;
}

void Driver::bumpEpoch(std::uint64_t EpochBumps::*counter) {
    bumpEpochSince(counter, 0);
}

namespace {

std::array<std::atomic<std::uint64_t>, 64>& queueSinces() {
    static std::array<std::atomic<std::uint64_t>, 64> sinces{};
    return sinces;
}

std::size_t queueSinceSlot(std::uint32_t queue) {
    return queue == 0 ? 0 : queue >= 0x20 && queue < 0x20 + 63 ? queue - 0x20 + 1 : 64;
}

void publishQueuePoint(std::uint32_t queue, std::uint64_t point) {
    const auto slot = queueSinceSlot(queue);
    if (slot >= queueSinces().size()) return;
    auto& published = queueSinces()[slot];
    auto current = published.load(std::memory_order_relaxed);
    while (current < point && !published.compare_exchange_weak(current, point, std::memory_order_release, std::memory_order_relaxed)) {}
}

void publishQueueSince(std::uint32_t queue) {
    publishQueuePoint(queue, GuestMemory::CollectSince());
}

}

void Driver::publishReadPoint() {
    publishQueuePoint(epochQueue(), GuestMemory::SteadyNanoseconds());
}

std::uint32_t& Driver::epochQueue() {
    static thread_local std::uint32_t queue = 0xffffffffu;
    return queue;
}

std::uint64_t Driver::queueEpochSince(std::uint32_t queue) {
    const auto slot = queueSinceSlot(queue);
    return slot < queueSinces().size() ? queueSinces()[slot].load(std::memory_order_acquire) : 0;
}

void Driver::bumpEpochSince(std::uint64_t EpochBumps::*counter, std::uint64_t since) {
    lastEpochBump() = std::chrono::steady_clock::now();
    GuestMemory::ShareCollectWalks(!Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::NoSharedCollectWalks));
    GuestMemory::BumpCollectEpochSince(since);
    if (DrawPipeline::Active()) DrawPipeline::NoteEpochSince(DrawPipeline::EpochToken().fetch_add(1, std::memory_order_relaxed) + 1, GuestMemory::CollectSince());
    publishQueueSince(epochQueue());
    ++(epochBumps().*counter);
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    struct Lags {
        std::uint64_t bumps = 0, now = 0, lagNs = 0;
        std::chrono::steady_clock::time_point reported = std::chrono::steady_clock::now();
    };
    static thread_local Lags lags;
    ++lags.bumps;
    if (since == 0) ++lags.now;
    else lags.lagNs += GuestMemory::SteadyNanoseconds() - GuestMemory::CollectSince();
    if (std::chrono::steady_clock::now() - lags.reported < std::chrono::seconds(10)) return;
    AgcDriver::ProfilePrint_nid_no_patch("[epoch] queue 0x%x (10 s): %llu ordering points, %llu at the bump, the rest %.2f ms earlier on average\n", epochQueue(), static_cast<unsigned long long>(lags.bumps), static_cast<unsigned long long>(lags.now), lags.bumps > lags.now ? static_cast<double>(lags.lagNs) / 1e6 / static_cast<double>(lags.bumps - lags.now) : 0.0);
    lags = Lags{};
}

void Driver::raiseEpochSince(std::uint64_t since) {
    const auto before = GuestMemory::CollectSince();
    GuestMemory::RaiseCollectSince(since);
    publishQueueSince(epochQueue());
    if (GuestMemory::CollectSince() != before && DrawPipeline::Active() && !Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::NoSharedCollectWalks)) DrawPipeline::NoteEpochSince(DrawPipeline::EpochToken().fetch_add(1, std::memory_order_relaxed) + 1, GuestMemory::CollectSince());
}

bool Driver::packetEpoch() {
    static const bool packet = std::getenv("APS5_PACKET_EPOCH") != nullptr;
    return packet;
}

}
