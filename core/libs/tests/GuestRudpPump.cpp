#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
int APS5_VABI sceRudpInit_nid_postfix(void*, int);
int APS5_VABI sceRudpEnd();
int APS5_VABI sceRudpProcessEvents(std::uint64_t timeout);
int APS5_VABI sceRudpNetFlush();
int APS5_VABI sceRudpGetMaxSegmentSize(std::uint16_t*);
int APS5_VABI sceRudpNetReceived(int socketId, const void* data, std::size_t dataSize, const void* sourceAddress, std::uint32_t sourceAddressSize);
int APS5_VABI sceRudpEnableInternalIOThread2(std::uint32_t, std::uint32_t, std::uint64_t);
}

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "rudp check failed at line %d\n", line);
        std::abort();
    }
}
#define CHECK(value) Require((value), __LINE__)

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001);
    constexpr int invalidArgument = static_cast<int>(0x80770004);
    std::uint16_t segmentSize = 0x5a5a;
    CHECK(sceRudpProcessEvents(0) == notInitialized);
    CHECK(sceRudpNetFlush() == notInitialized);
    CHECK(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    CHECK(sceRudpEnableInternalIOThread2(0, 0, 0) == notInitialized);
    CHECK(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    CHECK(segmentSize == 0x5a5a);
    CHECK(sceRudpGetMaxSegmentSize(nullptr) == notInitialized);

    alignas(8) static unsigned char pool[0xC58];
    CHECK(sceRudpInit_nid_postfix(pool, sizeof(pool)) == 0);
    CHECK(sceRudpProcessEvents(0) == 0);
    CHECK(sceRudpNetFlush() == 0);
    CHECK(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == 0);
    CHECK(sceRudpEnableInternalIOThread2(0x4000, 0, 0x3) == 0);
    CHECK(sceRudpGetMaxSegmentSize(nullptr) == invalidArgument);
    CHECK(sceRudpGetMaxSegmentSize(&segmentSize) == 0);
    CHECK(segmentSize == 1410);
    std::uint16_t segment[3] = {0, 0, 0x5a5a};
    CHECK(sceRudpGetMaxSegmentSize(&segment[1]) == 0);
    CHECK(segment[0] == 0 && segment[1] == 1410 && segment[2] == 0x5a5a);
    CHECK(sceRudpEnd() == 0);

    segmentSize = 0x5a5a;
    CHECK(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    CHECK(segmentSize == 0x5a5a);

    CHECK(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    return 0;
}
