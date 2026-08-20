#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceRtcCheckValid(const RtcDateTime* time) {
 (void)time;
 return 0;
}

int sceRtcConvertLocalTimeToUtc(const RtcTick* local_time, RtcTick* utc) {
 (void)local_time;
 (void)utc;
 return 0;
}

int sceRtcConvertUtcToLocalTime(const RtcTick* utc, RtcTick* local_time) {
 (void)utc;
 (void)local_time;
 return 0;
}

int sceRtcFormatRFC3339(char* date_time, const RtcTick* utc, int time_zone_minutes) {
 (void)date_time;
 (void)utc;
 (void)time_zone_minutes;
 return 0;
}

int sceRtcGetCurrentClock(RtcDateTime* time, int time_zone_minutes) {
 (void)time;
 (void)time_zone_minutes;
 return 0;
}

int sceRtcGetCurrentClockLocalTime(RtcDateTime* time) {
 (void)time;
 return 0;
}

int sceRtcGetCurrentNetworkTick(RtcTick* tick) {
 (void)tick;
 return 0;
}

int sceRtcGetCurrentTick(RtcTick* tick) {
 (void)tick;
 return 0;
}

int sceRtcGetDayOfWeek(int year, int month, int day) {
 (void)year;
 (void)month;
 (void)day;
 return 0;
}

int sceRtcGetTick(const RtcDateTime* time, RtcTick* tick) {
 (void)time;
 (void)tick;
 return 0;
}

int sceRtcGetTickResolution(void) {
 return 0;
}

int sceRtcGetTime_t(const RtcDateTime* time, int64_t* seconds) {
 (void)time;
 (void)seconds;
 return 0;
}

int sceRtcGetWin32FileTime(const RtcDateTime* time, uint64_t* win32_time) {
 (void)time;
 (void)win32_time;
 return 0;
}

int sceRtcIsLeapYear(int year) {
 (void)year;
 return 0;
}

int sceRtcParseRFC3339(RtcTick* utc, const char* date_time) {
 (void)utc;
 (void)date_time;
 return 0;
}

int sceRtcSetTick(RtcDateTime* time, const RtcTick* tick) {
 (void)time;
 (void)tick;
 return 0;
}

int sceRtcSetTime_t(RtcDateTime* time, int64_t seconds) {
 (void)time;
 (void)seconds;
 return 0;
}

int sceRtcSetWin32FileTime(RtcDateTime* time, uint64_t win32_time) {
 (void)time;
 (void)win32_time;
 return 0;
}

int sceRtcTickAddDays(RtcTick* dst, const RtcTick* src, int32_t days) {
 (void)dst;
 (void)src;
 (void)days;
 return 0;
}

int sceRtcTickAddHours(RtcTick* dst, const RtcTick* src, int32_t hours) {
 (void)dst;
 (void)src;
 (void)hours;
 return 0;
}

int sceRtcTickAddMicroseconds(RtcTick* dst, const RtcTick* src, int64_t usec) {
 (void)dst;
 (void)src;
 (void)usec;
 return 0;
}

int sceRtcTickAddMinutes(RtcTick* dst, const RtcTick* src, int64_t minutes) {
 (void)dst;
 (void)src;
 (void)minutes;
 return 0;
}

int sceRtcTickAddSeconds(RtcTick* dst, const RtcTick* src, int64_t seconds) {
 (void)dst;
 (void)src;
 (void)seconds;
 return 0;
}

int sceRtcTickAddTicks(RtcTick* dst, const RtcTick* src, int64_t ticks) {
 (void)dst;
 (void)src;
 (void)ticks;
 return 0;
}

int sceRtcTickAddWeeks(RtcTick* dst, const RtcTick* src, int32_t weeks) {
 (void)dst;
 (void)src;
 (void)weeks;
 return 0;
}

}
