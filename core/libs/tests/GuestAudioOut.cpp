#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void TestInvalidChannelConfiguration() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr int errorInvalidParam = static_cast<int>(0x80260001);
    constexpr std::uint32_t len = 256;
    constexpr std::uint32_t freq = 48000;

    auto CheckInvalid = [&](int port, std::uint32_t format, const char* message) {
        const int result = sceAudioOutOpen(userId, port, 0, len, freq, format);
        Require(result == errorInvalidFormat || result == errorInvalidParam, message);
    };

    CheckInvalid(0, 99, "channel configuration 99 rejected");
    CheckInvalid(0, 128, "channel configuration 128 rejected");
    CheckInvalid(0, 255, "channel configuration 255 rejected");
    CheckInvalid(0, 0xFFFFFFFFu, "channel configuration 0xFFFFFFFF rejected");

    CheckInvalid(1, 99, "bgm port channel configuration 99 rejected");
    CheckInvalid(1, 128, "bgm port channel configuration 128 rejected");
    CheckInvalid(1, 255, "bgm port channel configuration 255 rejected");
    CheckInvalid(1, 0xFFFFFFFFu, "bgm port channel configuration 0xFFFFFFFF rejected");

    CheckInvalid(2, 99, "voice port channel configuration 99 rejected");
    CheckInvalid(2, 128, "voice port channel configuration 128 rejected");
    CheckInvalid(2, 255, "voice port channel configuration 255 rejected");
    CheckInvalid(2, 0xFFFFFFFFu, "voice port channel configuration 0xFFFFFFFF rejected");

    CheckInvalid(3, 99, "personal port channel configuration 99 rejected");
    CheckInvalid(3, 128, "personal port channel configuration 128 rejected");
    CheckInvalid(3, 255, "personal port channel configuration 255 rejected");
    CheckInvalid(3, 0xFFFFFFFFu, "personal port channel configuration 0xFFFFFFFF rejected");

    CheckInvalid(4, 99, "padspk port channel configuration 99 rejected");
    CheckInvalid(4, 128, "padspk port channel configuration 128 rejected");
    CheckInvalid(4, 255, "padspk port channel configuration 255 rejected");
    CheckInvalid(4, 0xFFFFFFFFu, "padspk port channel configuration 0xFFFFFFFF rejected");

    CheckInvalid(10, 99, "vibration port channel configuration 99 rejected");
    CheckInvalid(10, 128, "vibration port channel configuration 128 rejected");
    CheckInvalid(10, 255, "vibration port channel configuration 255 rejected");
    CheckInvalid(10, 0xFFFFFFFFu, "vibration port channel configuration 0xFFFFFFFF rejected");
}

}

int main() {
    TestInvalidChannelConfiguration();
    return 0;
}
