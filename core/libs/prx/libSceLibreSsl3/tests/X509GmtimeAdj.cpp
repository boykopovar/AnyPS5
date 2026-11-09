#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_gmtime_adj check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t MallocFailure = 0x0dfff041;
constexpr std::uint64_t IllegalTimeValue = 0x0dfff0b8;
int liveBlocks = 0;
int callsBeforeFailure = -1;

bool Fail() {
    if (callsBeforeFailure == 0) {
        callsBeforeFailure = -1;
        return true;
    }
    if (callsBeforeFailure > 0) --callsBeforeFailure;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::malloc(size ? size : 1);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) --liveBlocks;
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::calloc(count ? count : 1, size ? size : 1);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

std::int64_t Now() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::int64_t Seconds(int year, unsigned month, unsigned day, int hour, int minute, int second) {
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
    Require(date.ok());
    return std::chrono::sys_days{date}.time_since_epoch().count() * 86400 + hour * 3600 + minute * 60 + second;
}

std::int64_t Parse(const Asn1String& value) {
    Require(value.data != nullptr);
    Require(value.type == 23 || value.type == 24);
    const int digits = value.type == 23 ? 2 : 4;
    Require(value.length == digits + 11);
    Require(value.data[value.length - 1] == 'Z');
    for (int index = 0; index < value.length - 1; ++index)
        Require(value.data[index] >= '0' && value.data[index] <= '9');
    int year = 0;
    for (int index = 0; index < digits; ++index) year = year * 10 + value.data[index] - '0';
    if (digits == 2) year += year < 50 ? 2000 : 1900;
    const int month = (value.data[digits] - '0') * 10 + value.data[digits + 1] - '0';
    const int day = (value.data[digits + 2] - '0') * 10 + value.data[digits + 3] - '0';
    const int hour = (value.data[digits + 4] - '0') * 10 + value.data[digits + 5] - '0';
    const int minute = (value.data[digits + 6] - '0') * 10 + value.data[digits + 7] - '0';
    const int second = (value.data[digits + 8] - '0') * 10 + value.data[digits + 9] - '0';
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}};
    Require(date.ok());
    Require(hour <= 23 && minute <= 59 && second <= 59);
    return std::chrono::sys_days{date}.time_since_epoch().count() * 86400 + hour * 3600 + minute * 60 + second;
}

void RequireNear(std::int64_t actual, std::int64_t expected) {
    Require(actual >= expected && actual <= expected + 5);
}

void FreeTime(Asn1String* time) {
    free_nid_postfix(time->data);
    free_nid_postfix(time);
}

void RequireRejected(std::int64_t adjustment) {
    Asn1String rejected{};
    Require(X509_gmtime_adj_nid_postfix(&rejected, adjustment) == nullptr);
    Require(rejected.data == nullptr && rejected.length == 0 && rejected.type == 0 && rejected.flags == 0);
    Require(ERR_get_error_nid_postfix() == IllegalTimeValue);
    Require(ERR_get_error_nid_postfix() == 0);
}
}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    Asn1String time{};
    const auto before = Now();
    Require(X509_gmtime_adj_nid_postfix(&time, 0) == &time);
    const auto after = Now();
    Require(time.type == 23 && time.length == 13);
    const auto current = Parse(time);
    Require(current >= before && current <= after + 1);

    const auto lastUtc = Seconds(2049, 12, 31, 23, 59, 59);
    Require(X509_gmtime_adj_nid_postfix(&time, lastUtc - Now()) == &time);
    Require(time.type == 23 && time.length == 13);
    RequireNear(Parse(time), lastUtc);

    const auto firstGeneralized = Seconds(2050, 1, 1, 0, 0, 0);
    Require(X509_gmtime_adj_nid_postfix(&time, firstGeneralized - Now()) == &time);
    Require(time.type == 24 && time.length == 15);
    RequireNear(Parse(time), firstGeneralized);

    const auto year3000 = Seconds(3000, 6, 15, 12, 34, 56);
    Require(X509_gmtime_adj_nid_postfix(&time, year3000 - Now()) == &time);
    Require(time.type == 24 && time.length == 15);
    RequireNear(Parse(time), year3000);

    const auto early = Seconds(1960, 1, 2, 3, 4, 5);
    Require(X509_gmtime_adj_nid_postfix(&time, early - Now()) == &time);
    Require(time.type == 23 && time.length == 13);
    RequireNear(Parse(time), early);

    const auto last = Seconds(9999, 12, 31, 23, 59, 59);
    Require(X509_gmtime_adj_nid_postfix(&time, last - Now()) == &time);
    Require(time.type == 24 && time.length == 15);
    RequireNear(Parse(time), last);

    const auto first1950 = Seconds(1950, 1, 1, 0, 0, 0);
    Require(X509_gmtime_adj_nid_postfix(&time, first1950 - Now()) == &time);
    Require(time.type == 23 && time.length == 13);
    RequireNear(Parse(time), first1950);

    RequireRejected(Seconds(1949, 12, 31, 23, 59, 59) - Now());
    RequireRejected(Seconds(1900, 6, 15, 12, 0, 0) - Now());
    RequireRejected(Seconds(10000, 1, 1, 0, 0, 0) - Now());
    RequireRejected(std::numeric_limits<std::int64_t>::max());
    RequireRejected(std::numeric_limits<std::int64_t>::min());

    const auto allocatedAt = Now();
    Asn1String* allocated = X509_gmtime_adj_nid_postfix(nullptr, 0);
    Require(allocated != nullptr);
    Require(allocated->type == 23 && allocated->length == 13 && allocated->flags == 0x040);
    Require(liveBlocks == 3);
    const auto allocatedTime = Parse(*allocated);
    Require(allocatedTime >= allocatedAt && allocatedTime <= Now() + 1);

    Require(X509_gmtime_adj_nid_postfix(allocated, year3000 - Now()) == allocated);
    Require(allocated->type == 24 && allocated->length == 15);
    RequireNear(Parse(*allocated), year3000);
    FreeTime(allocated);
    Require(liveBlocks == 1);

    unsigned char borrowedData = 0;
    Asn1String borrowed{1, 23, &borrowedData, 0x10};
    Require(X509_gmtime_adj_nid_postfix(&borrowed, 0) == &borrowed);
    Require(borrowed.data != &borrowedData && borrowed.length == 13 && borrowed.type == 23 && borrowed.flags == 0);
    free_nid_postfix(borrowed.data);

    const auto savedData = time.data;
    const auto savedLength = time.length;
    const auto savedType = time.type;
    callsBeforeFailure = 0;
    Require(X509_gmtime_adj_nid_postfix(&time, 0) == nullptr);
    Require(time.data == savedData && time.length == savedLength && time.type == savedType);
    Require(ERR_get_error_nid_postfix() == IllegalTimeValue);
    Require(ERR_get_error_nid_postfix() == 0);

    callsBeforeFailure = 0;
    Require(X509_gmtime_adj_nid_postfix(nullptr, 0) == nullptr && liveBlocks == 1);
    Require(ERR_get_error_nid_postfix() == IllegalTimeValue);
    Require(ERR_get_error_nid_postfix() == 0);

    callsBeforeFailure = 1;
    Require(X509_gmtime_adj_nid_postfix(nullptr, 0) == nullptr && liveBlocks == 1);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == 0);

    free_nid_postfix(time.data);
    std::puts("libSceLibreSsl3 X509_gmtime_adj tests passed");
}
