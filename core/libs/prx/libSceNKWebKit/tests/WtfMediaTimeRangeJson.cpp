#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::MediaAbi::MediaTimeRange;
using NKWebKit::WtfAbi::String;
using WTF::MediaTime;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "MediaTimeRange JSON check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr std::uint8_t Valid = MediaTime::Valid;
constexpr std::uint8_t HasBeenRounded = MediaTime::HasBeenRounded;
constexpr std::uint8_t PositiveInfinite = MediaTime::PositiveInfinite;
constexpr std::uint8_t NegativeInfinite = MediaTime::NegativeInfinite;
constexpr std::uint8_t Indefinite = MediaTime::Indefinite;
constexpr std::uint8_t DoubleValue = MediaTime::DoubleValue;

MediaTime Rational(std::int64_t value, std::uint32_t scale, std::uint8_t flags) {
    return MediaTime(value, scale, flags);
}

MediaTime Double(double value, std::uint8_t flags) {
    MediaTime time(0, MediaTime::DefaultTimeScale, flags);
    time.m_timeValueAsDouble = value;
    return time;
}

bool Same(const MediaTime& a, const MediaTime& b) {
    return a.m_timeValue == b.m_timeValue && a.m_timeScale == b.m_timeScale && a.m_timeFlags == b.m_timeFlags;
}

std::string ToJSON(const MediaTime& start, const MediaTime& end) {
    const MediaTimeRange range{start, end};
    const String result = _ZNK3WTF14MediaTimeRange12toJSONStringEv(&range);
    Require(Same(range.start, start) && Same(range.end, end));
    Require(result.impl != nullptr);
    Require(result.impl->hashAndFlags & 4);
    Require(result.impl->refCount == 2);
    return std::string(static_cast<const char*>(result.impl->data), result.impl->length);
}

const std::string Half = "{\"value\":0.5,\"numerator\":1,\"denominator\":2,\"flags\":1}";

std::string Start(const MediaTime& time) {
    const std::string json = ToJSON(time, Rational(1, 2, Valid));
    const std::string prefix = "{\"start\":";
    const std::string suffix = ",\"end\":" + Half + "}";
    Require(json.size() > prefix.size() + suffix.size());
    Require(json.compare(0, prefix.size(), prefix) == 0);
    Require(json.compare(json.size() - suffix.size(), suffix.size(), suffix) == 0);
    const std::string object = json.substr(prefix.size(), json.size() - prefix.size() - suffix.size());
    Require(ToJSON(Rational(1, 2, Valid), time) == "{\"start\":" + Half + ",\"end\":" + object + "}");
    return object;
}

void TestRationalTimes() {
    Require(Start(Rational(1, 2, Valid)) == Half);
    Require(Start(Rational(-7, 2, Valid | HasBeenRounded)) == "{\"value\":-3.5,\"numerator\":-7,\"denominator\":2,\"flags\":3}");
    Require(Start(Rational(1, 3, Valid)) == "{\"value\":0.3333333333333333,\"numerator\":1,\"denominator\":3,\"flags\":1}");
    Require(Start(Rational(1, 1000000000, Valid)) == "{\"value\":1e-9,\"numerator\":1,\"denominator\":1000000000,\"flags\":1}");
    Require(Start(Rational(1, 0, Valid)) == "{\"value\":null,\"numerator\":1,\"denominator\":0,\"flags\":1}");
    Require(Start(Rational(3000000000, 3000000000u, Valid)) == "{\"value\":1,\"numerator\":3000000000,\"denominator\":-1294967296,\"flags\":1}");
    Require(Start(Rational(std::numeric_limits<std::int64_t>::max(), 1, Valid)) == "{\"value\":9223372036854776000,\"numerator\":9223372036854776000,\"denominator\":1,\"flags\":1}");
    Require(Start(Rational(std::numeric_limits<std::int64_t>::min(), 4294967295u, Valid)) == "{\"value\":-2147483648.5,\"numerator\":-9223372036854776000,\"denominator\":-1,\"flags\":1}");
}

void TestSpecialTimes() {
    Require(Start(Rational(0, 0, 0)) == "{\"invalid\":true,\"numerator\":0,\"denominator\":0,\"flags\":0}");
    Require(Start(Rational(0, 1, Indefinite)) == "{\"invalid\":true,\"numerator\":0,\"denominator\":1,\"flags\":16}");
    Require(Start(Rational(5, 10, PositiveInfinite | HasBeenRounded)) == "{\"invalid\":true,\"numerator\":5,\"denominator\":10,\"flags\":6}");
    Require(Start(Rational(0, 1, Valid | Indefinite)) == "{\"value\":\"NaN\",\"numerator\":0,\"denominator\":1,\"flags\":17}");
    Require(Start(Rational(0, 1, Valid | Indefinite | PositiveInfinite)) == "{\"value\":\"NaN\",\"numerator\":0,\"denominator\":1,\"flags\":21}");
    Require(Start(Rational(0, 1, Valid | PositiveInfinite)) == "{\"value\":\"POSITIVE_INFINITY\",\"numerator\":0,\"denominator\":1,\"flags\":5}");
    Require(Start(Rational(0, 1, Valid | PositiveInfinite | NegativeInfinite)) == "{\"value\":\"POSITIVE_INFINITY\",\"numerator\":0,\"denominator\":1,\"flags\":13}");
    Require(Start(Rational(-1, 1, Valid | NegativeInfinite)) == "{\"value\":\"NEGATIVE_INFINITY\",\"numerator\":-1,\"denominator\":1,\"flags\":9}");
}

void TestDoubleTimes() {
    Require(Start(Double(2.5, Valid | DoubleValue)) == "{\"value\":2.5}");
    Require(Start(Double(-0.125, Valid | DoubleValue | HasBeenRounded)) == "{\"value\":-0.125}");
    Require(Start(Double(1e21, Valid | DoubleValue)) == "{\"value\":1e+21}");
    Require(Start(Double(1e-7, Valid | DoubleValue)) == "{\"value\":1e-7}");
    Require(Start(Double(0.0, Valid | DoubleValue)) == "{\"value\":0}");
    Require(Start(Double(1.5, DoubleValue)) == "{\"value\":null}");
    Require(Start(Double(1.5, Valid | DoubleValue | Indefinite)) == "{\"value\":null}");
    Require(Start(Double(1.5, Valid | DoubleValue | PositiveInfinite)) == "{\"value\":null}");
    Require(Start(Double(std::numeric_limits<double>::infinity(), Valid | DoubleValue)) == "{\"value\":null}");
}

void TestWholeRange() {
    Require(ToJSON(Rational(0, 0, 0), Rational(0, 1, Valid | PositiveInfinite)) == "{\"start\":{\"invalid\":true,\"numerator\":0,\"denominator\":0,\"flags\":0},\"end\":{\"value\":\"POSITIVE_INFINITY\",\"numerator\":0,\"denominator\":1,\"flags\":5}}");
    Require(ToJSON(Double(2.5, Valid | DoubleValue), Double(-1, Valid | DoubleValue)) == "{\"start\":{\"value\":2.5},\"end\":{\"value\":-1}}");
}

void TestNullRange() {
    bool threw = false;
    try {
        _ZNK3WTF14MediaTimeRange12toJSONStringEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }

    unsigned char* Guard() const { return base + pageSize; }
};

template <typename Action>
bool ThrowsNotImplemented(Action&& action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "_ZNK3WTF14MediaTimeRange12toJSONStringEv not implemented";
    }
    return false;
}

void TestGuestPointers() {
    GuardedPages pages;
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK3WTF14MediaTimeRange12toJSONStringEv(reinterpret_cast<const MediaTimeRange*>(0x10))); }));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK3WTF14MediaTimeRange12toJSONStringEv(reinterpret_cast<const MediaTimeRange*>(pages.Guard()))); }));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK3WTF14MediaTimeRange12toJSONStringEv(reinterpret_cast<const MediaTimeRange*>(pages.Guard() - 16))); }));

    const MediaTimeRange valid{Rational(1, 2, Valid), Rational(1, 2, Valid)};
    unsigned char* boundary = pages.Guard() - sizeof(MediaTimeRange);
    auto* placed = ::new (boundary) MediaTimeRange(valid);
    const String result = _ZNK3WTF14MediaTimeRange12toJSONStringEv(placed);
    Require(result.impl != nullptr);
    Require(std::string(static_cast<const char*>(result.impl->data), result.impl->length) == "{\"start\":" + Half + ",\"end\":" + Half + "}");
}

}

int main() {
    TestRationalTimes();
    TestSpecialTimes();
    TestDoubleTimes();
    TestWholeRange();
    TestNullRange();
    TestGuestPointers();
    return 0;
}
