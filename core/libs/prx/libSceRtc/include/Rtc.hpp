#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRtcCheckValid(const RtcDateTime* time);
int APS5_VABI sceRtcIsLeapYear(int year);
int APS5_VABI sceRtcGetDaysInMonth(int year, int month);
int APS5_VABI sceRtcGetDayOfWeek(int year, int month, int day);
int APS5_VABI sceRtcGetTickResolution(void);
int APS5_VABI sceRtcGetTick(const RtcDateTime* time, RtcTick* tick);
int APS5_VABI sceRtcSetTick(RtcDateTime* time, const RtcTick* tick);
int APS5_VABI sceRtcGetCurrentTick(RtcTick* tick);
int APS5_VABI sceRtcGetCurrentNetworkTick(RtcTick* tick);
int APS5_VABI sceRtcGetCurrentClock(RtcDateTime* time, int timeZoneMinutes);
int APS5_VABI sceRtcGetCurrentClockLocalTime(RtcDateTime* time);
int APS5_VABI sceRtcConvertUtcToLocalTime(const RtcTick* utc, RtcTick* localTime);
int APS5_VABI sceRtcConvertLocalTimeToUtc(const RtcTick* localTime, RtcTick* utc);
int APS5_VABI sceRtcGetTime_t(const RtcDateTime* time, int64_t* seconds);
int APS5_VABI sceRtcSetTime_t(RtcDateTime* time, int64_t seconds);
int APS5_VABI sceRtcGetWin32FileTime(const RtcDateTime* time, uint64_t* win32Time);
int APS5_VABI sceRtcSetWin32FileTime(RtcDateTime* time, uint64_t win32Time);
int APS5_VABI sceRtcGetDosTime(const RtcDateTime* time, uint32_t* dosTime);
int APS5_VABI sceRtcSetDosTime(RtcDateTime* time, uint32_t dosTime);
int APS5_VABI sceRtcFormatRFC3339(char* dateTime, const RtcTick* utc, int timeZoneMinutes);
int APS5_VABI sceRtcFormatRFC3339LocalTime(char* dateTime, const RtcTick* utc);
int APS5_VABI sceRtcFormatRFC2822(char* dateTime, const RtcTick* utc, int timeZoneMinutes);
int APS5_VABI sceRtcFormatRFC2822LocalTime(char* dateTime, const RtcTick* utc);
int APS5_VABI sceRtcParseRFC3339(RtcTick* utc, const char* dateTime);
int APS5_VABI sceRtcParseDateTime(RtcTick* utc, const char* dateTime);
int APS5_VABI sceRtcTickAddTicks(RtcTick* dst, const RtcTick* src, int64_t ticks);
int APS5_VABI sceRtcTickAddMicroseconds(RtcTick* dst, const RtcTick* src, int64_t usec);
int APS5_VABI sceRtcTickAddSeconds(RtcTick* dst, const RtcTick* src, int64_t seconds);
int APS5_VABI sceRtcTickAddMinutes(RtcTick* dst, const RtcTick* src, int64_t minutes);
int APS5_VABI sceRtcTickAddHours(RtcTick* dst, const RtcTick* src, int32_t hours);
int APS5_VABI sceRtcTickAddDays(RtcTick* dst, const RtcTick* src, int32_t days);
int APS5_VABI sceRtcTickAddWeeks(RtcTick* dst, const RtcTick* src, int32_t weeks);
int APS5_VABI sceRtcTickAddMonths(RtcTick* dst, const RtcTick* src, int32_t months);
int APS5_VABI sceRtcTickAddYears(RtcTick* dst, const RtcTick* src, int16_t years);
int APS5_VABI sceRtcInit(void);
int APS5_VABI sceRtcEnd(void);
int APS5_VABI sceRtcCompareTick(const RtcTick* first, const RtcTick* second);
int APS5_VABI sceRtcGetCurrentRawNetworkTick(RtcTick* tick);
int APS5_VABI sceRtcGetCurrentAdNetworkTick(RtcTick* tick);
int APS5_VABI sceRtcGetCurrentDebugNetworkTick(RtcTick* tick);
int APS5_VABI sceRtcSetCurrentTick(const RtcTick* tick);
int APS5_VABI sceRtcSetCurrentNetworkTick(const RtcTick* tick);
int APS5_VABI sceRtcSetCurrentAdNetworkTick(const RtcTick* tick);
int APS5_VABI sceRtcSetCurrentDebugNetworkTick(const RtcTick* tick);
int APS5_VABI sceRtcSetConf(int first, int second, int third, int fourth);
int APS5_VABI sceRtcFormatRFC3339Precise(char* dateTime, const RtcTick* utc, int timeZoneMinutes, int precision);
int APS5_VABI sceRtcFormatRFC3339PreciseLocalTime(char* dateTime, const RtcTick* utc, int precision);

}
