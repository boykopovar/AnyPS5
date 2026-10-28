#include "prx/libSceNpCommon/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "NP common RTC check failed at line %d\n", line);
    std::abort();
}
#define CHECK(value) Require((value), __LINE__)

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80558009u);
    constexpr int bufferTooSmall = static_cast<int>(0x80558008u);
    constexpr std::uint64_t unixEpochTick = 62135596800000000ull;
    std::int64_t seconds = -1;
    const RtcTick zero{0};
    CHECK(sceNpRtcConvertToPosixTime(&zero, &seconds) == invalidArgument && seconds == -1);
    const RtcTick epoch{unixEpochTick};
    CHECK(sceNpRtcConvertToPosixTime(&epoch, &seconds) == 0 && seconds == 0);
    const RtcTick dayLater{unixEpochTick + 86400000000ull + 1500000ull};
    CHECK(sceNpRtcConvertToPosixTime(&dayLater, &seconds) == 0 && seconds == 86401);

    char text[0x80];
    std::memset(text, 'x', sizeof(text));
    CHECK(sceNpRtcFormatRFC3339(text, sizeof(text) - 1, &epoch, 0) == bufferTooSmall && text[0] == 'x');
    CHECK(sceNpRtcFormatRFC3339(text, sizeof(text), &epoch, 0) == 0 && std::strcmp(text, "1970-01-01T00:00:00.00Z") == 0);
    CHECK(sceNpRtcFormatRFC3339(text, sizeof(text), &epoch, 90) == 0 && std::strcmp(text, "1970-01-01T01:30:00.00+01:30") == 0);
    std::puts("NP common RTC tests passed");
    return 0;
}
