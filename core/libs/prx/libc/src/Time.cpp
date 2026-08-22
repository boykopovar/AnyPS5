#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstring>

extern "C" {

int64_t libc_time_nid_postfix(int64_t* timer) {
    std::time_t t = std::time(nullptr);
    if (timer != nullptr) *timer = static_cast<int64_t>(t);
    return static_cast<int64_t>(t);
}

int64_t time_nid_postfix(int64_t* timer) {
    return libc_time_nid_postfix(timer);
}

double libc_difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

double difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

std::tm* libc_gmtime_nid_postfix(const int64_t* timer) {
    static thread_local std::tm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::gmtime(&t);
    if (converted == nullptr) return nullptr;
    result = *converted;
    return &result;
}

std::tm* libc_localtime_nid_postfix(const int64_t* timer) {
    static thread_local std::tm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::localtime(&t);
    if (converted == nullptr) return nullptr;
    result = *converted;
    return &result;
}

std::tm* localtime_nid_postfix(const int64_t* timer) {
    return libc_localtime_nid_postfix(timer);
}

int localtime_s_nid_postfix(std::tm* result, const int64_t* timer) {
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::localtime(&t);
    if (converted == nullptr) return -1;
    *result = *converted;
    return 0;
}

int gmtime_s_nid_postfix(std::tm* result, const int64_t* timer) {
    const std::time_t t = static_cast<std::time_t>(*timer);
    const std::tm* converted = std::gmtime(&t);
    if (converted == nullptr) return -1;
    *result = *converted;
    return 0;
}

int64_t libc_mktime_nid_postfix(std::tm* timeptr) {
    return static_cast<int64_t>(std::mktime(timeptr));
}

int64_t mktime_nid_postfix(std::tm* timeptr) {
    return static_cast<int64_t>(std::mktime(timeptr));
}

size_t libc_strftime_nid_postfix(char* str, size_t count, const char* format, const std::tm* timeptr) {
    return std::strftime(str, count, format, timeptr);
}

size_t strftime_nid_postfix(char* str, size_t count, const char* format, const std::tm* timeptr) {
    return std::strftime(str, count, format, timeptr);
}

}
