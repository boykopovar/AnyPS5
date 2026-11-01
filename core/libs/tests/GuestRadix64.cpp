#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Radix64 check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static void CheckNullPointers() {
    Check(a64l_nid_postfix(nullptr) == 0);

    Check(l64a_r_nid_postfix(0, nullptr, 0) == -1);
    Check(l64a_r_nid_postfix(0, nullptr, 10) == -1);
    Check(l64a_r_nid_postfix(1, nullptr, 0) == -1);
    Check(l64a_r_nid_postfix(1, nullptr, 10) == -1);
    Check(l64a_r_nid_postfix(3695, nullptr, 10) == -1);
    Check(l64a_r_nid_postfix(-1L, nullptr, 10) == -1);
}

static void CheckEmptyStrings() {
    Check(a64l_nid_postfix("") == 0);

    const char* str = l64a_nid_postfix(0);
    Check(str != nullptr);
    Check(std::strcmp(str, "") == 0);
    Check(*str == '\0');

    char buffer[4] = {'X', 'X', 'X', 'X'};
    Check(l64a_r_nid_postfix(0, buffer, 0) == -1);
    Check(l64a_r_nid_postfix(0, buffer, -1) == -1);
    Check(l64a_r_nid_postfix(0, buffer, 1) == 0 && buffer[0] == '\0');
    Check(l64a_r_nid_postfix(0, buffer, 4) == 0 && std::strcmp(buffer, "") == 0);
}

static void CheckSingleDigits() {
    Check(a64l_nid_postfix(".") == 0);
    Check(a64l_nid_postfix("/") == 1);
    Check(a64l_nid_postfix("0") == 2);
    Check(a64l_nid_postfix("1") == 3);
    Check(a64l_nid_postfix("9") == 11);
    Check(a64l_nid_postfix("A") == 12);
    Check(a64l_nid_postfix("B") == 13);
    Check(a64l_nid_postfix("Z") == 37);
    Check(a64l_nid_postfix("a") == 38);
    Check(a64l_nid_postfix("b") == 39);
    Check(a64l_nid_postfix("z") == 63);

    constexpr const char* chars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 64; ++i) {
        char single[2] = {chars[i], '\0'};
        Check(a64l_nid_postfix(single) == i);
        if (i > 0) {
            Check(std::strcmp(l64a_nid_postfix(i), single) == 0);
            char buf[4]{};
            Check(l64a_r_nid_postfix(i, buf, sizeof(buf)) == 0);
            Check(std::strcmp(buf, single) == 0);
        }
    }
}

static void CheckLittleEndianOrdering() {
    Check(a64l_nid_postfix("./") == 64);
    Check(std::strcmp(l64a_nid_postfix(64), "./") == 0);

    Check(a64l_nid_postfix("/.") == 1);

    Check(a64l_nid_postfix("FT") == 2001);
    Check(a64l_nid_postfix("FT....") == 2001);
    Check(a64l_nid_postfix("FT....") == a64l_nid_postfix("FT"));
    Check(std::strcmp(l64a_nid_postfix(2001), "FT") == 0);

    Check(a64l_nid_postfix("jt") == 3695);
    Check(a64l_nid_postfix("jt....") == 3695);
    Check(a64l_nid_postfix("jt....") == a64l_nid_postfix("jt"));
    Check(std::strcmp(l64a_nid_postfix(3695), "jt") == 0);

    Check(a64l_nid_postfix("...") == 0);
    Check(a64l_nid_postfix("///") == 4161);
    Check(std::strcmp(l64a_nid_postfix(4161), "///") == 0);

    Check(a64l_nid_postfix("zzz") == 262143);
    Check(std::strcmp(l64a_nid_postfix(262143), "zzz") == 0);
}

static void CheckBoundaries() {
    Check(a64l_nid_postfix("zzzzz1") == INT64_C(-1));
    Check(a64l_nid_postfix(".....0") == INT64_C(-2147483648));
    Check(std::strcmp(l64a_nid_postfix(INT64_C(0x100000001)), "/") == 0);
    Check(a64l_nid_postfix("zz") == 4095);
    Check(std::strcmp(l64a_nid_postfix(4095), "zz") == 0);

    Check(a64l_nid_postfix("../") == 4096);
    Check(std::strcmp(l64a_nid_postfix(4096), "../") == 0);

    Check(a64l_nid_postfix("zzzzz") == 1073741823L);
    Check(std::strcmp(l64a_nid_postfix(1073741823L), "zzzzz") == 0);

    Check(a64l_nid_postfix("zzzzz/") == 2147483647L);
    Check(std::strcmp(l64a_nid_postfix(2147483647L), "zzzzz/") == 0);

    Check(a64l_nid_postfix("zzzzz1") == -1L);
    Check(static_cast<std::uint32_t>(a64l_nid_postfix("zzzzz1")) == 0xFFFFFFFFu);
    Check(std::strcmp(l64a_nid_postfix(-1L), "zzzzz1") == 0);
    Check(std::strcmp(l64a_nid_postfix(0xFFFFFFFFu), "zzzzz1") == 0);

    Check(a64l_nid_postfix("yzzzz1") == -2L);
    Check(std::strcmp(l64a_nid_postfix(-2L), "yzzzz1") == 0);

    Check(a64l_nid_postfix(".zzzz1") == -64L);
    Check(std::strcmp(l64a_nid_postfix(-64L), ".zzzz1") == 0);

    const long minInt32 = static_cast<long>(static_cast<std::int32_t>(0x80000000u));
    Check(a64l_nid_postfix(".....0") == minInt32);
    Check(std::strcmp(l64a_nid_postfix(minInt32), ".....0") == 0);
}

static void CheckStringTermination() {
    Check(a64l_nid_postfix("FT#extra") == -43055);
    Check(a64l_nid_postfix("jt#extra") == -41361);
    Check(a64l_nid_postfix("./-abc") == -4032);
    Check(a64l_nid_postfix("z!foo") == -769);
    Check(a64l_nid_postfix("zzzzz1@xyz") == -1L);

    Check(a64l_nid_postfix("#FT") == -11);
    Check(a64l_nid_postfix("@extra") == -1209542005);
    Check(a64l_nid_postfix(" abc") == -14);
    Check(a64l_nid_postfix("	abc") == -37);
    Check(a64l_nid_postfix("-1") == -1);
    Check(a64l_nid_postfix("+1") == -3);
    Check(a64l_nid_postfix("~radix") == 1856400835);
}

static void CheckLengthCeiling() {
    Check(a64l_nid_postfix("12345678") == a64l_nid_postfix("123456"));
    Check(a64l_nid_postfix("FT....extra") == a64l_nid_postfix("FT...."));
    Check(a64l_nid_postfix("FT....extra") == 2001);
    Check(a64l_nid_postfix("jt....extra") == 3695);
    Check(a64l_nid_postfix("zzzzz1zzzz") == -1L);
}

static void CheckRoundTrip() {
    constexpr long positiveSamples[] = {
        0, 1, 2, 7, 15, 31, 63, 64, 65, 127, 255, 511, 1023, 2001, 2047, 3695, 4095, 4096,
        8191, 16383, 32767, 65535, 131071, 262143, 262144, 524287, 1048575, 2097151,
        4194303, 8388607, 16777215, 16777216, 33554431, 67108863, 134217727, 268435455,
        536870911, 1073741823, 1431655765, 2147483647
    };
    for (long val : positiveSamples) {
        const char* str = l64a_nid_postfix(val);
        Check(str != nullptr);
        Check(a64l_nid_postfix(str) == val);

        char buf[16]{};
        Check(l64a_r_nid_postfix(val, buf, sizeof(buf)) == 0);
        Check(std::strcmp(str, buf) == 0);
        Check(a64l_nid_postfix(buf) == val);
    }

    constexpr long signedSamples[] = {
        -1L, -2L, -64L, -2001L, -3695L, -65536L,
        static_cast<long>(static_cast<std::int32_t>(0x80000000u)),
        static_cast<long>(static_cast<std::int32_t>(0xAAAAAAAAu)),
        static_cast<long>(static_cast<std::int32_t>(0xDEADBEEFu))
    };
    for (long val : signedSamples) {
        const char* str = l64a_nid_postfix(val);
        Check(str != nullptr);
        Check(a64l_nid_postfix(str) == static_cast<long>(static_cast<std::int32_t>(val)));

        char buf[16]{};
        Check(l64a_r_nid_postfix(val, buf, sizeof(buf)) == 0);
        Check(std::strcmp(str, buf) == 0);
        Check(a64l_nid_postfix(buf) == static_cast<long>(static_cast<std::int32_t>(val)));
    }
}

static void CheckBufferCapacity() {
    char buf[16]{};

    Check(l64a_r_nid_postfix(0, buf, 0) == -1);
    Check(l64a_r_nid_postfix(0, buf, -1) == -1);
    Check(l64a_r_nid_postfix(0, buf, 1) == 0 && std::strcmp(buf, "") == 0);

    Check(l64a_r_nid_postfix(1, buf, 0) == -1);
    Check(l64a_r_nid_postfix(1, buf, 1) == -1 && std::strcmp(buf, "") == 0);
    Check(l64a_r_nid_postfix(1, buf, 2) == 0 && std::strcmp(buf, "/") == 0);

    Check(l64a_r_nid_postfix(64, buf, 1) == -1 && std::strcmp(buf, "") == 0);
    Check(l64a_r_nid_postfix(64, buf, 2) == -1 && std::strcmp(buf, ".") == 0);
    Check(l64a_r_nid_postfix(64, buf, 3) == 0 && std::strcmp(buf, "./") == 0);

    Check(l64a_r_nid_postfix(2001, buf, 2) == -1 && std::strcmp(buf, "F") == 0);
    Check(l64a_r_nid_postfix(2001, buf, 3) == 0 && std::strcmp(buf, "FT") == 0);

    Check(l64a_r_nid_postfix(3695, buf, 2) == -1 && std::strcmp(buf, "j") == 0);
    Check(l64a_r_nid_postfix(3695, buf, 3) == 0 && std::strcmp(buf, "jt") == 0);

    for (int len = -2; len <= 0; ++len) {
        Check(l64a_r_nid_postfix(-1L, buf, len) == -1);
    }
    for (int len = 1; len <= 6; ++len) {
        Check(l64a_r_nid_postfix(-1L, buf, len) == -1);
        Check(std::strlen(buf) == static_cast<std::size_t>(len - 1));
        Check(std::strncmp(buf, "zzzzz1", len - 1) == 0);
    }
    Check(l64a_r_nid_postfix(-1L, buf, 7) == 0 && std::strcmp(buf, "zzzzz1") == 0);
    Check(l64a_r_nid_postfix(-1L, buf, 8) == 0 && std::strcmp(buf, "zzzzz1") == 0);
    Check(l64a_r_nid_postfix(-1L, buf, 16) == 0 && std::strcmp(buf, "zzzzz1") == 0);

    std::array<char, 16> canaries;
    canaries.fill('#');
    Check(l64a_r_nid_postfix(-1L, canaries.data() + 1, 7) == 0);
    Check(canaries[0] == '#');
    Check(std::strcmp(canaries.data() + 1, "zzzzz1") == 0);
    Check(canaries[8] == '#');
}

static void CheckConcurrency() {
    constexpr std::array<long, 8> threadVals{1, 64, 2001, 3695, 4096, 262144, 2147483647, -1L};
    std::barrier start(static_cast<std::ptrdiff_t>(threadVals.size()));
    std::atomic<bool> threadSuccess{true};
    std::array<std::thread, threadVals.size()> workers;
    for (std::size_t i = 0; i < threadVals.size(); ++i) {
        workers[i] = std::thread([&, i] {
            const long v = threadVals[i];
            char expected[16]{};
            Check(l64a_r_nid_postfix(v, expected, sizeof(expected)) == 0);
            start.arrive_and_wait();
            for (int iter = 0; iter < 10000; ++iter) {
                const char* res = l64a_nid_postfix(v);
                if (res == nullptr || std::strcmp(res, expected) != 0) {
                    threadSuccess = false;
                    break;
                }
            }
        });
    }
    for (auto& w : workers) w.join();
    Check(threadSuccess.load());
}

int main() {
    CheckNullPointers();
    CheckEmptyStrings();
    CheckSingleDigits();
    CheckLittleEndianOrdering();
    CheckBoundaries();
    CheckStringTermination();
    CheckLengthCeiling();
    CheckRoundTrip();
    CheckBufferCapacity();
    CheckConcurrency();
    return 0;
}
