#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestTime.hpp"

namespace {

constexpr int timeUtc = 1;
constexpr std::int64_t nanosPerSecond = 1000000000;
constexpr std::int64_t secondsPerDay = 86400;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

template <typename T>
bool CopyIn(const void* source, T& out) {
    if (source == nullptr) return false;
    const ErrnoGuard savedErrno;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, sizeof(T));
    return GuestReadable_nid_no_patch(source, sizeof(T)) &&
        GuestCopyIn_nid_no_patch(&out, source, sizeof(T));
}

bool CopyOutBytes(void* destination, const void* source, std::size_t size) {
    if (destination == nullptr) return false;
    const ErrnoGuard savedErrno;
    GuestArena::HostWrite write(destination, size);
    return write.Open() && GuestWritable_nid_no_patch(destination, size) &&
        GuestCopyOut_nid_no_patch(destination, source, size);
}

template <typename T>
bool CopyOut(void* destination, const T& value) {
    return CopyOutBytes(destination, &value, sizeof(T));
}

std::int64_t floorDivide(std::int64_t value, std::int64_t divisor) {
    return value >= 0 ? value / divisor : -((-(value + 1)) / divisor + 1);
}

std::int64_t daysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = floorDivide(year, 400);
    const std::int64_t yearOfEra = year - era * 400;
    const std::int64_t dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

std::int64_t wrappingAdd(std::int64_t left, std::int64_t right) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
}

Xtime LoadXtime(const Xtime* value, const char* function) {
    Xtime local{};
    if (!CopyIn(value, local)) throw std::invalid_argument(std::string(function) + ": unreadable time");
    return local;
}

}  // namespace

extern "C" {

GuestTm* APS5_VABI gmtime_r_nid_postfix(const std::int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    std::int64_t timerValue = 0;
    if (!CopyIn(timer, timerValue)) {
        errno = EFAULT;
        return nullptr;
    }
    GuestTm local{};
    if (gmtime_s_nid_postfix(&timerValue, &local) == nullptr) return nullptr;
    local.tm_isdst = 0;
    if (!CopyOut(result, local)) {
        errno = EFAULT;
        return nullptr;
    }
    return result;
}

GuestTm* APS5_VABI localtime_r_nid_postfix(const std::int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    std::int64_t timerValue = 0;
    if (!CopyIn(timer, timerValue)) {
        errno = EFAULT;
        return nullptr;
    }
    GuestTm local{};
    if (localtime_s_nid_postfix(&timerValue, &local) == nullptr) return nullptr;
    if (!CopyOut(result, local)) {
        errno = EFAULT;
        return nullptr;
    }
    return result;
}

char* APS5_VABI ctime_r_nid_postfix(const std::int64_t* timer, char* buffer) {
    if (buffer == nullptr) throw std::invalid_argument("ctime_r: null buffer");
    GuestTm local{};
    if (localtime_r_nid_postfix(timer, &local) == nullptr) return nullptr;
    char output[26]{};
    if (asctime_r_nid_postfix(&local, output) == nullptr) return nullptr;
    if (!CopyOutBytes(buffer, output, std::strlen(output) + 1)) {
        errno = EFAULT;
        return nullptr;
    }
    return buffer;
}

std::int64_t APS5_VABI timegm_nid_postfix(GuestTm* time) {
    GuestTm guest{};
    if (!CopyIn(time, guest)) {
        errno = EFAULT;
        return -1;
    }
    const std::int64_t years = floorDivide(guest.tm_mon, 12);
    const std::int64_t month = guest.tm_mon - years * 12 + 1;
    const std::int64_t year = std::int64_t{guest.tm_year} + 1900 + years;
    const std::int64_t days = daysFromCivil(year, month, 1) + guest.tm_mday - 1;
    const std::int64_t seconds = days * secondsPerDay + std::int64_t{guest.tm_hour} * 3600 + std::int64_t{guest.tm_min} * 60 + guest.tm_sec;
    GuestTm normalized{};
    if (gmtime_s_nid_postfix(&seconds, &normalized) == nullptr) return -1;
    normalized.tm_isdst = 0;
    if (!CopyOut(time, normalized)) {
        errno = EFAULT;
        return -1;
    }
    return seconds;
}

int APS5_VABI xtime_get_nid_postfix(Xtime* time, int base) {
    if (time == nullptr || base != timeUtc) return 0;
    const std::int64_t micros = _Xtime_get_ticks_nid_postfix();
    Xtime now{};
    now.sec = floorDivide(micros, 1000000);
    now.nsec = (micros - now.sec * 1000000) * 1000;
    return CopyOut(time, now) ? timeUtc : 0;
}

Xtime APS5_VABI _Xtime_to_ts_nid_postfix(const Xtime* time) {
    return LoadXtime(time, "_Xtime_to_ts");
}

Xtime APS5_VABI _Xtime_diff_to_ts_nid_postfix(const Xtime* target) {
    const Xtime limit = LoadXtime(target, "_Xtime_diff_to_ts");
    Xtime now{};
    xtime_get_nid_postfix(&now, timeUtc);

    std::int64_t seconds = wrappingAdd(limit.sec, limit.nsec / nanosPerSecond);
    std::int64_t nanos = limit.nsec % nanosPerSecond;
    if (nanos < 0) {
        nanos += nanosPerSecond;
        seconds = wrappingAdd(seconds, -1);
    }
    seconds = wrappingAdd(seconds, -now.sec);
    if (nanos < now.nsec) {
        nanos += nanosPerSecond;
        seconds = wrappingAdd(seconds, -1);
    }
    nanos -= now.nsec;
    if (seconds < 0 || (seconds == 0 && nanos == 0)) return {0, 0};
    return {seconds, nanos};
}

std::int64_t APS5_VABI _ZNSt8__sce_v222_Xtime_diff_to_millis2EPK8timespecS2__nid_postfix(const Xtime* target, const Xtime* now) {
    const Xtime limit = LoadXtime(target, "std::__sce_v2::_Xtime_diff_to_millis2");
    const Xtime current = LoadXtime(now, "std::__sce_v2::_Xtime_diff_to_millis2");
    constexpr std::uint64_t billion = nanosPerSecond;
    std::uint64_t seconds = static_cast<std::uint64_t>(limit.sec);
    std::uint64_t nanos = static_cast<std::uint64_t>(limit.nsec);
    if (static_cast<std::int64_t>(nanos) < 0) {
        const std::uint64_t lowest = 0 - billion;
        const std::uint64_t whole = nanos < lowest ? (lowest - nanos - 1) / billion + 1 : 0;
        seconds -= whole + 1;
        nanos += (whole + 1) * billion;
    }
    if (static_cast<std::int64_t>(nanos) >= nanosPerSecond) {
        const std::uint64_t whole = ((nanos >= 2 * billion - 1 ? nanos - (2 * billion - 1) : 0) + billion - 1) / billion;
        seconds += whole + 1;
        nanos -= (whole + 1) * billion;
    }
    if (static_cast<std::int64_t>(nanos) < current.nsec) {
        seconds += ~static_cast<std::uint64_t>(current.sec);
        nanos += billion;
    } else {
        seconds -= static_cast<std::uint64_t>(current.sec);
    }
    nanos -= static_cast<std::uint64_t>(current.nsec);
    if (static_cast<std::int64_t>(seconds) < 0 || (seconds == 0 && static_cast<std::int64_t>(nanos) <= 0)) return 0;
    const std::int64_t millis = static_cast<std::int64_t>(nanos + 999999) / 1000000;
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(millis) + seconds * 1000);
}

}
