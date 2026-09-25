#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

#include "prx/libc/include/General.hpp"

extern "C" {

int64_t APS5_VABI libc_time_nid_postfix(int64_t* timer) {
    std::time_t t = std::time(nullptr);
    if (timer != nullptr) *timer = static_cast<int64_t>(t);
    return static_cast<int64_t>(t);
}

int64_t APS5_VABI time_nid_postfix(int64_t* timer) {
    return libc_time_nid_postfix(timer);
}

double APS5_VABI libc_difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

double APS5_VABI difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

std::tm* APS5_VABI libc_gmtime_nid_postfix(const int64_t* timer) {
    static thread_local std::tm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::gmtime(&t);
    if (converted == nullptr) return nullptr;
    result = *converted;
    return &result;
}

std::tm* APS5_VABI libc_localtime_nid_postfix(const int64_t* timer) {
    static thread_local std::tm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::localtime(&t);
    if (converted == nullptr) return nullptr;
    result = *converted;
    return &result;
}

std::tm* APS5_VABI localtime_nid_postfix(const int64_t* timer) {
    return libc_localtime_nid_postfix(timer);
}

std::tm* APS5_VABI localtime_s_nid_postfix(const int64_t* timer, std::tm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    const std::time_t t = static_cast<std::time_t>(*timer);
#ifdef _WIN32
    if (localtime_s(result, &t) != 0) return nullptr;
#else
    if (localtime_r(&t, result) == nullptr) return nullptr;
#endif
    return result;
}

std::tm* APS5_VABI gmtime_s_nid_postfix(const int64_t* timer, std::tm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    const std::time_t t = static_cast<std::time_t>(*timer);
#ifdef _WIN32
    if (gmtime_s(result, &t) != 0) return nullptr;
#else
    if (gmtime_r(&t, result) == nullptr) return nullptr;
#endif
    return result;
}

std::tm* APS5_VABI gmtime_nid_postfix(const int64_t* timer) {
    return libc_gmtime_nid_postfix(timer);
}

int64_t APS5_VABI libc_mktime_nid_postfix(std::tm* timeptr) {
    return static_cast<int64_t>(std::mktime(timeptr));
}

int64_t APS5_VABI mktime_nid_postfix(std::tm* timeptr) {
    return static_cast<int64_t>(std::mktime(timeptr));
}

size_t APS5_VABI libc_strftime_nid_postfix(char* str, size_t count, const char* format, const std::tm* timeptr) {
    return std::strftime(str, count, format, timeptr);
}

size_t APS5_VABI strftime_nid_postfix(char* str, size_t count, const char* format, const std::tm* timeptr) {
    return std::strftime(str, count, format, timeptr);
}

// The guest's CLOCKS_PER_SEC is 1000000: clock() reports process CPU time in microseconds.
int64_t APS5_VABI clock_nid_postfix() {
#ifdef _WIN32
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return -1;
    const auto toTicks = [](const FILETIME& time) { return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime; };
    return static_cast<int64_t>((toTicks(kernel) + toTicks(user)) / 10);
#else
    return static_cast<int64_t>(static_cast<double>(std::clock()) * 1000000.0 / CLOCKS_PER_SEC);
#endif
}

}
