#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

struct GuestOnce { std::int32_t state; void* mutex; };
static_assert(sizeof(GuestOnce) == 16 && offsetof(GuestOnce, mutex) == 8);
using Initializer = void (APS5_VABI *)();
extern "C" int APS5_VABI pthread_once_nid_postfix(GuestOnce*, Initializer);
static void Require(bool value) { if (!value) std::abort(); }
static GuestOnce control{}, nested{}, retry{};
static std::atomic<int> calls{0}, arrived{0};
static int published = 0, nestedValue = 0, attempts = 0;
static void APS5_VABI Nested() { nestedValue = 17; }
static void APS5_VABI Initialize() {
    ++calls;
    while (arrived.load() != 16) std::this_thread::yield();
    Require(pthread_once_nid_postfix(&nested, Nested) == 0);
    published = 42;
}
static void APS5_VABI Retry() {
    if (++attempts == 1) throw 7;
}
int main() {
    std::array<std::thread, 16> workers;
    for (auto& worker : workers) worker = std::thread([] {
        ++arrived;
        Require(pthread_once_nid_postfix(&control, Initialize) == 0);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && control.state == 1 && control.mutex == nullptr);
    Require(pthread_once_nid_postfix(&control, Initialize) == 0 && calls == 1);
    try { pthread_once_nid_postfix(&retry, Retry); std::abort(); }
    catch (int value) { Require(value == 7 && retry.state == 0); }
    Require(pthread_once_nid_postfix(&retry, Retry) == 0 && attempts == 2);
    Require(pthread_once_nid_postfix(nullptr, Initialize) == 22);
    Require(pthread_once_nid_postfix(&control, nullptr) == 22);
    GuestOnce invalid{9, nullptr};
    Require(pthread_once_nid_postfix(&invalid, Initialize) == 22);
}
