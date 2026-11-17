#include "prx/libSceRudp/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001);
    constexpr int invalidArgument = static_cast<int>(0x80770004);
    constexpr int notAcceptable = static_cast<int>(0x80770011);
    std::uint16_t segmentSize = 0x5a5a;
    Require(sceRudpProcessEvents(0) == notInitialized);
    Require(sceRudpNetFlush() == notInitialized);
    Require(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    Require(sceRudpEnableInternalIOThread2(0, 0, 0) == notInitialized);
    Require(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    Require(segmentSize == 0x5a5a);
    Require(sceRudpGetMaxSegmentSize(nullptr) == notInitialized);
    Require(sceRudpSetMaxSegmentSize(1200) == notInitialized);

    alignas(8) static unsigned char pool[0xC58];
    Require(sceRudpInit_nid_postfix(pool, sizeof(pool)) == 0);
    Require(sceRudpProcessEvents(0) == 0);
    Require(sceRudpNetFlush() == 0);
    Require(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == 0);
    Require(sceRudpEnableInternalIOThread2(0x4000, 0, 0x3) == 0);
    Require(sceRudpGetMaxSegmentSize(nullptr) == invalidArgument);
    Require(sceRudpGetMaxSegmentSize(&segmentSize) == 0);
    Require(segmentSize == 1410);
    std::uint16_t segment[3] = {0, 0, 0x5a5a};
    Require(sceRudpGetMaxSegmentSize(&segment[1]) == 0);
    Require(segment[0] == 0 && segment[1] == 1410 && segment[2] == 0x5a5a);
    Require(sceRudpSetMaxSegmentSize(1200) == 0);
    int context = -1;
    Require(sceRudpCreateContext(nullptr, nullptr, &context) == 0);
    Require(sceRudpSetMaxSegmentSize(1000) == notAcceptable);
    Require(sceRudpTerminate(context) == 0);
    Require(sceRudpSetMaxSegmentSize(1000) == 0);
    Require(sceRudpEnd() == 0);

    segmentSize = 0x5a5a;
    Require(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    Require(segmentSize == 0x5a5a);

    Require(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    return 0;
}
