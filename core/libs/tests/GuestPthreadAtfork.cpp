#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <thread>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> calls{0};

static void APS5_VABI Prepare() { ++calls; }
static void APS5_VABI Parent() { ++calls; }
static void APS5_VABI Child() { ++calls; }

static bool Same(const PthreadAtforkHandlers& entry, PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    return entry.prepare == prepare && entry.parent == parent && entry.child == child;
}

int main() {
    Require(PthreadAtforkHandlers_nid_no_patch().empty());
    Require(pthread_atfork_nid_postfix(Prepare, Parent, Child) == 0);
    Require(pthread_atfork_nid_postfix(nullptr, Parent, nullptr) == 0);
    Require(pthread_atfork_nid_postfix(nullptr, nullptr, nullptr) == 0);
    Require(pthread_atfork_nid_postfix(Prepare, Parent, Child) == 0);
    auto handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == 4);
    Require(Same(handlers[0], Prepare, Parent, Child));
    Require(Same(handlers[1], nullptr, Parent, nullptr));
    Require(Same(handlers[2], nullptr, nullptr, nullptr));
    Require(Same(handlers[3], Prepare, Parent, Child));

    constexpr std::size_t threadCount = 4;
    constexpr std::size_t perThread = 64;
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < threadCount; ++index) {
        threads.emplace_back([] {
            for (std::size_t count = 0; count < perThread; ++count) Require(pthread_atfork_nid_postfix(nullptr, nullptr, Child) == 0);
        });
    }
    for (auto& thread : threads) thread.join();
    handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == 4 + threadCount * perThread);
    Require(Same(handlers[3], Prepare, Parent, Child));
    for (std::size_t index = 4; index < handlers.size(); ++index) Require(Same(handlers[index], nullptr, nullptr, Child));
    Require(calls == 0);
    return 0;
}
