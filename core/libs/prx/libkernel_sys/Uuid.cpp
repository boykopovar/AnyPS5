#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

static_assert(sizeof(GuestUuid) == 16);

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestClockRealtime = 0;
constexpr int UuidgenMaxCount = 2048;
constexpr std::uint64_t UuidTimeMask = (std::uint64_t{1} << 60) - 1;
constexpr std::uint64_t UuidGregorianOffset = 0x01B21DD213814000ULL;

std::mutex uuidMutex;
std::array<std::uint8_t, 6> uuidNode{};
bool uuidNodeReady = false;
std::uint64_t uuidLastTime = 0;
std::uint16_t uuidLastSequence = 0;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

std::uint64_t UuidTime() {
    KernelTimespec now{};
    if (clock_gettime_nid_postfix(GuestClockRealtime, &now) != 0) throw std::runtime_error("uuidgen: clock_gettime(CLOCK_REALTIME) failed");
    const auto ticks = UuidGregorianOffset + static_cast<std::uint64_t>(now.tv_sec) * 10000000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 100;
    return ticks & UuidTimeMask;
}

}

extern "C" {

int APS5_VABI uuidgen_nid_postfix(GuestUuid* store, int count) {
    if (count < 1 || count > UuidgenMaxCount) return Fail(GuestEinval);
    std::uint64_t time = 0;
    std::uint16_t sequence = 0;
    std::array<std::uint8_t, 6> node{};
    {
        std::lock_guard lock(uuidMutex);
        if (!uuidNodeReady) {
            HostEntropy_nid_no_patch(uuidNode.data(), uuidNode.size());
            uuidNode[0] |= 0x01;
            uuidNodeReady = true;
        }
        node = uuidNode;
        time = UuidTime();
        if (uuidLastTime == 0) {
            HostEntropy_nid_no_patch(&sequence, sizeof(sequence));
            sequence &= 0x3fff;
        } else if (uuidLastTime >= time) {
            sequence = static_cast<std::uint16_t>((uuidLastSequence + 1) & 0x3fff);
        } else {
            sequence = uuidLastSequence;
        }
        uuidLastSequence = sequence;
        uuidLastTime = (time + static_cast<std::uint64_t>(count) - 1) & UuidTimeMask;
    }
    if (store == nullptr) return Fail(GuestEfault);
    const auto variantSequence = static_cast<std::uint16_t>(sequence | 0x8000);
    for (int index = 0; index < count; ++index, ++time) {
        store[index].timeLow = static_cast<std::uint32_t>(time);
        store[index].timeMid = static_cast<std::uint16_t>(time >> 32);
        store[index].timeHiAndVersion = static_cast<std::uint16_t>(((time >> 48) & 0x0fff) | 0x1000);
        store[index].clockSeqHiAndReserved = static_cast<std::uint8_t>(variantSequence >> 8);
        store[index].clockSeqLow = static_cast<std::uint8_t>(variantSequence);
        std::memcpy(store[index].node, node.data(), node.size());
    }
    return 0;
}

}
