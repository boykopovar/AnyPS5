#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

using Callback = void (APS5_VABI *)(void*);

extern "C" void APS5_VABI _ZNSt3__111__call_onceERVmPvPFvS2_E_nid_postfix(
    std::uint64_t* flag, void* argument, Callback callback);

static void Require(bool condition) { if (!condition) std::abort(); }

static std::atomic<int> arrived{0};
static std::atomic<int> calls{0};
static std::uint64_t nestedFlag = 0;
static int nestedValue = 0;
static int published = 0;
static int throws = 0;

static void CallOnce(std::uint64_t* flag, void* argument, Callback callback) {
    _ZNSt3__111__call_onceERVmPvPFvS2_E_nid_postfix(flag, argument, callback);
}

static void APS5_VABI Nested(void* argument) {
    nestedValue = *static_cast<int*>(argument);
}

static void APS5_VABI Initialize(void* argument) {
    ++calls;
    while (arrived.load() != 8) std::this_thread::yield();
    CallOnce(&nestedFlag, argument, Nested);
    published = 42;
}

static void APS5_VABI ThrowThenSucceed(void*) {
    if (++throws == 1) throw 7;
}

static void APS5_VABI Unreachable(void*) {
    std::abort();
}

int main() {
    std::uint64_t flag = 0;
    int token = 17;
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        ++arrived;
        CallOnce(&flag, &token, Initialize);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && flag == UINT64_MAX && nestedFlag == UINT64_MAX);
    CallOnce(&flag, nullptr, Unreachable);
    Require(calls == 1);

    std::uint64_t throwingFlag = 0;
    try { CallOnce(&throwingFlag, nullptr, ThrowThenSucceed); std::abort(); }
    catch (int value) { Require(value == 7 && throwingFlag == 0); }
    CallOnce(&throwingFlag, nullptr, ThrowThenSucceed);
    Require(throws == 2 && throwingFlag == UINT64_MAX);

    std::uint64_t alreadyComplete = UINT64_MAX;
    CallOnce(&alreadyComplete, nullptr, nullptr);
    std::uint64_t unknownState = 5;
    CallOnce(&unknownState, nullptr, Unreachable);
    Require(unknownState == 5);

    errno = ESRCH;
    try { CallOnce(nullptr, nullptr, Unreachable); std::abort(); }
    catch (const std::invalid_argument&) { Require(errno == ESRCH); }
    try { CallOnce(reinterpret_cast<std::uint64_t*>(1), nullptr, Unreachable); std::abort(); }
    catch (const std::invalid_argument&) { Require(errno == ESRCH); }
    std::uint64_t nullCallback = 0;
    try { CallOnce(&nullCallback, nullptr, nullptr); std::abort(); }
    catch (const std::invalid_argument&) { Require(nullCallback == 0); }
}
