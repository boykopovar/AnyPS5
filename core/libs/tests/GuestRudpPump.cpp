#include "prx/libSceRudp/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

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
    constexpr int notAcceptable = static_cast<int>(0x80770011);
    std::uint16_t segmentSize = 0x5a5a;
    CHECK(sceRudpProcessEvents(0) == notInitialized);
    CHECK(sceRudpNetFlush() == notInitialized);
    CHECK(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    CHECK(sceRudpEnableInternalIOThread2(0, 0, 0) == notInitialized);
    CHECK(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    CHECK(segmentSize == 0x5a5a);
    CHECK(sceRudpGetMaxSegmentSize(nullptr) == notInitialized);
    CHECK(sceRudpSetMaxSegmentSize(1200) == notInitialized);

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
    CHECK(sceRudpSetMaxSegmentSize(1200) == 0);
    int context = -1;
    CHECK(sceRudpCreateContext(nullptr, nullptr, &context) == 0);
    CHECK(sceRudpSetMaxSegmentSize(1000) == notAcceptable);
    CHECK(sceRudpTerminate(context) == 0);
    CHECK(sceRudpSetMaxSegmentSize(1000) == 0);
    CHECK(sceRudpEnd() == 0);

    segmentSize = 0x5a5a;
    CHECK(sceRudpGetMaxSegmentSize(&segmentSize) == notInitialized);
    CHECK(segmentSize == 0x5a5a);

    CHECK(sceRudpNetReceived(0, nullptr, 0, nullptr, 0) == notInitialized);
    return 0;
}
