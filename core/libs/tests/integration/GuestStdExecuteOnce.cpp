#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <thread>

using Callback = int (APS5_VABI *)(void*, void*, void**);
extern "C" int APS5_VABI std_execute_once_nid_postfix(int*, Callback, void*);
static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> calls{0}, arrived{0};
static int control = 0, nested = 0, failing = 0, throwing = 0;
static int published = 0, nestedValue = 0, failures = 0, throws = 0;
static int APS5_VABI Nested(void*, void* arg, void**) {
    nestedValue = *static_cast<int*>(arg);
    return 1;
}
static int APS5_VABI Initialize(void* first, void* arg, void** third) {
    Require(first == nullptr && third == nullptr && arg != nullptr);
    ++calls;
    while (arrived.load() != 16) std::this_thread::yield();
    int value = 17;
    Require(std_execute_once_nid_postfix(&nested, Nested, &value) == 1);
    published = 42;
    return 5;
}
static int APS5_VABI Fail(void*, void*, void**) {
    ++failures;
    return 0;
}
static int APS5_VABI Throw(void*, void*, void**) {
    if (++throws == 1) throw 7;
    return 1;
}
static int APS5_VABI Unreachable(void*, void*, void**) {
    std::abort();
}

int main() {
    int token = 0;
    std::array<std::thread, 16> workers;
    for (auto& worker : workers) worker = std::thread([&token] {
        ++arrived;
        Require(std_execute_once_nid_postfix(&control, Initialize, &token) == 1);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && control == 1 && nested == 1);
    Require(std_execute_once_nid_postfix(&control, Unreachable, nullptr) == 1 && calls == 1);

    Require(std_execute_once_nid_postfix(&failing, Fail, nullptr) == 0 && failing == 0 && failures == 1);
    Require(std_execute_once_nid_postfix(&failing, Fail, nullptr) == 0 && failing == 0 && failures == 2);

    try {
        std_execute_once_nid_postfix(&throwing, Throw, nullptr);
        std::abort();
    } catch (int value) {
        Require(value == 7 && throwing == 0);
    }
    Require(std_execute_once_nid_postfix(&throwing, Throw, nullptr) == 1 && throwing == 1 && throws == 2);

    int preset = -3;
    Require(std_execute_once_nid_postfix(&preset, Unreachable, nullptr) == 1 && preset == -3);
}
