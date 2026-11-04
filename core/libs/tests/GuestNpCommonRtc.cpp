#include "prx/libSceNpCommon/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceRtc/include/Rtc.hpp"
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

    NpTime now{0};
    CHECK(_ZN3sce2np4Time12GetUserClockEPS1_(&now) == 0 && now.tick > unixEpochTick);
    NpTime time{0};
    const RtcDateTime secondDay{1970, 1, 2, 3, 4, 5, 6};
    CHECK(_ZN3sce2np4Time11SetDateTimeERKNS0_8DateTimeE(&time, &secondDay) == 0 && time.tick == unixEpochTick + 97445000006ull);
    RtcDateTime dateTime{};
    CHECK(_ZN3sce2np4Time11GetDateTimeEPNS0_8DateTimeE(&time, &dateTime) == 0 && std::memcmp(&dateTime, &secondDay, sizeof(dateTime)) == 0);
    CHECK(_ZNK3sce2np4Time18ConvertToPosixTimeEPl(&time, &seconds) == 0 && seconds == 97445);
    CHECK(_ZNK3sce2np4Time18ConvertToPosixTimeEPl(&zero, &seconds) == invalidArgument);

    time = epoch;
    CHECK(_ZN3sce2np4Time15AddMicroSecondsEl(&time, 1) == &time && time.tick == unixEpochTick + 1);
    CHECK(_ZN3sce2np4Time10AddSecondsEl(&time, 2) == &time && time.tick == unixEpochTick + 2000001);
    CHECK(_ZN3sce2np4Time10AddMinutesEl(&time, -1) == &time && time.tick == unixEpochTick - 57999999);
    CHECK(_ZN3sce2np4Time8AddHoursEl(&time, 1) == &time && _ZN3sce2np4Time7AddDaysEl(&time, 1) == &time
          && time.tick == unixEpochTick + 86400000000ull + 3600000000ull - 57999999);
    time = epoch;
    CHECK(_ZN3sce2np4Time8AddHoursEl(&time, 0x100000001ll) == &time && time.tick == unixEpochTick + 3600000000ull);
    const NpTime step{5};
    CHECK(_ZN3sce2np4TimeplERKS1_(&time, &step) == &time && _ZN3sce2np4TimeplERK10SceRtcTick(&time, &step) == &time
          && time.tick == unixEpochTick + 3600000010ull);

    const NpTime low{5};
    const NpTime high{0x8000000000000000ull};
    CHECK(_ZN3sce2npltERKNS0_4TimeES3_(&low, &high) && _ZN3sce2npgtERK10SceRtcTickRKNS0_4TimeE(&high, &low)
          && _ZN3sce2npleERKNS0_4TimeERK10SceRtcTick(&low, &low));
    CHECK(_ZN3sce2npgeERKNS0_4TimeES3_(&high, &low) && !_ZN3sce2npgeERKNS0_4TimeERK10SceRtcTick(&low, &high)
          && !_ZN3sce2npltERK10SceRtcTickRKNS0_4TimeE(&low, &low));
    CHECK(_ZN3sce2npeqERKNS0_4TimeES3_(&low, &step) && _ZN3sce2npneERKNS0_4TimeERK10SceRtcTick(&low, &high)
          && !_ZN3sce2npeqERK10SceRtcTickRKNS0_4TimeE(&low, &high));

    const NpTime start{unixEpochTick};
    const NpTime later{unixEpochTick + 2500000};
    std::int64_t remaining = -1;
    CHECK(!_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(&later, &start, 3000000, &remaining) && remaining == 500000);
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(&later, &start, 2500000, &remaining) && remaining == 0);
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(&start, &later, 3000000, &remaining) && remaining == 0);
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(&later, &start, -1, nullptr));
    std::uint32_t remainingShort = 7;
    CHECK(!_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(&later, &start, 2500001, &remainingShort) && remainingShort == 1);
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(&later, &start, 2500000, &remainingShort) && remainingShort == 0);
    CHECK(!_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&later, &start, 4, &remaining) && remaining == 1);
    CHECK(_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&later, &start, 2, &remaining) && remaining == 0);

    std::memset(text, 'x', sizeof(text));
    CHECK(_ZNK3sce2np4Time13FormatRFC3339EPcmi(&epoch, text, sizeof(text) - 1, 0) == bufferTooSmall && text[0] == 'x');
    CHECK(_ZNK3sce2np4Time13FormatRFC3339EPcmi(&epoch, text, sizeof(text), 0) == 0 && std::strcmp(text, "1970-01-01T00:00:00.00Z") == 0
          && text[sizeof(text) - 1] == '\0');
    const NpTime wrapLow{0};
    const NpTime wrapHigh{~0ull};
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(&wrapLow, &wrapHigh, 10, &remaining) && remaining == 0);
    remainingShort = 7;
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(&wrapLow, &wrapHigh, 10, &remainingShort) && remainingShort == 0);
    CHECK(_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&wrapLow, &wrapHigh, 10, &remaining) && remaining == 0);
    const NpTime pastShort{unixEpochTick + 0x100000001ull};
    CHECK(_ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(&pastShort, &start, 10, &remainingShort) && remainingShort == 0);
    remaining = -1;
    CHECK(_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&later, &start, -1, &remaining) && remaining == 0);
    const NpTime threeSeconds{unixEpochTick + 3000000};
    CHECK(_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&threeSeconds, &start, 3, &remaining) && remaining == 0);
    CHECK(_ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(&later, &start, 1ll << 58, &remaining) && remaining == 0);

    time = epoch;
    CHECK(_ZN3sce2np4Time7AddDaysEl(&time, 0x100000001ll) == &time && time.tick == unixEpochTick + 86400000000ull);

    const RtcTick networkSet{unixEpochTick + 1000000000000ull};
    const RtcTick debugSet{unixEpochTick + 2000000000000ull};
    CHECK(sceRtcSetCurrentNetworkTick(&networkSet) == 0 && sceRtcSetCurrentDebugNetworkTick(&debugSet) == 0);
    NpTime clock{0};
    CHECK(_ZN3sce2np4Time15GetNetworkClockEPS1_(&clock) == 0 && clock.tick >= networkSet.tick && clock.tick < debugSet.tick);
    CHECK(_ZN3sce2np4Time20GetDebugNetworkClockEPS1_(&clock) == 0 && clock.tick >= debugSet.tick);

    const RtcDateTime badDay{1970, 2, 30, 0, 0, 0, 0};
    time = epoch;
    CHECK(_ZN3sce2np4Time11SetDateTimeERKNS0_8DateTimeE(&time, &badDay) < 0 && time.tick == unixEpochTick);

    using Compare = bool (APS5_VABI*)(const NpTime*, const NpTime*);
    const Compare same[] = {_ZN3sce2npeqERKNS0_4TimeES3_, _ZN3sce2npneERKNS0_4TimeES3_, _ZN3sce2npltERKNS0_4TimeES3_,
                            _ZN3sce2npleERKNS0_4TimeES3_, _ZN3sce2npgtERKNS0_4TimeES3_, _ZN3sce2npgeERKNS0_4TimeES3_};
    const Compare tickFirst[] = {_ZN3sce2npeqERK10SceRtcTickRKNS0_4TimeE, _ZN3sce2npneERK10SceRtcTickRKNS0_4TimeE,
                                 _ZN3sce2npltERK10SceRtcTickRKNS0_4TimeE, _ZN3sce2npleERK10SceRtcTickRKNS0_4TimeE,
                                 _ZN3sce2npgtERK10SceRtcTickRKNS0_4TimeE, _ZN3sce2npgeERK10SceRtcTickRKNS0_4TimeE};
    const Compare timeFirst[] = {_ZN3sce2npeqERKNS0_4TimeERK10SceRtcTick, _ZN3sce2npneERKNS0_4TimeERK10SceRtcTick,
                                 _ZN3sce2npltERKNS0_4TimeERK10SceRtcTick, _ZN3sce2npleERKNS0_4TimeERK10SceRtcTick,
                                 _ZN3sce2npgtERKNS0_4TimeERK10SceRtcTick, _ZN3sce2npgeERKNS0_4TimeERK10SceRtcTick};
    const NpTime aliasLow{5 + 0x100000000ull};
    const struct { const NpTime* lhs; const NpTime* rhs; bool expected[6]; } pairs[] = {
        {&low, &high, {false, true, true, true, false, false}},
        {&high, &low, {false, true, false, false, true, true}},
        {&low, &step, {true, false, false, true, false, true}},
        {&low, &aliasLow, {false, true, true, true, false, false}},
    };
    const Compare* const formSets[] = {same, tickFirst, timeFirst};
    for (const Compare* forms : formSets) {
        for (const auto& pair : pairs) {
            for (int op = 0; op < 6; ++op) CHECK(forms[op](pair.lhs, pair.rhs) == pair.expected[op]);
        }
    }
    std::puts("NP common RTC tests passed");
    return 0;
}
