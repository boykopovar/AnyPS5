#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "uuidgen check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

std::uint64_t Timestamp(const GuestUuid& uuid) {
    return (static_cast<std::uint64_t>(uuid.timeHiAndVersion & 0x0fff) << 48) | (static_cast<std::uint64_t>(uuid.timeMid) << 32) | uuid.timeLow;
}

std::uint16_t Sequence(const GuestUuid& uuid) {
    return static_cast<std::uint16_t>(((uuid.clockSeqHiAndReserved & 0x3f) << 8) | uuid.clockSeqLow);
}

}

int main() {
    std::array<GuestUuid, 3> first{};
    std::array<GuestUuid, 2> second{};
    Require(uuidgen_nid_postfix(first.data(), 0) == -1 && Error() == 22);
    Require(uuidgen_nid_postfix(first.data(), 2049) == -1 && Error() == 22);
    Require(uuidgen_nid_postfix(nullptr, 1) == -1 && Error() == 14);
    Require(uuidgen_nid_postfix(reinterpret_cast<GuestUuid*>(0x10), 1) == -1 && Error() == 14);
    Require(uuidgen_nid_postfix(first.data(), static_cast<int>(first.size())) == 0);
    Require(uuidgen_nid_postfix(second.data(), static_cast<int>(second.size())) == 0);
    for (const auto& uuid : first) {
        Require((uuid.timeHiAndVersion >> 12) == 1);
        Require((uuid.clockSeqHiAndReserved & 0xc0) == 0x80);
        Require((uuid.node[0] & 0x01) == 0);
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

    Require(sceKernelCreateUuidV1(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateUuidV1(reinterpret_cast<std::uint8_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    std::array<std::uint8_t, 16> kernelUuidBytes{};
    Require(sceKernelCreateUuidV1(kernelUuidBytes.data()) == 0);
    Require((kernelUuidBytes[6] >> 4) == 1);
    Require((kernelUuidBytes[8] & 0xc0) == 0x80);
    Require((kernelUuidBytes[10] & 0x01) == 0);
    Require(std::memcmp(&kernelUuidBytes[10], first[0].node, sizeof(first[0].node)) == 0);

    Require(sceKernelCreateUuidV7(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateUuidV7(reinterpret_cast<std::uint8_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    std::array<std::uint8_t, 16> versionSeven{};
    std::array<std::uint8_t, 16> otherVersionSeven{};
    const auto before = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    Require(sceKernelCreateUuidV7(versionSeven.data()) == 0);
    Require(sceKernelCreateUuidV7(otherVersionSeven.data()) == 0);
    const auto after = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::uint64_t milliseconds = 0;
    for (int index = 0; index < 6; ++index) milliseconds = (milliseconds << 8) | versionSeven[index];
    Require(milliseconds + 1000 >= before && milliseconds <= after + 1000);
    Require((versionSeven[6] >> 4) == 7 && (versionSeven[8] & 0xc0) == 0x80);
    Require((otherVersionSeven[6] >> 4) == 7 && (otherVersionSeven[8] & 0xc0) == 0x80);
    Require(std::memcmp(&versionSeven[6], &otherVersionSeven[6], 10) != 0);
}
