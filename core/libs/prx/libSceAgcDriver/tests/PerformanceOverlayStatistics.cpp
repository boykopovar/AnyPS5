#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using AgcDriver::OverlayContent;
using AgcDriver::OverlayCorner;
using AgcDriver::OverlayMetrics;
using AgcDriver::OverlayRect;
using AgcDriver::OverlayStatistics;
using Clock = OverlayStatistics::Clock;
using namespace std::chrono_literals;

constexpr Clock::time_point Start = Clock::time_point{} + std::chrono::hours(1);

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

bool within(double value, double expected, double tolerance = 1e-9) {
    return std::abs(value - expected) <= tolerance;
}

void fpsWindowTests() {
    OverlayStatistics statistics;
    require(statistics.Fps() == 0 && statistics.LastFrameMs() == 0 && statistics.FrameTimes().empty(), "a new overlay shows statistics");
    statistics.AddPresent(Start, 0, 0);
    require(statistics.Fps() == 0, "one present reads as a frame rate");
    for (int i = 1; i <= 120; ++i) statistics.AddPresent(Start + i * 10ms, 0, 0);
    require(within(statistics.Fps(), 100.0), "a present every 10 ms reads as " + std::to_string(statistics.Fps()) + " FPS, not 100");
    statistics.AddPresent(Start + 1700ms, 0, 0);
    require(within(statistics.Fps(), 51.0), "after a 500 ms stall the last second holds 52 presents, read as " + std::to_string(statistics.Fps()) + " FPS, not 51");
    statistics.AddPresent(Start + 3700ms, 0, 0);
    require(within(statistics.Fps(), 0.5), "after a 2 s stall the last second holds one present, read as " + std::to_string(statistics.Fps()) + " FPS, not the last frame's 0.5");
    statistics.AddPresent(Start + 3750ms, 0, 0);
    require(within(statistics.Fps(), 20.0), "two presents 50 ms apart read as " + std::to_string(statistics.Fps()) + " FPS, not 20");
    bool refused = false;
    try {
        statistics.AddPresent(Start + 3740ms, 0, 0);
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a present earlier than the previous one was accepted");
    statistics.Reset();
    require(statistics.Fps() == 0 && statistics.LastFrameMs() == 0 && statistics.FrameTimes().empty(), "a reset kept statistics");
}

void frameRingTests() {
    OverlayStatistics statistics;
    auto at = Start;
    statistics.AddPresent(at, 0, 0);
    for (int i = 1; i <= 4; ++i) {
        at += std::chrono::milliseconds(i);
        statistics.AddPresent(at, 0, 0);
    }
    const auto partial = statistics.FrameTimes();
    require(partial.size() == 4 && within(partial[0], 1) && within(partial[3], 4), "the frame times of a short run are not in present order");
    for (int i = 5; i <= 150; ++i) {
        at += std::chrono::milliseconds(i);
        statistics.AddPresent(at, 0, 0);
    }
    const auto times = statistics.FrameTimes();
    require(times.size() == OverlayStatistics::FrameHistory, "the ring holds " + std::to_string(times.size()) + " frames, not 120");
    for (std::size_t i = 0; i < times.size(); ++i) require(within(times[i], 31.0 + static_cast<double>(i)), "frame " + std::to_string(i) + " of the ring is " + std::to_string(times[i]) + " ms");
    require(within(statistics.LastFrameMs(), 150), "the last frame is not the newest");
    const auto range = statistics.FrameTimeRange();
    require(within(range.minimum, 31) && within(range.maximum, 150) && within(range.average, 90.5), "the graph range of 31..150 ms reads as " + std::to_string(range.minimum) + "/" + std::to_string(range.average) + "/" + std::to_string(range.maximum));
}

void counterTests() {
    OverlayStatistics statistics;
    constexpr std::uint64_t Earlier = 1'000'000'000u;
    for (std::uint64_t i = 0; i <= 60; ++i) statistics.AddPresent(Start + i * 16ms, Earlier + i * 4'000'000u, Earlier + i * 1'500'000u);
    require(within(statistics.GpuMsPerFrame(), 4.0) && within(statistics.DriverMsPerFrame(), 1.5), "4 ms of GPU and 1.5 ms of driver time per frame read as " + std::to_string(statistics.GpuMsPerFrame()) + " and " + std::to_string(statistics.DriverMsPerFrame()));
    statistics.AddPresent(Start + 61 * 16ms, 5, 5);
    require(statistics.GpuMsPerFrame() == 0 && statistics.DriverMsPerFrame() == 0, "a counter that went back is read as time");
}

void formatTests() {
    const std::pair<double, const char*> cases[] = {{59.94, "59.9"}, {0, "0.0"}, {99.94, "99.9"}, {99.96, "100"}, {144.4, "144"}, {12345, "9999"}, {-3, "0.0"}, {std::nan(""), "0.0"}};
    for (const auto& [value, expected] : cases) require(AgcDriver::FormatOverlayValue(value) == expected, std::to_string(value) + " is shown as '" + AgcDriver::FormatOverlayValue(value) + "', not '" + expected + "'");
    const std::pair<double, const char*> rates[] = {{0.5, "0.50"}, {1000.0 / 12000.0, "0.08"}, {0.05, "0.05"}, {0.004, "0.00"}, {0.994, "0.99"}, {0.996, "1.0"}, {1.5, "1.5"}, {59.94, "59.9"}, {144.4, "144"}, {12345, "9999"}, {0, "0.0"}, {-1, "0.0"}, {std::nan(""), "0.0"}};
    for (const auto& [value, expected] : rates) require(AgcDriver::FormatOverlayRate(value) == expected, "a rate of " + std::to_string(value) + " is shown as '" + AgcDriver::FormatOverlayRate(value) + "', not '" + expected + "'");
}

void lowRateTests() {
    OverlayStatistics slow;
    constexpr std::uint64_t Gpu[] = {30'000'000u, 45'000'000u, 20'000'000u};
    constexpr std::uint64_t Driver[] = {12'000'000u, 20'000'000u, 8'000'000u};
    std::uint64_t gpuTotal = 0;
    std::uint64_t driverTotal = 0;
    slow.AddPresent(Start, 0, 0);
    require(slow.Fps() == 0 && slow.GpuMsPerFrame() == 0 && slow.DriverMsPerFrame() == 0, "a single present reads as a frame");
    for (int i = 0; i < 3; ++i) {
        gpuTotal += Gpu[i];
        driverTotal += Driver[i];
        slow.AddPresent(Start + (i + 1) * 2s, gpuTotal, driverTotal);
        require(within(slow.Fps(), 0.5) && within(slow.LastFrameMs(), 2000), "a present every 2 s reads as " + std::to_string(slow.Fps()) + " FPS and " + std::to_string(slow.LastFrameMs()) + " ms, not 0.5 and 2000");
        require(within(slow.GpuMsPerFrame(), static_cast<double>(Gpu[i]) / 1e6) && within(slow.DriverMsPerFrame(), static_cast<double>(Driver[i]) / 1e6), "frame " + std::to_string(i + 1) + " every 2 s reads " + std::to_string(slow.GpuMsPerFrame()) + " ms of GPU and " + std::to_string(slow.DriverMsPerFrame()) + " ms of driver time, not its own");
    }
    const auto everyTwoSeconds = AgcDriver::ComposeOverlay(slow, true, true);
    require(everyTwoSeconds.above == std::vector<std::string>{"PROFILED", "FPS: 0.50", "FRAME: 2000 MS"}, "a present every 2 s shows " + std::to_string(everyTwoSeconds.above.size()) + " lines above the graph, the last '" + everyTwoSeconds.above.back() + "'");
    require(everyTwoSeconds.below[0] == "MIN/AVG/MAX: 2000/2000/2000" && everyTwoSeconds.below[1] == "GPU: 20.0 MS" && everyTwoSeconds.below[2] == "DRIVER: 8.0 MS", "a present every 2 s shows '" + everyTwoSeconds.below[0] + "', '" + everyTwoSeconds.below[1] + "' and '" + everyTwoSeconds.below[2] + "'");

    OverlayStatistics glacial;
    glacial.AddPresent(Start, 0, 0);
    for (int i = 1; i <= 5; ++i) glacial.AddPresent(Start + i * 12s, static_cast<std::uint64_t>(i) * 250'000'000u, static_cast<std::uint64_t>(i) * 12'500'000'000u);
    require(within(glacial.Fps(), 1000.0 / 12000.0) && within(glacial.LastFrameMs(), 12000) && within(glacial.GpuMsPerFrame(), 250) && within(glacial.DriverMsPerFrame(), 12500), "a present every 12 s reads as " + std::to_string(glacial.Fps()) + " FPS, " + std::to_string(glacial.LastFrameMs()) + " ms, " + std::to_string(glacial.GpuMsPerFrame()) + " and " + std::to_string(glacial.DriverMsPerFrame()) + " ms");
    const auto everyTwelveSeconds = AgcDriver::ComposeOverlay(glacial, true, true);
    require(everyTwelveSeconds.above == std::vector<std::string>{"PROFILED", "FPS: 0.08", "FRAME: 9999 MS"}, "a present every 12 s shows " + std::to_string(everyTwelveSeconds.above.size()) + " lines above the graph, the last '" + everyTwelveSeconds.above.back() + "'");
    require(everyTwelveSeconds.below[0] == "MIN/AVG/MAX: 9999/9999/9999" && everyTwelveSeconds.below[1] == "GPU: 250 MS" && everyTwelveSeconds.below[2] == "DRIVER: 9999 MS", "a present every 12 s shows '" + everyTwelveSeconds.below[0] + "', '" + everyTwelveSeconds.below[1] + "' and '" + everyTwelveSeconds.below[2] + "'");
    require(everyTwelveSeconds.frameTimes == std::vector<double>(5, 12000.0), "the graph does not hold the five 12 s frames");
    const auto layout = AgcDriver::LayOutOverlay(everyTwelveSeconds, {1920, 1080}, OverlayCorner::TopLeft);
    std::size_t bars = 0;
    for (const auto& rect : layout.rects) {
        if (rect.color != static_cast<std::uint32_t>(AgcDriver::OverlayColor::Graph)) continue;
        ++bars;
        require(rect.height == OverlayMetrics::GraphHeight * layout.scale / 2, "steady 12 s frames do not reach half the graph");
    }
    require(bars == 5, "the graph has " + std::to_string(bars) + " bars for five 12 s frames");

    OverlayStatistics rising;
    auto at = Start;
    std::uint64_t total = 0;
    rising.AddPresent(at, 0, 0);
    const auto step = [&](Clock::duration interval, std::uint64_t gpuNanoseconds) {
        at += interval;
        total += gpuNanoseconds;
        rising.AddPresent(at, total, 0);
    };
    const auto check = [&](double fps, double gpuMs, const char* shown, const std::string& when) {
        require(within(rising.Fps(), fps) && within(rising.GpuMsPerFrame(), gpuMs) && AgcDriver::FormatOverlayRate(rising.Fps()) == shown, when + " reads as " + std::to_string(rising.Fps()) + " FPS ('" + AgcDriver::FormatOverlayRate(rising.Fps()) + "') and " + std::to_string(rising.GpuMsPerFrame()) + " ms of GPU time, not " + std::to_string(fps) + " ('" + shown + "') and " + std::to_string(gpuMs));
    };
    step(2s, 40'000'000u);
    check(0.5, 40, "0.50", "a 2 s frame");
    step(1250ms, 30'000'000u);
    check(0.8, 30, "0.80", "a 1.25 s frame");
    step(1001ms, 20'000'000u);
    check(1000.0 / 1001.0, 20, "1.0", "a 1.001 s frame, the last second holding one present");
    step(1000ms, 10'000'000u);
    check(1.0, 10, "1.0", "a 1 s frame, the last second holding two presents");
    step(750ms, 6'000'000u);
    check(1.0 / 0.75, 6, "1.3", "a 750 ms frame");
    for (int i = 0; i < 8; ++i) step(250ms, i % 2 == 0 ? 2'000'000u : 6'000'000u);
    check(4.0, 4.0, "4.0", "four presents a second, alternating 2 and 6 ms of GPU time");
    step(3s, 9'000'000u);
    check(1.0 / 3.0, 9, "0.33", "a 3 s frame after the fast ones");
}

void settingsTests() {
    const auto hidden = AgcDriver::ParseOverlaySettings(nullptr, nullptr);
    require(hidden.page == 0 && hidden.corner == OverlayCorner::TopLeft, "the overlay is not off and top-left by default");
    require(AgcDriver::ParseOverlaySettings("", "").page == 0 && AgcDriver::ParseOverlaySettings("0", nullptr).page == 0, "an empty or 0 ANYPS5_OVERLAY shows the overlay");
    require(AgcDriver::ParseOverlaySettings("1", nullptr).page == 1, "ANYPS5_OVERLAY=1 does not start on page 1");
    require(AgcDriver::ParseOverlaySettings("2", "br").page == 2 && AgcDriver::ParseOverlaySettings("2", "br").corner == OverlayCorner::BottomRight, "ANYPS5_OVERLAY=2 does not start on page 2 in the chosen corner");
    const std::pair<const char*, OverlayCorner> corners[] = {{"tl", OverlayCorner::TopLeft}, {"tr", OverlayCorner::TopRight}, {"bl", OverlayCorner::BottomLeft}, {"br", OverlayCorner::BottomRight}};
    for (const auto& [name, corner] : corners) require(AgcDriver::ParseOverlaySettings("1", name).corner == corner, std::string("ANYPS5_OVERLAY_POS=") + name + " picks another corner");
    const auto refused = [](const char* enabled, const char* position, const char* variable) {
        try {
            static_cast<void>(AgcDriver::ParseOverlaySettings(enabled, position));
        } catch (const std::exception& error) {
            return std::string(error.what()).find(variable) != std::string::npos;
        }
        return false;
    };
    require(refused("yes", nullptr, "ANYPS5_OVERLAY") && refused("3", nullptr, "ANYPS5_OVERLAY") && refused("02", nullptr, "ANYPS5_OVERLAY") && refused("-1", nullptr, "ANYPS5_OVERLAY"), "an ANYPS5_OVERLAY value other than 0, 1 or 2 was accepted or not named");
    try {
        static_cast<void>(AgcDriver::ParseOverlaySettings("3", nullptr));
    } catch (const std::exception& error) {
        require(std::string(error.what()).find("0, 1 or 2") != std::string::npos, std::string("the ANYPS5_OVERLAY error does not list 0, 1 and 2: ") + error.what());
    }
    require(refused("1", "center", "ANYPS5_OVERLAY_POS") && refused(nullptr, "TL", "ANYPS5_OVERLAY_POS"), "an unknown ANYPS5_OVERLAY_POS was accepted or not named");
}

void composeTests() {
    OverlayStatistics statistics;
    for (std::uint64_t i = 0; i <= 100; ++i) statistics.AddPresent(Start + i * 10ms, i * 2'000'000u, i * 500'000u);
    const auto plain = AgcDriver::ComposeOverlay(statistics, false, false);
    require(plain.above.size() == 2 && plain.above[0] == "FPS: 100" && plain.above[1] == "FRAME: 10.0 MS", "the frame lines read '" + plain.above[0] + "' and '" + plain.above[1] + "'");
    require(plain.below.size() == 1 && plain.below[0] == "MIN/AVG/MAX: 10.0/10.0/10.0", "the graph line reads '" + plain.below[0] + "'");
    require(plain.frameTimes.size() == 100, "the graph does not hold every frame time");
    const auto profiled = AgcDriver::ComposeOverlay(statistics, true, true);
    require(profiled.below.size() == 3 && profiled.below[1] == "GPU: 2.0 MS" && profiled.below[2] == "DRIVER: 0.5 MS", "the profiled lines read '" + profiled.below[1] + "' and '" + profiled.below[2] + "'");
    std::vector<std::string> marked{"PROFILED"};
    marked.insert(marked.end(), plain.above.begin(), plain.above.end());
    require(profiled.above == marked && AgcDriver::ComposeOverlay(statistics, true, false).above == marked && AgcDriver::ComposeOverlay(statistics, false, true).above == marked, "a profiled panel does not start with PROFILED above the unprofiled lines: it starts with '" + profiled.above.front() + "'");
    require(AgcDriver::ComposeOverlay(statistics, true, false).below.size() == 2 && AgcDriver::ComposeOverlay(statistics, false, true).below[1].rfind("DRIVER", 0) == 0, "a line is shown without its source");
}

std::vector<std::uint8_t> coverage(const AgcDriver::OverlayLayout& layout, std::uint32_t color) {
    const auto& bounds = layout.bounds;
    std::vector<std::uint8_t> covered(static_cast<std::size_t>(bounds.width) * static_cast<std::size_t>(bounds.height), 0);
    for (const auto& rect : layout.rects) {
        if (rect.color != color) continue;
        for (std::int32_t y = rect.y; y < rect.y + rect.height; ++y) {
            for (std::int32_t x = rect.x; x < rect.x + rect.width; ++x) ++covered[static_cast<std::size_t>(y - bounds.y) * static_cast<std::size_t>(bounds.width) + static_cast<std::size_t>(x - bounds.x)];
        }
    }
    return covered;
}

void requireLineDrawn(const AgcDriver::OverlayLayout& layout, const std::vector<std::uint8_t>& text, const std::string& line, std::int32_t top) {
    const auto s = layout.scale;
    for (std::size_t column = 0; column < line.size(); ++column) {
        const auto& glyph = AgcDriver::OverlayGlyph(line[column]);
        for (std::int32_t gy = 0; gy < OverlayMetrics::GlyphHeight; ++gy) {
            for (std::int32_t gx = 0; gx < OverlayMetrics::Advance; ++gx) {
                const bool set = gx < OverlayMetrics::GlyphWidth && (glyph[static_cast<std::size_t>(gy)] & (0x10u >> gx)) != 0;
                for (std::int32_t py = 0; py < s; ++py) {
                    for (std::int32_t px = 0; px < s; ++px) {
                        const auto x = (OverlayMetrics::Padding + static_cast<std::int32_t>(column) * OverlayMetrics::Advance + gx) * s + px;
                        const auto y = (top + gy) * s + py;
                        require(text[static_cast<std::size_t>(y) * static_cast<std::size_t>(layout.bounds.width) + static_cast<std::size_t>(x)] == (set ? 1 : 0), "'" + std::string(1, line[column]) + "' of '" + line + "' is drawn wrong at glyph pixel " + std::to_string(gx) + "," + std::to_string(gy));
                    }
                }
            }
        }
    }
}

void layoutTests() {
    OverlayContent content{{"FPS: 60.0", "FRAME: 16.7 MS"}, {}, {"MIN/AVG/MAX: 16.7/16.7/16.7"}};
    for (int i = 0; i < 40; ++i) content.frameTimes.push_back(i == 39 ? 50.0 : 10.0);
    const std::pair<VkExtent2D, std::int32_t> scales[] = {{{1280, 720}, 2}, {{1920, 1080}, 2}, {{2560, 1440}, 3}, {{3840, 2160}, 5}, {{320, 180}, 1}, {{2560, 720}, 2}};
    for (const auto& [extent, scale] : scales) require(AgcDriver::OverlayScale(extent, content) == scale, std::to_string(extent.width) + "x" + std::to_string(extent.height) + " draws the overlay at scale " + std::to_string(AgcDriver::OverlayScale(extent, content)) + ", not " + std::to_string(scale));

    const VkExtent2D extent{1920, 1080};
    const auto layout = AgcDriver::LayOutOverlay(content, extent, OverlayCorner::TopLeft);
    const auto s = layout.scale;
    const auto margin = OverlayMetrics::Margin * s;
    require(layout.bounds.x == margin && layout.bounds.y == margin && layout.bounds.width == (OverlayMetrics::InnerWidth + 2 * OverlayMetrics::Padding) * s, "the top-left panel is not at the margin");
    require(layout.rects.front().x == layout.bounds.x && layout.rects.front().width == layout.bounds.width && layout.rects.front().color == static_cast<std::uint32_t>(AgcDriver::OverlayColor::Background), "the background is not drawn first over the whole panel");
    for (const auto& rect : layout.rects) {
        require(rect.width > 0 && rect.height > 0 && rect.x >= layout.bounds.x && rect.y >= layout.bounds.y && rect.x + rect.width <= layout.bounds.x + layout.bounds.width && rect.y + rect.height <= layout.bounds.y + layout.bounds.height, "a rectangle leaves the panel");
    }

    const auto text = coverage(layout, static_cast<std::uint32_t>(AgcDriver::OverlayColor::Text));
    const auto lineTop = [&](std::size_t line) { return OverlayMetrics::Padding + static_cast<std::int32_t>(line) * OverlayMetrics::LinePitch; };
    requireLineDrawn(layout, text, content.above[0], lineTop(0));
    requireLineDrawn(layout, text, content.above[1], lineTop(1));
    requireLineDrawn(layout, text, content.below[0], lineTop(2) + OverlayMetrics::GraphHeight + OverlayMetrics::GraphGap);

    std::vector<OverlayRect> bars;
    for (const auto& rect : layout.rects) {
        if (rect.color == static_cast<std::uint32_t>(AgcDriver::OverlayColor::Graph)) bars.push_back(rect);
    }
    require(bars.size() == content.frameTimes.size(), "the graph has " + std::to_string(bars.size()) + " bars for 40 frames");
    const auto graphBottom = layout.bounds.y + (lineTop(2) + OverlayMetrics::GraphHeight) * s;
    const auto full = OverlayMetrics::GraphHeight * s;
    require(bars.back().x == layout.bounds.x + (OverlayMetrics::Padding + static_cast<std::int32_t>(OverlayStatistics::FrameHistory) - 1) * s, "the newest frame is not at the right end of the graph");
    for (std::size_t i = 0; i + 1 < bars.size(); ++i) require(bars[i + 1].x == bars[i].x + s && bars[i].width == s, "the graph bars are not side by side");
    require(bars.back().height == full && bars.back().y + bars.back().height == graphBottom, "the slowest frame does not fill the graph");
    require(bars.front().height == static_cast<std::int32_t>(std::lround(10.0 / 50.0 * full)) && bars.front().y + bars.front().height == graphBottom, "a 10 ms frame is not a fifth of a 50 ms one");
    OverlayContent steady = content;
    std::fill(steady.frameTimes.begin(), steady.frameTimes.end(), 16.0);
    for (const auto& rect : AgcDriver::LayOutOverlay(steady, extent, OverlayCorner::TopLeft).rects) {
        if (rect.color == static_cast<std::uint32_t>(AgcDriver::OverlayColor::Graph)) require(rect.height == full / 2, "steady frames do not reach half the graph");
    }

    const std::pair<OverlayCorner, std::pair<bool, bool>> corners[] = {{OverlayCorner::TopRight, {true, false}}, {OverlayCorner::BottomLeft, {false, true}}, {OverlayCorner::BottomRight, {true, true}}};
    for (const auto& [corner, sides] : corners) {
        const auto placed = AgcDriver::LayOutOverlay(content, extent, corner).bounds;
        const auto x = sides.first ? static_cast<std::int32_t>(extent.width) - margin - placed.width : margin;
        const auto y = sides.second ? static_cast<std::int32_t>(extent.height) - margin - placed.height : margin;
        require(placed.x == x && placed.y == y && placed.width == layout.bounds.width && placed.height == layout.bounds.height, "a corner places the panel elsewhere");
    }
    OverlayContent profiled = content;
    profiled.below.push_back("GPU: 2.0 MS");
    require(AgcDriver::LayOutOverlay(profiled, extent, OverlayCorner::TopLeft).bounds.height == layout.bounds.height + OverlayMetrics::LinePitch * s, "a profiled line does not add one line to the panel");

    bool refused = false;
    try {
        static_cast<void>(AgcDriver::OverlayGlyph('q'));
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a character without a glyph was drawn");
}

constexpr std::uint64_t GiB = 1ull << 30u;

OverlayStatistics countedStatistics() {
    OverlayStatistics statistics;
    for (std::uint64_t i = 0; i <= 60; ++i) {
        AgcDriver::OverlayCounters counters;
        counters.gpuNanoseconds = i * 4'000'000u;
        counters.driverNanoseconds = i * 1'500'000u;
        counters.draws = 5000 + i * 1240;
        counters.dispatches = 70 + i * 85;
        counters.pipelines = i < 50 ? 1500 : 1503;
        statistics.AddPresent(Start + i * 16ms, counters);
    }
    return statistics;
}

AgcDriver::OverlayDetails fullDetails() {
    AgcDriver::OverlayDetails details;
    details.vram = AgcDriver::OverlayMemory{static_cast<std::uint64_t>(6.1 * GiB), static_cast<std::uint64_t>(7.8 * GiB)};
    details.ram = static_cast<std::uint64_t>(3.2 * GiB);
    details.display = {1920, 1080};
    details.window = {2560, 1440};
    return details;
}

void packetStatisticsTests() {
    const auto statistics = countedStatistics();
    require(within(statistics.DrawsPerFrame(), 1240) && within(statistics.DispatchesPerFrame(), 85), "1240 draws and 85 dispatches per frame read as " + std::to_string(statistics.DrawsPerFrame()) + " and " + std::to_string(statistics.DispatchesPerFrame()));
    require(statistics.PipelinesInWindow() == 3 && statistics.Latest().pipelines == 1503, "3 pipelines created within the last second of 1503 read as " + std::to_string(statistics.PipelinesInWindow()) + " of " + std::to_string(statistics.Latest().pipelines));
    require(within(statistics.GpuMsPerFrame(), 4.0) && within(statistics.DriverMsPerFrame(), 1.5), "the time counters changed with the packet counters");
    auto stalled = statistics;
    auto counters = stalled.Latest();
    counters.draws += 900;
    counters.dispatches += 40;
    counters.pipelines += 7;
    stalled.AddPresent(Start + 3000ms, counters);
    require(within(stalled.DrawsPerFrame(), 900) && within(stalled.DispatchesPerFrame(), 40) && stalled.PipelinesInWindow() == 7 && stalled.Latest().pipelines == 1510, "a present alone in its second reads as " + std::to_string(stalled.DrawsPerFrame()) + " draws, " + std::to_string(stalled.DispatchesPerFrame()) + " dispatches and " + std::to_string(stalled.PipelinesInWindow()) + " pipelines, not the last frame's 900, 40 and 7");
    auto restarted = statistics;
    counters = restarted.Latest();
    counters.draws = 3;
    restarted.AddPresent(Start + 61 * 16ms, counters);
    require(restarted.DrawsPerFrame() == 0, "a draw counter that went back is read as draws");
    OverlayStatistics empty;
    require(empty.PipelinesInWindow() == 0 && empty.Latest().draws == 0, "an overlay without presents shows packet counts");
}

void packetCounterTests() {
    const auto before = AgcDriver::PacketCounters::Sum();
    auto& counters = AgcDriver::PacketCounters::ThisThread();
    require(&counters == &AgcDriver::PacketCounters::ThisThread(), "one thread got two packet counters");
    for (int i = 0; i < 1240; ++i) counters.CountDraw();
    for (int i = 0; i < 85; ++i) counters.CountDispatch();
    bool separate = false;
    std::thread worker([&] {
        auto& own = AgcDriver::PacketCounters::ThisThread();
        separate = &own != &counters;
        for (int i = 0; i < 1000; ++i) own.CountDraw();
        own.CountDispatch();
    });
    worker.join();
    require(separate, "two workers share packet counters");
    const auto after = AgcDriver::PacketCounters::Sum();
    require(after.draws - before.draws == 2240 && after.dispatches - before.dispatches == 86, "two workers' 2240 draws and 86 dispatches sum to " + std::to_string(after.draws - before.draws) + " and " + std::to_string(after.dispatches - before.dispatches));
    std::thread next([] { AgcDriver::PacketCounters::ThisThread().CountDraw(); });
    next.join();
    require(AgcDriver::PacketCounters::Sum().draws == after.draws + 1, "the counts of a worker that ended were lost, or a new worker restarted them");
}

void countFormatTests() {
    const std::pair<double, const char*> counts[] = {{0, "0"}, {0.4, "0"}, {0.5, "1"}, {85, "85"}, {1239.6, "1240"}, {9999, "9999"}, {9999.5, "10K"}, {12345, "12K"}, {999999, "999K"}, {1e6, "1M"}, {2.5e9, "999M"}, {-3, "0"}, {std::nan(""), "0"}, {INFINITY, "0"}};
    for (const auto& [value, expected] : counts) require(AgcDriver::FormatOverlayCount(value) == expected, std::to_string(value) + " is counted as '" + AgcDriver::FormatOverlayCount(value) + "', not '" + expected + "'");
    const std::pair<std::uint64_t, const char*> sizes[] = {{0, "0.0"}, {static_cast<std::uint64_t>(6.1 * GiB), "6.1"}, {static_cast<std::uint64_t>(7.8 * GiB), "7.8"}, {GiB / 2, "0.5"}, {1536 * GiB, "1536"}, {20000 * GiB, "9999"}};
    for (const auto& [bytes, expected] : sizes) require(AgcDriver::FormatOverlayGigabytes(bytes) == expected, std::to_string(bytes) + " bytes are shown as '" + AgcDriver::FormatOverlayGigabytes(bytes) + "' GB, not '" + expected + "'");
}

void detailComposeTests() {
    const auto statistics = countedStatistics();
    const auto full = AgcDriver::ComposeOverlay(statistics, true, true, fullDetails());
    const std::vector<std::string> expected{"MIN/AVG/MAX: 16.0/16.0/16.0", "GPU: 4.0 MS", "DRIVER: 1.5 MS", "DRAWS: 1240  DISPATCH: 85", "SHADERS: 3  TOTAL: 1503", "VRAM: 6.1/7.8 GB", "RAM: 3.2 GB", "RES: 1920X1080 / 2560X1440"};
    require(full.below == expected, "the full panel's lines below the graph read '" + (full.below.empty() ? std::string() : full.below.back()) + "' last, of " + std::to_string(full.below.size()));
    for (const auto& line : full.below) require(line.size() <= static_cast<std::size_t>(OverlayMetrics::Columns), "'" + line + "' is wider than the panel");

    OverlayStatistics uncounted;
    for (int i = 0; i <= 60; ++i) uncounted.AddPresent(Start + i * 16ms, 0, 0);
    const auto plain = AgcDriver::ComposeOverlay(uncounted, false, false);
    const auto none = AgcDriver::ComposeOverlay(uncounted, false, false, AgcDriver::OverlayDetails{});
    require(none.above == plain.above && none.below == plain.below && none.below.size() == 1, "a panel without packets, pipelines, memory or a display buffer shows more than the first lines");

    auto details = fullDetails();
    details.vram.reset();
    details.ram.reset();
    details.display = {};
    const auto partial = AgcDriver::ComposeOverlay(statistics, false, false, details);
    require(partial.below.size() == 3 && partial.below.back().rfind("SHADERS", 0) == 0, "a line is shown without its data: the last line is '" + partial.below.back() + "'");

    details = fullDetails();
    details.display = {3840, 2160};
    details.window = {16384, 16384};
    require(AgcDriver::ComposeOverlay(uncounted, false, false, details).below.back() == "RES: 3840X2160/16384X16384", "a long RES line is not written without spaces");
    details.display = {12345, 12345};
    details.window = {12345, 12345};
    require(AgcDriver::ComposeOverlay(uncounted, false, false, details).below.back() == "RAM: 3.2 GB", "a RES line wider than the panel is shown");
    details = fullDetails();
    details.window = {};
    require(AgcDriver::ComposeOverlay(uncounted, false, false, details).below.back() == "RAM: 3.2 GB", "RES is shown without a window size");
    auto large = countedStatistics();
    auto counters = large.Latest();
    counters.draws += 12345 * 2;
    counters.pipelines = 123456;
    large.AddPresent(Start + 62 * 16ms, counters);
    const auto busy = AgcDriver::ComposeOverlay(large, false, false);
    require(busy.below.size() == 3 && busy.below[2] == "SHADERS: 121K  TOTAL: 123K", "large counts are not shortened: '" + busy.below.back() + "'");
}

void fullPanelLayoutTests() {
    auto content = AgcDriver::ComposeOverlay(countedStatistics(), true, true, fullDetails());
    content.below[3] = "DRAWS: 9999  DISPATCH: 12K";
    content.below[4] = "SHADERS: 999M  TOTAL: 9999";
    const VkExtent2D hd{1920, 1080};
    const auto layout = AgcDriver::LayOutOverlay(content, hd, OverlayCorner::TopLeft);
    require(layout.scale == 2 && layout.bounds.height * 3 <= static_cast<std::int32_t>(hd.height), "the full panel at 1080p is " + std::to_string(layout.bounds.height) + " pixels tall at scale " + std::to_string(layout.scale) + ", more than a third of the window");
    const auto text = coverage(layout, static_cast<std::uint32_t>(AgcDriver::OverlayColor::Text));
    std::int32_t top = OverlayMetrics::Padding;
    for (const auto& line : content.above) {
        requireLineDrawn(layout, text, line, top);
        top += OverlayMetrics::LinePitch;
    }
    top += OverlayMetrics::GraphHeight + OverlayMetrics::GraphGap;
    for (const auto& line : content.below) {
        requireLineDrawn(layout, text, line, top);
        top += OverlayMetrics::LinePitch;
    }
    require(layout.bounds.height == (top - OverlayMetrics::LinePitch + OverlayMetrics::GlyphHeight + OverlayMetrics::Padding) * layout.scale, "the panel does not end one padding below its last line");
    const std::pair<VkExtent2D, std::int32_t> scales[] = {{{1280, 720}, 2}, {{2560, 1440}, 3}, {{3840, 2160}, 5}, {{320, 180}, 1}};
    for (const auto& [extent, scale] : scales) {
        const auto placed = AgcDriver::LayOutOverlay(content, extent, OverlayCorner::BottomRight);
        require(placed.scale == scale && placed.bounds.x >= 0 && placed.bounds.y >= 0 && placed.bounds.y + placed.bounds.height <= static_cast<std::int32_t>(extent.height), "the full panel does not fit " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + " at scale " + std::to_string(scale));
    }
    const std::string added = "BCHKLOTW";
    for (const char letter : added) {
        const auto& glyph = AgcDriver::OverlayGlyph(letter);
        require(std::any_of(glyph.begin(), glyph.end(), [](std::uint8_t row) { return row != 0; }) && std::all_of(glyph.begin(), glyph.end(), [](std::uint8_t row) { return row <= 0x1f; }), std::string("the glyph of '") + letter + "' is empty or wider than 5 pixels");
        for (const char other : std::string("0123456789ADEFGIMNPRSUVX") + added) {
            if (other != letter) require(glyph != AgcDriver::OverlayGlyph(other), std::string("'") + letter + "' and '" + other + "' have the same glyph");
        }
    }
}

void fontTests() {
    const auto characters = AgcDriver::OverlayFontCharacters();
    for (const char character : characters) {
        const auto& glyph = AgcDriver::OverlayGlyph(character);
        require(std::all_of(glyph.begin(), glyph.end(), [](std::uint8_t row) { return row <= 0x1f; }), std::string("the glyph of '") + character + "' is wider than 5 pixels");
        require(character == ' ' || std::any_of(glyph.begin(), glyph.end(), [](std::uint8_t row) { return row != 0; }), std::string("the glyph of '") + character + "' is empty");
        for (const char other : characters) {
            if (other != character) require(glyph != AgcDriver::OverlayGlyph(other), std::string("'") + character + "' and '" + other + "' have the same glyph");
        }
    }
    for (const char letter : std::string("JQYZ%+-_")) require(characters.find(letter) != std::string_view::npos, std::string("the font lists no '") + letter + "'");
    for (const char missing : std::string("qjy(>,#")) {
        bool refused = false;
        try {
            static_cast<void>(AgcDriver::OverlayGlyph(missing));
        } catch (const std::exception&) {
            refused = true;
        }
        require(refused && characters.find(missing) == std::string_view::npos, std::string("'") + missing + "' is drawn or listed although the font has no glyph for it");
    }
}

void widePanelTests() {
    OverlayContent wide;
    wide.columns = 48;
    wide.graph = false;
    for (int i = 0; i < 20; ++i) wide.below.push_back(std::string(48, i % 2 == 0 ? 'W' : 'Z'));
    const VkExtent2D small{1280, 720};
    const auto layout = AgcDriver::LayOutOverlay(wide, small, OverlayCorner::TopLeft);
    const auto s = layout.scale;
    require(layout.bounds.width == (48 * OverlayMetrics::Advance - 1 + 2 * OverlayMetrics::Padding) * s, "a 48-column panel is " + std::to_string(layout.bounds.width) + " pixels wide at scale " + std::to_string(s));
    require(layout.bounds.height == (2 * OverlayMetrics::Padding + 20 * OverlayMetrics::LinePitch - (OverlayMetrics::LinePitch - OverlayMetrics::GlyphHeight)) * s, "a panel without a graph keeps the graph's rows");
    const auto text = coverage(layout, static_cast<std::uint32_t>(AgcDriver::OverlayColor::Text));
    for (std::size_t line = 0; line < wide.below.size(); ++line) requireLineDrawn(layout, text, wide.below[line], OverlayMetrics::Padding + static_cast<std::int32_t>(line) * OverlayMetrics::LinePitch);
    for (const auto& rect : layout.rects) require(rect.color != static_cast<std::uint32_t>(AgcDriver::OverlayColor::Graph), "a panel without a graph draws graph bars");
    const std::pair<VkExtent2D, std::int32_t> scales[] = {{{1280, 720}, 2}, {{1920, 1080}, 2}, {{3840, 2160}, 5}, {{320, 180}, 1}};
    for (const auto& [extent, scale] : scales) require(AgcDriver::OverlayScale(extent, wide) == scale, "a 48-column panel at " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + " is drawn at scale " + std::to_string(AgcDriver::OverlayScale(extent, wide)) + ", not " + std::to_string(scale));
    auto widest = wide;
    widest.columns = 140;
    require(AgcDriver::OverlayScale({3840, 2160}, widest) == 4, "a 140-column panel at 3840x2160 is not shrunk from scale 5 to 4");
    const auto refused = [](const OverlayContent& content) {
        try {
            static_cast<void>(AgcDriver::LayOutOverlay(content, {1920, 1080}, OverlayCorner::TopLeft));
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };
    auto longer = wide;
    longer.below.push_back(std::string(49, 'W'));
    require(refused(longer), "a line longer than the panel's columns was drawn");
    auto graphless = wide;
    graphless.frameTimes = {16.0};
    require(refused(graphless), "frame times were laid out on a panel without a graph");
    OverlayContent cramped{{"FPS: 60.0"}, {16.0}, {}};
    cramped.columns = 20;
    require(refused(cramped), "a graph of 120 frames was laid out on a 20-column panel");
    cramped.columns = 21;
    require(!refused(cramped), "a 21-column panel cannot hold the graph");
}

void pageCycleTests() {
    for (std::uint32_t start = 0; start < AgcDriver::OverlayPageCount; ++start) {
        for (std::uint64_t presses = 0; presses <= 3; ++presses) {
            const auto page = AgcDriver::OverlayPageAfter(start, presses);
            require(page == (start + presses) % 3, "starting on page " + std::to_string(start) + ", " + std::to_string(presses) + " presses of F10 show page " + std::to_string(page));
        }
    }
    require(AgcDriver::OverlayPageAfter(2, ~std::uint64_t{0}) == 2 && AgcDriver::OverlayPageAfter(1, ~std::uint64_t{0} - 1) == 0, "the page wraps wrongly after 2^64 presses");
    bool refused = false;
    try {
        static_cast<void>(AgcDriver::OverlayPageAfter(3, 0));
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a starting page 3 was accepted");
}

AgcDriver::OverlayDetailSample fullDetailSample() {
    AgcDriver::OverlayDetailSample sample;
    sample.fps = 59.94;
    sample.frameMs = 16.7;
    sample.busy = {{"Q0", 63.4}, {"Q20", 4.2}, {"COMMIT", 12}, {"FINISH", 3}, {"PRESENT", 5.5}, {"Q21", 100}, {"Q22", 0}};
    sample.phases = {{"Driver.Packet", 3.21}, {"Vulkan.GraphicsPipelineLibrary.shader_compile_and_link", 12.5}, {"Recorder.Sync", 0.4}, {"GpuMutex.wait.draw", 0.06}, {"Driver.Draw.capture_resources", 0.9}};
    sample.queueWaits = AgcDriver::OverlayQueueWaits{0.4, 0.2, 1.3};
    sample.renderPassesPerFrame = 12;
    sample.pipelinesCompiling = 3;
    sample.pipelinesPerSecond = 12;
    sample.drawCacheHitPercent = 97.1;
    sample.pipelineCacheHitPercent = 99.9;
    sample.flipsPerSecond = 60;
    sample.presentsPerSecond = 59.94;
    sample.droppedFrames = 1;
    sample.repeatedPresents = 2;
    sample.commit = AgcDriver::OverlayMemory{static_cast<std::uint64_t>(12.3 * GiB), static_cast<std::uint64_t>(31.9 * GiB)};
    sample.pageFaultsPerSecond = 1234;
    sample.hostImports = 123;
    sample.hostImportBytes = AgcDriver::OverlayMemory{static_cast<std::uint64_t>(1.2 * GiB), 6 * GiB};
    sample.cpuCopyMegabytesPerSecond = 12.3;
    sample.addressCopyCapPercent = 3;
    sample.unimplementedPerSecond = 0;
    sample.unimplementedTotal = 4;
    sample.refusedShaders = 2;
    sample.title = "PPSA01234";
    sample.presentMode = "fifo_latest_ready";
    return sample;
}

void requireDrawable(const OverlayContent& content, const std::string& what) {
    const auto font = AgcDriver::OverlayFontCharacters();
    for (const auto* lines : {&content.above, &content.below}) {
        for (const auto& line : *lines) {
            require(line.size() <= static_cast<std::size_t>(content.columns), what + ": '" + line + "' is wider than its " + std::to_string(content.columns) + " columns");
            for (const char character : line) {
                require(font.find(character) != std::string_view::npos, what + ": '" + line + "' has '" + std::string(1, character) + "', which the font cannot draw");
                static_cast<void>(AgcDriver::OverlayGlyph(character));
            }
        }
    }
}

void pageTwoComposeTests() {
    const auto full = AgcDriver::ComposeOverlayDetails(fullDetailSample());
    require(full.columns == OverlayMetrics::DetailColumns && !full.graph && full.above.empty() && full.frameTimes.empty(), "page 2 is not a 46-column panel without a graph");
    const auto phase = [](const std::string& name, const std::string& amount) { return name + std::string(46 - name.size() - amount.size(), ' ') + amount; };
    const std::vector<std::string> expected{
        "FPS: 59.9  FRAME: 16.7 MS",
        "BUSY %: Q0 63 Q20 4 COMMIT 12 FINISH 3",
        "        PRESENT 6 Q21 100 Q22 0",
        phase("DRIVER.PACKET", " 3.2 MS/FRAME"),
        "VULKAN.GRAPHICSPIPELINELIBRARY.S 12.5 MS/FRAME",
        phase("RECORDER.SYNC", " 0.4 MS/FRAME"),
        phase("GPUMUTEX.WAIT.DRAW", " 0.1 MS/FRAME"),
        phase("DRIVER.DRAW.CAPTURE_RESOURCES", " 0.9 MS/FRAME"),
        "Q0 WAIT MS/FRAME: MEM 0.4 LOCK 0.2 GPU 1.3",
        "RENDER PASSES/FRAME: 12.0",
        "PIPELINES: COMPILING 3  NEW/S 12",
        "HIT %: DRAW CACHE 97.1  PIPELINES 99.9",
        "FLIPS/S: 60.0  PRESENTS/S: 59.9",
        "DROPPED/S: 1  REPEATED/S: 2",
        "COMMIT: 12.3/31.9 GB  FAULTS/S: 1234",
        "HOST IMPORTS: 123  1.2/6.0 GB",
        "CPU COPIES: 12.3 MB/S  CAP: 3.0%",
        "UNIMPLEMENTED: 0/S  4 TOTAL",
        "REFUSED SHADERS: 2",
        "TITLE: PPSA01234  PRESENT: FIFO_LATEST_READY",
    };
    require(full.below.size() == expected.size(), "page 2 of a full sample has " + std::to_string(full.below.size()) + " lines, not " + std::to_string(expected.size()));
    for (std::size_t line = 0; line < expected.size(); ++line) require(full.below[line] == expected[line], "page 2 line " + std::to_string(line) + " reads '" + full.below[line] + "', not '" + expected[line] + "'");
    requireDrawable(full, "page 2 of a full sample");

    const auto empty = AgcDriver::ComposeOverlayDetails({});
    require(empty.below == std::vector<std::string>{"FPS: 0.0  FRAME: 0.0 MS"}, "page 2 of an empty sample shows " + std::to_string(empty.below.size()) + " lines, the last '" + empty.below.back() + "'");

    auto partial = fullDetailSample();
    partial.flipsPerSecond.reset();
    partial.commit.reset();
    partial.title.clear();
    partial.repeatedPresents.reset();
    partial.cpuCopyMegabytesPerSecond.reset();
    const auto some = AgcDriver::ComposeOverlayDetails(partial);
    const auto has = [&](const std::string& line) { return std::find(some.below.begin(), some.below.end(), line) != some.below.end(); };
    require(has("PRESENTS/S: 59.9") && has("DROPPED/S: 1") && has("FAULTS/S: 1234") && has("PRESENT: FIFO_LATEST_READY") && has("CPU COPIES: CAP: 3.0%"), "a line with a missing source does not show the rest of its values alone");

    AgcDriver::OverlayDetailSample odd;
    odd.busy = {{"queue worker 0x57", 1e9}, {"q(1)", 1}};
    odd.phases = {{"Weird<name>#with,symbols/and lower case and a very long tail beyond the panel", 1e9}};
    odd.title = "ab-cd_12";
    odd.presentMode = "mailbox";
    const auto sanitized = AgcDriver::ComposeOverlayDetails(odd);
    requireDrawable(sanitized, "page 2 of names the font cannot draw");
    require(AgcDriver::OverlayText("abc-XYZ_09(%)é") == "ABC-XYZ_09 %   ", "OverlayText maps '" + AgcDriver::OverlayText("abc-XYZ_09(%)é") + "'");
    for (const auto& line : sanitized.below) require(line.size() <= 46, "'" + line + "' is wider than page 2");

    OverlayStatistics statistics = countedStatistics();
    auto details = fullDetails();
    details.display = {12345, 12345};
    details.window = {16384, 16384};
    requireDrawable(AgcDriver::ComposeOverlay(statistics, true, true, details), "page 1 of a full panel");
}

void pageTwoLayoutTests() {
    const auto content = AgcDriver::ComposeOverlayDetails(fullDetailSample());
    const auto lines = static_cast<std::int32_t>(content.below.size());
    const auto width = 46 * OverlayMetrics::Advance - 1 + 2 * OverlayMetrics::Padding;
    const auto height = 2 * OverlayMetrics::Padding + lines * OverlayMetrics::LinePitch - (OverlayMetrics::LinePitch - OverlayMetrics::GlyphHeight);
    require(width + 2 * OverlayMetrics::Margin <= 1280 && height + 2 * OverlayMetrics::Margin <= 720, "page 2 does not fit 1280x720 at scale 1");
    const std::pair<VkExtent2D, std::int32_t> scales[] = {{{1280, 720}, 2}, {{1920, 1080}, 2}, {{3840, 2160}, 5}, {{640, 360}, 1}};
    for (const auto& [extent, scale] : scales) {
        for (const auto corner : {OverlayCorner::TopLeft, OverlayCorner::BottomRight}) {
            const auto layout = AgcDriver::LayOutOverlay(content, extent, corner);
            require(layout.scale == scale, "page 2 at " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + " is drawn at scale " + std::to_string(layout.scale) + ", not " + std::to_string(scale));
            require(layout.bounds.width == width * scale && layout.bounds.height == height * scale, "page 2's panel is " + std::to_string(layout.bounds.width) + "x" + std::to_string(layout.bounds.height) + " at scale " + std::to_string(scale));
            require(layout.bounds.x >= 0 && layout.bounds.y >= 0 && layout.bounds.x + layout.bounds.width <= static_cast<std::int32_t>(extent.width) && layout.bounds.y + layout.bounds.height <= static_cast<std::int32_t>(extent.height), "page 2 leaves " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
            for (const auto& rect : layout.rects) require(rect.color != static_cast<std::uint32_t>(AgcDriver::OverlayColor::Graph), "page 2 draws graph bars");
            if (corner != OverlayCorner::TopLeft) continue;
            const auto text = coverage(layout, static_cast<std::uint32_t>(AgcDriver::OverlayColor::Text));
            for (std::size_t line = 0; line < content.below.size(); ++line) requireLineDrawn(layout, text, content.below[line], OverlayMetrics::Padding + static_cast<std::int32_t>(line) * OverlayMetrics::LinePitch);
        }
    }
    auto stretched = content;
    while (stretched.below.size() < 60) stretched.below.push_back("PIPELINES: COMPILING 3  NEW/S 12");
    require(AgcDriver::OverlayScale({3840, 2160}, stretched) == 3, "a 60-line page 2 at 3840x2160 is not shrunk to scale 3");
}

void sampleTests() {
    require(AgcDriver::OverlaySampleDue(std::nullopt, Start), "the first frame is not sampled");
    require(!AgcDriver::OverlaySampleDue(Start, Start + 999ms) && AgcDriver::OverlaySampleDue(Start, Start + 1000ms), "memory is sampled more than once a second, or not after a second");
    static std::uint8_t* volatile touched = nullptr;
    const auto before = AgcDriver::ReadProcessPrivateBytes();
    require(before.has_value() && *before != 0, "the process's private memory cannot be read");
    constexpr std::size_t Bytes = std::size_t{64} << 20u;
    const auto block = std::make_unique<std::uint8_t[]>(Bytes);
    touched = block.get();
    std::memset(touched, 0x5a, Bytes);
    const auto after = AgcDriver::ReadProcessPrivateBytes();
    require(after.has_value() && *after >= *before + Bytes * 3 / 4, "touching 64 MiB raised the private memory from " + std::to_string(*before) + " to " + std::to_string(after.value_or(0)) + " bytes");
    require(touched[Bytes - 1] == 0x5a, "the touched block was not written");
}

constexpr const char* PacketScope = "Driver.Packet";
constexpr const char* WorkerWaitScope = "Driver.WorkerWait";
constexpr const char* LockScope = "GpuMutex.wait";
constexpr const char* SyncScope = "Recorder.Sync";
constexpr const char* SerialScope = "Recorder.WaitSerial";
constexpr const char* WaitScope = "Graphics.Wait";
constexpr const char* FinishScope = "Driver.DrawFinish";
constexpr const char* MemoryStage = "memory_wait";
constexpr const char* RenderingStage = "rendering_wait";
constexpr const char* DrawSite = "draw";
constexpr const char* PresentSite = "present";
constexpr const char* FenceStage = "fence_wait";

AgcDriver::TraceThread::Total total(const char* scope, const char* stage, std::chrono::milliseconds length, std::uint64_t count) {
    return AgcDriver::TraceThread::Total{scope, stage, std::chrono::duration_cast<Clock::duration>(length), count};
}

void traceDeltaTests() {
    using AgcDriver::TraceThreadTotals;
    const std::vector<TraceThreadTotals> before{
        {"queue worker 0x0", {total(PacketScope, nullptr, 100ms, 10), total(PacketScope, MemoryStage, 5ms, 1), total(WorkerWaitScope, nullptr, 50ms, 2)}},
        {"queue worker 0x1", {total(PacketScope, nullptr, 10ms, 1)}},
    };
    const std::vector<TraceThreadTotals> now{
        {"queue worker 0x0", {total(PacketScope, nullptr, 500ms, 60), total(PacketScope, MemoryStage, 35ms, 3), total(PacketScope, RenderingStage, 10ms, 1), total(LockScope, DrawSite, 20ms, 4), total(LockScope, PresentSite, 5ms, 1), total(SyncScope, nullptr, 6ms, 2), total(SerialScope, nullptr, 4ms, 1), total(WaitScope, FenceStage, 5ms, 1), total(WaitScope, nullptr, 9ms, 1), total(WorkerWaitScope, nullptr, 450ms, 40)}},
        {"queue worker 0x1", {total(PacketScope, nullptr, 110ms, 2)}},
        {"queue worker 0xa", {total(PacketScope, nullptr, 50ms, 1)}},
        {"draw committer", {total(AgcDriver::PerformanceTrace::RenderPassScope, nullptr, 0ms, 120), total(FinishScope, nullptr, 80ms, 40), total(SyncScope, nullptr, 7ms, 1)}},
    };
    const auto figures = AgcDriver::OverlayTraceDeltas(before, now, 1s, 40, true);
    require(figures.busy.size() == 3 && figures.busy[0].name == "Q0" && figures.busy[1].name == "Q1" && figures.busy[2].name == "Qa", "the busy queues are not Q0, Q1 and Qa in that order");
    require(within(figures.busy[0].percent, 40) && within(figures.busy[1].percent, 10) && within(figures.busy[2].percent, 5), "the busy shares over a second are not 40, 10 and 5 %");
    const std::vector<std::pair<std::string, double>> phases{{"Driver.Packet", 13.75}, {"Driver.DrawFinish", 2}, {"Driver.Packet.memory_wait", 0.75}, {"GpuMutex.wait.draw", 0.5}, {"Recorder.Sync", 0.325}};
    require(figures.phases.size() == phases.size(), "page 2 does not keep the top 5 phases");
    for (std::size_t index = 0; index < phases.size(); ++index) require(figures.phases[index].name == phases[index].first && within(figures.phases[index].msPerFrame, phases[index].second), "phase " + std::to_string(index) + " is " + figures.phases[index].name + " at " + std::to_string(figures.phases[index].msPerFrame) + " ms per frame, not " + phases[index].first);
    require(figures.queueWaits && within(figures.queueWaits->memoryMs, 1.0) && within(figures.queueWaits->lockMs, 0.625) && within(figures.queueWaits->gpuMs, 0.375), "queue 0's waits per frame are not 1 ms on memory, 0.625 ms on the lock and 0.375 ms on the GPU");
    require(figures.renderPassesPerFrame && within(*figures.renderPassesPerFrame, 3), "120 render passes over 40 frames are not 3 per frame");
    require(!AgcDriver::OverlayTraceDeltas(before, now, 1s, 40, false).renderPassesPerFrame, "render passes are shown while they are not counted");
    const auto noFrames = AgcDriver::OverlayTraceDeltas(before, now, 1s, 0, true);
    require(noFrames.busy.size() == 3 && noFrames.phases.empty() && !noFrames.queueWaits && !noFrames.renderPassesPerFrame, "per-frame figures are shown for a second without frames");
    require(AgcDriver::OverlayTraceDeltas(before, now, Clock::duration{}, 40, true).busy.empty(), "busy shares are shown for no elapsed time");
    const auto noQueue = AgcDriver::OverlayTraceDeltas({}, std::vector<TraceThreadTotals>{now[3]}, 1s, 40, true);
    require(noQueue.busy.empty() && !noQueue.queueWaits && noQueue.renderPassesPerFrame, "a trace without queue workers shows busy shares or queue waits");
    AgcDriver::OverlayDetailSample sample;
    sample.busy = figures.busy;
    sample.phases = figures.phases;
    sample.queueWaits = figures.queueWaits;
    sample.renderPassesPerFrame = figures.renderPassesPerFrame;
    requireDrawable(AgcDriver::ComposeOverlayDetails(sample), "page 2 from trace totals");
}

void hitRateTests() {
    require(within(AgcDriver::OverlayHitPercent({100, 80}, {300, 230}).value_or(-1), 75), "150 hits of 200 lookups are not 75 %");
    require(within(AgcDriver::OverlayHitPercent({1000, 900}, {200, 150}).value_or(-1), 75), "counters that restarted (the draw cache's 10 s report) are not read from zero");
    require(!AgcDriver::OverlayHitPercent({5, 5}, {5, 5}), "a hit rate is shown without lookups");
    require(!AgcDriver::OverlayHitPercent({0, 0}, {4, 5}), "more hits than lookups give a hit rate");
}

void presentModeTests() {
    require(AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_FIFO_KHR) == "fifo" && AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_FIFO_LATEST_READY_KHR) == "fifo_latest_ready" && AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_MAILBOX_KHR) == "mailbox" && AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_IMMEDIATE_KHR) == "immediate", "the present modes are not named as ANYPS5_PRESENT_MODE names them");
    require(AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_FIFO_RELAXED_KHR).empty() && AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_MAX_ENUM_KHR).empty(), "a present mode the driver never selects has a name");
    AgcDriver::OverlayDetailSample sample;
    sample.title = "PPSA01234";
    sample.presentMode = AgcDriver::OverlayPresentModeName(VK_PRESENT_MODE_FIFO_LATEST_READY_KHR);
    const auto content = AgcDriver::ComposeOverlayDetails(sample);
    require(content.below.back() == "TITLE: PPSA01234  PRESENT: FIFO_LATEST_READY", "the longest present mode is drawn as '" + content.below.back() + "'");
}

void operatingSystemTests() {
    const auto parsed = AgcDriver::ParseCommitCharge("MemTotal:       16318412 kB\nCommitLimit:     8159204 kB\nCommitted_AS:   12345678 kB\nVmallocTotal:   34359738367 kB\n");
    require(parsed && parsed->used == 12345678ull * 1024 && parsed->budget == 8159204ull * 1024, "Committed_AS and CommitLimit are not read from /proc/meminfo's text");
    require(!AgcDriver::ParseCommitCharge("Committed_AS:   12345678 kB\n"), "a commit charge is shown without its limit");
    require(!AgcDriver::ParseCommitCharge("CommitLimit:     8159204 kB\nCommitted_AS:   12 MB\n"), "a value in another unit is read as kB");
    require(!AgcDriver::ParseCommitCharge(""), "an empty /proc/meminfo gives a commit charge");
    const auto commit = AgcDriver::ReadCommitCharge();
#ifdef __APPLE__
    require(!commit, "a commit charge is read on macOS, which reports none");
#else
    require(commit && commit->used != 0 && commit->budget != 0, "the system's commit charge and limit cannot be read");
#endif
    static std::uint8_t* volatile touched = nullptr;
    const auto before = AgcDriver::ReadPageFaults();
    require(before.has_value(), "the process's page faults cannot be read");
    constexpr std::size_t Bytes = std::size_t{16} << 20u;
    const auto block = std::make_unique<std::uint8_t[]>(Bytes);
    touched = block.get();
    std::memset(touched, 0x3c, Bytes);
    const auto after = AgcDriver::ReadPageFaults();
    require(after && *after > *before, "touching 16 MiB of new memory did not raise the page faults from " + std::to_string(*before));
    require(touched[Bytes - 1] == 0x3c, "the touched block was not written");
}

void addressCopyFoldTests() {
    using AgcDriver::Graphics::AddressCopyCounters;
    const auto start = AddressCopyCounters::Fold();
    auto& counters = AddressCopyCounters::ThisThread();
    require(&counters == &AddressCopyCounters::ThisThread(), "one thread got two address copy counters");
    counters.Count(std::uint64_t{3} << 20u);
    counters.Count(std::uint64_t{1} << 20u);
    bool separate = false;
    std::thread worker([&] {
        auto& own = AddressCopyCounters::ThisThread();
        separate = &own != &counters;
        own.Count(std::uint64_t{5} << 20u);
    });
    worker.join();
    require(separate, "two builders share address copy counters");
    const auto folded = AddressCopyCounters::Fold();
    require(folded.bytes - start.bytes == std::uint64_t{9} << 20u && folded.largest == std::uint64_t{5} << 20u, "two builders' 9 MiB of copies with a largest build of 5 MiB fold to " + std::to_string(folded.bytes - start.bytes) + " bytes, largest " + std::to_string(folded.largest));
    const auto idle = AddressCopyCounters::Fold();
    require(idle.bytes == folded.bytes && idle.largest == 0, "a second without copies keeps the previous second's largest build");
    std::thread next([] { AddressCopyCounters::ThisThread().Count(std::uint64_t{2} << 20u); });
    next.join();
    const auto reused = AddressCopyCounters::Fold();
    require(reused.bytes == idle.bytes + (std::uint64_t{2} << 20u) && reused.largest == std::uint64_t{2} << 20u, "the copies of a builder that ended were lost, or a new builder restarted them");
    if (std::getenv("APS5_ADDRESS_COPY_MAX_MIB") == nullptr) require(AddressCopyCounters::Limit() == std::uint64_t{64} << 20u, "the address copy cap is not 64 MiB by default");
    AgcDriver::Graphics::SetAddressCopyLimit(std::uint64_t{8} << 20u);
    require(AddressCopyCounters::Limit() == std::uint64_t{8} << 20u, "the cap in use is not the one set");
    AgcDriver::Graphics::SetAddressCopyLimit(0);
}

void driverSourceTests() {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        NotImplemented_nid_no_patch("performance overlay test");
    } catch (const std::runtime_error&) {
    }
    require(NotImplementedCalls_nid_no_patch() == before + 1, "an unimplemented-function call is not counted");
    if (std::getenv("APS5_SKIP_REFUSED_DRAWS") == nullptr) require(!AgcDriver::DriverDetail::RefusedShaderCount(), "refused shaders are counted while a refused shader still stops the title");
    require(AgcDriver::DriverDetail::RegistrationPreparationsPending() == std::optional<std::size_t>{0}, "a process that prepared no shaders has a compile backlog");
}

void traceSwitchTests() {
    using AgcDriver::PerformanceOverlay;
    using AgcDriver::PerformanceTrace;
    require(!PerformanceTrace::Configured(), "the overlay statistics test runs with APS5_TRACE_* set");
    require(PerformanceOverlay::Page() == 0 && !PerformanceTrace::Recording() && !PerformanceTrace::OverlayTotals(), "the trace records before page 2 is shown");
    std::string failure;
    std::thread([&] {
        if (PerformanceTrace::Thread() != nullptr) failure = "a thread is bound to the trace while nothing records";
    }).join();
    require(failure.empty(), failure);
    PerformanceOverlay::Toggle();
    require(PerformanceOverlay::Page() == 1 && !PerformanceTrace::Recording() && !PerformanceTrace::OverlayTotals(), "page 1 turns the trace on");
    PerformanceOverlay::Toggle();
    require(PerformanceOverlay::Page() == 2 && PerformanceTrace::Recording() && PerformanceTrace::OverlayTotals(), "page 2 does not turn the trace totals on");
    std::thread([&] {
        PerformanceTrace::BindThread("queue worker 0x5");
        auto* thread = PerformanceTrace::Thread();
        if (thread == nullptr || thread != PerformanceTrace::Thread()) {
            failure = "a named worker is not bound once while page 2 shows";
            return;
        }
        const auto begin = Clock::now();
        thread->Add(PacketScope, nullptr, begin, begin + 3ms);
        PerformanceTrace::Count(AgcDriver::PerformanceTrace::RenderPassScope);
        PerformanceTrace::Count(AgcDriver::PerformanceTrace::RenderPassScope);
        const auto snapshot = thread->Collect();
        if (!snapshot.slices.empty() || snapshot.dropped) failure = "page 2's trace totals buffer slices";
    }).join();
    require(failure.empty(), failure);
    std::thread([&] {
        if (PerformanceTrace::Thread() != nullptr) failure = "a thread that never named itself is bound while page 2 shows";
    }).join();
    require(failure.empty(), failure);
    const auto threads = PerformanceTrace::ThreadTotals();
    require(threads.has_value(), "the trace totals cannot be read");
    const auto worker = std::find_if(threads->begin(), threads->end(), [](const AgcDriver::TraceThreadTotals& thread) { return thread.name == "queue worker 0x5"; });
    require(worker != threads->end(), "the named worker's totals are missing");
    std::uint64_t passes = 0;
    Clock::duration packets{};
    for (const auto& entry : worker->totals) {
        if (std::string_view(entry.scope) == AgcDriver::PerformanceTrace::RenderPassScope) passes += entry.count;
        if (std::string_view(entry.scope) == PacketScope && entry.stage == nullptr) packets += entry.length;
    }
    require(passes == 2 && packets == 3ms, "the worker's totals hold " + std::to_string(passes) + " render passes and " + std::to_string(std::chrono::duration<double, std::milli>(packets).count()) + " ms of packets, not 2 and 3 ms");
    PerformanceOverlay::Toggle();
    require(PerformanceOverlay::Page() == 0 && !PerformanceTrace::Recording() && !PerformanceTrace::OverlayTotals(), "leaving page 2 leaves the trace totals on");
    std::thread([&] {
        PerformanceTrace::Count(AgcDriver::PerformanceTrace::RenderPassScope);
        if (PerformanceTrace::Thread() != nullptr) failure = "a thread is bound to the trace after page 2 was left";
    }).join();
    require(failure.empty(), failure);
}

void costReport() {
    auto& counters = AgcDriver::PacketCounters::ThisThread();
    constexpr int Increments = 20'000'000;
    const auto counted = Clock::now();
    for (int i = 0; i < Increments; ++i) counters.CountDraw();
    const auto countNs = std::chrono::duration<double, std::nano>(Clock::now() - counted).count() / Increments;
    const auto summed = Clock::now();
    for (int i = 0; i < 1000; ++i) static_cast<void>(AgcDriver::PacketCounters::Sum());
    const auto sumUs = std::chrono::duration<double, std::micro>(Clock::now() - summed).count() / 1000;
    const auto sampled = Clock::now();
    for (int i = 0; i < 100; ++i) static_cast<void>(AgcDriver::ReadProcessPrivateBytes());
    const auto ramUs = std::chrono::duration<double, std::micro>(Clock::now() - sampled).count() / 100;
    std::printf("hidden cost: %.2f ns per counted draw or dispatch packet; shown: %.2f us to sum the packet counters per frame, %.1f us to read the private memory once a second\n", countNs, sumUs, ramUs);
}

void paletteTests() {
    const auto sdr = AgcDriver::OverlayPalette(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    require(sdr[1] == std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f} && sdr[0][3] > 0.0f && sdr[0][3] < 1.0f, "the SDR palette is not opaque white text over a translucent panel");
    const auto srgb = AgcDriver::OverlayPalette(VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    require(within(srgb[2][1], std::pow((0.8 + 0.055) / 1.055, 2.4), 1e-5) && srgb[1][0] == 1.0f, "an sRGB swapchain gets encoded colors");
    const auto hdr = AgcDriver::OverlayPalette(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT);
    for (std::size_t channel = 0; channel < 3; ++channel) require(within(hdr[1][channel], 0.5807, 0.0015), "HDR10 white text is not at 203 nits (PQ " + std::to_string(hdr[1][channel]) + ")");
    require(std::lround(hdr[0][0] * 1023.0f) == 0 && hdr[0][3] == sdr[0][3], "the HDR10 panel is not black at the same opacity");
}

}

int main() {
    try {
        fpsWindowTests();
        frameRingTests();
        counterTests();
        formatTests();
        lowRateTests();
        settingsTests();
        composeTests();
        layoutTests();
        paletteTests();
        packetStatisticsTests();
        packetCounterTests();
        countFormatTests();
        detailComposeTests();
        fullPanelLayoutTests();
        fontTests();
        widePanelTests();
        pageCycleTests();
        pageTwoComposeTests();
        pageTwoLayoutTests();
        sampleTests();
        traceDeltaTests();
        hitRateTests();
        presentModeTests();
        operatingSystemTests();
        addressCopyFoldTests();
        driverSourceTests();
        traceSwitchTests();
        costReport();
        std::cout << "performance overlay statistics, layout, palette, packet, pipeline, memory, resolution, page 2 source and trace switch tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
