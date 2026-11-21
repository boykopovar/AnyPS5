#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexPlanScratch.hpp"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

struct Probe {
    std::vector<int> numbers;
    std::vector<std::shared_ptr<int>> owners;

    void clear() noexcept {
        numbers.clear();
        owners.clear();
    }
};

struct ProbeTag {};

using ProbeLease = AgcDriver::ExclusiveScratch<Probe, ProbeTag>;

void ReusesCapacityAndStartsEmpty() {
    const int* first = nullptr;
    {
        const ProbeLease lease;
        lease->numbers.reserve(64);
        lease->numbers.push_back(1);
        first = lease->numbers.data();
        Check(ProbeLease::Busy(), "a lease marks the thread's scratch busy");
    }
    Check(!ProbeLease::Busy(), "the end of a lease frees the thread's scratch");
    Check(ProbeLease::Resting().numbers.empty(), "a scratch rests empty");
    const ProbeLease lease;
    Check(lease->numbers.empty() && lease->numbers.capacity() >= 64, "the next lease finds an empty list with the capacity of the last");
    lease->numbers.push_back(2);
    Check(lease->numbers.data() == first, "the next lease reuses the last one's storage");
}

void NestedLeaseGetsItsOwn() {
    const ProbeLease outer;
    outer->numbers = {1, 2, 3};
    Check(!outer.Nested(), "the first lease on a thread uses the thread's scratch");
    {
        const ProbeLease inner;
        Check(inner.Nested(), "a lease taken while another holds the scratch is nested");
        Check(&*inner != &*outer, "a nested lease gets a scratch of its own");
        Check(inner->numbers.empty(), "a nested lease starts empty");
        inner->numbers.assign(100, 7);
        Check(outer->numbers.size() == 3 && outer->numbers[0] == 1 && outer->numbers[2] == 3, "a nested lease leaves the outer one's lists alone");
        {
            const ProbeLease deepest;
            Check(deepest.Nested() && &*deepest != &*inner && &*deepest != &*outer && deepest->numbers.empty(), "a third lease gets another scratch");
            deepest->numbers.push_back(9);
        }
        Check(inner->numbers.size() == 100, "the nested lease keeps its lists while a deeper one ends");
    }
    Check(ProbeLease::Busy(), "the outer lease still holds the thread's scratch after a nested one ends");
    Check(outer->numbers.size() == 3 && outer->numbers[1] == 2, "the outer lease's lists survive nested leases");
}

void ExceptionReleasesTheScratch() {
    std::weak_ptr<int> watched;
    bool caught = false;
    try {
        const ProbeLease lease;
        auto owner = std::make_shared<int>(5);
        watched = owner;
        lease->owners.push_back(std::move(owner));
        lease->numbers.push_back(1);
        throw std::runtime_error("unwinding");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Check(caught, "the test throws");
    Check(!ProbeLease::Busy(), "unwinding frees the thread's scratch");
    Check(ProbeLease::Resting().owners.empty() && ProbeLease::Resting().numbers.empty(), "unwinding empties the scratch");
    Check(watched.expired(), "unwinding releases what the scratch held");
}

void ThreadsHaveTheirOwn() {
    const ProbeLease mine;
    mine->numbers.push_back(1);
    const Probe* other = nullptr;
    bool otherNested = true;
    bool otherEmpty = false;
    std::thread([&] {
        const ProbeLease lease;
        other = &*lease;
        otherNested = lease.Nested();
        otherEmpty = lease->numbers.empty();
    }).join();
    Check(other != nullptr && other != &*mine, "another thread has a scratch of its own");
    Check(!otherNested && otherEmpty, "another thread's first lease is neither nested nor filled");
    Check(mine->numbers.size() == 1, "another thread's lease leaves this one's alone");
}

void VertexPlanScratchEmptiesEveryList() {
    using namespace AgcDriver::Graphics;
    std::weak_ptr<int> watched;
    {
        const VertexPlanLease lease;
        auto owner = std::make_shared<int>(1);
        watched = owner;
        lease->fetches.push_back({1, 2, 3, 4, 5});
        lease->fetchValid.push_back(1);
        lease->fetchOf.push_back(1);
        lease->zeroed.push_back(std::shared_ptr<Buffer>(owner, reinterpret_cast<Buffer*>(owner.get())));
        lease->isSolo.push_back(true);
        lease->plannedFetches.push_back({1, 2, 3, 4, 5});
        lease->plannedOf.push_back(1);
        lease->rangeHandles.push_back(VK_NULL_HANDLE);
        lease->rangeOffsets.push_back(8);
        Check(!lease.Nested(), "the vertex plan lease uses the thread's scratch");
    }
    const auto& resting = VertexPlanLease::Resting();
    Check(resting.fetches.empty() && resting.fetchValid.empty() && resting.fetchOf.empty() && resting.zeroed.empty() && resting.isSolo.empty() && resting.plannedFetches.empty() && resting.plannedOf.empty() && resting.rangeHandles.empty() && resting.rangeOffsets.empty(), "a vertex plan scratch rests empty in every list");
    Check(watched.expired(), "a vertex plan scratch does not keep a zero buffer past its draw");
    Check(resting.fetches.capacity() >= 1 && resting.rangeOffsets.capacity() >= 1, "a vertex plan scratch keeps its capacity");
}

}

int main() {
    ReusesCapacityAndStartsEmpty();
    NestedLeaseGetsItsOwn();
    ExceptionReleasesTheScratch();
    ThreadsHaveTheirOwn();
    VertexPlanScratchEmptiesEveryList();
    if (failures != 0) {
        std::fprintf(stderr, "%d exclusive scratch checks failed\n", failures);
        return 1;
    }
    std::printf("Exclusive scratch tests passed\n");
    return 0;
}
