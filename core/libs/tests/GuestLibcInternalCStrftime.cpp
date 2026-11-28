#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

static_assert(std::is_same_v<decltype(&_CStrftime_nid_postfix),
                             size_t (APS5_VABI*)(char*, size_t, const char*, const GuestTm*)>);

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(value) Require((value), __LINE__)

template <typename TAction>
bool Rejects(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

GuestTm MakeTime() {
    GuestTm time{};
    time.tm_year = 120;
    time.tm_mon = 0;
    time.tm_mday = 1;
    time.tm_hour = 12;
    time.tm_min = 34;
    time.tm_sec = 56;
    time.tm_wday = 3;
    time.tm_yday = 0;
    return time;
}

void TestFormats() {
    const GuestTm time = MakeTime();
    char buffer[64];
    char expected[64];
    std::memset(buffer, '#', sizeof(buffer));
    std::memset(expected, '#', sizeof(expected));

    const size_t length = _CStrftime_nid_postfix(buffer, sizeof(buffer), "%Y-%m-%d", &time);
    const size_t expectedLength = strftime_nid_postfix(expected, sizeof(expected), "%Y-%m-%d", &time);
    REQUIRE(length == 10);
    REQUIRE(length == expectedLength);
    REQUIRE(std::memcmp(buffer, "2020-01-01", 11) == 0);
    REQUIRE(std::memcmp(buffer, expected, sizeof(buffer)) == 0);

    std::memset(buffer, '#', sizeof(buffer));
    std::memset(expected, '#', sizeof(expected));
    const size_t clockLength = _CStrftime_nid_postfix(buffer, sizeof(buffer), "%H:%M:%S", &time);
    const size_t clockExpected = strftime_nid_postfix(expected, sizeof(expected), "%H:%M:%S", &time);
    REQUIRE(clockLength == 8);
    REQUIRE(clockLength == clockExpected);
    REQUIRE(std::memcmp(buffer, "12:34:56", 9) == 0);
    REQUIRE(std::memcmp(buffer, expected, sizeof(buffer)) == 0);
}

void TestShortBuffer() {
    const GuestTm time = MakeTime();
    char buffer[8];
    std::memset(buffer, '#', sizeof(buffer));
    REQUIRE(_CStrftime_nid_postfix(buffer, sizeof(buffer), "%Y-%m-%d", &time) == 0);
}

void TestZeroCount() {
    const GuestTm time = MakeTime();
    REQUIRE(_CStrftime_nid_postfix(nullptr, 0, "%Y", &time) == 0);
}

void TestNullTime() {
    char buffer[32];
    REQUIRE(Rejects([&] { (void)_CStrftime_nid_postfix(buffer, sizeof(buffer), "%Y", nullptr); }));
}

void TestMatchesLibcNullFormat() {
    const GuestTm time = MakeTime();
    char buffer[128];
    char expected[128];
    std::memset(buffer, '#', sizeof(buffer));
    std::memset(expected, '#', sizeof(expected));
    const size_t length = _CStrftime_nid_postfix(buffer, sizeof(buffer), nullptr, &time);
    const size_t expectedLength = strftime_nid_postfix(expected, sizeof(expected), nullptr, &time);
    REQUIRE(length == expectedLength);
    REQUIRE(length != 0);
    REQUIRE(std::memcmp(buffer, expected, sizeof(buffer)) == 0);
}

}

int main() {
    TestFormats();
    TestShortBuffer();
    TestZeroCount();
    TestNullTime();
    TestMatchesLibcNullFormat();
    std::puts("guest libc internal cstrftime tests passed");
    return 0;
}
