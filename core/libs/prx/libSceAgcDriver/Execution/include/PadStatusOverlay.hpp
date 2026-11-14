#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PADSTATUSOVERLAY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PADSTATUSOVERLAY_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Execution/include/PadStatus.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver {

enum class PadStatusMode : std::uint8_t { Off, Events };

struct PadStatusMetrics {
    static constexpr std::int32_t Columns = 32;
    static constexpr std::int32_t KindColumns = 12;
};

PadStatusMode ParsePadStatusMode(const char* value);
std::string FormatPadStatusLine(std::size_t slot, const PadSlotStatus& status);
OverlayCorner PadStatusCorner(OverlayCorner overlay);
OverlayContent ComposePadStatus(const std::vector<std::string>& lines);
OverlayLayout LayOutPadStatus(const std::vector<std::string>& lines, VkExtent2D extent, OverlayCorner overlay);
bool PadStatusDrawn(bool visible, bool keyboardShown);
std::optional<std::vector<std::string>> PadStatusOverlayLines();

class PadStatusDisplay {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr Clock::duration Duration = std::chrono::seconds(4);

    explicit PadStatusDisplay(PadStatusMode mode);

    bool Observe(Clock::time_point now, const PadStatusSnapshot& snapshot);
    void Request(Clock::time_point now);
    bool Visible(Clock::time_point now) const;
    bool Wants(Clock::time_point now) const;
    bool Poll(Clock::time_point now, const PadStatusProvider& provider);
    std::vector<std::string> Lines() const;

private:
    PadStatusMode mode;
    std::optional<PadStatusSnapshot> previous;
    std::array<bool, PadStatusSlots> seen{};
    std::optional<Clock::time_point> shownUntil;
    std::optional<PadStatusSnapshot> polled;
};

}

extern "C" void AgcDriverPadStatusPoll_nid_postfix(const AgcDriver::PadStatusProvider* provider);
extern "C" void AgcDriverPadStatusShow_nid_postfix();

#endif
