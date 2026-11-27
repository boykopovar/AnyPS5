#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libc atomic CAS check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr int SeqCst = 5;

struct AlignedBuffer1 {
    std::uint8_t before;
    std::uint8_t target;
    std::uint8_t after;
};

struct AlignedBuffer2 {
    std::uint8_t before[2];
    std::uint16_t target;
    std::uint8_t after[2];
};

struct AlignedBuffer4 {
    std::uint8_t before[4];
    std::uint32_t target;
    std::uint8_t after[4];
};

struct AlignedBuffer8 {
    std::uint8_t before[8];
    std::uint64_t target;
    std::uint8_t after[8];
};

static void CheckSentinels(const void* buf, std::size_t size, std::uint8_t value) {
    const auto* bytes = static_cast<const std::uint8_t*>(buf);
    for (std::size_t i = 0; i < size; ++i) {
        Require(bytes[i] == value);
    }
}

static void TestWidth1NeighborsAndFailure() {
    AlignedBuffer1 buf;
    std::memset(&buf, 0xaa, sizeof(buf));
    buf.target = 0x55;
    std::uint8_t expected = 0x66;
    Require(__atomic_compare_exchange_1_nid_postfix(&buf.target, &expected, 0x33, SeqCst, SeqCst) == 0);
    Require(buf.target == 0x55);
    Require(expected == 0x55);
    Require(buf.before == 0xaa);
    Require(buf.after == 0xaa);

    Require(__atomic_compare_exchange_1_nid_postfix(&buf.target, &expected, 0x33, SeqCst, SeqCst) == 1);
    Require(buf.target == 0x33);
    Require(expected == 0x55);
    Require(buf.before == 0xaa);
    Require(buf.after == 0xaa);
}

static void TestWidth2SuccessAndFailure() {
    AlignedBuffer2 buf;
    std::memset(&buf, 0xaa, sizeof(buf));
    buf.target = 0x1234;
    std::uint16_t expected = 0x5678;

    Require(__atomic_compare_exchange_2_nid_postfix(&buf.target, &expected, 0x9abc, SeqCst, SeqCst) == 0);
    Require(buf.target == 0x1234);
    Require(expected == 0x1234);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);

    Require(__atomic_compare_exchange_2_nid_postfix(&buf.target, &expected, 0x5678, SeqCst, SeqCst) == 1);
    Require(buf.target == 0x5678);
    Require(expected == 0x1234);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);
}

static void TestWidth4SuccessAndFailure() {
    AlignedBuffer4 buf;
    std::memset(&buf, 0xaa, sizeof(buf));
    buf.target = 0x11223344;
    std::uint32_t expected = 0x55667788;

    Require(__atomic_compare_exchange_4_nid_postfix(&buf.target, &expected, 0x99aabbcc, SeqCst, SeqCst) == 0);
    Require(buf.target == 0x11223344);
    Require(expected == 0x11223344);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);

    Require(__atomic_compare_exchange_4_nid_postfix(&buf.target, &expected, 0x99aabbcc, SeqCst, SeqCst) == 1);
    Require(buf.target == 0x99aabbcc);
    Require(expected == 0x11223344);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);
}

static void TestWidth8SuccessAndFailure() {
    AlignedBuffer8 buf;
    std::memset(&buf, 0xaa, sizeof(buf));
    buf.target = 0x1122334455667788ull;
    std::uint64_t expected = 0x99aabbccddeeff00ull;

    Require(__atomic_compare_exchange_8_nid_postfix(&buf.target, &expected, 0x0102030405060708ull, SeqCst, SeqCst) == 0);
    Require(buf.target == 0x1122334455667788ull);
    Require(expected == 0x1122334455667788ull);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);

    Require(__atomic_compare_exchange_8_nid_postfix(&buf.target, &expected, 0x0102030405060708ull, SeqCst, SeqCst) == 1);
    Require(buf.target == 0x0102030405060708ull);
    Require(expected == 0x1122334455667788ull);
    CheckSentinels(buf.before, sizeof(buf.before), 0xaa);
    CheckSentinels(buf.after, sizeof(buf.after), 0xaa);
}

int main() {
    TestWidth1NeighborsAndFailure();
    TestWidth2SuccessAndFailure();
    TestWidth4SuccessAndFailure();
    TestWidth8SuccessAndFailure();
    return 0;
}
