#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>
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

std::uint64_t UnixMilliseconds() {
    KernelTimespec now{};
    if (clock_gettime_nid_postfix(GuestClockRealtime, &now) != 0) throw std::runtime_error("sceKernelCreateUuidV7: clock_gettime(CLOCK_REALTIME) failed");
    return static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 1000000ULL;
}

void GenerateUuidV1(GuestUuid* destination, int count) {
    std::uint64_t time = 0;
    std::uint16_t sequence = 0;
    std::array<std::uint8_t, 6> node{};
    {
        std::lock_guard lock(uuidMutex);
        if (!uuidNodeReady) {
            HostEntropy_nid_no_patch(uuidNode.data(), uuidNode.size());
            uuidNode[0] &= 0xfe;
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
    const auto variantSequence = static_cast<std::uint16_t>(sequence | 0x8000);
    for (int index = 0; index < count; ++index, ++time) {
        destination[index].timeLow = static_cast<std::uint32_t>(time);
        destination[index].timeMid = static_cast<std::uint16_t>(time >> 32);
        destination[index].timeHiAndVersion = static_cast<std::uint16_t>(((time >> 48) & 0x0fff) | 0x1000);
        destination[index].clockSeqHiAndReserved = static_cast<std::uint8_t>(variantSequence >> 8);
        destination[index].clockSeqLow = static_cast<std::uint8_t>(variantSequence);
        std::memcpy(destination[index].node, node.data(), node.size());
    }
}

}

extern "C" {

int APS5_VABI uuidgen_nid_postfix(GuestUuid* store, int count) {
    if (count < 1 || count > UuidgenMaxCount) return Fail(GuestEinval);
    std::vector<GuestUuid> generated(static_cast<std::size_t>(count));
    GenerateUuidV1(generated.data(), count);
    if (!GuestCopyOut_nid_no_patch(store, generated.data(), sizeof(GuestUuid) * static_cast<std::size_t>(count))) {
        return Fail(GuestEfault);
    }
    return 0;
}

int APS5_VABI sceKernelCreateUuidV1(std::uint8_t* uuid) {
    if (!uuid) return SCE_KERNEL_ERROR_EINVAL;
    GuestUuid generated{};
    GenerateUuidV1(&generated, 1);
    std::array<std::uint8_t, 16> bytes{};
    bytes[0] = static_cast<std::uint8_t>(generated.timeLow >> 24);
    bytes[1] = static_cast<std::uint8_t>(generated.timeLow >> 16);
    bytes[2] = static_cast<std::uint8_t>(generated.timeLow >> 8);
    bytes[3] = static_cast<std::uint8_t>(generated.timeLow);
    bytes[4] = static_cast<std::uint8_t>(generated.timeMid >> 8);
    bytes[5] = static_cast<std::uint8_t>(generated.timeMid);
    bytes[6] = static_cast<std::uint8_t>(generated.timeHiAndVersion >> 8);
    bytes[7] = static_cast<std::uint8_t>(generated.timeHiAndVersion);
    bytes[8] = generated.clockSeqHiAndReserved;
    bytes[9] = generated.clockSeqLow;
    std::memcpy(&bytes[10], generated.node, sizeof(generated.node));
    if (!GuestCopyOut_nid_no_patch(uuid, bytes.data(), bytes.size())) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int APS5_VABI sceKernelCreateUuidV7(std::uint8_t* uuid) {
    if (!uuid) return SCE_KERNEL_ERROR_EINVAL;
    std::array<std::uint8_t, 16> bytes{};
    HostEntropy_nid_no_patch(bytes.data() + 6, bytes.size() - 6);
    const std::uint64_t milliseconds = UnixMilliseconds();
    for (int index = 0; index < 6; ++index) bytes[index] = static_cast<std::uint8_t>(milliseconds >> (40 - 8 * index));
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x70);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
    if (!GuestCopyOut_nid_no_patch(uuid, bytes.data(), bytes.size())) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

}
