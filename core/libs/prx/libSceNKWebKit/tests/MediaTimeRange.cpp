#include "prx/libSceNKWebKit/include/MediaTimeRangeExports.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::MediaAbi::MediaTimeRange;
using WTF::MediaTime;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "media time range check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string Serialize(const MediaTimeRange* range) {
    errno = EDOM;
    const auto result = _ZNK3WTF14MediaTimeRange12toJSONStringEv(range);
    Require(errno == EDOM);
    Require(result.impl && (result.impl->hashAndFlags & 4));
    return {static_cast<const char*>(result.impl->data), result.impl->length};
}

void Expect(const MediaTime& start, const MediaTime& end, std::string_view startJson, std::string_view endJson) {
    const MediaTimeRange range{start, end};
    const auto before = range;
    const auto result = Serialize(&range);
    const auto expected = "{\"start\":" + std::string(startJson) + ",\"end\":" + std::string(endJson) + "}";
    if (result != expected) std::fprintf(stderr, "expected %s\nreceived %s\n", expected.c_str(), result.c_str());
    Require(result == expected);
    Require(range.start.m_timeValue == before.start.m_timeValue && range.end.m_timeValue == before.end.m_timeValue);
    Require(range.start.m_timeScale == before.start.m_timeScale && range.end.m_timeScale == before.end.m_timeScale);
    Require(range.start.m_timeFlags == before.start.m_timeFlags && range.end.m_timeFlags == before.end.m_timeFlags);
}

MediaTime Double(double value, std::uint8_t flags = MediaTime::Valid | MediaTime::DoubleValue) {
    MediaTime time(0, 12345, flags);
    time.m_timeValueAsDouble = value;
    return time;
}

void RequireRejected(const MediaTimeRange* range) {
    bool rejected = false;
    errno = EDOM;
    try {
        _ZNK3WTF14MediaTimeRange12toJSONStringEv(range);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && errno == EDOM);
}

void TestGuardedRange() {
    std::size_t pageSize{};
    unsigned char* memory{};
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr);
    DWORD previous{};
    Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    const auto size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    pageSize = static_cast<std::size_t>(size);
    auto* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    Require(mapping != MAP_FAILED);
    memory = static_cast<unsigned char*>(mapping);
    Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0);
#endif
    const MediaTimeRange range{MediaTime(1, 2, 1), MediaTime(3, 4, 1)};
    constexpr auto fieldBytes = offsetof(MediaTimeRange, end) + offsetof(MediaTime, m_timeFlags) + sizeof(std::uint8_t);
    auto* storage = memory + pageSize - fieldBytes;
    std::memcpy(storage, &range, fieldBytes);
#ifdef _WIN32
    Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(memory, pageSize, PROT_READ) == 0);
#endif
    Require(Serialize(reinterpret_cast<const MediaTimeRange*>(storage)) == Serialize(&range));
    RequireRejected(reinterpret_cast<const MediaTimeRange*>(storage + 1));
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(memory, pageSize * 2) == 0);
#endif
}

} // namespace

int main() {
    Expect(MediaTime(1, 2, 1), MediaTime(-3, 4, 3),
        R"({"value":0.5,"numerator":1,"denominator":2,"flags":1})",
        R"({"value":-0.75,"numerator":-3,"denominator":4,"flags":3})");
    Expect(MediaTime(-1, 1, 0), MediaTime(0, 1, 17),
        R"({"invalid":true,"numerator":-1,"denominator":1,"flags":0})",
        R"({"value":"NaN","numerator":0,"denominator":1,"flags":17})");
    Expect(MediaTime(0, 1, 5), MediaTime(-1, 1, 9),
        R"({"value":"POSITIVE_INFINITY","numerator":0,"denominator":1,"flags":5})",
        R"({"value":"NEGATIVE_INFINITY","numerator":-1,"denominator":1,"flags":9})");
    Expect(MediaTime(9, 7, 21), MediaTime(9, 7, 13),
        R"({"value":"NaN","numerator":9,"denominator":7,"flags":21})",
        R"({"value":"POSITIVE_INFINITY","numerator":9,"denominator":7,"flags":13})");
    Expect(MediaTime(0, 4294967295u, 129), MediaTime(1, 0, 1),
        R"({"value":0,"numerator":0,"denominator":-1,"flags":129})",
        R"({"value":null,"numerator":1,"denominator":0,"flags":1})");
    Expect(MediaTime(std::numeric_limits<std::int64_t>::max(), 1, 1), MediaTime(std::numeric_limits<std::int64_t>::min(), 1, 1),
        R"({"value":9223372036854776000,"numerator":9223372036854776000,"denominator":1,"flags":1})",
        R"({"value":-9223372036854776000,"numerator":-9223372036854776000,"denominator":1,"flags":1})");
    Expect(Double(1.25), Double(-0.0), R"({"value":1.25})", R"({"value":0})");
    Expect(Double(1e21), Double(1e-7), R"({"value":1e+21})", R"({"value":1e-7})");
    Expect(Double(std::numeric_limits<double>::infinity()), Double(std::numeric_limits<double>::quiet_NaN()), R"({"value":null})", R"({"value":null})");
    Expect(Double(42, 32), Double(42, 49), R"({"value":null})", R"({"value":null})");
    RequireRejected(nullptr);
    RequireRejected(reinterpret_cast<const MediaTimeRange*>(std::uintptr_t{0x10}));
    RequireRejected(reinterpret_cast<const MediaTimeRange*>(std::numeric_limits<std::uintptr_t>::max() - 8));
    TestGuardedRange();
    std::puts("MediaTimeRange: rational and double times, JSON special values, boundaries and guarded guest memory passed");
}
