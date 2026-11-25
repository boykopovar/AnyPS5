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

void TestGetdst() {
    Require(_Getdst_nid_postfix(nullptr) == nullptr, "null input returned non-null");
    Require(_Getdst_nid_postfix("") == nullptr, "empty input returned non-null");
    Require(_Getdst_nid_postfix(":") == nullptr, "lone separator returned non-null");
    Require(_Getdst_nid_postfix("::") == nullptr, "empty rule returned non-null");

    const char* r1 = _Getdst_nid_postfix(":0101");
    Require(r1 != nullptr, "default rule returned null");
    const auto* b1 = reinterpret_cast<const unsigned char*>(r1);
    Require(b1[0] == 0, "r1 byte 0 != 0");
    Require(b1[1] == 0, "r1 byte 1 != 0");
    Require(b1[2] == 0, "r1 byte 2 != 0");
    Require(b1[3] == 0, "r1 byte 3 != 0");
    Require(b1[4] == 0, "r1 byte 4 != 0");
    Require(b1[5] == 0xff, "r1 byte 5 != 0xff");
    Require(b1[6] == 0, "r1 byte 6 != 0");
    Require(b1[7] == 0, "r1 byte 7 != 0");
    Require(b1[8] == 0, "r1 byte 8 != 0");
    Require(b1[9] == 0, "r1 byte 9 != 0");
    std::free(const_cast<char*>(r1));

    const char* r2 = _Getdst_nid_postfix(":(2007)031502+1");
    Require(r2 != nullptr, "rule 2 returned null");
    const auto* b2 = reinterpret_cast<const unsigned char*>(r2);
    Require(b2[0] == 8, "r2 byte 0 != 8");
    Require(b2[1] == 2, "r2 byte 1 != 2");
    Require(b2[2] == 0x0e, "r2 byte 2 != 0x0e");
    Require(b2[3] == 2, "r2 byte 3 != 2");
    Require(b2[4] == 0x6b, "r2 byte 4 != 0x6b");
    Require(b2[5] == 0xff, "r2 byte 5 != 0xff");
    Require(b2[6] == 0, "r2 byte 6 != 0");
    Require(b2[7] == 0, "r2 byte 7 != 0");
    Require(b2[8] == 0, "r2 byte 8 != 0");
    Require(b2[9] == 0x6b, "r2 byte 9 != 0x6b");
    std::free(const_cast<char*>(r2));

    const char* r3 = _Getdst_nid_postfix(":(2155)1132-0");
    Require(r3 != nullptr, "rule 3 returned null");
    const auto* b3 = reinterpret_cast<const unsigned char*>(r3);
    Require(b3[0] == 7, "r3 byte 0 != 7");
    Require(b3[1] == 0, "r3 byte 1 != 0");
    Require(b3[2] == 0x1f, "r3 byte 2 != 0x1f");
    Require(b3[3] == 0x0a, "r3 byte 3 != 0x0a");
    Require(b3[4] == 0xff, "r3 byte 4 != 0xff");
    Require(b3[5] == 0xff, "r3 byte 5 != 0xff");
    Require(b3[6] == 0, "r3 byte 6 != 0");
    Require(b3[7] == 0, "r3 byte 7 != 0");
    Require(b3[8] == 0, "r3 byte 8 != 0");
    Require(b3[9] == 0xff, "r3 byte 9 != 0xff");
    std::free(const_cast<char*>(r3));

    const char* r4 = _Getdst_nid_postfix("|(2007)031502+1|110102-0");
    Require(r4 != nullptr, "rule 4 returned null");
    const auto* b4 = reinterpret_cast<const unsigned char*>(r4);
    Require(b4[0] == 8, "r4 byte 0 != 8");
    Require(b4[1] == 2, "r4 byte 1 != 2");
    Require(b4[2] == 0x0e, "r4 byte 2 != 0x0e");
    Require(b4[3] == 2, "r4 byte 3 != 2");
    Require(b4[4] == 0x6b, "r4 byte 4 != 0x6b");
    Require(b4[5] == 7, "r4 byte 5 != 7");
    Require(b4[6] == 2, "r4 byte 6 != 2");
    Require(b4[7] == 0, "r4 byte 7 != 0");
    Require(b4[8] == 10, "r4 byte 8 != 10");
    Require(b4[9] == 0x6b, "r4 byte 9 != 0x6b");
    Require(b4[10] == 0xff, "r4 byte 10 != 0xff");
    Require(b4[11] == 0, "r4 byte 11 != 0");
    Require(b4[12] == 0, "r4 byte 12 != 0");
    Require(b4[13] == 0, "r4 byte 13 != 0");
    Require(b4[14] == 0x6b, "r4 byte 14 != 0x6b");
    std::free(const_cast<char*>(r4));

    Require(_Getdst_nid_postfix(":0001") == nullptr, "month 00 was not rejected");
    Require(_Getdst_nid_postfix(":1301") == nullptr, "month 13 was not rejected");
    Require(_Getdst_nid_postfix(":0100") == nullptr, "day 00 was not rejected");
    Require(_Getdst_nid_postfix(":01") == nullptr, "2 digits was not rejected");
    Require(_Getdst_nid_postfix(":01010") == nullptr, "5 digits was not rejected");
    Require(_Getdst_nid_postfix(":0101020") == nullptr, "7 digits was not rejected");
    Require(_Getdst_nid_postfix(":(1899)0101") == nullptr, "year < 1900 was not rejected");
    Require(_Getdst_nid_postfix(":(2000)") == nullptr, "missing date was not rejected");
    Require(_Getdst_nid_postfix(":0101+") == nullptr, "bare sign was not rejected");
    Require(_Getdst_nid_postfix(":0101+7") == nullptr, "weekday > 6 was not rejected");
    Require(_Getdst_nid_postfix(":0101:") == nullptr, "trailing separator was not rejected");
    Require(_Getdst_nid_postfix(":0101x") == nullptr, "trailing non-digit was not rejected");
}

}

int main() {
    TestZoneAndTzoff();
    TestGettime();
    TestGetdst();
    TestSpinLock();
    return 0;
}
