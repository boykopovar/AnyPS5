#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCEOVERLAY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCEOVERLAY_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver {

namespace Graphics {
class Buffer;
}

enum class OverlayCorner : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

inline constexpr std::uint32_t OverlayPageCount = 3;

struct OverlaySettings {
    std::uint32_t page = 0;
    OverlayCorner corner = OverlayCorner::TopLeft;
};

OverlaySettings ParseOverlaySettings(const char* enabled, const char* position);
std::uint32_t OverlayPageAfter(std::uint32_t start, std::uint64_t presses);

class PerformanceCounters {
public:
    static void AddGpuNanoseconds(std::uint64_t nanoseconds);
    static void AddDriverNanoseconds(std::uint64_t nanoseconds);
    static void CountPipeline();
    static std::uint64_t GpuNanoseconds();
    static std::uint64_t DriverNanoseconds();
    static std::uint64_t Pipelines();
    static bool GpuEnabled();
    static bool DriverEnabled();
};

class PacketCounters {
public:
    struct Totals {
        std::uint64_t draws = 0;
        std::uint64_t dispatches = 0;
    };

    static PacketCounters& ThisThread();
    static Totals Sum();

    void CountDraw() { draws.store(draws.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed); }
    void CountDispatch() { dispatches.store(dispatches.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed); }

private:
    alignas(64) std::atomic<std::uint64_t> draws{0};
    std::atomic<std::uint64_t> dispatches{0};
};

struct OverlayCounters {
    std::uint64_t gpuNanoseconds = 0;
    std::uint64_t driverNanoseconds = 0;
    std::uint64_t draws = 0;
    std::uint64_t dispatches = 0;
    std::uint64_t pipelines = 0;
};

class OverlayStatistics {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t FrameHistory = 120;
    static constexpr Clock::duration Window = std::chrono::seconds(1);

    struct Range {
        double minimum = 0;
        double average = 0;
        double maximum = 0;
    };

    void AddPresent(Clock::time_point at, std::uint64_t gpuNanoseconds, std::uint64_t driverNanoseconds);
    void AddPresent(Clock::time_point at, const OverlayCounters& counters);
    void Reset();
    double Fps() const;
    double LastFrameMs() const;
    std::vector<double> FrameTimes() const;
    Range FrameTimeRange() const;
    double GpuMsPerFrame() const;
    double DriverMsPerFrame() const;
    double DrawsPerFrame() const;
    double DispatchesPerFrame() const;
    std::uint64_t PipelinesInWindow() const;
    OverlayCounters Latest() const;

private:
    struct Sample {
        Clock::time_point at;
        OverlayCounters counters;
    };

    double perFrame(std::uint64_t OverlayCounters::*counter) const;

    std::deque<Sample> window;
    std::optional<Sample> previous;
    std::array<double, FrameHistory> frameTimes{};
    std::size_t frameCount = 0;
    std::size_t frameNext = 0;
};

struct OverlayRect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t color = 0;
};

enum class OverlayColor : std::uint32_t { Background = 0, Text = 1, Graph = 2 };
inline constexpr std::size_t OverlayColorCount = 3;

struct OverlayMetrics {
    static constexpr std::int32_t GlyphWidth = 5;
    static constexpr std::int32_t GlyphHeight = 7;
    static constexpr std::int32_t Advance = 6;
    static constexpr std::int32_t LinePitch = 9;
    static constexpr std::int32_t Columns = 27;
    static constexpr std::int32_t Padding = 3;
    static constexpr std::int32_t Margin = 4;
    static constexpr std::int32_t GraphHeight = 20;
    static constexpr std::int32_t GraphGap = 2;
    static constexpr std::int32_t InnerWidth = Columns * Advance - 1;
    static constexpr std::int32_t DetailColumns = 46;
};

struct OverlayContent {
    std::vector<std::string> above;
    std::vector<double> frameTimes;
    std::vector<std::string> below;
    std::int32_t columns = OverlayMetrics::Columns;
    bool graph = true;
};

struct OverlayMemory {
    std::uint64_t used = 0;
    std::uint64_t budget = 0;
};

struct OverlayDetails {
    std::optional<OverlayMemory> vram;
    std::optional<std::uint64_t> ram;
    VkExtent2D display{};
    VkExtent2D window{};
};

struct OverlayThreadBusy {
    std::string name;
    double percent = 0;
};

struct OverlayPhase {
    std::string name;
    double msPerFrame = 0;
};

struct OverlayQueueWaits {
    double memoryMs = 0;
    double lockMs = 0;
    double gpuMs = 0;
};

struct OverlayDetailSample {
    double fps = 0;
    double frameMs = 0;
    std::vector<OverlayThreadBusy> busy;
    std::vector<OverlayPhase> phases;
    std::optional<OverlayQueueWaits> queueWaits;
    std::optional<double> renderPassesPerFrame;
    std::optional<std::uint64_t> pipelinesCompiling;
    std::optional<double> pipelinesPerSecond;
    std::optional<double> drawCacheHitPercent;
    std::optional<double> pipelineCacheHitPercent;
    std::optional<double> flipsPerSecond;
    std::optional<double> presentsPerSecond;
    std::optional<std::uint64_t> droppedFrames;
    std::optional<std::uint64_t> repeatedPresents;
    std::optional<OverlayMemory> commit;
    std::optional<double> pageFaultsPerSecond;
    std::optional<std::uint64_t> hostImports;
    std::optional<OverlayMemory> hostImportBytes;
    std::optional<double> cpuCopyMegabytesPerSecond;
    std::optional<double> addressCopyCapPercent;
    std::optional<double> unimplementedPerSecond;
    std::optional<std::uint64_t> unimplementedTotal;
    std::optional<std::uint64_t> refusedShaders;
    std::string title;
    std::string presentMode;
};

struct OverlayHits {
    std::uint64_t lookups = 0;
    std::uint64_t hits = 0;
};

struct OverlayDriverCounters {
    std::uint64_t flips = 0;
    std::optional<OverlayHits> drawCache;
};

struct OverlayPresentInfo {
    std::uint64_t displayAddress = 0;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    std::uint64_t acquireDrops = 0;
};

struct OverlayTraceFigures {
    std::vector<OverlayThreadBusy> busy;
    std::vector<OverlayPhase> phases;
    std::optional<OverlayQueueWaits> queueWaits;
    std::optional<double> renderPassesPerFrame;
};

inline constexpr std::size_t OverlayTopPhases = 5;

struct OverlayLayout {
    OverlayRect bounds;
    std::vector<OverlayRect> rects;
    std::int32_t scale = 1;
};

const std::array<std::uint8_t, OverlayMetrics::GlyphHeight>& OverlayGlyph(char character);
std::string_view OverlayFontCharacters();
std::string FormatOverlayValue(double value);
std::string FormatOverlayRate(double value);
std::string FormatOverlayCount(double value);
std::string FormatOverlayGigabytes(std::uint64_t bytes);
OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver);
OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver, const OverlayDetails& details);
std::string OverlayText(std::string_view text);
OverlayContent ComposeOverlayDetails(const OverlayDetailSample& sample);
OverlayTraceFigures OverlayTraceDeltas(std::span<const TraceThreadTotals> before, std::span<const TraceThreadTotals> now, OverlayStatistics::Clock::duration elapsed, std::uint64_t presents, bool passes);
std::optional<double> OverlayHitPercent(const OverlayHits& before, const OverlayHits& now);
std::string_view OverlayPresentModeName(VkPresentModeKHR mode);
bool OverlaySampleDue(std::optional<OverlayStatistics::Clock::time_point> sampledAt, OverlayStatistics::Clock::time_point now);
std::optional<OverlayMemory> ReadVramBudget(const Graphics::Context& context);
std::optional<std::uint64_t> ReadProcessPrivateBytes();
std::optional<OverlayMemory> ParseCommitCharge(std::string_view meminfo);
std::optional<OverlayMemory> ReadCommitCharge();
std::optional<std::uint64_t> ReadPageFaults();
std::int32_t OverlayScale(VkExtent2D extent, const OverlayContent& content);
OverlayLayout LayOutOverlay(const OverlayContent& content, VkExtent2D extent, OverlayCorner corner);
std::array<std::array<float, 4>, OverlayColorCount> OverlayPalette(VkFormat format, VkColorSpaceKHR colorSpace);

class PerformanceOverlay {
public:
    PerformanceOverlay(const Graphics::Context& context, std::size_t slots);
    ~PerformanceOverlay();
    PerformanceOverlay(const PerformanceOverlay&) = delete;
    PerformanceOverlay& operator=(const PerformanceOverlay&) = delete;

    static bool Visible();
    static std::uint32_t Page();
    static void Toggle();
    static OverlayCorner Corner();
    static void SetDriverCounters(OverlayDriverCounters (*source)());

    void NotePresent(OverlayStatistics::Clock::time_point at, VkExtent2D display, const OverlayPresentInfo& info = {});
    const OverlayStatistics& Statistics() const { return statistics; }
    const OverlayDetails& Details() const { return details; }
    const OverlayDetailSample& DetailSample() const { return detailSample; }
    std::uint32_t ShownPage() const { return page; }
    OverlayRect Record(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout initialLayout, VkImageLayout finalLayout);
    OverlayRect Record(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout initialLayout, VkImageLayout finalLayout, const OverlayContent& content, OverlayCorner corner);
    void Release(std::size_t slot) noexcept;

private:
    struct Slot {
        std::unique_ptr<Graphics::Buffer> rects;
        std::size_t capacity = 0;
        std::vector<std::pair<VkImageView, VkFramebuffer>> transient;
    };

    VkRenderPass renderPass(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout);
    VkPipeline pipeline(VkFormat format, VkRenderPass compatible);
    void releaseTransient(Slot& slot) noexcept;
    void release() noexcept;
    struct DetailCounters {
        std::optional<OverlayDriverCounters> driver;
        OverlayHits pipelines;
        std::uint64_t copiedBytes = 0;
        std::optional<std::uint64_t> pageFaults;
        std::uint64_t unimplemented = 0;
        std::uint64_t acquireDrops = 0;
    };

    struct DetailState {
        std::optional<OverlayStatistics::Clock::time_point> sampledAt;
        DetailCounters base;
        std::uint64_t repeats = 0;
        std::uint64_t lastDisplayAddress = 0;
        std::optional<OverlayStatistics::Clock::time_point> traceSampledAt;
        std::vector<TraceThreadTotals> traceBase;
        std::uint64_t tracePresents = 0;
    };

    void restartIfToggled();
    void noteDetailPresent(OverlayStatistics::Clock::time_point at, const OverlayPresentInfo& info);
    void sampleDetails(OverlayStatistics::Clock::time_point at, const OverlayPresentInfo& info);
    void sampleTrace(OverlayStatistics::Clock::time_point at);

    Graphics::Context context;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    std::map<std::tuple<VkFormat, VkImageLayout, VkImageLayout>, VkRenderPass> renderPasses;
    std::map<VkFormat, VkPipeline> pipelines;
    std::vector<Slot> slots;
    OverlayStatistics statistics;
    OverlayDetails details;
    OverlayDetailSample detailSample;
    DetailState detail;
    std::optional<OverlayStatistics::Clock::time_point> sampledAt;
    std::uint64_t toggles = 0;
    std::uint32_t page = 0;
};

}

#endif
