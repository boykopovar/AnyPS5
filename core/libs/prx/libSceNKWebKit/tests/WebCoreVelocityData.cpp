#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using WebCore::FloatPoint;
using WebCore::HistoricalVelocityData;
using WebCore::VelocityData;
using WTF::MonotonicTime;

constexpr unsigned NumberRespectingIntegers = 2;
constexpr double kThreshold = 0x1.1111111111111p-6;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VelocityData check failed at line %u\n", location.line());
        std::abort();
    }
}

VelocityData Feed(HistoricalVelocityData& history, FloatPoint position, double scale, double timestamp) {
    return _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
        &history, position, scale, MonotonicTime{timestamp});
}

bool Same(const VelocityData& v, float horizontal, float vertical, float scale, double time) {
    return v.horizontalVelocity == horizontal && v.verticalVelocity == vertical && v.scaleChangeRate == scale
        && v.lastUpdateTime.m_value == time;
}

bool Entry(const HistoricalVelocityData& history, unsigned index, double time, float x, float y, double scale) {
    const auto& data = history.m_positionHistory[index];
    return data.timestamp.m_value == time && data.position.m_x == x && data.position.m_y == y && data.scale == scale;
}

void TestRingBuffer() {
    HistoricalVelocityData history;

    Check(Same(Feed(history, {10.0f, 20.0f}, 1.0, 1.0), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 1.0);
    Check(Entry(history, 1, 1.0, 10.0f, 20.0f, 1.0));

    Check(Same(Feed(history, {20.0f, 40.0f}, 2.0, 1.5), 20.0f, 40.0f, 2.0f, 1.5));
    Check(history.m_historySize == 2);
    Check(history.m_latestDataIndex == 2);
    Check(Entry(history, 2, 1.5, 20.0f, 40.0f, 2.0));

    Check(Same(Feed(history, {30.0f, 60.0f}, 3.0, 2.0), 20.0f, 40.0f, 2.0f, 2.0));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 0);
    Check(Entry(history, 0, 2.0, 30.0f, 60.0f, 3.0));

    Check(Same(Feed(history, {70.0f, 0.0f}, 3.0, 2.5), 40.0f, -13.333333015441895f, 1.3333333730697632f, 2.5));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 2.5);
    Check(Entry(history, 1, 2.5, 70.0f, 0.0f, 3.0));

    Check(Same(Feed(history, {80.0f, 0.0f}, 3.0, 2.51), 59.405941009521484f, -39.60396194458008f, 0.9900990128517151f, 2.51));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 2.5);
    Check(Entry(history, 1, 2.51, 80.0f, 0.0f, 3.0));
    Check(Entry(history, 2, 1.5, 20.0f, 40.0f, 2.0));
    Check(Entry(history, 0, 2.0, 30.0f, 60.0f, 3.0));

    Check(Same(Feed(history, {50.0f, 25.0f}, 0.5, 3.0), 20.0f, -10.0f, -1.0f, 3.0));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 2);
    Check(history.m_lastAppendTimestamp.m_value == 3.0);
    Check(Entry(history, 2, 3.0, 50.0f, 25.0f, 0.5));
}

void TestFirstSampleInsideThreshold() {
    HistoricalVelocityData history;
    Check(Same(Feed(history, {5.0f, 6.0f}, 2.0, kThreshold), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 0);
    Check(history.m_latestDataIndex == 0);
    Check(history.m_lastAppendTimestamp.m_value == 0.0);
    Check(Entry(history, 0, kThreshold, 5.0f, 6.0f, 2.0));

    Check(Same(Feed(history, {7.0f, 8.0f}, 1.0, 1.0), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(Entry(history, 1, 1.0, 7.0f, 8.0f, 1.0));
}

void TestDeltaEqualToThreshold() {
    constexpr double first = 0x1.1111111111113p-6;
    constexpr double second = 0x1.1111111111112p-5;
    static_assert(second - first == kThreshold);

    HistoricalVelocityData history;
    Feed(history, {0.0f, 0.0f}, 1.0, first);
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);

    Check(Same(Feed(history, {4.0f, 4.0f}, 1.0, second), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == first);
    Check(Entry(history, 1, second, 4.0f, 4.0f, 1.0));
}

TextStream NewStream(unsigned flags, bool multiLine, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = flags;
    stream.multiLineMode = multiLine;
    stream.indent = indent;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

std::string Dump(const VelocityData& value, unsigned flags = 0, bool multiLine = true, int indent = 0) {
    TextStream stream = NewStream(flags, multiLine, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(stream, value);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

void TestDump() {
    Check(Dump(VelocityData{}) == "\n(timestamp 0.00)");
    Check(Dump(VelocityData{1.5f, 0.0f, -2.25f, MonotonicTime{3.0}}) == "\n(timestamp 3.00)\n(horizontalVelocity 1.50)\n(scaleChangeRate -2.25)");
    Check(Dump(VelocityData{0.0f, 0.5f, 0.0f, MonotonicTime{1.25}}) == "\n(timestamp 1.25)\n(verticalVelocity 0.50)");
    Check(Dump(VelocityData{1.0f, 2.0f, 3.0f, MonotonicTime{4.0}}, 0, false) == " (timestamp 4.00) (horizontalVelocity 1.00) (verticalVelocity 2.00) (scaleChangeRate 3.00)");
    Check(Dump(VelocityData{1.0f, 0.0f, 0.0f, MonotonicTime{4.0}}, 0, true, 1) == "\n  (timestamp 4.00)\n  (horizontalVelocity 1.00)");
    Check(Dump(VelocityData{2.0f, 0.0f, 0.5f, MonotonicTime{7.0}}, NumberRespectingIntegers) == "\n(timestamp 7)\n(horizontalVelocity 2)\n(scaleChangeRate 0.50)");
}

}  // namespace

int main() {
    TestRingBuffer();
    TestFirstSampleInsideThreshold();
    TestDeltaEqualToThreshold();
    TestDump();
    return 0;
}
