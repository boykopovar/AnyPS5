#include "prx/libc/include/DebugNidStops.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "DebugNidStops check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

namespace {

std::uint64_t ParseOrAbort(const char* text) {
    std::uint64_t nid = 0;
    Check(Debug::ParseNid(text, nid));
    return nid;
}

void WaitForStops(const Debug::NidStopRegistry& registry, std::size_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (registry.Size() != expected) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

struct EventLog {
    std::mutex mutex;
    std::vector<std::string> entries;

    void Add(const char* entry) {
        std::lock_guard lock(mutex);
        entries.emplace_back(entry);
    }
};

void CheckUnarmedDoesNotPause() {
    Debug::NidBreakpointTable table;
    Debug::NidStopRegistry registry;
    const std::uint64_t armed = ParseOrAbort("4J2sUJmuHZQ");
    const std::uint64_t other = ParseOrAbort("hcuQgD53UxM");
    const std::uint64_t args[2] = {1, 2};
    Check(table.Add(armed) == Debug::NidBreakpointTable::AddResult::Added);
    Check(registry.Hit(table, other, args, 2) == 0);
    Check(registry.Size() == 0);
    Check(registry.SkippedCount() == 0);
}

void CheckPauseAndResume() {
    Debug::NidBreakpointTable table;
    Debug::NidStopRegistry registry;
    const std::uint64_t nid = ParseOrAbort("4J2sUJmuHZQ");
    Check(table.Add(nid) == Debug::NidBreakpointTable::AddResult::Added);

    EventLog events;
    std::uint64_t returnedId = 0;
    const std::uint64_t args[3] = {0x11, 0x22, 0x33};
    std::thread paused([&] {
        events.Add("before");
        returnedId = registry.Hit(table, nid, args, 3);
        events.Add("after");
    });
    WaitForStops(registry, 1);

    std::thread other([&] {
        events.Add("other-1");
        events.Add("other-2");
    });
    other.join();
    Check(events.entries.size() == 3);

    Debug::NidStopRegistry::Stop listed[2];
    Check(registry.List(listed, 2) == 1);
    Check(listed[0].id != 0);
    Check(listed[0].nid == nid);
    Check(listed[0].argCount == 3);
    Check(listed[0].args[0] == 0x11 && listed[0].args[1] == 0x22 && listed[0].args[2] == 0x33);

    Check(table.IsArmed(nid));
    Check(table.Remove(nid));
    Check(table.Add(nid) == Debug::NidBreakpointTable::AddResult::Added);
    Check(registry.Size() == 1);

    Check(!registry.Continue(0));
    Check(!registry.Continue(listed[0].id + 1000));
    Check(registry.Continue(listed[0].id));
    Check(!registry.Continue(listed[0].id));
    paused.join();

    Check(returnedId == listed[0].id);
    Check(registry.Size() == 0);
    const std::vector<std::string> expected = {"before", "other-1", "other-2", "after"};
    Check(events.entries == expected);
}

void CheckCapacitySkipsWithoutPausing() {
    Debug::NidBreakpointTable table;
    Debug::NidStopRegistry registry;
    const std::uint64_t nid = ParseOrAbort("4J2sUJmuHZQ");
    Check(table.Add(nid) == Debug::NidBreakpointTable::AddResult::Added);

    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < Debug::NidStopRegistry::kCapacity; ++i)
        threads.emplace_back([&] { registry.Hit(table, nid, nullptr, 0); });
    WaitForStops(registry, Debug::NidStopRegistry::kCapacity);

    Check(registry.Hit(table, nid, nullptr, 0) == 0);
    Check(registry.SkippedCount() == 1);
    Check(registry.Size() == Debug::NidStopRegistry::kCapacity);

    std::vector<Debug::NidStopRegistry::Stop> listed(Debug::NidStopRegistry::kCapacity);
    Check(registry.List(listed.data(), listed.size()) == Debug::NidStopRegistry::kCapacity);
    for (const auto& stop : listed)
        Check(registry.Continue(stop.id));
    for (auto& thread : threads)
        thread.join();
    Check(registry.Size() == 0);
}

}

int main() {
    Check(Debug::NidStops().Size() == 0);
    CheckUnarmedDoesNotPause();
    CheckPauseAndResume();
    CheckCapacitySkipsWithoutPausing();
    return 0;
}
