#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

extern "C" {
int APS5_VABI uuidgen_nid_postfix(void*, int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

struct Uuid {
    std::uint32_t timeLow;
    std::uint16_t timeMid;
    std::uint16_t timeHiAndVersion;
    std::uint8_t clockSeqHiAndReserved;
    std::uint8_t clockSeqLow;
    std::uint8_t node[6];
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "uuidgen check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

std::uint64_t Timestamp(const Uuid& uuid) {
    return (static_cast<std::uint64_t>(uuid.timeHiAndVersion & 0x0fff) << 48) | (static_cast<std::uint64_t>(uuid.timeMid) << 32) | uuid.timeLow;
}

std::uint16_t Sequence(const Uuid& uuid) {
    return static_cast<std::uint16_t>(((uuid.clockSeqHiAndReserved & 0x3f) << 8) | uuid.clockSeqLow);
}

}

int main() {
    std::array<Uuid, 3> first{};
    std::array<Uuid, 2> second{};
    Require(uuidgen_nid_postfix(first.data(), 0) == -1 && Error() == 22);
    Require(uuidgen_nid_postfix(first.data(), 2049) == -1 && Error() == 22);
    Require(uuidgen_nid_postfix(nullptr, 1) == -1 && Error() == 14);
    Require(uuidgen_nid_postfix(first.data(), static_cast<int>(first.size())) == 0);
    Require(uuidgen_nid_postfix(second.data(), static_cast<int>(second.size())) == 0);
    for (const auto& uuid : first) {
        Require((uuid.timeHiAndVersion >> 12) == 1);
        Require((uuid.clockSeqHiAndReserved & 0xc0) == 0x80);
        Require((uuid.node[0] & 0x01) != 0);
        Require(std::memcmp(uuid.node, first[0].node, sizeof(uuid.node)) == 0);
        Require(Sequence(uuid) == Sequence(first[0]));
    }
    Require(Timestamp(first[1]) == Timestamp(first[0]) + 1 && Timestamp(first[2]) == Timestamp(first[0]) + 2);
    Require(std::memcmp(second[0].node, first[0].node, sizeof(first[0].node)) == 0);
    const bool later = Timestamp(second[0]) > Timestamp(first[2]) && Sequence(second[0]) == Sequence(first[0]);
    const bool bumped = Timestamp(second[0]) <= Timestamp(first[2]) && Sequence(second[0]) == ((Sequence(first[0]) + 1) & 0x3fff);
    Require(later || bumped);
    const auto unixTicks = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 100);
    const auto gregorianTicks = unixTicks + 0x01B21DD213814000ULL;
    const auto stamp = Timestamp(first[0]);
    Require(stamp + 600000000ULL > gregorianTicks && stamp < gregorianTicks + 600000000ULL);
}
