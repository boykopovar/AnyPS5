#include "ResolverState.hpp"

#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Gate {
public:
    void Enter() {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
    }
    void Wait() {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, std::chrono::seconds(2), [&] { return entered; }), "lookup did not start");
    }
    void Release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
private:
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
};

template <typename TAction>
int BlockedLookup(NetResolver::State& state, NetResolver::LookupKind kind, int& output, TAction action, int hostResult = 0) {
    Gate gate;
    int result = 0;
    std::exception_ptr exception;
    std::thread worker([&] {
        try {
            result = state.Run(kind, [&] { gate.Enter(); return hostResult; }, [&] { output = 77; return 0; });
        } catch (...) {
            exception = std::current_exception();
        }
    });
    try {
        gate.Wait();
        action();
    } catch (...) {
        gate.Release();
        worker.join();
        throw;
    }
    gate.Release();
    worker.join();
    if (exception) std::rethrow_exception(exception);
    return result;
}

void CheckActiveAbort(NetResolver::LookupKind kind) {
    NetResolver::State state;
    int output = 19;
    const int result = BlockedLookup(state, kind, output, [&] {
        Require(state.Run(kind, [] { return 0; }, [] { return 0; }) == NetResolver::Busy, "concurrent lookup was accepted");
        Require(state.Abort(3) == 0, "active abort failed");
        int status = 0;
        Require(state.GetError(status) == 0 && status == NetResolver::Interrupted, "abort status was not published");
    });
    Require(result == NetResolver::Interrupted && output == 19, "aborted result touched output");
    int status = 0;
    Require(state.GetError(status) == 0 && status == NetResolver::Interrupted, "late host result cleared abort status");
    Require(state.Run(kind, [] { return 0; }, [] { return 0; }) == 0, "active abort incorrectly preserved flags");
}

void CheckUnknownFlagsDuringLookup() {
    NetResolver::State state;
    int output = 19;
    const int result = BlockedLookup(state, NetResolver::LookupKind::Ntoa, output, [&] {
        bool threw = false;
        try { state.Abort(5); } catch (const std::runtime_error&) { threw = true; }
        Require(threw, "unknown flags were accepted during lookup");
        Require(state.Run(NetResolver::LookupKind::Ntoa, [] { return 0; }, [] { return 0; }) == NetResolver::Busy, "unknown flags cancelled active lookup");
    });
    Require(result == 0 && output == 77, "unknown flags prevented normal completion");
}

void CheckPreservedAborts() {
    NetResolver::State state;
    int calls = 0;
    const auto work = [&] { ++calls; return 0; };
    Require(state.Abort(1) == 0 && state.Abort(2) == 0, "preserved abort failed");
    Require(state.Run(NetResolver::LookupKind::Ntoa, work, [] { return 0; }) == NetResolver::Interrupted, "Ntoa abort missing");
    Require(state.Run(NetResolver::LookupKind::Ntoa, work, [] { return 0; }) == 0, "Ntoa abort was not consumed");
    Require(state.Run(NetResolver::LookupKind::Aton, work, [] { return 0; }) == NetResolver::Interrupted, "Aton abort missing");
    Require(state.Run(NetResolver::LookupKind::Aton, work, [] { return 0; }) == 0 && calls == 2, "preserved abort called host lookup");
    Require(state.Abort(0) == 0, "idle abort failed");
    Require(state.Run(NetResolver::LookupKind::Ntoa, work, [] { return 0; }) == 0, "idle zero abort cancelled next lookup");
    bool threw = false;
    try { state.Abort(4); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "unknown abort flags were accepted");
}

void CheckReuseBeforeOldCompletion() {
    NetResolver::State state;
    int oldOutput = 19;
    int newOutput = 23;
    const int newError = static_cast<int>(0x804101e1u);
    const int result = BlockedLookup(state, NetResolver::LookupKind::Ntoa, oldOutput, [&] {
        Require(state.Abort(0) == 0, "abort before reuse failed");
        Require(state.Run(NetResolver::LookupKind::Aton, [] { return 0; }, [&] { newOutput = 31; return 0; }) == 0, "resolver reuse failed");
        Require(state.Run(NetResolver::LookupKind::Ntoa, [&] { return newError; }, [] { return 0; }) == newError, "new error was lost");
    });
    int status = 0;
    Require(result == NetResolver::Interrupted && oldOutput == 19 && newOutput == 31, "old lookup overwrote reused output");
    Require(state.GetError(status) == 0 && status == newError, "old lookup overwrote new status");
}

void CheckDestroy(NetResolver::LookupKind kind) {
    NetResolver::State state;
    int output = 19;
    const int result = BlockedLookup(state, kind, output, [&] { state.Destroy(); });
    int status = 23;
    Require(result == NetResolver::Invalid && output == 19, "destroyed lookup touched output");
    Require(state.GetError(status) == NetResolver::Invalid && status == 23, "destroyed status touched output");
    Require(state.Abort(0) == NetResolver::Invalid, "destroyed resolver accepted abort");
    Require(state.Run(kind, [] { return 0; }, [] { return 0; }) == NetResolver::Invalid, "destroyed resolver accepted lookup");
}

void CheckOldCompletionWhileReusedLookupRuns() {
    NetResolver::State state;
    Gate oldGate;
    Gate newGate;
    int oldResult = 0;
    int newResult = 0;
    int output = 19;
    std::thread oldWorker([&] {
        oldResult = state.Run(NetResolver::LookupKind::Ntoa, [&] { oldGate.Enter(); return 0; }, [&] { output = 77; return 0; });
    });
    std::thread newWorker;
    bool stillBusy = false;
    try {
        oldGate.Wait();
        Require(state.Abort(0) == 0, "overlapping abort failed");
        newWorker = std::thread([&] {
            newResult = state.Run(NetResolver::LookupKind::Aton, [&] { newGate.Enter(); return 0; }, [&] { output = 31; return 0; });
        });
        newGate.Wait();
        oldGate.Release();
        oldWorker.join();
        stillBusy = state.Run(NetResolver::LookupKind::Ntoa, [] { return 0; }, [] { return 0; }) == NetResolver::Busy;
    } catch (...) {
        oldGate.Release();
        newGate.Release();
        if (oldWorker.joinable()) oldWorker.join();
        if (newWorker.joinable()) newWorker.join();
        throw;
    }
    newGate.Release();
    newWorker.join();
    Require(stillBusy && oldResult == NetResolver::Interrupted && newResult == 0 && output == 31, "old completion cleared the new active lookup");
}

void CheckFailureAndException() {
    NetResolver::State state;
    int output = 19;
    const int result = BlockedLookup(state, NetResolver::LookupKind::Aton, output, [&] { state.Abort(0); }, static_cast<int>(0x804101e1u));
    Require(result == NetResolver::Interrupted && output == 19, "host error overrode abort");
    bool threw = false;
    try {
        state.Run(NetResolver::LookupKind::Ntoa, []() -> int { throw std::runtime_error("host failure"); }, [] { return 0; });
    } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "host exception was swallowed");
    Require(state.Run(NetResolver::LookupKind::Ntoa, [] { return 0; }, [] { return 0; }) == 0, "exception left resolver busy");
}

}

int main() {
    try {
        CheckPreservedAborts();
        CheckActiveAbort(NetResolver::LookupKind::Ntoa);
        CheckActiveAbort(NetResolver::LookupKind::Aton);
        CheckUnknownFlagsDuringLookup();
        CheckReuseBeforeOldCompletion();
        CheckOldCompletionWhileReusedLookupRuns();
        CheckDestroy(NetResolver::LookupKind::Ntoa);
        CheckDestroy(NetResolver::LookupKind::Aton);
        CheckFailureAndException();
        std::cout << "resolver cancellation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
