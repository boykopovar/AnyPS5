#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

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

}

int main() {
    TestRationalTimes();
    TestSpecialTimes();
    TestDoubleTimes();
    TestWholeRange();
    TestNullRange();
    return 0;
}
