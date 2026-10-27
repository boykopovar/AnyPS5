#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/PerformanceOverlayFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/PerformanceOverlayVert_spv.h"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <charconv>
#include <initializer_list>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace AgcDriver {
namespace {

static_assert(sizeof(OverlayRect) == 20, "overlay rectangles are read as one ivec4 and one uint per instance");

struct OverlayParameters {
    std::array<std::array<float, 4>, OverlayColorCount> palette;
    std::array<float, 2> extent;
};

std::atomic<std::uint64_t> toggleCount{0};
std::atomic<std::uint32_t> currentPage{0};
std::atomic<std::uint64_t> gpuTotal{0};
std::atomic<std::uint64_t> driverTotal{0};
std::atomic<std::uint64_t> pipelineTotal{0};
std::atomic<OverlayDriverCounters (*)()> driverCounters{nullptr};

struct PacketCounterRegistry {
    std::mutex mutex;
    std::deque<PacketCounters> counters;
    std::vector<PacketCounters*> unused;
};

PacketCounterRegistry& packetCounterRegistry() {
    static auto* registry = new PacketCounterRegistry;
    return *registry;
}

struct PacketCounterLease {
    PacketCounters* counters = nullptr;
    ~PacketCounterLease() {
        if (counters == nullptr) return;
        auto& registry = packetCounterRegistry();
        std::lock_guard lock(registry.mutex);
        registry.unused.push_back(counters);
    }
};

constexpr double GigabyteBytes = 1024.0 * 1024.0 * 1024.0;

const OverlaySettings& settings() {
    static const OverlaySettings parsed = [] {
        const auto read = ParseOverlaySettings(std::getenv("ANYPS5_OVERLAY"), std::getenv("ANYPS5_OVERLAY_POS"));
        currentPage.store(read.page, std::memory_order_relaxed);
        if (read.page == 2) PerformanceTrace::SetOverlayTotals(true);
        return read;
    }();
    return parsed;
}

std::string busyName(std::string_view thread) {
    auto queue = thread.substr(TraceFormat::WorkerPrefix.size());
    if (queue.starts_with("0x")) queue.remove_prefix(2);
    return "Q" + std::string(queue);
}

std::optional<std::uint64_t> meminfoKilobytes(std::string_view line, std::string_view name) {
    if (!line.starts_with(name)) return std::nullopt;
    auto rest = line.substr(name.size());
    while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
    std::uint64_t value = 0;
    const auto [last, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (error != std::errc{} || std::string_view(last, static_cast<std::size_t>(rest.data() + rest.size() - last)) != " kB") return std::nullopt;
    return value * 1024u;
}

std::int32_t panelWidth(std::int32_t columns, std::int32_t scale) {
    return (columns * OverlayMetrics::Advance - 1 + 2 * OverlayMetrics::Padding) * scale;
}

std::int32_t panelHeight(const OverlayContent& content, std::int32_t scale) {
    const auto lines = static_cast<std::int32_t>(content.above.size() + content.below.size());
    const auto glyphGap = OverlayMetrics::LinePitch - OverlayMetrics::GlyphHeight;
    const auto graph = content.graph ? OverlayMetrics::GraphHeight + OverlayMetrics::GraphGap : 0;
    return (2 * OverlayMetrics::Padding + lines * OverlayMetrics::LinePitch + graph - glyphGap) * scale;
}

bool fits(VkExtent2D extent, const OverlayContent& content, std::int32_t scale) {
    const auto margins = 2 * OverlayMetrics::Margin * scale;
    return panelWidth(content.columns, scale) + margins <= static_cast<std::int32_t>(extent.width) && panelHeight(content, scale) + margins <= static_cast<std::int32_t>(extent.height);
}

float srgbDecode(float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float pqEncode(float nits) {
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 4096.0 * 128.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 4096.0 * 32.0;
    constexpr double c3 = 2392.0 / 4096.0 * 32.0;
    const double power = std::pow(std::clamp(static_cast<double>(nits) / 10000.0, 0.0, 1.0), m1);
    return static_cast<float>(std::pow((c1 + c2 * power) / (1.0 + c3 * power), m2));
}

bool srgbFormat(VkFormat format) {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}

}

OverlaySettings ParseOverlaySettings(const char* enabled, const char* position) {
    OverlaySettings parsed;
    const std::string_view page = enabled != nullptr ? enabled : "";
    if (page == "1") parsed.page = 1;
    else if (page == "2") parsed.page = 2;
    else if (!page.empty() && page != "0") throw std::runtime_error("ANYPS5_OVERLAY must be 0, 1 or 2, not '" + std::string(page) + "'");
    const std::string_view corner = position != nullptr ? position : "";
    if (corner.empty() || corner == "tl") parsed.corner = OverlayCorner::TopLeft;
    else if (corner == "tr") parsed.corner = OverlayCorner::TopRight;
    else if (corner == "bl") parsed.corner = OverlayCorner::BottomLeft;
    else if (corner == "br") parsed.corner = OverlayCorner::BottomRight;
    else throw std::runtime_error("ANYPS5_OVERLAY_POS must be tl, tr, bl or br, not '" + std::string(corner) + "'");
    return parsed;
}

std::uint32_t OverlayPageAfter(std::uint32_t start, std::uint64_t presses) {
    Graphics::Require(start < OverlayPageCount, "the performance overlay has no such page");
    return static_cast<std::uint32_t>((start + presses % OverlayPageCount) % OverlayPageCount);
}

void PerformanceCounters::AddGpuNanoseconds(std::uint64_t nanoseconds) {
    gpuTotal.fetch_add(nanoseconds, std::memory_order_relaxed);
}

void PerformanceCounters::AddDriverNanoseconds(std::uint64_t nanoseconds) {
    driverTotal.fetch_add(nanoseconds, std::memory_order_relaxed);
}

void PerformanceCounters::CountPipeline() {
    pipelineTotal.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t PerformanceCounters::GpuNanoseconds() {
    return gpuTotal.load(std::memory_order_relaxed);
}

std::uint64_t PerformanceCounters::DriverNanoseconds() {
    return driverTotal.load(std::memory_order_relaxed);
}

std::uint64_t PerformanceCounters::Pipelines() {
    return pipelineTotal.load(std::memory_order_relaxed);
}

PacketCounters& PacketCounters::ThisThread() {
    auto& lease = HostThreadLocal<PacketCounterLease, PacketCounterLease>();
    if (lease.counters == nullptr) {
        auto& registry = packetCounterRegistry();
        std::lock_guard lock(registry.mutex);
        if (registry.unused.empty()) {
            lease.counters = &registry.counters.emplace_back();
        } else {
            lease.counters = registry.unused.back();
            registry.unused.pop_back();
        }
    }
    return *lease.counters;
}

PacketCounters::Totals PacketCounters::Sum() {
    auto& registry = packetCounterRegistry();
    std::lock_guard lock(registry.mutex);
    Totals totals;
    for (const auto& counters : registry.counters) {
        totals.draws += counters.draws.load(std::memory_order_relaxed);
        totals.dispatches += counters.dispatches.load(std::memory_order_relaxed);
    }
    return totals;
}

bool PerformanceCounters::GpuEnabled() {
    return Graphics::Recorder::GpuTimingEnabled();
}

bool PerformanceCounters::DriverEnabled() {
    static const bool enabled = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return enabled;
}

void OverlayStatistics::AddPresent(Clock::time_point at, std::uint64_t gpuNanoseconds, std::uint64_t driverNanoseconds) {
    AddPresent(at, OverlayCounters{gpuNanoseconds, driverNanoseconds});
}

void OverlayStatistics::AddPresent(Clock::time_point at, const OverlayCounters& counters) {
    if (!window.empty()) {
        Graphics::Require(at >= window.back().at, "presents reached the performance overlay out of order");
        frameTimes[frameNext] = std::chrono::duration<double, std::milli>(at - window.back().at).count();
        frameNext = (frameNext + 1) % FrameHistory;
        frameCount = std::min(frameCount + 1, FrameHistory);
        previous = window.back();
    }
    window.push_back({at, counters});
    while (at - window.front().at > Window) window.pop_front();
}

void OverlayStatistics::Reset() {
    window.clear();
    previous.reset();
    frameTimes = {};
    frameCount = 0;
    frameNext = 0;
}

double OverlayStatistics::Fps() const {
    if (window.size() < 2) {
        const auto frameMs = LastFrameMs();
        return frameMs > 0 ? 1000.0 / frameMs : 0;
    }
    const auto seconds = std::chrono::duration<double>(window.back().at - window.front().at).count();
    return seconds > 0 ? static_cast<double>(window.size() - 1) / seconds : 0;
}

double OverlayStatistics::LastFrameMs() const {
    return frameCount == 0 ? 0 : frameTimes[(frameNext + FrameHistory - 1) % FrameHistory];
}

std::vector<double> OverlayStatistics::FrameTimes() const {
    std::vector<double> ordered;
    ordered.reserve(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) ordered.push_back(frameTimes[(frameNext + FrameHistory - frameCount + i) % FrameHistory]);
    return ordered;
}

OverlayStatistics::Range OverlayStatistics::FrameTimeRange() const {
    const auto times = FrameTimes();
    if (times.empty()) return {};
    Range range{times.front(), 0, times.front()};
    double sum = 0;
    for (const auto time : times) {
        range.minimum = std::min(range.minimum, time);
        range.maximum = std::max(range.maximum, time);
        sum += time;
    }
    range.average = sum / static_cast<double>(times.size());
    return range;
}

double OverlayStatistics::perFrame(std::uint64_t OverlayCounters::*counter) const {
    const bool windowed = window.size() >= 2;
    if (!windowed && !previous) return 0;
    const auto first = windowed ? window.front().counters.*counter : previous->counters.*counter;
    const auto last = window.back().counters.*counter;
    const auto frames = windowed ? window.size() - 1 : std::size_t{1};
    return last >= first ? static_cast<double>(last - first) / static_cast<double>(frames) : 0;
}

double OverlayStatistics::GpuMsPerFrame() const {
    return perFrame(&OverlayCounters::gpuNanoseconds) / 1e6;
}

double OverlayStatistics::DriverMsPerFrame() const {
    return perFrame(&OverlayCounters::driverNanoseconds) / 1e6;
}

double OverlayStatistics::DrawsPerFrame() const {
    return perFrame(&OverlayCounters::draws);
}

double OverlayStatistics::DispatchesPerFrame() const {
    return perFrame(&OverlayCounters::dispatches);
}

std::uint64_t OverlayStatistics::PipelinesInWindow() const {
    if (window.empty()) return 0;
    const auto first = window.size() < 2 && previous ? previous->counters.pipelines : window.front().counters.pipelines;
    const auto last = window.back().counters.pipelines;
    return last >= first ? last - first : 0;
}

OverlayCounters OverlayStatistics::Latest() const {
    return window.empty() ? OverlayCounters{} : window.back().counters;
}

const std::array<std::uint8_t, OverlayMetrics::GlyphHeight>& OverlayGlyph(char character) {
    using Glyph = std::array<std::uint8_t, OverlayMetrics::GlyphHeight>;
    static constexpr Glyph space{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static constexpr std::array<Glyph, 10> digits{{
        {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
        {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
        {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
        {0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e},
        {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
        {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e},
        {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
        {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
        {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
    }};
    static constexpr Glyph period{0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};
    static constexpr Glyph colon{0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00};
    static constexpr Glyph slash{0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00};
    static constexpr Glyph percent{0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
    static constexpr Glyph plus{0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00};
    static constexpr Glyph minus{0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static constexpr Glyph underscore{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f};
    static constexpr Glyph a{0x0e, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11};
    static constexpr Glyph b{0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    static constexpr Glyph c{0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e};
    static constexpr Glyph d{0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c};
    static constexpr Glyph e{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    static constexpr Glyph f{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10};
    static constexpr Glyph g{0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f};
    static constexpr Glyph h{0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    static constexpr Glyph i{0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static constexpr Glyph j{0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c};
    static constexpr Glyph k{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static constexpr Glyph l{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    static constexpr Glyph m{0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
    static constexpr Glyph n{0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
    static constexpr Glyph o{0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static constexpr Glyph p{0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    static constexpr Glyph q{0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d};
    static constexpr Glyph r{0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    static constexpr Glyph s{0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    static constexpr Glyph t{0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static constexpr Glyph u{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static constexpr Glyph v{0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04};
    static constexpr Glyph w{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a};
    static constexpr Glyph x{0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    static constexpr Glyph y{0x11, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04};
    static constexpr Glyph z{0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f};
    if (character >= '0' && character <= '9') return digits[static_cast<std::size_t>(character - '0')];
    switch (character) {
        case ' ': return space;
        case '.': return period;
        case ':': return colon;
        case '/': return slash;
        case '%': return percent;
        case '+': return plus;
        case '-': return minus;
        case '_': return underscore;
        case 'A': return a;
        case 'B': return b;
        case 'C': return c;
        case 'D': return d;
        case 'E': return e;
        case 'F': return f;
        case 'G': return g;
        case 'H': return h;
        case 'I': return i;
        case 'J': return j;
        case 'K': return k;
        case 'L': return l;
        case 'M': return m;
        case 'N': return n;
        case 'O': return o;
        case 'P': return p;
        case 'Q': return q;
        case 'R': return r;
        case 'S': return s;
        case 'T': return t;
        case 'U': return u;
        case 'V': return v;
        case 'W': return w;
        case 'X': return x;
        case 'Y': return y;
        case 'Z': return z;
        default: throw std::runtime_error(std::string("the performance overlay font has no glyph for '") + character + "'");
    }
}

std::string_view OverlayFontCharacters() {
    return " .:/%+-_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
}

std::string FormatOverlayValue(double value) {
    const double clamped = std::isfinite(value) ? std::clamp(value, 0.0, 9999.0) : 0.0;
    char text[16];
    std::snprintf(text, sizeof(text), clamped < 99.95 ? "%.1f" : "%.0f", clamped);
    return text;
}

std::string FormatOverlayRate(double value) {
    if (!std::isfinite(value) || value <= 0 || value >= 0.995) return FormatOverlayValue(value);
    char text[16];
    std::snprintf(text, sizeof(text), "%.2f", value);
    return text;
}

std::string FormatOverlayCount(double value) {
    const long long rounded = std::isfinite(value) && value > 0 ? std::llround(std::min(value, 1e12)) : 0;
    char text[24];
    if (rounded < 10000) std::snprintf(text, sizeof(text), "%lld", rounded);
    else if (rounded < 1000000) std::snprintf(text, sizeof(text), "%lldK", rounded / 1000);
    else std::snprintf(text, sizeof(text), "%lldM", std::min<long long>(rounded / 1000000, 999));
    return text;
}

std::string FormatOverlayGigabytes(std::uint64_t bytes) {
    return FormatOverlayValue(static_cast<double>(bytes) / GigabyteBytes);
}

OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver) {
    return ComposeOverlay(statistics, gpu, driver, OverlayDetails{});
}

OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver, const OverlayDetails& details) {
    OverlayContent content;
    if (gpu || driver) content.above.push_back("PROFILED");
    content.above.push_back("FPS: " + FormatOverlayRate(statistics.Fps()));
    content.above.push_back("FRAME: " + FormatOverlayValue(statistics.LastFrameMs()) + " MS");
    content.frameTimes = statistics.FrameTimes();
    const auto range = statistics.FrameTimeRange();
    content.below.push_back("MIN/AVG/MAX: " + FormatOverlayValue(range.minimum) + "/" + FormatOverlayValue(range.average) + "/" + FormatOverlayValue(range.maximum));
    if (gpu) content.below.push_back("GPU: " + FormatOverlayValue(statistics.GpuMsPerFrame()) + " MS");
    if (driver) content.below.push_back("DRIVER: " + FormatOverlayValue(statistics.DriverMsPerFrame()) + " MS");
    const auto latest = statistics.Latest();
    if (latest.draws != 0 || latest.dispatches != 0) content.below.push_back("DRAWS: " + FormatOverlayCount(statistics.DrawsPerFrame()) + "  DISPATCH: " + FormatOverlayCount(statistics.DispatchesPerFrame()));
    if (latest.pipelines != 0) content.below.push_back("SHADERS: " + FormatOverlayCount(static_cast<double>(statistics.PipelinesInWindow())) + "  TOTAL: " + FormatOverlayCount(static_cast<double>(latest.pipelines)));
    if (details.vram) content.below.push_back("VRAM: " + FormatOverlayGigabytes(details.vram->used) + "/" + FormatOverlayGigabytes(details.vram->budget) + " GB");
    if (details.ram) content.below.push_back("RAM: " + FormatOverlayGigabytes(*details.ram) + " GB");
    if (details.display.width != 0 && details.display.height != 0 && details.window.width != 0 && details.window.height != 0) {
        const auto size = [](VkExtent2D extent) { return std::to_string(extent.width) + "X" + std::to_string(extent.height); };
        for (const std::string_view separator : {" / ", "/"}) {
            auto line = "RES: " + size(details.display) + std::string(separator) + size(details.window);
            if (line.size() > static_cast<std::size_t>(content.columns)) continue;
            content.below.push_back(std::move(line));
            break;
        }
    }
    return content;
}

std::string OverlayText(std::string_view text) {
    const auto font = OverlayFontCharacters();
    std::string shown;
    shown.reserve(text.size());
    for (const char character : text) {
        const char upper = character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A') : character;
        shown.push_back(font.find(upper) != std::string_view::npos ? upper : ' ');
    }
    return shown;
}

OverlayContent ComposeOverlayDetails(const OverlayDetailSample& sample) {
    OverlayContent content;
    content.columns = OverlayMetrics::DetailColumns;
    content.graph = false;
    const auto width = static_cast<std::size_t>(content.columns);
    const auto add = [&](std::string line) {
        if (line.size() > width) line.resize(width);
        content.below.push_back(std::move(line));
    };
    const auto joined = [&](const std::string& label, std::initializer_list<std::optional<std::string>> parts) {
        std::string line = label;
        bool any = false;
        for (const auto& part : parts) {
            if (!part) continue;
            line += (line.empty() ? "" : any ? "  " : " ") + *part;
            any = true;
        }
        if (any) add(std::move(line));
    };
    const auto shown = [](const auto& value, auto format) -> std::optional<std::string> {
        if (!value) return std::nullopt;
        return format(*value);
    };
    const auto value = [](double number) { return FormatOverlayValue(number); };
    const auto count = [](double number) { return FormatOverlayCount(number); };
    const auto rate = [](double number) { return FormatOverlayRate(number); };
    add("FPS: " + rate(sample.fps) + "  FRAME: " + value(sample.frameMs) + " MS");
    if (!sample.busy.empty()) {
        const std::string label = "BUSY %:";
        std::string line = label;
        for (const auto& thread : sample.busy) {
            const auto item = " " + OverlayText(thread.name) + " " + count(thread.percent);
            if (line.size() > label.size() && line.size() + item.size() > width) {
                add(std::move(line));
                line = std::string(label.size(), ' ');
            }
            line += item;
        }
        add(std::move(line));
    }
    for (const auto& phase : sample.phases) {
        const auto amount = " " + value(phase.msPerFrame) + " MS/FRAME";
        auto name = OverlayText(phase.name);
        name.resize(std::min(name.size(), width - amount.size()));
        add(name + std::string(width - name.size() - amount.size(), ' ') + amount);
    }
    if (sample.queueWaits) add("Q0 WAIT MS/FRAME: MEM " + value(sample.queueWaits->memoryMs) + " LOCK " + value(sample.queueWaits->lockMs) + " GPU " + value(sample.queueWaits->gpuMs));
    joined("RENDER PASSES/FRAME:", {shown(sample.renderPassesPerFrame, value)});
    joined("PIPELINES:", {shown(sample.pipelinesCompiling, [&](std::uint64_t pending) { return "COMPILING " + count(static_cast<double>(pending)); }), shown(sample.pipelinesPerSecond, [&](double made) { return "NEW/S " + count(made); })});
    joined("HIT %:", {shown(sample.drawCacheHitPercent, [&](double percent) { return "DRAW CACHE " + value(percent); }), shown(sample.pipelineCacheHitPercent, [&](double percent) { return "PIPELINES " + value(percent); })});
    joined("", {shown(sample.flipsPerSecond, [&](double flips) { return "FLIPS/S: " + rate(flips); }), shown(sample.presentsPerSecond, [&](double presents) { return "PRESENTS/S: " + rate(presents); })});
    joined("", {shown(sample.droppedFrames, [&](std::uint64_t dropped) { return "DROPPED/S: " + count(static_cast<double>(dropped)); }), shown(sample.repeatedPresents, [&](std::uint64_t repeated) { return "REPEATED/S: " + count(static_cast<double>(repeated)); })});
    joined("", {shown(sample.commit, [](const OverlayMemory& memory) { return "COMMIT: " + FormatOverlayGigabytes(memory.used) + "/" + FormatOverlayGigabytes(memory.budget) + " GB"; }), shown(sample.pageFaultsPerSecond, [&](double faults) { return "FAULTS/S: " + count(faults); })});
    joined("HOST IMPORTS:", {shown(sample.hostImports, [&](std::uint64_t imports) { return count(static_cast<double>(imports)); }), shown(sample.hostImportBytes, [](const OverlayMemory& memory) { return FormatOverlayGigabytes(memory.used) + "/" + FormatOverlayGigabytes(memory.budget) + " GB"; })});
    joined("CPU COPIES:", {shown(sample.cpuCopyMegabytesPerSecond, [&](double megabytes) { return value(megabytes) + " MB/S"; }), shown(sample.addressCopyCapPercent, [&](double percent) { return "CAP: " + value(percent) + "%"; })});
    joined("UNIMPLEMENTED:", {shown(sample.unimplementedPerSecond, [&](double calls) { return count(calls) + "/S"; }), shown(sample.unimplementedTotal, [&](std::uint64_t calls) { return count(static_cast<double>(calls)) + " TOTAL"; })});
    joined("REFUSED SHADERS:", {shown(sample.refusedShaders, [&](std::uint64_t refused) { return count(static_cast<double>(refused)); })});
    joined("", {sample.title.empty() ? std::nullopt : std::optional<std::string>("TITLE: " + OverlayText(sample.title)), sample.presentMode.empty() ? std::nullopt : std::optional<std::string>("PRESENT: " + OverlayText(sample.presentMode))});
    return content;
}

OverlayTraceFigures OverlayTraceDeltas(std::span<const TraceThreadTotals> before, std::span<const TraceThreadTotals> now, OverlayStatistics::Clock::duration elapsed, std::uint64_t presents, bool passes) {
    using Duration = OverlayStatistics::Clock::duration;
    const auto milliseconds = [](Duration length) { return std::chrono::duration<double, std::milli>(length).count(); };
    std::map<std::string, Duration> busy;
    std::map<std::string, Duration> phases;
    std::optional<OverlayQueueWaits> waits;
    std::uint64_t renderPasses = 0;
    for (std::size_t index = 0; index < now.size(); ++index) {
        const auto& thread = now[index];
        const auto totals = TraceFormat::Since(thread.totals, index < before.size() ? std::span<const TraceThread::Total>(before[index].totals) : std::span<const TraceThread::Total>());
        const bool worker = std::string_view(thread.name).starts_with(TraceFormat::WorkerPrefix);
        const bool firstQueue = thread.name == std::string(TraceFormat::WorkerPrefix) + "0x0";
        if (firstQueue && !waits) waits = OverlayQueueWaits{};
        for (const auto& total : totals) {
            const std::string_view scope = total.scope;
            const std::string_view stage = total.stage != nullptr ? total.stage : "";
            if (scope == PerformanceTrace::RenderPassScope) {
                renderPasses += total.count;
                continue;
            }
            if (worker && total.stage == nullptr && std::find(TraceFormat::WorkerBusyScopes.begin(), TraceFormat::WorkerBusyScopes.end(), scope) != TraceFormat::WorkerBusyScopes.end()) busy[thread.name] += total.length;
            if (scope != TraceFormat::WorkerWaitScope) phases[TraceFormat::Name(total.scope, total.stage)] += total.length;
            if (!firstQueue) continue;
            if (scope == "Driver.Packet" && (stage == "memory_wait" || stage == "rendering_wait")) waits->memoryMs += milliseconds(total.length);
            else if (scope == "GpuMutex.wait") waits->lockMs += milliseconds(total.length);
            else if ((total.stage == nullptr && (scope == "Recorder.Sync" || scope == "Recorder.WaitSerial")) || (scope == "Graphics.Wait" && stage == "fence_wait")) waits->gpuMs += milliseconds(total.length);
        }
        if (worker && !busy.contains(thread.name)) busy[thread.name] = Duration{};
    }
    OverlayTraceFigures figures;
    if (elapsed > Duration{}) {
        for (const auto& [name, length] : busy) figures.busy.push_back({busyName(name), 100.0 * static_cast<double>(length.count()) / static_cast<double>(elapsed.count())});
        std::stable_sort(figures.busy.begin(), figures.busy.end(), [](const OverlayThreadBusy& left, const OverlayThreadBusy& right) { return left.name.size() < right.name.size(); });
    }
    if (presents == 0) return figures;
    const double perFrame = 1.0 / static_cast<double>(presents);
    std::vector<std::pair<std::string, Duration>> top(phases.begin(), phases.end());
    std::stable_sort(top.begin(), top.end(), [](const auto& left, const auto& right) { return left.second > right.second; });
    for (std::size_t index = 0; index < top.size() && index < OverlayTopPhases; ++index) figures.phases.push_back({top[index].first, milliseconds(top[index].second) * perFrame});
    if (waits) figures.queueWaits = OverlayQueueWaits{waits->memoryMs * perFrame, waits->lockMs * perFrame, waits->gpuMs * perFrame};
    if (passes) figures.renderPassesPerFrame = static_cast<double>(renderPasses) * perFrame;
    return figures;
}

std::optional<double> OverlayHitPercent(const OverlayHits& before, const OverlayHits& now) {
    const bool reset = now.lookups < before.lookups || now.hits < before.hits;
    const auto lookups = reset ? now.lookups : now.lookups - before.lookups;
    const auto hits = reset ? now.hits : now.hits - before.hits;
    if (lookups == 0 || hits > lookups) return std::nullopt;
    return 100.0 * static_cast<double>(hits) / static_cast<double>(lookups);
}

std::string_view OverlayPresentModeName(VkPresentModeKHR mode) {
    switch (mode) {
        case VK_PRESENT_MODE_FIFO_KHR: return "fifo";
        case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR: return "fifo_latest_ready";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "mailbox";
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "immediate";
        default: return {};
    }
}

bool OverlaySampleDue(std::optional<OverlayStatistics::Clock::time_point> sampledAt, OverlayStatistics::Clock::time_point now) {
    return !sampledAt || now - *sampledAt >= std::chrono::seconds(1);
}

std::optional<OverlayMemory> ReadVramBudget(const Graphics::Context& context) {
    if (context.memoryProperties2 == nullptr || context.physical == VK_NULL_HANDLE) return std::nullopt;
    const auto heap = Graphics::LargestDeviceLocalHeap(context.memory);
    if (!heap) return std::nullopt;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &reported};
    context.memoryProperties2(context.physical, &properties);
    if (reported.heapBudget[*heap] == 0) return std::nullopt;
    return OverlayMemory{reported.heapUsage[*heap], reported.heapBudget[*heap]};
}

std::optional<std::uint64_t> ReadProcessPrivateBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) == 0) return std::nullopt;
    return static_cast<std::uint64_t>(counters.PrivateUsage);
#else
    std::FILE* status = std::fopen("/proc/self/status", "r");
    if (status == nullptr) return std::nullopt;
    std::optional<std::uint64_t> anonymousKb;
    std::uint64_t swapKb = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), status) != nullptr) {
        unsigned long long value = 0;
        if (std::sscanf(line, "RssAnon: %llu kB", &value) == 1) anonymousKb = value;
        else if (std::sscanf(line, "VmSwap: %llu kB", &value) == 1) swapKb = value;
    }
    std::fclose(status);
    if (!anonymousKb) return std::nullopt;
    return (*anonymousKb + swapKb) * 1024u;
#endif
}

std::optional<OverlayMemory> ParseCommitCharge(std::string_view meminfo) {
    std::optional<std::uint64_t> committed;
    std::optional<std::uint64_t> limit;
    while (!meminfo.empty()) {
        const auto end = meminfo.find('\n');
        const auto line = meminfo.substr(0, end);
        meminfo = end == std::string_view::npos ? std::string_view{} : meminfo.substr(end + 1);
        if (const auto value = meminfoKilobytes(line, "Committed_AS:")) committed = value;
        else if (const auto value = meminfoKilobytes(line, "CommitLimit:")) limit = value;
    }
    if (!committed || !limit) return std::nullopt;
    return OverlayMemory{*committed, *limit};
}

std::optional<OverlayMemory> ReadCommitCharge() {
#ifdef _WIN32
    PERFORMANCE_INFORMATION information{};
    information.cb = sizeof(information);
    if (K32GetPerformanceInfo(&information, sizeof(information)) == 0) return std::nullopt;
    const auto page = static_cast<std::uint64_t>(information.PageSize);
    return OverlayMemory{static_cast<std::uint64_t>(information.CommitTotal) * page, static_cast<std::uint64_t>(information.CommitLimit) * page};
#else
    std::FILE* meminfo = std::fopen("/proc/meminfo", "r");
    if (meminfo == nullptr) return std::nullopt;
    std::string text;
    char block[1024];
    for (std::size_t read = 0; (read = std::fread(block, 1, sizeof(block), meminfo)) != 0;) text.append(block, read);
    std::fclose(meminfo);
    return ParseCommitCharge(text);
#endif
}

std::optional<std::uint64_t> ReadPageFaults() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0) return std::nullopt;
    return static_cast<std::uint64_t>(counters.PageFaultCount);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return std::nullopt;
    return static_cast<std::uint64_t>(usage.ru_minflt) + static_cast<std::uint64_t>(usage.ru_majflt);
#endif
}

std::int32_t OverlayScale(VkExtent2D extent, const OverlayContent& content) {
    auto scale = std::max<std::int32_t>(1, static_cast<std::int32_t>((extent.height + 240) / 480));
    while (scale > 1 && !fits(extent, content, scale)) --scale;
    return scale;
}

OverlayLayout LayOutOverlay(const OverlayContent& content, VkExtent2D extent, OverlayCorner corner) {
    Graphics::Require(extent.width != 0 && extent.height != 0, "the performance overlay needs a destination");
    Graphics::Require(content.frameTimes.size() <= OverlayStatistics::FrameHistory, "the performance overlay graph holds at most 120 frames");
    Graphics::Require(content.columns > 0, "the performance overlay panel needs at least one column");
    Graphics::Require(content.graph ? content.columns * OverlayMetrics::Advance - 1 >= static_cast<std::int32_t>(OverlayStatistics::FrameHistory) : content.frameTimes.empty(), "the performance overlay graph needs a graph row as wide as its 120 frames");
    OverlayLayout layout;
    const auto scale = OverlayScale(extent, content);
    layout.scale = scale;
    const auto width = panelWidth(content.columns, scale);
    const auto height = panelHeight(content, scale);
    const auto margin = OverlayMetrics::Margin * scale;
    const bool right = corner == OverlayCorner::TopRight || corner == OverlayCorner::BottomRight;
    const bool bottom = corner == OverlayCorner::BottomLeft || corner == OverlayCorner::BottomRight;
    const auto left = std::max(0, right ? static_cast<std::int32_t>(extent.width) - margin - width : margin);
    const auto top = std::max(0, bottom ? static_cast<std::int32_t>(extent.height) - margin - height : margin);
    layout.bounds = {left, top, width, height, static_cast<std::uint32_t>(OverlayColor::Background)};
    layout.rects.push_back(layout.bounds);
    const auto text = [&](const std::string& line, std::int32_t row) {
        Graphics::Require(line.size() <= static_cast<std::size_t>(content.columns), "a performance overlay line is wider than the panel");
        for (std::size_t column = 0; column < line.size(); ++column) {
            const auto& glyph = OverlayGlyph(line[column]);
            const auto glyphLeft = left + (OverlayMetrics::Padding + static_cast<std::int32_t>(column) * OverlayMetrics::Advance) * scale;
            for (std::int32_t y = 0; y < OverlayMetrics::GlyphHeight; ++y) {
                const auto bits = glyph[static_cast<std::size_t>(y)];
                std::int32_t x = 0;
                while (x < OverlayMetrics::GlyphWidth) {
                    if ((bits & (0x10u >> x)) == 0) {
                        ++x;
                        continue;
                    }
                    auto end = x;
                    while (end < OverlayMetrics::GlyphWidth && (bits & (0x10u >> end)) != 0) ++end;
                    layout.rects.push_back({glyphLeft + x * scale, top + (row + y) * scale, (end - x) * scale, scale, static_cast<std::uint32_t>(OverlayColor::Text)});
                    x = end;
                }
            }
        }
    };
    auto row = OverlayMetrics::Padding;
    for (const auto& line : content.above) {
        text(line, row);
        row += OverlayMetrics::LinePitch;
    }
    const auto graphBottom = top + (row + OverlayMetrics::GraphHeight) * scale;
    if (content.graph && !content.frameTimes.empty()) {
        double maximum = 0;
        double sum = 0;
        for (const auto time : content.frameTimes) {
            maximum = std::max(maximum, time);
            sum += time;
        }
        const double fullScale = std::max(maximum, 2.0 * sum / static_cast<double>(content.frameTimes.size()));
        const auto first = static_cast<std::int32_t>(OverlayStatistics::FrameHistory - content.frameTimes.size());
        for (std::size_t i = 0; i < content.frameTimes.size() && fullScale > 0; ++i) {
            const auto pixels = std::clamp(static_cast<std::int32_t>(std::lround(content.frameTimes[i] / fullScale * OverlayMetrics::GraphHeight * scale)), 1, OverlayMetrics::GraphHeight * scale);
            layout.rects.push_back({left + (OverlayMetrics::Padding + first + static_cast<std::int32_t>(i)) * scale, graphBottom - pixels, scale, pixels, static_cast<std::uint32_t>(OverlayColor::Graph)});
        }
    }
    if (content.graph) row += OverlayMetrics::GraphHeight + OverlayMetrics::GraphGap;
    for (const auto& line : content.below) {
        text(line, row);
        row += OverlayMetrics::LinePitch;
    }
    return layout;
}

std::array<std::array<float, 4>, OverlayColorCount> OverlayPalette(VkFormat format, VkColorSpaceKHR colorSpace) {
    std::array<std::array<float, 4>, OverlayColorCount> palette{{{0.0f, 0.0f, 0.0f, 0.6f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.35f, 0.8f, 0.35f, 1.0f}, {0.06f, 0.07f, 0.09f, 0.94f}, {0.22f, 0.24f, 0.29f, 1.0f}, {1.0f, 0.78f, 0.2f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}};
    if (colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        for (auto& color : palette) {
            const float red = srgbDecode(color[0]);
            const float green = srgbDecode(color[1]);
            const float blue = srgbDecode(color[2]);
            const std::array<float, 3> bt2020{
                0.6274f * red + 0.3293f * green + 0.0433f * blue,
                0.0691f * red + 0.9195f * green + 0.0114f * blue,
                0.0164f * red + 0.0880f * green + 0.8956f * blue};
            for (std::size_t channel = 0; channel < 3; ++channel) color[channel] = pqEncode(bt2020[channel] * PqReferenceWhiteNits);
        }
    } else if (srgbFormat(format)) {
        for (auto& color : palette) {
            for (std::size_t channel = 0; channel < 3; ++channel) color[channel] = srgbDecode(color[channel]);
        }
    }
    return palette;
}

PerformanceOverlay::PerformanceOverlay(const Graphics::Context& context, std::size_t slotCount) : context(context), slots(slotCount) {
    Graphics::Require(slotCount != 0, "the performance overlay needs at least one slot");
    try {
        const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(OverlayParameters)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        Graphics::Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout performance overlay");
        vertex = Graphics::CreateShaderModule(context, PERFORMANCE_OVERLAY_VERT_SPV, sizeof(PERFORMANCE_OVERLAY_VERT_SPV), "vkCreateShaderModule performance overlay vertex");
        fragment = Graphics::CreateShaderModule(context, PERFORMANCE_OVERLAY_FRAG_SPV, sizeof(PERFORMANCE_OVERLAY_FRAG_SPV), "vkCreateShaderModule performance overlay fragment");
    } catch (...) {
        release();
        throw;
    }
}

PerformanceOverlay::~PerformanceOverlay() {
    release();
}

void PerformanceOverlay::releaseTransient(Slot& slot) noexcept {
    for (const auto& [view, framebuffer] : slot.transient) {
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    }
    slot.transient.clear();
}

void PerformanceOverlay::Release(std::size_t index) noexcept {
    if (index < slots.size()) releaseTransient(slots[index]);
}

void PerformanceOverlay::release() noexcept {
    for (auto& slot : slots) {
        releaseTransient(slot);
        slot.rects.reset();
        slot.capacity = 0;
    }
    for (const auto& [pass, entry] : pipelines) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, entry, nullptr);
    pipelines.clear();
    for (const auto& [key, entry] : renderPasses) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, entry, nullptr);
    renderPasses.clear();
    if (fragment) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, fragment, nullptr);
    if (vertex) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, vertex, nullptr);
    if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    fragment = vertex = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
}

bool PerformanceOverlay::Visible() {
    static_cast<void>(settings());
    return currentPage.load(std::memory_order_relaxed) != 0;
}

std::uint32_t PerformanceOverlay::Page() {
    static_cast<void>(settings());
    return currentPage.load(std::memory_order_relaxed);
}

void PerformanceOverlay::Toggle() {
    static std::mutex toggleMutex;
    std::lock_guard lock(toggleMutex);
    const auto presses = toggleCount.load(std::memory_order_relaxed) + 1;
    const auto shown = OverlayPageAfter(settings().page, presses);
    currentPage.store(shown, std::memory_order_relaxed);
    toggleCount.store(presses, std::memory_order_relaxed);
    PerformanceTrace::SetOverlayTotals(shown == 2);
}

void PerformanceOverlay::SetDriverCounters(OverlayDriverCounters (*source)()) {
    driverCounters.store(source, std::memory_order_release);
}

OverlayCorner PerformanceOverlay::Corner() {
    return settings().corner;
}

void PerformanceOverlay::NotePresent(OverlayStatistics::Clock::time_point at, VkExtent2D display, const OverlayPresentInfo& info) {
    restartIfToggled();
    if (page == 2) {
        noteDetailPresent(at, info);
        return;
    }
    if (OverlaySampleDue(sampledAt, at)) {
        details.vram = ReadVramBudget(context);
        details.ram = ReadProcessPrivateBytes();
        sampledAt = at;
    }
    details.display = display;
    const auto packets = PacketCounters::Sum();
    statistics.AddPresent(at, OverlayCounters{PerformanceCounters::GpuEnabled() ? PerformanceCounters::GpuNanoseconds() : 0, PerformanceCounters::DriverEnabled() ? PerformanceCounters::DriverNanoseconds() : 0, packets.draws, packets.dispatches, PerformanceCounters::Pipelines()});
}

void PerformanceOverlay::restartIfToggled() {
    const auto current = toggleCount.load(std::memory_order_relaxed);
    page = Page();
    if (current == toggles) return;
    statistics.Reset();
    details = {};
    detailSample = {};
    detail = {};
    sampledAt.reset();
    toggles = current;
}

void PerformanceOverlay::noteDetailPresent(OverlayStatistics::Clock::time_point at, const OverlayPresentInfo& info) {
    PerformanceTrace::BindThread("presenter");
    statistics.AddPresent(at, OverlayCounters{0, 0, 0, 0, PerformanceCounters::Pipelines()});
    if (info.displayAddress != 0 && info.displayAddress == detail.lastDisplayAddress) ++detail.repeats;
    detail.lastDisplayAddress = info.displayAddress;
    ++detail.tracePresents;
    detailSample.fps = statistics.Fps();
    detailSample.frameMs = statistics.LastFrameMs();
    detailSample.presentsPerSecond = statistics.Fps();
    detailSample.pipelinesPerSecond = static_cast<double>(statistics.PipelinesInWindow());
    if (OverlaySampleDue(detail.sampledAt, at)) sampleDetails(at, info);
}

void PerformanceOverlay::sampleDetails(OverlayStatistics::Clock::time_point at, const OverlayPresentInfo& info) {
    DetailCounters now;
    if (const auto source = driverCounters.load(std::memory_order_acquire)) now.driver = source();
    const auto pipelines = Graphics::PipelineStoreTotals();
    now.pipelines = OverlayHits{pipelines.hits + pipelines.misses, pipelines.hits};
    const auto copies = Graphics::AddressCopyCounters::Fold();
    now.copiedBytes = copies.bytes;
    now.pageFaults = ReadPageFaults();
    now.unimplemented = NotImplementedCalls_nid_no_patch();
    now.acquireDrops = info.acquireDrops;
    const auto& before = detail.base;
    if (detail.sampledAt) {
        const double seconds = std::chrono::duration<double>(at - *detail.sampledAt).count();
        const auto perSecond = [&](std::uint64_t count) { return static_cast<double>(count) / seconds; };
        if (now.driver && before.driver) {
            detailSample.flipsPerSecond = perSecond(now.driver->flips - before.driver->flips);
            if (now.driver->drawCache && before.driver->drawCache) detailSample.drawCacheHitPercent = OverlayHitPercent(*before.driver->drawCache, *now.driver->drawCache);
        }
        detailSample.pipelineCacheHitPercent = OverlayHitPercent(before.pipelines, now.pipelines);
        detailSample.cpuCopyMegabytesPerSecond = perSecond(now.copiedBytes - before.copiedBytes) / (1024.0 * 1024.0);
        if (const auto limit = Graphics::AddressCopyCounters::Limit(); limit != 0) detailSample.addressCopyCapPercent = 100.0 * static_cast<double>(copies.largest) / static_cast<double>(limit);
        if (now.pageFaults && before.pageFaults) detailSample.pageFaultsPerSecond = perSecond(*now.pageFaults - *before.pageFaults);
        detailSample.unimplementedPerSecond = perSecond(now.unimplemented - before.unimplemented);
        detailSample.droppedFrames = static_cast<std::uint64_t>(std::llround(perSecond(now.acquireDrops - before.acquireDrops)));
        detailSample.repeatedPresents = static_cast<std::uint64_t>(std::llround(perSecond(detail.repeats)));
    } else if (HasAppParamJson_nid_no_patch()) {
        const auto id = GetAppTitleId_nid_postfix();
        detailSample.title.assign(id.value, strnlen(id.value, sizeof(id.value)));
    }
    if (now.driver && before.driver && !now.driver->drawCache) now.driver->drawCache = before.driver->drawCache;
    detailSample.unimplementedTotal = now.unimplemented;
    if (const auto pending = DriverDetail::RegistrationPreparationsPending()) detailSample.pipelinesCompiling = *pending;
    if (const auto refused = DriverDetail::RefusedShaderCount()) detailSample.refusedShaders = *refused;
    else detailSample.refusedShaders.reset();
    detailSample.commit = ReadCommitCharge();
    if (context.hostImportAlignment != 0) {
        const auto imports = Graphics::HostImportCounters();
        detailSample.hostImports = imports.live;
        detailSample.hostImportBytes = OverlayMemory{imports.liveBytes, imports.liveBytes + Graphics::HostImportBudgetRemaining(context)};
    }
    detailSample.presentMode = std::string(OverlayPresentModeName(info.presentMode));
    sampleTrace(at);
    detail.base = std::move(now);
    detail.repeats = 0;
    detail.sampledAt = at;
}

void PerformanceOverlay::sampleTrace(OverlayStatistics::Clock::time_point at) {
    if (!PerformanceTrace::Recording()) {
        detail.traceSampledAt.reset();
        detail.traceBase.clear();
        detailSample.busy.clear();
        detailSample.phases.clear();
        detailSample.queueWaits.reset();
        detailSample.renderPassesPerFrame.reset();
        return;
    }
    auto threads = PerformanceTrace::ThreadTotals();
    if (!threads) return;
    if (detail.traceSampledAt) {
        auto figures = OverlayTraceDeltas(detail.traceBase, *threads, at - *detail.traceSampledAt, detail.tracePresents, PerformanceTrace::OverlayTotals());
        detailSample.busy = std::move(figures.busy);
        detailSample.phases = std::move(figures.phases);
        detailSample.queueWaits = figures.queueWaits;
        detailSample.renderPassesPerFrame = figures.renderPassesPerFrame;
    }
    detail.traceBase = std::move(*threads);
    detail.traceSampledAt = at;
    detail.tracePresents = 0;
}

VkRenderPass PerformanceOverlay::renderPass(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout) {
    const auto key = std::tuple{format, initialLayout, finalLayout};
    if (const auto found = renderPasses.find(key); found != renderPasses.end()) return found->second;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    Graphics::Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0, "the performance overlay cannot blend into this format");
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = initialLayout;
    attachment.finalLayout = finalLayout;
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    const bool transfer = finalLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkSubpassDependency dependencies[2] = {
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, transfer ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, transfer ? VK_ACCESS_TRANSFER_READ_BIT : VkAccessFlags{0}, 0},
    };
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 2;
    info.pDependencies = dependencies;
    VkRenderPass created = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &info, nullptr, &created), "vkCreateRenderPass performance overlay");
    return renderPasses.emplace(key, created).first->second;
}

VkPipeline PerformanceOverlay::pipeline(VkRenderPass compatible) {
    if (const auto found = pipelines.find(compatible); found != pipelines.end()) return found->second;
    VkPipelineShaderStageCreateInfo stages[2]{{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment;
    stages[1].pName = "main";
    const VkVertexInputBindingDescription binding{0, sizeof(OverlayRect), VK_VERTEX_INPUT_RATE_INSTANCE};
    const VkVertexInputAttributeDescription attributes[2] = {{0, 0, VK_FORMAT_R32G32B32A32_SINT, 0}, {1, 0, VK_FORMAT_R32_UINT, 16}};
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterization{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &rasterization;
    info.pMultisampleState = &multisample;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = pipelineLayout;
    info.renderPass = compatible;
    VkPipeline created = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, VK_NULL_HANDLE, 1, &info, nullptr, &created), "vkCreateGraphicsPipelines performance overlay");
    return pipelines.emplace(compatible, created).first->second;
}

OverlayRect PerformanceOverlay::Record(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout initialLayout, VkImageLayout finalLayout) {
    if (page == 2) return Record(commands, slot, destination, format, colorSpace, extent, initialLayout, finalLayout, ComposeOverlayDetails(detailSample), Corner());
    auto shown = details;
    shown.window = extent;
    return Record(commands, slot, destination, format, colorSpace, extent, initialLayout, finalLayout, ComposeOverlay(statistics, PerformanceCounters::GpuEnabled(), PerformanceCounters::DriverEnabled(), shown), Corner());
}

OverlayRect PerformanceOverlay::Record(VkCommandBuffer commands, std::size_t index, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout initialLayout, VkImageLayout finalLayout, const OverlayContent& content, OverlayCorner corner) {
    return RecordRects(commands, index, destination, format, colorSpace, extent, initialLayout, finalLayout, LayOutOverlay(content, extent, corner));
}

OverlayRect PerformanceOverlay::RecordRects(VkCommandBuffer commands, std::size_t index, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout initialLayout, VkImageLayout finalLayout, const OverlayLayout& layout) {
    Graphics::Require(index < slots.size(), "performance overlay slot is out of range");
    Graphics::Require(destination != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0, "the performance overlay destination is unavailable");
    Graphics::Require(!layout.rects.empty(), "an overlay layout has nothing to draw");
    auto& slot = slots[index];
    releaseTransient(slot);
    if (slot.capacity < layout.rects.size()) {
        const auto capacity = std::max<std::size_t>(layout.rects.size(), 1024);
        slot.rects = std::make_unique<Graphics::Buffer>(context, capacity * sizeof(OverlayRect), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        slot.capacity = capacity;
    }
    std::memcpy(slot.rects->Bytes().data(), layout.rects.data(), layout.rects.size() * sizeof(OverlayRect));
    const auto pass = renderPass(format, initialLayout, finalLayout);
    const auto drawing = pipeline(pass);
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = destination;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageViewUsageCreateInfo usageInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    for (const auto layout : {initialLayout, finalLayout}) {
        if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) usageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) usageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    viewInfo.pNext = &usageInfo;
    VkImageView view = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView performance overlay");
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = pass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &view;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    const auto created = context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer);
    slot.transient.emplace_back(view, framebuffer);
    Graphics::Check(created, "vkCreateFramebuffer performance overlay");
    const auto right = std::min<std::int32_t>(layout.bounds.x + layout.bounds.width, static_cast<std::int32_t>(extent.width));
    const auto bottom = std::min<std::int32_t>(layout.bounds.y + layout.bounds.height, static_cast<std::int32_t>(extent.height));
    const VkRect2D area{{layout.bounds.x, layout.bounds.y}, {static_cast<std::uint32_t>(std::max(1, right - layout.bounds.x)), static_cast<std::uint32_t>(std::max(1, bottom - layout.bounds.y))}};
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = pass;
    begin.framebuffer = framebuffer;
    begin.renderArea = area;
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, drawing);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
    context.Function<PFN_vkCmdSetViewport>("vkCmdSetViewport")(commands, 0, 1, &viewport);
    context.Function<PFN_vkCmdSetScissor>("vkCmdSetScissor")(commands, 0, 1, &area);
    const OverlayParameters parameters{OverlayPalette(format, colorSpace), {static_cast<float>(extent.width), static_cast<float>(extent.height)}};
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(parameters), &parameters);
    const VkBuffer buffer = slot.rects->Handle();
    const VkDeviceSize offset = 0;
    context.Function<PFN_vkCmdBindVertexBuffers>("vkCmdBindVertexBuffers")(commands, 0, 1, &buffer, &offset);
    context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 4, static_cast<std::uint32_t>(layout.rects.size()), 0, 0);
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
    return layout.bounds;
}

}

extern "C" void AgcDriverTogglePerformanceOverlay_nid_postfix() {
    AgcDriver::PerformanceOverlay::Toggle();
}
