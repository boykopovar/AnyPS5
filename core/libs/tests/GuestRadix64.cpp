#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

static void CheckNullPointers() {
    Require(a64l_nid_postfix(nullptr) == 0);

    Require(l64a_r_nid_postfix(0, nullptr, 0) == -1);
    Require(l64a_r_nid_postfix(0, nullptr, 10) == -1);
    Require(l64a_r_nid_postfix(1, nullptr, 0) == -1);
    Require(l64a_r_nid_postfix(1, nullptr, 10) == -1);
    Require(l64a_r_nid_postfix(3695, nullptr, 10) == -1);
    Require(l64a_r_nid_postfix(-1L, nullptr, 10) == -1);

    errno = 71;
    Require(a64l_nid_postfix(reinterpret_cast<const char*>(0x10)) == 0 && errno == 71);
    Require(l64a_r_nid_postfix(3695, reinterpret_cast<char*>(0x10), 10) == -1 && errno == 71);
    Require(l64a_r_nid_postfix(0, reinterpret_cast<char*>(0x10), 10) == -1 && errno == 71);
}

static void CheckEmptyStrings() {
    Require(a64l_nid_postfix("") == 0);

    const char* str = l64a_nid_postfix(0);
    Require(str != nullptr);
    Require(std::strcmp(str, "") == 0);
    Require(*str == '\0');

    char buffer[4] = {'X', 'X', 'X', 'X'};
    Require(l64a_r_nid_postfix(0, buffer, 0) == -1);
    Require(l64a_r_nid_postfix(0, buffer, -1) == -1);
    Require(l64a_r_nid_postfix(0, buffer, 1) == 0 && buffer[0] == '\0');
    Require(l64a_r_nid_postfix(0, buffer, 4) == 0 && std::strcmp(buffer, "") == 0);
}

static void CheckSingleDigits() {
    Require(a64l_nid_postfix(".") == 0);
    Require(a64l_nid_postfix("/") == 1);
    Require(a64l_nid_postfix("0") == 2);
    Require(a64l_nid_postfix("1") == 3);
    Require(a64l_nid_postfix("9") == 11);
    Require(a64l_nid_postfix("A") == 12);
    Require(a64l_nid_postfix("B") == 13);
    Require(a64l_nid_postfix("Z") == 37);
    Require(a64l_nid_postfix("a") == 38);
    Require(a64l_nid_postfix("b") == 39);
    Require(a64l_nid_postfix("z") == 63);

    constexpr const char* chars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 64; ++i) {
        char single[2] = {chars[i], '\0'};
        Require(a64l_nid_postfix(single) == i);
        if (i > 0) {
            Require(std::strcmp(l64a_nid_postfix(i), single) == 0);
            char buf[4]{};
            Require(l64a_r_nid_postfix(i, buf, sizeof(buf)) == 0);
            Require(std::strcmp(buf, single) == 0);
        }
    }
}

static void CheckLittleEndianOrdering() {
    Require(a64l_nid_postfix("./") == 64);
    Require(std::strcmp(l64a_nid_postfix(64), "./") == 0);

    Require(a64l_nid_postfix("/.") == 1);

    Require(a64l_nid_postfix("FT") == 2001);
    Require(a64l_nid_postfix("FT....") == 2001);
    Require(a64l_nid_postfix("FT....") == a64l_nid_postfix("FT"));
    Require(std::strcmp(l64a_nid_postfix(2001), "FT") == 0);

    Require(a64l_nid_postfix("jt") == 3695);
    Require(a64l_nid_postfix("jt....") == 3695);
    Require(a64l_nid_postfix("jt....") == a64l_nid_postfix("jt"));
    Require(std::strcmp(l64a_nid_postfix(3695), "jt") == 0);

    Require(a64l_nid_postfix("...") == 0);
    Require(a64l_nid_postfix("///") == 4161);
    Require(std::strcmp(l64a_nid_postfix(4161), "///") == 0);

    Require(a64l_nid_postfix("zzz") == 262143);
    Require(std::strcmp(l64a_nid_postfix(262143), "zzz") == 0);
}

static void CheckBoundaries() {
    Require(a64l_nid_postfix("zzzzz1") == INT64_C(-1));
    Require(a64l_nid_postfix(".....0") == INT64_C(-2147483648));
    Require(std::strcmp(l64a_nid_postfix(INT64_C(0x100000001)), "/") == 0);
    Require(std::strcmp(l64a_nid_postfix(INT64_C(0x100000000)), "") == 0);
    Require(std::strcmp(l64a_nid_postfix(INT64_C(0x7fffffff00000000)), "") == 0);
    Require(std::strcmp(l64a_nid_postfix(INT64_C(-0x100000000)), "") == 0);
    char highBuf[16]{};
    Require(l64a_r_nid_postfix(INT64_C(0x100000000), highBuf, sizeof(highBuf)) == 0 && std::strcmp(highBuf, "") == 0);
    Require(l64a_r_nid_postfix(INT64_C(0x100000001), highBuf, sizeof(highBuf)) == 0 && std::strcmp(highBuf, "/") == 0);
    Require(l64a_r_nid_postfix(INT64_C(0x7fffffff00000000), highBuf, sizeof(highBuf)) == 0 && std::strcmp(highBuf, "") == 0);
    Require(l64a_r_nid_postfix(INT64_C(-0x100000000), highBuf, sizeof(highBuf)) == 0 && std::strcmp(highBuf, "") == 0);
    Require(a64l_nid_postfix("zz") == 4095);
    Require(std::strcmp(l64a_nid_postfix(4095), "zz") == 0);

    Require(a64l_nid_postfix("../") == 4096);
    Require(std::strcmp(l64a_nid_postfix(4096), "../") == 0);

    Require(a64l_nid_postfix("zzzzz") == 1073741823L);
    Require(std::strcmp(l64a_nid_postfix(1073741823L), "zzzzz") == 0);

    Require(a64l_nid_postfix("zzzzz/") == 2147483647L);
    Require(std::strcmp(l64a_nid_postfix(2147483647L), "zzzzz/") == 0);

    Require(a64l_nid_postfix("zzzzz1") == -1L);
    Require(static_cast<std::uint32_t>(a64l_nid_postfix("zzzzz1")) == 0xFFFFFFFFu);
    Require(std::strcmp(l64a_nid_postfix(-1L), "zzzzz1") == 0);
    Require(std::strcmp(l64a_nid_postfix(0xFFFFFFFFu), "zzzzz1") == 0);

    Require(a64l_nid_postfix("yzzzz1") == -2L);
    Require(std::strcmp(l64a_nid_postfix(-2L), "yzzzz1") == 0);

    Require(a64l_nid_postfix(".zzzz1") == -64L);
    Require(std::strcmp(l64a_nid_postfix(-64L), ".zzzz1") == 0);

    const long minInt32 = static_cast<long>(static_cast<std::int32_t>(0x80000000u));
    Require(a64l_nid_postfix(".....0") == minInt32);
    Require(std::strcmp(l64a_nid_postfix(minInt32), ".....0") == 0);
}

static void CheckStringTermination() {
    Require(a64l_nid_postfix("FT#extra") == -43055);
    Require(a64l_nid_postfix("jt#extra") == -41361);
    Require(a64l_nid_postfix("./-abc") == -4032);
    Require(a64l_nid_postfix("z!foo") == -769);
    Require(a64l_nid_postfix("zzzzz1@xyz") == -1L);

    Require(a64l_nid_postfix("#FT") == -11);
    Require(a64l_nid_postfix("@extra") == -1209542005);
    Require(a64l_nid_postfix(" abc") == -14);
    Require(a64l_nid_postfix("	abc") == -37);
    Require(a64l_nid_postfix("-1") == -1);
    Require(a64l_nid_postfix("+1") == -3);
    Require(a64l_nid_postfix("~radix") == 1856400835);
}

static void CheckLengthCeiling() {
    Require(a64l_nid_postfix("12345678") == a64l_nid_postfix("123456"));
    Require(a64l_nid_postfix("FT....extra") == a64l_nid_postfix("FT...."));
    Require(a64l_nid_postfix("FT....extra") == 2001);
    Require(a64l_nid_postfix("jt....extra") == 3695);
    Require(a64l_nid_postfix("zzzzz1zzzz") == -1L);
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
        Require(str != nullptr);
        Require(a64l_nid_postfix(str) == val);

        char buf[16]{};
        Require(l64a_r_nid_postfix(val, buf, sizeof(buf)) == 0);
        Require(std::strcmp(str, buf) == 0);
        Require(a64l_nid_postfix(buf) == val);
    }

    constexpr long signedSamples[] = {
        -1L, -2L, -64L, -2001L, -3695L, -65536L,
        static_cast<long>(static_cast<std::int32_t>(0x80000000u)),
        static_cast<long>(static_cast<std::int32_t>(0xAAAAAAAAu)),
        static_cast<long>(static_cast<std::int32_t>(0xDEADBEEFu))
    };
    for (long val : signedSamples) {
        const char* str = l64a_nid_postfix(val);
        Require(str != nullptr);
        Require(a64l_nid_postfix(str) == static_cast<long>(static_cast<std::int32_t>(val)));

        char buf[16]{};
        Require(l64a_r_nid_postfix(val, buf, sizeof(buf)) == 0);
        Require(std::strcmp(str, buf) == 0);
        Require(a64l_nid_postfix(buf) == static_cast<long>(static_cast<std::int32_t>(val)));
    }
}

static void CheckBufferCapacity() {
    constexpr char kSentinel = '~';
    std::array<char, 16> buf;
    auto resetBuf = [&] {
        buf.fill(kSentinel);
    };

    resetBuf();
    Require(l64a_r_nid_postfix(0, buf.data(), 0) == -1);
    Require(buf[0] == kSentinel);
    Require(l64a_r_nid_postfix(0, buf.data(), -1) == -1);
    Require(buf[0] == kSentinel);
    Require(l64a_r_nid_postfix(0, buf.data(), 1) == 0);
    Require(buf[0] == '\0');
    Require(buf[1] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(1, buf.data(), 0) == -1);
    Require(buf[0] == kSentinel);
    Require(l64a_r_nid_postfix(1, buf.data(), 1) == -1);
    Require(buf[0] == '/');
    Require(buf[1] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(1, buf.data(), 2) == 0);
    Require(buf[0] == '/');
    Require(buf[1] == '\0');
    Require(buf[2] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(64, buf.data(), 1) == -1);
    Require(buf[0] == '.');
    Require(buf[1] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(64, buf.data(), 2) == -1);
    Require(buf[0] == '.');
    Require(buf[1] == '/');
    Require(buf[2] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(64, buf.data(), 3) == 0);
    Require(std::strcmp(buf.data(), "./") == 0);
    Require(buf[3] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(2001, buf.data(), 1) == -1);
    Require(buf[0] == 'F');
    Require(buf[1] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(2001, buf.data(), 2) == -1);
    Require(buf[0] == 'F');
    Require(buf[1] == 'T');
    Require(buf[2] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(2001, buf.data(), 3) == 0);
    Require(std::strcmp(buf.data(), "FT") == 0);
    Require(buf[3] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(3695, buf.data(), 1) == -1);
    Require(buf[0] == 'j');
    Require(buf[1] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(3695, buf.data(), 2) == -1);
    Require(buf[0] == 'j');
    Require(buf[1] == 't');
    Require(buf[2] == kSentinel);
    resetBuf();
    Require(l64a_r_nid_postfix(3695, buf.data(), 3) == 0);
    Require(std::strcmp(buf.data(), "jt") == 0);
    Require(buf[3] == kSentinel);

    for (int len = -2; len <= 0; ++len) {
        resetBuf();
        Require(l64a_r_nid_postfix(-1L, buf.data(), len) == -1);
        Require(buf[0] == kSentinel);
    }
    for (int len = 1; len <= 6; ++len) {
        resetBuf();
        Require(l64a_r_nid_postfix(-1L, buf.data(), len) == -1);
        Require(std::strncmp(buf.data(), "zzzzz1", static_cast<std::size_t>(len)) == 0);
        Require(buf[static_cast<std::size_t>(len)] == kSentinel);
    }
    resetBuf();
    Require(l64a_r_nid_postfix(-1L, buf.data(), 7) == 0);
    Require(std::strcmp(buf.data(), "zzzzz1") == 0);
    Require(buf[7] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(-1L, buf.data(), 8) == 0);
    Require(std::strcmp(buf.data(), "zzzzz1") == 0);
    Require(buf[7] == kSentinel);

    resetBuf();
    Require(l64a_r_nid_postfix(-1L, buf.data(), 16) == 0);
    Require(std::strcmp(buf.data(), "zzzzz1") == 0);
    Require(buf[7] == kSentinel);

    std::array<char, 16> canaries;
    canaries.fill('#');
    Require(l64a_r_nid_postfix(-1L, canaries.data() + 1, 7) == 0);
    Require(canaries[0] == '#');
    Require(std::strcmp(canaries.data() + 1, "zzzzz1") == 0);
    Require(canaries[8] == '#');
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
            Require(l64a_r_nid_postfix(v, expected, sizeof(expected)) == 0);
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
    Require(threadSuccess.load());
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
