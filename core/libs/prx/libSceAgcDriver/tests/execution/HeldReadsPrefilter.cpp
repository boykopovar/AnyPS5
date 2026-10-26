#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using Clock = std::chrono::steady_clock;

constexpr std::uint64_t HeldAddress = 0x10000000ull;
constexpr std::size_t HeldBytes = 0x1000;
constexpr std::uint64_t OtherAddress = 0x20000000ull;
constexpr auto CommitDeadline = std::chrono::seconds(10);
constexpr auto StartDeadline = std::chrono::seconds(10);
constexpr auto CleanupBound = std::chrono::seconds(5);

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct Parked {
    std::atomic<bool> release{false};
    std::atomic<bool> running{false};
};

class ParkedCommit {
public:
    ParkedCommit(std::vector<DrawPipeline::Range> reads, bool announce, bool releaseReads) : state(std::make_shared<Parked>()) {
        auto parked = state;
        DrawPipeline::Queue0().Enqueue([parked, announce, releaseReads] {
            if (releaseReads) DrawPipeline::Queue0().ReleaseRunningReads();
            if (announce) parked->running = true;
            const auto deadline = Clock::now() + CommitDeadline;
            while (!parked->release.load() && Clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }, {}, 0, {}, std::move(reads), true);
    }
    ParkedCommit(ParkedCommit&& other) noexcept : state(std::move(other.state)), drained(std::exchange(other.drained, true)) {}
    ParkedCommit(const ParkedCommit&) = delete;
    ParkedCommit& operator=(const ParkedCommit&) = delete;
    ParkedCommit& operator=(ParkedCommit&&) = delete;
    ~ParkedCommit() { Release(); }

    void AwaitStart(Clock::duration deadline) {
        const auto limit = Clock::now() + deadline;
        while (!state->running.load()) {
            Require(Clock::now() < limit, "the parked commit never started");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void Release() {
        if (drained) return;
        drained = true;
        state->release = true;
        DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    }

private:
    std::shared_ptr<Parked> state;
    bool drained = false;
};

ParkedCommit Park(std::vector<DrawPipeline::Range> reads, bool announce = true, bool releaseReads = false, Clock::duration startDeadline = StartDeadline) {
    ParkedCommit commit(std::move(reads), announce, releaseReads);
    commit.AwaitStart(startDeadline);
    return commit;
}

struct Counts {
    std::uint64_t queries;
    std::uint64_t walks;
};

Counts Count() {
    return {DrawPipeline::HeldReadQueries(), DrawPipeline::HeldReadWalks()};
}

std::vector<DrawPipeline::Range> HeldRange() {
    return {{HeldAddress, HeldAddress + HeldBytes}};
}

void RequireIdle(const std::string& what) {
    Require(!DrawPipeline::Queue0().Busy(), what + " left the draw pipeline busy");
    Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), what + " left a held read behind");
}

void QueueWithoutReadsSkipsTheWalk() {
    const auto commit = Park({});
    const auto before = Count();
    Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), "a queue holding no read ranges reported a held read");
    const auto after = Count();
    Require(after.queries == before.queries + 1, "a query against a queue holding no read ranges was not counted");
    Require(after.walks == before.walks, "a query against a queue holding no read ranges walked the queue");
}

void QueuedRangeIsWalked() {
    auto commit = Park(HeldRange());
    const auto before = Count();
    Require(DrawPipeline::HeldReads(HeldAddress + 16, 16), "a read range held by the queued item was not reported");
    Require(!DrawPipeline::HeldReads(OtherAddress, HeldBytes), "a range no queued item reads was reported as held");
    const auto after = Count();
    Require(after.queries == before.queries + 2 && after.walks == before.walks + 2, "queries against a queue holding read ranges did not walk it");
    commit.Release();
    const auto drained = Count();
    Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), "a drained queue still reported a held read");
    Require(Count().walks == drained.walks, "a query after the item holding the range retired still walked the queue");
}

void ReleasedRunningReadsSkipTheWalk() {
    const auto commit = Park(HeldRange(), true, true);
    const auto before = Count();
    Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), "a range the running item released was still reported as held");
    Require(Count().walks == before.walks, "a query after the running item released its reads still walked the queue");
}

void PlantedFailureCleansUp() {
    const auto start = Clock::now();
    bool planted = false;
    try {
        const auto commit = Park(HeldRange());
        Require(false, "planted");
    } catch (const std::runtime_error& error) {
        planted = std::string(error.what()) == "planted";
    }
    Require(planted, "the planted failure did not surface");
    Require(Clock::now() - start < CleanupBound, "cleaning up after the planted failure took longer than the bound");
    RequireIdle("the planted failure");
}

void StartupTimeoutCleansUp() {
    const auto start = Clock::now();
    bool timedOut = false;
    try {
        const auto commit = Park(HeldRange(), false, false, std::chrono::milliseconds(200));
    } catch (const std::runtime_error& error) {
        timedOut = std::string(error.what()) == "the parked commit never started";
    }
    Require(timedOut, "the commit that never announced itself did not time out");
    Require(Clock::now() - start < CleanupBound, "cleaning up after the startup timeout took longer than the bound");
    RequireIdle("the startup timeout");
}

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0) return 77;
        QueueWithoutReadsSkipsTheWalk();
        QueuedRangeIsWalked();
        ReleasedRunningReadsSkipTheWalk();
        PlantedFailureCleansUp();
        StartupTimeoutCleansUp();
        std::puts("held read prefilter tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
