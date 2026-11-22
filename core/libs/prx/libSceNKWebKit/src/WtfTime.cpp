// Derived from Source/WTF/wtf/TimingScope.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// Derived from Source/WTF/wtf/DateMath.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010 &yet, LLC. (nate@andyet.net)
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.

 * Copyright 2006-2008 the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace NKWebKit {
double CurrentWallTime();
double CurrentMonotonicTime();
}  // namespace NKWebKit

namespace WTF {

static double currentWallTimeSeconds() {
    return NKWebKit::CurrentWallTime();
}

static double currentMonotonicTimeSeconds() {
    return NKWebKit::CurrentMonotonicTime();
}

static bool isLeapYear(int year) {
    if (year % 4 != 0) {
        return false;
    }
    if (year % 100 != 0) {
        return true;
    }
    return (year % 400 == 0);
}

static int dayInYear(int year, int month, int day) {
    static const int firstDayOfMonth[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
    bool leap = isLeapYear(year);
    int m = (month >= 0 && month < 12) ? month : 0;
    return firstDayOfMonth[leap ? 1 : 0][m] + day - 1;
}

static double daysFrom1970ToYear(int year) {
    const double yMinus1 = static_cast<double>(year) - 1.0;
    const double by4 = std::floor(yMinus1 / 4.0) - (1970 / 4);
    const double by100 = std::floor(yMinus1 / 100.0) - (1970 / 100);
    const double by400 = std::floor(yMinus1 / 400.0) - (1970 / 400);
    return 365.0 * (year - 1970.0) + by4 - by100 + by400;
}

static double ymdhmsToMilliseconds(int year, long mon, long day, long hour, long minute, long second, double milliseconds) {
    static const int firstDayOfMonth[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
    int mday = firstDayOfMonth[isLeapYear(year) ? 1 : 0][mon - 1];
    double ydays = daysFrom1970ToYear(year);
    double dateMs = milliseconds + second * 1000.0 + minute * 60000.0 + hour * 3600000.0 + (mday + day - 1 + ydays) * 86400000.0;
    if (dateMs < -8640000000000000.0 || dateMs > 8640000000000000.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return dateMs;
}

static bool parseInt(const char* string, char** stopPosition, int* result) {
    const std::int64_t value = strtol_nid_postfix(string, stopPosition, 10);
    if (string == *stopPosition || value <= std::numeric_limits<int>::min() || value >= std::numeric_limits<int>::max()) {
        return false;
    }
    *result = static_cast<int>(value);
    return true;
}

static bool parseLong(const char* string, char** stopPosition, std::int64_t* result) {
    *result = strtol_nid_postfix(string, stopPosition, 10);
    return string != *stopPosition && *result != std::numeric_limits<std::int64_t>::min() && *result != std::numeric_limits<std::int64_t>::max();
}

static const char* parseES5DatePortion(const char* currentPosition, int& year, long& month, long& day) {
    char* postParse = nullptr;
    if (!parseInt(currentPosition, &postParse, &year)) {
        return nullptr;
    }
    if (*currentPosition == '-' && year == 0) {
        return nullptr;
    }
    if (*postParse != '-') {
        return postParse;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    month = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    if (*postParse != '-') {
        return postParse;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    day = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    return postParse;
}

static const char* parseES5TimePortion(const char* currentPosition, long& hours, long& minutes, long& seconds, double& milliseconds, bool& isLocalTime, long& timeZoneSeconds) {
    isLocalTime = false;
    char* postParse = nullptr;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    hours = std::strtol(currentPosition, &postParse, 10);
    if (*postParse != ':' || (postParse - currentPosition) != 2) {
        return nullptr;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    minutes = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    currentPosition = postParse;

    if (*currentPosition == ':') {
        ++currentPosition;
        if (*currentPosition < '0' || *currentPosition > '9') {
            return nullptr;
        }
        seconds = std::strtol(currentPosition, &postParse, 10);
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
        if (*postParse == '.') {
            currentPosition = postParse + 1;
            if (*currentPosition < '0' || *currentPosition > '9') {
                return nullptr;
            }
            std::int64_t frac = 0;
            if (!parseLong(currentPosition, &postParse, &frac)) {
                return nullptr;
            }
            const std::int64_t numFracDigits = postParse - currentPosition;
            milliseconds = frac * std::pow(10.0, static_cast<double>(-numFracDigits + 3));
        }
        currentPosition = postParse;
    }

    if (*currentPosition == 'Z') {
        return currentPosition + 1;
    }

    bool tzNegative = false;
    if (*currentPosition == '-') {
        tzNegative = true;
    } else if (*currentPosition == '+') {
        tzNegative = false;
    } else {
        isLocalTime = true;
        return currentPosition;
    }
    ++currentPosition;

    long tzHours = 0;
    long tzHoursAbs = 0;
    long tzMinutes = 0;

    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    tzHours = std::strtol(currentPosition, &postParse, 10);
    if (*postParse != ':') {
        if ((postParse - currentPosition) == 2) {
            tzHoursAbs = std::labs(tzHours);
        } else if ((postParse - currentPosition) == 4) {
            tzHoursAbs = std::labs(tzHours);
            tzMinutes = tzHoursAbs % 100;
            tzHoursAbs = tzHoursAbs / 100;
        } else {
            return nullptr;
        }
    } else {
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
        tzHoursAbs = std::labs(tzHours);
        currentPosition = postParse + 1;
        if (*currentPosition < '0' || *currentPosition > '9') {
            return nullptr;
        }
        tzMinutes = std::strtol(currentPosition, &postParse, 10);
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
    }
    currentPosition = postParse;
    if (tzHoursAbs > 24 || tzMinutes < 0 || tzMinutes > 59) {
        return nullptr;
    }
    timeZoneSeconds = 60 * (tzMinutes + (60 * tzHoursAbs));
    if (tzNegative) {
        timeZoneSeconds = -timeZoneSeconds;
    }
    return currentPosition;
}

}  // namespace WTF

namespace {

struct TimingCallData {
    double totalDuration{0.0};
    unsigned callCount{0};
    double maxDuration{0.0};
};

class TimingTotals {
public:
    TimingCallData Add(const char* name, double duration) {
        std::lock_guard lock(m_lock);
        auto& result = m_totals[name];
        ++result.callCount;
        result.maxDuration = std::max(result.maxDuration, duration);
        result.totalDuration += duration;
        return result;
    }

private:
    std::mutex m_lock;
    std::unordered_map<const char*, TimingCallData> m_totals;
};

TimingTotals& Totals() {
    static TimingTotals* totals = new TimingTotals();
    return *totals;
}

template <typename T>
bool loadTime(T& value, const T* source) {
    return GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
}

WTF::WallTime approximateWallTime(double monotonic) {
    if (std::isinf(monotonic)) {
        return WTF::WallTime{monotonic};
    }
    return WTF::WallTime{monotonic - WTF::currentMonotonicTimeSeconds() + WTF::currentWallTimeSeconds()};
}

WTF::MonotonicTime approximateMonotonicTime(double wall) {
    if (std::isinf(wall)) {
        return WTF::MonotonicTime{wall};
    }
    return WTF::MonotonicTime{wall - WTF::currentWallTimeSeconds() + WTF::currentMonotonicTimeSeconds()};
}

}  // namespace

extern "C" {
WTF::MonotonicTime APS5_VABI _ZN3WTF13MonotonicTime3nowEv() {
    return WTF::MonotonicTime{WTF::currentMonotonicTimeSeconds()};
}

void APS5_VABI _ZN3WTF11TimingScope11scopeDidEndEv(WTF::TimingScope* self) {
    if (!self) throw std::invalid_argument("TimingScope::scopeDidEnd: null scope");
    WTF::TimingScope scope;
    if (!loadTime(scope, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!scope.m_name) throw std::invalid_argument("TimingScope::scopeDidEnd: null name");
    if (!scope.m_logIterationInterval) throw std::invalid_argument("TimingScope::scopeDidEnd: zero log iteration interval");
    const double duration = _ZN3WTF13MonotonicTime3nowEv().m_value - scope.m_startTime.m_value;
    const auto data = Totals().Add(scope.m_name, duration);
    if (!(data.callCount % scope.m_logIterationInterval))
        WTFLogAlways("%s: %u calls, mean duration: %.6fms, total duration: %.6fms, max duration %.6fms", scope.m_name, data.callCount,
            data.totalDuration / data.callCount * 1000.0, data.totalDuration * 1000.0, data.maxDuration * 1000.0);
}

WTF::WallTime APS5_VABI _ZN3WTF8WallTime3nowEv() {
    return WTF::WallTime{WTF::currentWallTimeSeconds()};
}

WTF::WallTime APS5_VABI _ZNK3WTF13MonotonicTime19approximateWallTimeEv(const WTF::MonotonicTime* self) {
    WTF::MonotonicTime time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return approximateWallTime(time.m_value);
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF8WallTime24approximateMonotonicTimeEv(const WTF::WallTime* self) {
    WTF::WallTime time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return approximateMonotonicTime(time.m_value);
}

WTF::TimeWithDynamicClockType APS5_VABI _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(WTF::ClockType type) {
    switch (type) {
    case WTF::ClockType::Wall:
        return WTF::TimeWithDynamicClockType{WTF::currentWallTimeSeconds(), WTF::ClockType::Wall};
    case WTF::ClockType::Monotonic:
        return WTF::TimeWithDynamicClockType{WTF::currentMonotonicTimeSeconds(), WTF::ClockType::Monotonic};
    case WTF::ClockType::Approximate:
        return WTF::TimeWithDynamicClockType{WTF::currentMonotonicTimeSeconds(), WTF::ClockType::Approximate};
    }
    return WTF::TimeWithDynamicClockType{0.0, WTF::ClockType::Wall};
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType13monotonicTimeEv(const WTF::TimeWithDynamicClockType* self) {
    WTF::TimeWithDynamicClockType time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::MonotonicTime{time.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType8wallTimeEv(const WTF::TimeWithDynamicClockType* self) {
    WTF::TimeWithDynamicClockType time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::WallTime{time.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv(const WTF::TimeWithDynamicClockType* self) {
    WTF::TimeWithDynamicClockType time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (time.m_type == WTF::ClockType::Wall) {
        return WTF::WallTime{time.m_value};
    }
    return approximateWallTime(time.m_value);
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv(const WTF::TimeWithDynamicClockType* self) {
    WTF::TimeWithDynamicClockType time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (time.m_type == WTF::ClockType::Monotonic || time.m_type == WTF::ClockType::Approximate) {
        return WTF::MonotonicTime{time.m_value};
    }
    return approximateMonotonicTime(time.m_value);
}

WTF::TimeWithDynamicClockType APS5_VABI _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(const WTF::TimeWithDynamicClockType* self) {
    WTF::TimeWithDynamicClockType time;
    if (!loadTime(time, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(time.m_type);
}

WTF::Seconds APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    WTF::TimeWithDynamicClockType time;
    WTF::TimeWithDynamicClockType right;
    if (!loadTime(time, self) || !loadTime(right, &other)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::Seconds{time.m_value - right.m_value};
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    WTF::TimeWithDynamicClockType time;
    WTF::TimeWithDynamicClockType right;
    if (!loadTime(time, self) || !loadTime(right, &other)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return time.m_value > right.m_value;
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    WTF::TimeWithDynamicClockType time;
    WTF::TimeWithDynamicClockType right;
    if (!loadTime(time, self) || !loadTime(right, &other)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return time.m_value >= right.m_value;
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    WTF::TimeWithDynamicClockType time;
    WTF::TimeWithDynamicClockType right;
    if (!loadTime(time, self) || !loadTime(right, &other)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return time.m_value <= right.m_value;
}

bool APS5_VABI _ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(const WTF::TimeWithDynamicClockType& time) {
    WTF::TimeWithDynamicClockType deadline;
    if (!loadTime(deadline, &time)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!(deadline.m_value > 0.0)) {
        return true;
    }
    if (std::isinf(deadline.m_value)) {
        return false;
    }
    WTF::TimeWithDynamicClockType now = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(deadline.m_type);
    return deadline.m_value <= now.m_value;
}

void APS5_VABI _ZN3WTF5sleepERKNS_24TimeWithDynamicClockTypeE(const WTF::TimeWithDynamicClockType& time) {
    WTF::TimeWithDynamicClockType deadline;
    if (!loadTime(deadline, &time)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    WTF::TimeWithDynamicClockType now = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(deadline.m_type);
    double diff = deadline.m_value - now.m_value;
    if (diff > 0.0) {
        _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{diff});
    }
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF7SecondsmiENS_13MonotonicTimeE(const WTF::Seconds* self, WTF::MonotonicTime other) {
    WTF::Seconds seconds;
    if (!loadTime(seconds, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::MonotonicTime{seconds.m_value - other.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF7SecondsmiENS_8WallTimeE(const WTF::Seconds* self, WTF::WallTime other) {
    WTF::Seconds seconds;
    if (!loadTime(seconds, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::WallTime{seconds.m_value - other.m_value};
}

WTF::TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsmiERKNS_24TimeWithDynamicClockTypeE(const WTF::Seconds* self, const WTF::TimeWithDynamicClockType& other) {
    WTF::Seconds seconds;
    WTF::TimeWithDynamicClockType right;
    if (!loadTime(seconds, self) || !loadTime(right, &other)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::TimeWithDynamicClockType{seconds.m_value - right.m_value, right.m_type};
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF7SecondsplENS_13MonotonicTimeE(const WTF::Seconds* self, WTF::MonotonicTime other) {
    WTF::Seconds seconds;
    if (!loadTime(seconds, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::MonotonicTime{seconds.m_value + other.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF7SecondsplENS_8WallTimeE(const WTF::Seconds* self, WTF::WallTime other) {
    WTF::Seconds seconds;
    if (!loadTime(seconds, self)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WTF::WallTime{seconds.m_value + other.m_value};
}

double APS5_VABI _ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(const WTF::CPUTime* self, const WTF::CPUTime& reference) {
    WTF::CPUTime current;
    WTF::CPUTime base;
    if (!loadTime(current, self) || !loadTime(base, &reference)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    double cpuUsageDelta = (current.systemTime.m_value + current.userTime.m_value) - (base.systemTime.m_value + base.userTime.m_value);
    double cpuTimeDelta = current.cpuTime.m_value - base.cpuTime.m_value;
    return cpuUsageDelta * 100.0 / cpuTimeDelta;
}

void APS5_VABI _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(WTF::GregorianDateTime* self) {
    const std::int64_t localTime = time_nid_postfix(nullptr);
    GuestTm localTM{};
    if (localtime_r_nid_postfix(&localTime, &localTM) == nullptr) {
        throw std::overflow_error("setToCurrentLocalTime: localtime_r failed");
    }
    WTF::GregorianDateTime date;
    date.m_year = localTM.tm_year + 1900;
    date.m_month = localTM.tm_mon;
    date.m_monthDay = localTM.tm_mday;
    date.m_yearDay = localTM.tm_yday;
    date.m_weekDay = localTM.tm_wday;
    date.m_hour = localTM.tm_hour;
    date.m_minute = localTM.tm_min;
    date.m_second = localTM.tm_sec;
    date.m_isDST = localTM.tm_isdst;
    date.m_utcOffsetInMinute = static_cast<int>(localTM.tm_gmtoff / 60);
    if (!GuestCopyOut_nid_no_patch(self, &date, sizeof(date))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

static int32_t calculateUTCOffset() {
    GuestTm localt{};
    localt.tm_mday = 1;
    localt.tm_year = 119;
    const std::int64_t localTimestamp = mktime_nid_postfix(&localt);
    if (localTimestamp == -1) {
        throw std::runtime_error("calculateUTCOffset: mktime failed");
    }
    return static_cast<int32_t>((1546300800 - localTimestamp) * 1000);
}

WTF::LocalTimeOffset APS5_VABI _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(double ms, WTF::TimeType inputTimeType) {
    if (std::isnan(ms) || std::isinf(ms)) {
        throw std::invalid_argument("calculateLocalTimeOffset: NaN or Inf timestamp");
    }
    double localToUTCTimeOffset = (inputTimeType == WTF::TimeType::LocalTime) ? calculateUTCOffset() : 0.0;
    double localTimeSeconds = (ms - localToUTCTimeOffset) / 1000.0;
    constexpr double maxUnixTime = 4102444740.0;
    if (localTimeSeconds > maxUnixTime) {
        localTimeSeconds = maxUnixTime;
    } else if (localTimeSeconds < 0.0) {
        localTimeSeconds += 86400.0;
    }
    constexpr double minRepresentableTimeT = -9223372036854775808.0;
    if (localTimeSeconds < minRepresentableTimeT) {
        throw std::overflow_error("calculateLocalTimeOffset: timestamp out of representable time_t range");
    }
    const std::int64_t localTime = static_cast<std::int64_t>(localTimeSeconds);
    GuestTm localTM{};
    if (localtime_r_nid_postfix(&localTime, &localTM) == nullptr) {
        throw std::overflow_error("calculateLocalTimeOffset: localtime_r failed");
    }
    return WTF::LocalTimeOffset{localTM.tm_isdst > 0, static_cast<int>(localTM.tm_gmtoff * 1000)};
}

double APS5_VABI _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb(const char* dateString, bool& isLocalTime) {
    isLocalTime = false;
    static const long daysPerMonth[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = 0;
    long month = 1;
    long day = 1;
    long hours = 0;
    long minutes = 0;
    long seconds = 0;
    double milliseconds = 0.0;
    long timeZoneSeconds = 0;

    const char* current = WTF::parseES5DatePortion(dateString, year, month, day);
    if (!current) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (*current == 'T' || *current == 't' || *current == ' ') {
        current = WTF::parseES5TimePortion(current + 1, hours, minutes, seconds, milliseconds, isLocalTime, timeZoneSeconds);
        if (!current) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    if (*current != '\0') {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (month < 1 || month > 12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (day < 1 || day > daysPerMonth[month - 1]) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (month == 2 && day > 28 && !WTF::isLeapYear(year)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (hours < 0 || hours > 24) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (hours == 24 && (minutes || seconds)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (minutes < 0 || minutes > 59) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (seconds < 0 || seconds >= 61) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (seconds == 60) {
        milliseconds = 0.0;
    }
    return WTF::ymdhmsToMilliseconds(year, month, day, hours, minutes, seconds, milliseconds) - (timeZoneSeconds * 1000.0);
}

}
