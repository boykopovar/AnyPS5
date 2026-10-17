#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCEOVERLAY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCEOVERLAY_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver {

namespace Graphics {
class Buffer;
}

enum class OverlayCorner : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

struct OverlaySettings {
    bool visible = false;
    OverlayCorner corner = OverlayCorner::TopLeft;
};

OverlaySettings ParseOverlaySettings(const char* enabled, const char* position);

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

struct OverlayContent {
    std::vector<std::string> above;
    std::vector<double> frameTimes;
    std::vector<std::string> below;
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

struct OverlayLayout {
    OverlayRect bounds;
    std::vector<OverlayRect> rects;
    std::int32_t scale = 1;
};

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
};

const std::array<std::uint8_t, OverlayMetrics::GlyphHeight>& OverlayGlyph(char character);
std::string FormatOverlayValue(double value);
std::string FormatOverlayRate(double value);
std::string FormatOverlayCount(double value);
std::string FormatOverlayGigabytes(std::uint64_t bytes);
OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver);
OverlayContent ComposeOverlay(const OverlayStatistics& statistics, bool gpu, bool driver, const OverlayDetails& details);
bool OverlaySampleDue(std::optional<OverlayStatistics::Clock::time_point> sampledAt, OverlayStatistics::Clock::time_point now);
std::optional<OverlayMemory> ReadVramBudget(const Graphics::Context& context);
std::optional<std::uint64_t> ReadProcessPrivateBytes();
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
    static void Toggle();
    static OverlayCorner Corner();

    void NotePresent(OverlayStatistics::Clock::time_point at, VkExtent2D display);
    const OverlayStatistics& Statistics() const { return statistics; }
    const OverlayDetails& Details() const { return details; }
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
    void restartIfToggled();

    Graphics::Context context;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    std::map<std::tuple<VkFormat, VkImageLayout, VkImageLayout>, VkRenderPass> renderPasses;
    std::map<VkFormat, VkPipeline> pipelines;
    std::vector<Slot> slots;
    OverlayStatistics statistics;
    OverlayDetails details;
    std::optional<OverlayStatistics::Clock::time_point> sampledAt;
    std::uint64_t toggles = 0;
};

}

#endif
