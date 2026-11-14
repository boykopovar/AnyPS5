#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr unsigned Workers = 16;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Call-once-ex check failed at line %u\n", location.line());
    std::abort();
}

template<class Exception, class Function>
void RequireThrow(Function function) {
    bool thrown = false;
    try {
        function();
    } catch (const Exception&) {
        thrown = true;
    }
    Require(thrown);
}

void APS5_VABI Increment(void* argument) {
    ++*static_cast<int*>(argument);
}

int nullCalls = 0;

void APS5_VABI NullArgument(void* argument) {
    Require(argument == nullptr);
    ++nullCalls;
}

struct CallbackFailure {
    int marker;
};

using VirtualCallback = void (APS5_VABI *)(void*);

struct CallTarget {
    const VirtualCallback* table;
    void* observed = nullptr;
    int firstCalls = 0;
    int secondCalls = 0;
    bool throwing = false;
};

void APS5_VABI FirstSlot(void* argument) {
    auto& target = *static_cast<CallTarget*>(argument);
    target.observed = argument;
    ++target.firstCalls;
    if (target.throwing) throw CallbackFailure{67};
}

void APS5_VABI SecondSlot(void* argument) {
    ++static_cast<CallTarget*>(argument)->secondCalls;
}

void CheckVirtualDispatch() {
    const std::array<VirtualCallback, 2> table{FirstSlot, SecondSlot};
    CallTarget target{table.data()};
    _Do_call_nid_postfix(&target);
    _Do_call_nid_postfix(&target);
    Require(target.observed == &target && target.firstCalls == 2 && target.secondCalls == 0);
    target.throwing = true;
    try {
        _Do_call_nid_postfix(&target);
        Require(false);
    } catch (const CallbackFailure& failure) {
        Require(failure.marker == 67);
    }
    Require(target.observed == &target && target.firstCalls == 3 && target.secondCalls == 0);
}

void CheckVirtualCallOnce() {
    const std::array<VirtualCallback, 2> table{FirstSlot, SecondSlot};
    CallTarget target{table.data()};
    target.throwing = true;
    std::int32_t flag = 0;
    try {
        _Call_onceEx_nid_postfix(&flag, _Do_call_nid_postfix, &target);
        Require(false);
    } catch (const CallbackFailure& failure) {
        Require(failure.marker == 67);
    }
    Require(flag == 0 && target.observed == &target && target.firstCalls == 1 && target.secondCalls == 0);
    target.throwing = false;
    std::thread retry([&] { _Call_onceEx_nid_postfix(&flag, _Do_call_nid_postfix, &target); });
    retry.join();
    Require(flag == 1 && target.observed == &target && target.firstCalls == 2 && target.secondCalls == 0);
    _Call_onceEx_nid_postfix(&flag, _Do_call_nid_postfix, &target);
    Require(flag == 1 && target.firstCalls == 2 && target.secondCalls == 0);
}

struct Mutation {
    std::int32_t flag = 0;
    int calls = 0;
    bool throwing = false;
};

void APS5_VABI MutateFlag(void* argument) {
    auto& state = *static_cast<Mutation*>(argument);
    Require(state.flag == 0);
    ++state.calls;
    state.flag = -17;
    if (state.throwing) throw CallbackFailure{93};
}

void CheckFlagsAndArguments() {
    int calls = 0;
    for (std::int32_t flag : {1, 2, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        const auto original = flag;
        _Call_onceEx_nid_postfix(&flag, Increment, &calls);
        _Call_onceEx_nid_postfix(&flag, nullptr, nullptr);
        Require(flag == original && calls == 0);
    }
    std::int32_t flag = 0;
    _Call_onceEx_nid_postfix(&flag, Increment, &calls);
    Require(flag == 1 && calls == 1);
    _Call_onceEx_nid_postfix(&flag, Increment, &calls);
    Require(flag == 1 && calls == 1);
    flag = 0;
    _Call_onceEx_nid_postfix(&flag, NullArgument, nullptr);
    Require(flag == 1 && nullCalls == 1);
    flag = 0;
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(&flag, nullptr, nullptr); });
    Require(flag == 0);
    Mutation state;
    _Call_onceEx_nid_postfix(&state.flag, MutateFlag, &state);
    Require(state.flag == 1 && state.calls == 1);
    state.flag = 0;
    state.throwing = true;
    try {
        _Call_onceEx_nid_postfix(&state.flag, MutateFlag, &state);
        Require(false);
    } catch (const CallbackFailure& failure) {
        Require(failure.marker == 93);
    }
    Require(state.flag == -17 && state.calls == 2);
    _Call_onceEx_nid_postfix(&state.flag, nullptr, nullptr);
    Require(state.flag == -17 && state.calls == 2);
}

struct ConcurrentState {
    std::int32_t flag = 0;
    std::atomic<unsigned> arrived{0};
    std::atomic<unsigned> attempts{0};
    std::atomic<unsigned> exceptions{0};
    std::array<int, 8> published{};
    bool throwing = false;
};

void APS5_VABI Publish(void* argument) {
    auto& state = *static_cast<ConcurrentState*>(argument);
    const auto attempt = state.attempts.fetch_add(1) + 1;
    Require(attempt <= (state.throwing ? 2u : 1u));
    Require(state.flag == 0);
    while (state.arrived.load() != Workers) std::this_thread::yield();
    if (state.throwing && attempt == 1) throw CallbackFailure{41};
    for (unsigned index = 0; index < state.published.size(); ++index) state.published[index] = 100 + static_cast<int>(index);
}

void CheckConcurrentPublication(bool throwing) {
    ConcurrentState state;
    state.throwing = throwing;
    std::barrier start(static_cast<std::ptrdiff_t>(Workers));
    std::array<std::thread, Workers> workers;
    for (auto& worker : workers) {
        worker = std::thread([&] {
            start.arrive_and_wait();
            state.arrived.fetch_add(1);
            try {
                _Call_onceEx_nid_postfix(&state.flag, Publish, &state);
                for (unsigned index = 0; index < state.published.size(); ++index) Require(state.published[index] == 100 + static_cast<int>(index));
            } catch (const CallbackFailure& failure) {
                Require(throwing && failure.marker == 41);
                state.exceptions.fetch_add(1);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(state.flag == 1 && state.attempts == (throwing ? 2u : 1u));
    Require(state.exceptions == (throwing ? 1u : 0u));
}

struct NestedState {
    std::int32_t outer = 0;
    std::int32_t inner = 0;
    int innerCalls = 0;
    int outerCalls = 0;
};

void APS5_VABI InitializeInner(void* argument) {
    auto& state = *static_cast<NestedState*>(argument);
    Require(state.outer == 0 && state.inner == 0);
    ++state.innerCalls;
}

void APS5_VABI InitializeOuter(void* argument) {
    auto& state = *static_cast<NestedState*>(argument);
    _Call_onceEx_nid_postfix(&state.inner, InitializeInner, &state);
    Require(state.outer == 0 && state.inner == 1 && state.innerCalls == 1);
    _Call_onceEx_nid_postfix(&state.inner, nullptr, nullptr);
    ++state.outerCalls;
}

void CheckRecursiveInitialization() {
    NestedState state;
    _Call_onceEx_nid_postfix(&state.outer, InitializeOuter, &state);
    Require(state.outer == 1 && state.inner == 1 && state.outerCalls == 1 && state.innerCalls == 1);
}

struct SerializationState {
    std::atomic<unsigned> active{0};
    std::atomic<unsigned> calls{0};
    std::atomic<bool> overlap{false};
};

void APS5_VABI SerializedCallback(void* argument) {
    auto& state = *static_cast<SerializationState*>(argument);
    if (state.active.fetch_add(1) != 0) state.overlap.store(true);
    for (unsigned index = 0; index < 64; ++index) std::this_thread::yield();
    state.calls.fetch_add(1);
    state.active.fetch_sub(1);
}

void CheckDifferentFlags() {
    for (unsigned round = 0; round < 8; ++round) {
        SerializationState state;
        std::array<std::int32_t, Workers> flags{};
        std::barrier start(static_cast<std::ptrdiff_t>(Workers));
        std::array<std::thread, Workers> workers;
        for (unsigned index = 0; index < Workers; ++index) {
            workers[index] = std::thread([&, index] {
                start.arrive_and_wait();
                _Call_onceEx_nid_postfix(&flags[index], SerializedCallback, &state);
            });
        }
        for (auto& worker : workers) worker.join();
        Require(!state.overlap && state.active == 0 && state.calls == Workers);
        for (const auto flag : flags) Require(flag == 1);
    }
}

class FlagPage {
public:
    FlagPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous;
        Require(VirtualProtect(memory + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + size, size, PROT_NONE) == 0);
#endif
    }

    FlagPage(const FlagPage&) = delete;
    FlagPage& operator=(const FlagPage&) = delete;

    ~FlagPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size * 2);
#endif
    }

    std::int32_t* Flag() { return reinterpret_cast<std::int32_t*>(memory); }
    std::int32_t* Unreadable() { return reinterpret_cast<std::int32_t*>(memory + size); }

    void SetWritable(bool writable) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory, size, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, size, writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0);
#endif
    }

private:
    char* memory;
    std::size_t size;
};

void CheckVirtualPointers() {
    FlagPage page;
    RequireThrow<std::invalid_argument>([] { _Do_call_nid_postfix(nullptr); });
    RequireThrow<std::invalid_argument>([&] { _Do_call_nid_postfix(page.Unreadable()); });
    CallTarget target{nullptr};
    RequireThrow<std::invalid_argument>([&] { _Do_call_nid_postfix(&target); });
    target.table = reinterpret_cast<const VirtualCallback*>(page.Unreadable());
    RequireThrow<std::invalid_argument>([&] { _Do_call_nid_postfix(&target); });
    const std::array<VirtualCallback, 2> emptyTable{nullptr, SecondSlot};
    target.table = emptyTable.data();
    RequireThrow<std::invalid_argument>([&] { _Do_call_nid_postfix(&target); });
    std::int32_t flag = 0;
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(&flag, _Do_call_nid_postfix, &target); });
    Require(flag == 0 && target.observed == nullptr && target.firstCalls == 0 && target.secondCalls == 0);
    const std::array<VirtualCallback, 2> table{FirstSlot, SecondSlot};
    target.table = table.data();
    _Call_onceEx_nid_postfix(&flag, _Do_call_nid_postfix, &target);
    Require(flag == 1 && target.observed == &target && target.firstCalls == 1 && target.secondCalls == 0);
}

void CheckPointers() {
    int calls = 0;
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(nullptr, Increment, &calls); });
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(reinterpret_cast<std::int32_t*>(0x10), Increment, &calls); });
    FlagPage page;
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(page.Unreadable(), Increment, &calls); });
    Require(calls == 0);
    *page.Flag() = -3;
    page.SetWritable(false);
    _Call_onceEx_nid_postfix(page.Flag(), nullptr, nullptr);
    Require(*page.Flag() == -3);
    page.SetWritable(true);
    *page.Flag() = 0;
    page.SetWritable(false);
    RequireThrow<std::invalid_argument>([&] { _Call_onceEx_nid_postfix(page.Flag(), Increment, &calls); });
    Require(calls == 1 && *page.Flag() == 0);
    page.SetWritable(true);
    std::thread retry([&] { _Call_onceEx_nid_postfix(page.Flag(), Increment, &calls); });
    retry.join();
    Require(calls == 2 && *page.Flag() == 1);
}

}

int main() {
    CheckVirtualDispatch();
    CheckVirtualCallOnce();
    CheckVirtualPointers();
    CheckFlagsAndArguments();
    CheckConcurrentPublication(false);
    CheckConcurrentPublication(true);
    CheckRecursiveInitialization();
    CheckDifferentFlags();
    CheckPointers();
}
