#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <stdexcept>

namespace {

constexpr std::int32_t UtcTime = 23;
constexpr std::int32_t GeneralizedTime = 24;
constexpr std::int64_t Borrowed = 0x10;

}

extern "C" Asn1String* APS5_VABI X509_gmtime_adj_nid_postfix(Asn1String* time, std::int64_t adjustment) {
    if (!time) return nullptr;
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() + adjustment;
    const std::time_t stamp = static_cast<std::time_t>(seconds);
    const std::tm* broken = std::gmtime(&stamp);
    if (!broken) return nullptr;
    const int year = broken->tm_year + 1900;
    const int month = broken->tm_mon + 1;
    const int day = broken->tm_mday;
    const int hour = broken->tm_hour;
    const int minute = broken->tm_min;
    const int second = broken->tm_sec;
    const bool utc = year >= 1950 && year <= 2049;
    const int digits = utc ? 2 : 4;
    const int length = digits + 11;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length + 1)));
    if (!data) return nullptr;
    if (utc) std::snprintf(reinterpret_cast<char*>(data), static_cast<std::size_t>(length + 1), "%02d%02d%02d%02d%02d%02dZ", year % 100, month, day, hour, minute, second);
    else std::snprintf(reinterpret_cast<char*>(data), static_cast<std::size_t>(length + 1), "%04d%02d%02d%02d%02d%02dZ", year, month, day, hour, minute, second);
    data[length - 1] = 'Z';
    if (time->data && !(time->flags & Borrowed)) free_nid_postfix(time->data);
    time->data = data;
    time->length = length;
    time->type = utc ? UtcTime : GeneralizedTime;
    time->flags = 0;
    return time;
}
