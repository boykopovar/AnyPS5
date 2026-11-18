#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal time and lock test failed: %s\n", message);
    std::abort();
}

void TestZoneAndTzoff() {
    const char* zone = _Getzone_nid_postfix();
    Require(zone != nullptr, "zone was null");
    Require(std::strcmp(zone, ":") == 0, "zone was not colon");

    int tzoff = _Tzoff_nid_postfix();
    Require(tzoff == 0, "tzoff was not 0");
}

void TestGettime() {
    int len = -1;
    const char* p = _Gettime_nid_postfix(nullptr, 0, &len);
    Require(p != nullptr, "gettime null input returned null");
    Require(len == 0, "gettime null input len non-zero");

    len = -1;
    p = _Gettime_nid_postfix("", 0, &len);
    Require(p != nullptr && *p == '\0', "gettime empty input did not return empty string");
    Require(len == 0, "gettime empty input len non-zero");

    const char* s = ":UTC:GMT:EST:EDT";
    len = -1;
    p = _Gettime_nid_postfix(s, 0, &len);
    Require(len == 3, "field 0 length != 3");
    Require(std::strncmp(p, "UTC", 3) == 0, "field 0 != UTC");

    len = -1;
    p = _Gettime_nid_postfix(s, 1, &len);
    Require(len == 3, "field 1 length != 3");
    Require(std::strncmp(p, "GMT", 3) == 0, "field 1 != GMT");

    len = -1;
    p = _Gettime_nid_postfix(s, 2, &len);
    Require(len == 3, "field 2 length != 3");
    Require(std::strncmp(p, "EST", 3) == 0, "field 2 != EST");

    len = -1;
    p = _Gettime_nid_postfix(s, 3, &len);
    Require(len == 3, "field 3 length != 3");
    Require(std::strncmp(p, "EDT", 3) == 0, "field 3 != EDT");

    len = -1;
    p = _Gettime_nid_postfix(s, 4, &len);
    Require(len == 0, "out of bounds field 4 len != 0");
    Require(p != nullptr && *p == '\0', "out of bounds field 4 did not return terminating nul");

    len = -1;
    p = _Gettime_nid_postfix(s, -1, &len);
    Require(len == 0, "negative field len != 0");
    Require(p != nullptr && *p == '\0', "negative field did not return terminating nul");

    p = _Gettime_nid_postfix(s, 1, nullptr);
    Require(std::strncmp(p, "GMT", 3) == 0, "null len pointer failed to return field pointer");
}

void TestSpinLock() {
    _Lock_spin_lock_nid_postfix(nullptr, 0);
    _Unlock_spin_lock_nid_postfix(nullptr, 0);

    char lockByte = 0;
    _Lock_spin_lock_nid_postfix(&lockByte, 0);
    Require(lockByte == 1, "lock byte not set to 1 after acquire");
    _Unlock_spin_lock_nid_postfix(&lockByte, 0);
    Require(lockByte == 0, "lock byte not set to 0 after release");

    int counter = 0;
    constexpr int kThreads = 4;
    constexpr int kIterations = 2500;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                _Lock_spin_lock_nid_postfix(&lockByte, 0);
                ++counter;
                _Unlock_spin_lock_nid_postfix(&lockByte, 0);
            }
        });
    }
    for (auto& t : workers) {
        t.join();
    }
    Require(counter == kThreads * kIterations, "spin lock concurrency race condition detected");
    Require(lockByte == 0, "lock byte not cleared after test");
}

}

int main() {
    TestZoneAndTzoff();
    TestGettime();
    TestSpinLock();
    return 0;
}
