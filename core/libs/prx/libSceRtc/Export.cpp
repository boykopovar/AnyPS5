#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceRtcCheckValid(const RtcDateTime* time) {
 (void)time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcConvertLocalTimeToUtc(const RtcTick* local_time, RtcTick* utc) {
 (void)local_time;
 (void)utc;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcConvertUtcToLocalTime(const RtcTick* utc, RtcTick* local_time) {
 (void)utc;
 (void)local_time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcFormatRFC3339(char* date_time, const RtcTick* utc, int time_zone_minutes) {
 (void)date_time;
 (void)utc;
 (void)time_zone_minutes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetCurrentClock(RtcDateTime* time, int time_zone_minutes) {
 (void)time;
 (void)time_zone_minutes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetCurrentClockLocalTime(RtcDateTime* time) {
 (void)time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetCurrentNetworkTick(RtcTick* tick) {
 (void)tick;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetCurrentTick(RtcTick* tick) {
 (void)tick;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetDayOfWeek(int year, int month, int day) {
 (void)year;
 (void)month;
 (void)day;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetTick(const RtcDateTime* time, RtcTick* tick) {
 (void)time;
 (void)tick;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetTickResolution(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetTime_t(const RtcDateTime* time, int64_t* seconds) {
 (void)time;
 (void)seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcGetWin32FileTime(const RtcDateTime* time, uint64_t* win32_time) {
 (void)time;
 (void)win32_time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcIsLeapYear(int year) {
 (void)year;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcParseRFC3339(RtcTick* utc, const char* date_time) {
 (void)utc;
 (void)date_time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcSetTick(RtcDateTime* time, const RtcTick* tick) {
 (void)time;
 (void)tick;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcSetTime_t(RtcDateTime* time, int64_t seconds) {
 (void)time;
 (void)seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcSetWin32FileTime(RtcDateTime* time, uint64_t win32_time) {
 (void)time;
 (void)win32_time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddDays(RtcTick* dst, const RtcTick* src, int32_t days) {
 (void)dst;
 (void)src;
 (void)days;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddHours(RtcTick* dst, const RtcTick* src, int32_t hours) {
 (void)dst;
 (void)src;
 (void)hours;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddMicroseconds(RtcTick* dst, const RtcTick* src, int64_t usec) {
 (void)dst;
 (void)src;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddMinutes(RtcTick* dst, const RtcTick* src, int64_t minutes) {
 (void)dst;
 (void)src;
 (void)minutes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddSeconds(RtcTick* dst, const RtcTick* src, int64_t seconds) {
 (void)dst;
 (void)src;
 (void)seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddTicks(RtcTick* dst, const RtcTick* src, int64_t ticks) {
 (void)dst;
 (void)src;
 (void)ticks;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRtcTickAddWeeks(RtcTick* dst, const RtcTick* src, int32_t weeks) {
 (void)dst;
 (void)src;
 (void)weeks;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
