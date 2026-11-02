#include "prx/libSceAgcDriver/Execution/include/PadStatusOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace AgcDriver {
namespace {

bool batteryRank(PadPower power, int& rank) {
    switch (power) {
        case PadPower::Empty: rank = 0; return true;
        case PadPower::Low: rank = 1; return true;
        case PadPower::Medium: rank = 2; return true;
        case PadPower::Full: rank = 3; return true;
        case PadPower::Unknown:
        case PadPower::Wired: break;
    }
    return false;
}

bool batteryDropped(PadPower before, PadPower after) {
    int from = 0;
    int to = 0;
    return batteryRank(before, from) && batteryRank(after, to) && to < from;
}

bool padEvent(const PadSlotStatus& before, const PadSlotStatus& after) {
    if (after.connected != before.connected) return true;
    if (!after.connected) return false;
    return after.kind != before.kind || after.name != before.name || batteryDropped(before.power, after.power);
}

std::string_view powerText(PadPower power) {
    switch (power) {
        case PadPower::Empty: return "BATTERY EMPTY";
        case PadPower::Low: return "BATTERY LOW";
        case PadPower::Medium: return "BATTERY MEDIUM";
        case PadPower::Full: return "BATTERY FULL";
        case PadPower::Wired: return "WIRED";
        case PadPower::Unknown: break;
    }
    return "BATTERY UNKNOWN";
}

std::string kindText(const PadSlotStatus& status) {
    switch (status.kind) {
        case PadKind::DualSense: return "DUALSENSE";
        case PadKind::DualShock4: return "DUALSHOCK 4";
        case PadKind::DualShock3: return "DUALSHOCK 3";
        case PadKind::Xbox: return "XBOX";
        case PadKind::Switch: return "SWITCH";
        case PadKind::Unknown:
        case PadKind::Other: break;
    }
    auto text = OverlayText(status.name);
    const auto first = text.find_first_not_of(' ');
    if (first == std::string::npos) return "PAD";
    text = text.substr(first, text.find_last_not_of(' ') - first + 1);
    return text;
}

}

PadStatusMode ParsePadStatusMode(const char* value) {
    const std::string_view mode = value != nullptr ? value : "";
    if (mode.empty() || mode == "0") return PadStatusMode::Off;
    if (mode == "1") return PadStatusMode::Events;
    throw std::runtime_error("ANYPS5_PAD_STATUS must be 0 or 1, not '" + std::string(mode) + "'");
}

std::string FormatPadStatusLine(std::size_t slot, const PadSlotStatus& status) {
    Graphics::Require(slot < PadStatusSlots, "a pad status line is for one of the four player slots");
    const auto prefix = "P" + std::to_string(slot + 1) + " ";
    if (!status.connected) return prefix + "DISCONNECTED";
    auto kind = kindText(status);
    constexpr auto limit = static_cast<std::size_t>(PadStatusMetrics::KindColumns);
    if (kind.size() > limit) kind = kind.substr(0, limit - 1) + ".";
    return prefix + kind + "  " + std::string(powerText(status.power));
}

OverlayCorner PadStatusCorner(OverlayCorner overlay) {
    switch (overlay) {
        case OverlayCorner::TopLeft: return OverlayCorner::TopRight;
        case OverlayCorner::TopRight: return OverlayCorner::TopLeft;
        case OverlayCorner::BottomLeft: return OverlayCorner::BottomRight;
        case OverlayCorner::BottomRight: return OverlayCorner::BottomLeft;
    }
    throw std::runtime_error("the performance overlay has no such corner");
}

OverlayContent ComposePadStatus(const std::vector<std::string>& lines) {
    Graphics::Require(!lines.empty(), "the pad status panel has nothing to show");
    OverlayContent content;
    content.above = lines;
    content.columns = PadStatusMetrics::Columns;
    content.graph = false;
    return content;
}

OverlayLayout LayOutPadStatus(const std::vector<std::string>& lines, VkExtent2D extent, OverlayCorner overlay) {
    return LayOutOverlay(ComposePadStatus(lines), extent, PadStatusCorner(overlay));
}

bool PadStatusDrawn(bool visible, bool keyboardShown) {
    return visible && !keyboardShown;
}

PadStatusDisplay::PadStatusDisplay(PadStatusMode mode) : mode(mode) {
}

bool PadStatusDisplay::Observe(Clock::time_point now, const PadStatusSnapshot& snapshot) {
    const PadStatusSnapshot baseline;
    const auto& before = previous ? *previous : baseline;
    bool event = false;
    for (std::size_t slot = 0; slot < PadStatusSlots; ++slot) {
        event = event || padEvent(before.slots[slot], snapshot.slots[slot]);
        seen[slot] = seen[slot] || snapshot.slots[slot].connected;
    }
    previous = snapshot;
    if (event && mode == PadStatusMode::Events) shownUntil = now + Duration;
    return event;
}

void PadStatusDisplay::Request(Clock::time_point now) {
    shownUntil = now + Duration;
}

bool PadStatusDisplay::Visible(Clock::time_point now) const {
    return shownUntil.has_value() && now < *shownUntil;
}

bool PadStatusDisplay::Wants(Clock::time_point now) const {
    return mode == PadStatusMode::Events || Visible(now);
}

bool PadStatusDisplay::Poll(Clock::time_point now, const PadStatusProvider& provider) {
    if (!Wants(now)) {
        polled.reset();
        return false;
    }
    auto snapshot = provider.read();
    if (!polled.has_value() || *polled != snapshot) Observe(now, snapshot);
    polled = std::move(snapshot);
    return true;
}

std::vector<std::string> PadStatusDisplay::Lines() const {
    std::vector<std::string> lines;
    for (std::size_t slot = 0; slot < PadStatusSlots; ++slot) {
        if (seen[slot] && previous) lines.push_back(FormatPadStatusLine(slot, previous->slots[slot]));
    }
    if (lines.empty()) lines.push_back("NO CONTROLLER");
    return lines;
}

namespace {

std::mutex displayMutex;

PadStatusDisplay& display() {
    static PadStatusDisplay shared(ParsePadStatusMode(std::getenv("ANYPS5_PAD_STATUS")));
    return shared;
}

}

std::optional<std::vector<std::string>> PadStatusOverlayLines() {
    std::lock_guard lock(displayMutex);
    auto& shared = display();
    if (!shared.Visible(PadStatusDisplay::Clock::now())) return std::nullopt;
    return shared.Lines();
}

}

extern "C" void AgcDriverPadStatusPoll_nid_postfix(const AgcDriver::PadStatusProvider* provider) {
    std::lock_guard lock(AgcDriver::displayMutex);
    AgcDriver::display().Poll(AgcDriver::PadStatusDisplay::Clock::now(), *provider);
}

extern "C" void AgcDriverPadStatusShow_nid_postfix() {
    std::lock_guard lock(AgcDriver::displayMutex);
    AgcDriver::display().Request(AgcDriver::PadStatusDisplay::Clock::now());
}
