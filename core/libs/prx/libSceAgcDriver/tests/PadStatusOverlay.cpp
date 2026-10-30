#include "prx/libSceAgcDriver/Execution/include/PadStatusOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::PadKind;
using AgcDriver::PadPower;
using AgcDriver::PadSlotStatus;
using AgcDriver::PadStatusDisplay;
using AgcDriver::PadStatusMode;
using AgcDriver::PadStatusSnapshot;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

PadStatusDisplay::Clock::time_point at(int milliseconds) {
    return PadStatusDisplay::Clock::time_point{} + std::chrono::milliseconds(milliseconds);
}

PadSlotStatus pad(PadKind kind, PadPower power, const std::string& name = {}) {
    return {true, kind, name, power};
}

PadStatusSnapshot snapshot(const std::vector<PadSlotStatus>& slots) {
    PadStatusSnapshot result;
    for (std::size_t index = 0; index < slots.size(); ++index) result.slots[index] = slots[index];
    return result;
}

bool parseRefused(const char* value) {
    try {
        AgcDriver::ParsePadStatusMode(value);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("ANYPS5_PAD_STATUS") != std::string::npos;
    }
    return false;
}

void modeTests() {
    require(AgcDriver::ParsePadStatusMode(nullptr) == PadStatusMode::Off, "an unset ANYPS5_PAD_STATUS is not off");
    require(AgcDriver::ParsePadStatusMode("") == PadStatusMode::Off, "an empty ANYPS5_PAD_STATUS is not off");
    require(AgcDriver::ParsePadStatusMode("0") == PadStatusMode::Off, "ANYPS5_PAD_STATUS=0 is not off");
    require(AgcDriver::ParsePadStatusMode("1") == PadStatusMode::Events, "ANYPS5_PAD_STATUS=1 is not the events mode");
    for (const char* value : {"2", "01", " 1", "1 ", "on", "true", "-1", "events", "00"}) require(parseRefused(value), std::string("ANYPS5_PAD_STATUS='") + value + "' is not refused with the variable's name");
    std::printf("pad status mode: unset, empty and 0 are off, 1 is events, anything else is refused\n");
}

void hiddenByDefaultTests() {
    PadStatusDisplay off(PadStatusMode::Off);
    require(!off.Visible(at(0)), "a new display is visible");
    require(off.Observe(at(0), snapshot({pad(PadKind::DualSense, PadPower::Wired)})), "a connect is not reported as an event");
    require(!off.Visible(at(0)) && !off.Visible(at(1)), "the mode 0 display shows a connect");
    require(off.Observe(at(100), snapshot({})), "a disconnect is not reported as an event");
    require(!off.Visible(at(100)), "the mode 0 display shows a disconnect");
    require(off.Observe(at(200), snapshot({pad(PadKind::Xbox, PadPower::Full)})), "a second connect is not reported");
    require(off.Observe(at(300), snapshot({pad(PadKind::Xbox, PadPower::Low)})) && !off.Visible(at(300)), "the mode 0 display shows a battery drop");
    PadStatusDisplay quiet(PadStatusMode::Events);
    require(!quiet.Observe(at(0), snapshot({})) && !quiet.Visible(at(0)), "an empty first observation shows the panel");
    for (int step = 1; step < 200; ++step) require(!quiet.Observe(at(step * 16), snapshot({})) && !quiet.Visible(at(step * 16)), "an unchanged snapshot shows the panel");
    std::printf("pad status: mode 0 and an idle mode 1 never show the panel\n");
}

void expiryTests() {
    PadStatusDisplay display(PadStatusMode::Events);
    display.Observe(at(0), snapshot({}));
    require(display.Observe(at(1000), snapshot({pad(PadKind::DualSense, PadPower::Wired)})), "a connect is not an event");
    require(display.Visible(at(1000)), "the panel is hidden when the controller connects");
    require(display.Visible(at(1001)) && display.Visible(at(4999)), "the panel hides before its 4 seconds are over");
    require(!display.Visible(at(5000)), "the panel is still shown 4 seconds after the event");
    require(!display.Visible(at(60000)), "the panel comes back by itself");
    require(PadStatusDisplay::Duration == std::chrono::seconds(4), "the panel's duration is not 4 seconds");
    std::printf("pad status: shown for 4 s after a connect, then gone\n");
}

void initialConnectTests() {
    PadStatusDisplay display(PadStatusMode::Events);
    require(display.Observe(at(0), snapshot({pad(PadKind::Xbox, PadPower::Medium)})) && display.Visible(at(0)), "a controller present at start does not show the panel");
    require(!display.Visible(at(4000)), "the start panel stays");
    std::printf("pad status: a controller present at the first observation counts as a connect\n");
}

void extensionTests() {
    PadStatusDisplay display(PadStatusMode::Events);
    display.Observe(at(0), snapshot({}));
    display.Observe(at(0), snapshot({pad(PadKind::DualSense, PadPower::Wired)}));
    require(!display.Observe(at(3000), snapshot({pad(PadKind::DualSense, PadPower::Wired)})), "an unchanged snapshot is an event");
    require(!display.Visible(at(4000)), "an observation without an event extended the panel");
    PadStatusDisplay extended(PadStatusMode::Events);
    extended.Observe(at(0), snapshot({}));
    extended.Observe(at(0), snapshot({pad(PadKind::DualSense, PadPower::Wired)}));
    require(extended.Observe(at(3000), snapshot({pad(PadKind::DualSense, PadPower::Wired), pad(PadKind::Xbox, PadPower::Full)})), "a second connect is not an event");
    require(extended.Visible(at(6999)), "a new event did not restart the 4 seconds");
    require(!extended.Visible(at(7000)), "the restarted panel outlives its 4 seconds");
    extended.Observe(at(6000), snapshot({pad(PadKind::DualSense, PadPower::Wired)}));
    require(extended.Visible(at(9999)) && !extended.Visible(at(10000)), "a disconnect did not restart the 4 seconds");
    std::printf("pad status: a new event restarts the 4 s, a quiet observation does not\n");
}

void batteryTests() {
    struct Case {
        PadPower before;
        PadPower after;
        bool event;
    };
    const Case cases[] = {
        {PadPower::Full, PadPower::Medium, true},
        {PadPower::Medium, PadPower::Low, true},
        {PadPower::Low, PadPower::Empty, true},
        {PadPower::Full, PadPower::Empty, true},
        {PadPower::Full, PadPower::Low, true},
        {PadPower::Medium, PadPower::Medium, false},
        {PadPower::Low, PadPower::Medium, false},
        {PadPower::Empty, PadPower::Full, false},
        {PadPower::Medium, PadPower::Full, false},
        {PadPower::Full, PadPower::Wired, false},
        {PadPower::Wired, PadPower::Low, false},
        {PadPower::Wired, PadPower::Wired, false},
        {PadPower::Unknown, PadPower::Low, false},
        {PadPower::Medium, PadPower::Unknown, false},
        {PadPower::Unknown, PadPower::Unknown, false},
    };
    for (const auto& item : cases) {
        PadStatusDisplay display(PadStatusMode::Events);
        display.Observe(at(0), snapshot({pad(PadKind::Xbox, item.before)}));
        const bool shownAtConnect = display.Visible(at(1000));
        require(shownAtConnect, "the connect did not show the panel");
        const bool event = display.Observe(at(10000), snapshot({pad(PadKind::Xbox, item.after)}));
        require(event == item.event, "a battery change was classified wrongly");
        require(display.Visible(at(10001)) == item.event, "a battery change shows the panel wrongly");
    }
    PadStatusDisplay second(PadStatusMode::Events);
    second.Observe(at(0), snapshot({pad(PadKind::Xbox, PadPower::Full), pad(PadKind::DualSense, PadPower::Full)}));
    require(second.Observe(at(10000), snapshot({pad(PadKind::Xbox, PadPower::Full), pad(PadKind::DualSense, PadPower::Low)})) && second.Visible(at(10001)), "a drop on the second slot is not an event");
    std::printf("pad status: only a drop to a lower battery class is an event\n");
}

void replacementTests() {
    PadStatusDisplay display(PadStatusMode::Events);
    display.Observe(at(0), snapshot({pad(PadKind::Xbox, PadPower::Full)}));
    require(display.Observe(at(10000), snapshot({pad(PadKind::DualSense, PadPower::Full)})) && display.Visible(at(10001)), "swapping the controller in a slot is not an event");
    require(display.Observe(at(20000), snapshot({pad(PadKind::Other, PadPower::Full, "ONE")})) && display.Visible(at(20001)), "a controller of another kind is not an event");
    require(display.Observe(at(30000), snapshot({pad(PadKind::Other, PadPower::Full, "TWO")})) && display.Visible(at(30001)), "a controller of the same kind and another name is not an event");
    require(!display.Observe(at(40000), snapshot({pad(PadKind::Other, PadPower::Full, "TWO")})) && !display.Visible(at(40000)), "the same controller is an event");
    std::printf("pad status: swapping the controller in a slot is an event\n");
}

void requestTests() {
    PadStatusDisplay off(PadStatusMode::Off);
    off.Request(at(500));
    require(off.Visible(at(500)) && off.Visible(at(4499)), "a request does not show the panel in mode 0");
    require(!off.Visible(at(4500)), "a request in mode 0 stays past 4 seconds");
    off.Request(at(3000));
    require(off.Visible(at(6999)) && !off.Visible(at(7000)), "a second request does not restart the 4 seconds");
    PadStatusDisplay events(PadStatusMode::Events);
    events.Observe(at(0), snapshot({pad(PadKind::Xbox, PadPower::Full)}));
    events.Request(at(3000));
    require(events.Visible(at(6999)) && !events.Visible(at(7000)), "a request on a panel that is shown does not restart the 4 seconds");
    events.Request(at(20000));
    require(events.Visible(at(20001)) && !events.Visible(at(24000)), "a request after the panel expired does not show it");
    std::printf("pad status: a request shows the panel for 4 s in both modes\n");
}

void providerTests() {
    std::vector<PadStatusSnapshot> script{snapshot({}), snapshot({pad(PadKind::DualSense, PadPower::Wired)}), snapshot({pad(PadKind::DualSense, PadPower::Wired)}), snapshot({})};
    std::size_t step = 0;
    AgcDriver::PadStatusProvider provider{[&] { return script[step]; }};
    PadStatusDisplay display(PadStatusMode::Events);
    std::vector<bool> shown;
    for (step = 0; step < script.size(); ++step) {
        display.Observe(at(static_cast<int>(step) * 1000), provider.read());
        shown.push_back(display.Visible(at(static_cast<int>(step) * 1000)));
    }
    require((shown == std::vector<bool>{false, true, true, true}), "the injected provider did not drive the display");
    require(display.Visible(at(6999)) && !display.Visible(at(7000)), "the last event of the script did not set the expiry");
    std::printf("pad status: an injected provider drives the display without SDL\n");
}

void lineTests() {
    using AgcDriver::FormatPadStatusLine;
    require(FormatPadStatusLine(0, pad(PadKind::DualSense, PadPower::Wired)) == "P1 DUALSENSE  WIRED", "a wired DualSense line");
    require(FormatPadStatusLine(1, pad(PadKind::Xbox, PadPower::Medium)) == "P2 XBOX  BATTERY MEDIUM", "a medium Xbox line");
    require(FormatPadStatusLine(2, PadSlotStatus{}) == "P3 DISCONNECTED", "a disconnected slot line");
    require(FormatPadStatusLine(3, pad(PadKind::Switch, PadPower::Low)) == "P4 SWITCH  BATTERY LOW", "a Switch line");
    require(FormatPadStatusLine(0, pad(PadKind::DualShock3, PadPower::Empty)) == "P1 DUALSHOCK 3  BATTERY EMPTY", "a DualShock 3 line");
    require(FormatPadStatusLine(0, pad(PadKind::Unknown, PadPower::Wired)) == "P1 PAD  WIRED", "an unknown controller without a name");
    require(FormatPadStatusLine(1, pad(PadKind::Other, PadPower::Full, "")) == "P2 PAD  BATTERY FULL", "another controller without a name");
    const std::pair<PadPower, const char*> powers[] = {
        {PadPower::Unknown, "P1 DUALSHOCK 4  BATTERY UNKNOWN"},
        {PadPower::Empty, "P1 DUALSHOCK 4  BATTERY EMPTY"},
        {PadPower::Low, "P1 DUALSHOCK 4  BATTERY LOW"},
        {PadPower::Medium, "P1 DUALSHOCK 4  BATTERY MEDIUM"},
        {PadPower::Full, "P1 DUALSHOCK 4  BATTERY FULL"},
        {PadPower::Wired, "P1 DUALSHOCK 4  WIRED"},
    };
    for (const auto& [power, expected] : powers) require(FormatPadStatusLine(0, pad(PadKind::DualShock4, power)) == expected, std::string("the line for ") + expected);
    std::printf("pad status lines: every power level and kind formats as expected\n");
}

void nameTests() {
    using AgcDriver::FormatPadStatusLine;
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Full, "8BitDo Ultimate 2C Wireless")) == "P1 8BITDO ULTI.  BATTERY FULL", "a long name is not cut to the kind column");
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Wired, "ABCDEFGHIJKL")) == "P1 ABCDEFGHIJKL  WIRED", "a name of exactly 12 characters is cut although it fits");
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Wired, "ABCDEFGHIJKLM")) == "P1 ABCDEFGHIJK.  WIRED", "a name of 13 characters is not cut with a dot");
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Wired, "  Pad (2) !  ")) == "P1 PAD  2  WIRED", "a name is not reduced to the glyphs the font has");
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Wired, "   ")) == "P1 PAD  WIRED", "a blank name is not shown as PAD");
    require(FormatPadStatusLine(0, pad(PadKind::Other, PadPower::Wired, "\xc3\xa9\xc3\xa9")) == "P1 PAD  WIRED", "a name without a glyph is not shown as PAD");
    require(FormatPadStatusLine(0, pad(PadKind::Xbox, PadPower::Wired, "A very long name that is not shown")) == "P1 XBOX  WIRED", "a known kind shows its own label, not the name");
    const PadKind kinds[] = {PadKind::Unknown, PadKind::DualSense, PadKind::DualShock4, PadKind::DualShock3, PadKind::Xbox, PadKind::Switch, PadKind::Other};
    const PadPower levels[] = {PadPower::Unknown, PadPower::Empty, PadPower::Low, PadPower::Medium, PadPower::Full, PadPower::Wired};
    const std::string longName(80, 'w');
    for (const auto kind : kinds) {
        for (const auto power : levels) {
            for (std::size_t slot = 0; slot < AgcDriver::PadStatusSlots; ++slot) {
                const auto line = FormatPadStatusLine(slot, pad(kind, power, longName));
                require(line.size() <= static_cast<std::size_t>(AgcDriver::PadStatusMetrics::Columns), "a line is wider than the panel: " + line);
                require(AgcDriver::OverlayText(line) == line, "a line has a character the overlay font cannot draw: " + line);
            }
        }
    }
    bool refused = false;
    try {
        FormatPadStatusLine(AgcDriver::PadStatusSlots, PadSlotStatus{});
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a fifth slot is formatted");
    std::printf("pad status lines: long names are cut, every line fits the panel\n");
}

void listTests() {
    PadStatusDisplay display(PadStatusMode::Off);
    require(display.Lines() == std::vector<std::string>{"NO CONTROLLER"}, "a display that saw nothing does not say so");
    display.Observe(at(0), snapshot({}));
    require(display.Lines() == std::vector<std::string>{"NO CONTROLLER"}, "a display that saw no controller does not say so");
    display.Observe(at(1), snapshot({pad(PadKind::DualSense, PadPower::Wired), pad(PadKind::Xbox, PadPower::Medium)}));
    require((display.Lines() == std::vector<std::string>{"P1 DUALSENSE  WIRED", "P2 XBOX  BATTERY MEDIUM"}), "two connected controllers are not listed");
    display.Observe(at(2), snapshot({PadSlotStatus{}, pad(PadKind::Xbox, PadPower::Low)}));
    require((display.Lines() == std::vector<std::string>{"P1 DISCONNECTED", "P2 XBOX  BATTERY LOW"}), "a slot that lost its controller is not listed as disconnected");
    PadSlotStatus stale;
    stale.kind = PadKind::DualSense;
    stale.power = PadPower::Full;
    display.Observe(at(3), snapshot({stale, pad(PadKind::Xbox, PadPower::Low), PadSlotStatus{}, pad(PadKind::Switch, PadPower::Full)}));
    require((display.Lines() == std::vector<std::string>{"P1 DISCONNECTED", "P2 XBOX  BATTERY LOW", "P4 SWITCH  BATTERY FULL"}), "a slot that never had a controller is listed");
    PadStatusDisplay first(PadStatusMode::Events);
    first.Observe(at(0), snapshot({PadSlotStatus{}, PadSlotStatus{}, stale}));
    require(first.Lines() == std::vector<std::string>{"NO CONTROLLER"}, "stale fields of an unconnected slot are listed");
    std::printf("pad status lines: one line per slot that ever had a controller\n");
}

using AgcDriver::OverlayColor;
using AgcDriver::OverlayCorner;
using AgcDriver::OverlayLayout;
using AgcDriver::OverlayRect;

bool disjoint(const OverlayRect& a, const OverlayRect& b) {
    return a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y;
}

std::size_t glyphRuns(const std::string& line) {
    std::size_t runs = 0;
    for (const char character : line) {
        for (const auto bits : AgcDriver::OverlayGlyph(character)) {
            bool inside = false;
            for (int x = 0; x < AgcDriver::OverlayMetrics::GlyphWidth; ++x) {
                const bool set = (bits & (0x10u >> x)) != 0;
                if (set && !inside) ++runs;
                inside = set;
            }
        }
    }
    return runs;
}

void layoutTests() {
    const std::vector<std::string> one{"P1 DUALSENSE  WIRED"};
    const auto big = AgcDriver::LayOutPadStatus(one, {1920, 1080}, OverlayCorner::TopLeft);
    require(big.scale == 2, "the panel is not drawn at scale 2 on 1080 lines");
    require(big.bounds.x == 1518 && big.bounds.y == 8 && big.bounds.width == 394 && big.bounds.height == 26, "the one-line panel is not at the top right of 1920x1080");
    const std::vector<std::string> four{"P1 DUALSENSE  WIRED", "P2 XBOX  BATTERY MEDIUM", "P3 DISCONNECTED", "P4 DUALSHOCK 4  BATTERY UNKNOWN"};
    const auto small = AgcDriver::LayOutPadStatus(four, {1280, 720}, OverlayCorner::TopLeft);
    require(small.scale == 2, "the panel is not drawn at scale 2 on 720 lines");
    require(small.bounds.x == 1280 - 8 - 394 && small.bounds.y == 8 && small.bounds.width == 394 && small.bounds.height == 80, "the four-line panel is not at the top right of 1280x720");
    const auto bottom = AgcDriver::LayOutPadStatus(four, {1280, 720}, OverlayCorner::TopRight);
    require(bottom.bounds.x == 8 && bottom.bounds.y == 8, "the panel does not move to the top left when the overlay is at the top right");
    const auto low = AgcDriver::LayOutPadStatus(four, {1280, 720}, OverlayCorner::BottomLeft);
    require(low.bounds.x == 1280 - 8 - 394 && low.bounds.y == 720 - 8 - 80, "the panel does not follow the overlay to the bottom");
    const auto lowRight = AgcDriver::LayOutPadStatus(four, {1280, 720}, OverlayCorner::BottomRight);
    require(lowRight.bounds.x == 8 && lowRight.bounds.y == 720 - 8 - 80, "the panel is not at the bottom left when the overlay is at the bottom right");
    const auto tiny = AgcDriver::LayOutPadStatus(one, {640, 360}, OverlayCorner::TopLeft);
    require(tiny.scale == 1 && tiny.bounds.width == 197 && tiny.bounds.height == 13, "the panel is not drawn at scale 1 on 360 lines");
    std::printf("pad status layout: panel size and corner on 1920x1080, 1280x720 and 640x360\n");
}

void paintTests() {
    const std::vector<std::string> four{"P1 DUALSENSE  WIRED", "P2 XBOX  BATTERY MEDIUM", "P3 DISCONNECTED", "P4 DUALSHOCK 4  BATTERY UNKNOWN"};
    const auto layout = AgcDriver::LayOutPadStatus(four, {1280, 720}, OverlayCorner::TopLeft);
    std::size_t expected = 1;
    for (const auto& line : four) expected += glyphRuns(line);
    require(layout.rects.size() == expected, "the panel does not draw one rectangle per glyph run plus its background");
    require(layout.rects.front().color == static_cast<std::uint32_t>(OverlayColor::Background) && layout.rects.front().x == layout.bounds.x, "the first rectangle is not the background");
    for (std::size_t index = 1; index < layout.rects.size(); ++index) {
        const auto& rect = layout.rects[index];
        require(rect.color == static_cast<std::uint32_t>(OverlayColor::Text), "a glyph is not drawn in the text colour");
        require(rect.x >= layout.bounds.x && rect.y >= layout.bounds.y && rect.x + rect.width <= layout.bounds.x + layout.bounds.width && rect.y + rect.height <= layout.bounds.y + layout.bounds.height, "a glyph lies outside the panel");
    }
    const auto widest = AgcDriver::LayOutPadStatus({std::string(AgcDriver::PadStatusMetrics::Columns, 'W')}, {1280, 720}, OverlayCorner::TopLeft);
    std::int32_t right = 0;
    for (const auto& rect : widest.rects) right = std::max(right, rect.x + rect.width);
    require(right <= widest.bounds.x + widest.bounds.width, "a full-width line leaves the panel");
    bool refused = false;
    try {
        AgcDriver::LayOutPadStatus({std::string(AgcDriver::PadStatusMetrics::Columns + 1, 'W')}, {1280, 720}, OverlayCorner::TopLeft);
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a line wider than the panel is laid out");
    refused = false;
    try {
        AgcDriver::LayOutPadStatus({}, {1280, 720}, OverlayCorner::TopLeft);
    } catch (const std::exception&) {
        refused = true;
    }
    require(refused, "a panel without lines is laid out");
    std::printf("pad status layout: one rectangle per glyph run, all inside the panel\n");
}

void cornerTests() {
    require(AgcDriver::PadStatusCorner(OverlayCorner::TopLeft) == OverlayCorner::TopRight, "top left does not mirror to top right");
    require(AgcDriver::PadStatusCorner(OverlayCorner::TopRight) == OverlayCorner::TopLeft, "top right does not mirror to top left");
    require(AgcDriver::PadStatusCorner(OverlayCorner::BottomLeft) == OverlayCorner::BottomRight, "bottom left does not mirror to bottom right");
    require(AgcDriver::PadStatusCorner(OverlayCorner::BottomRight) == OverlayCorner::BottomLeft, "bottom right does not mirror to bottom left");
    const std::vector<std::string> four{"P1 DUALSENSE  WIRED", "P2 XBOX  BATTERY MEDIUM", "P3 DISCONNECTED", "P4 DUALSHOCK 4  BATTERY UNKNOWN"};
    const AgcDriver::OverlayStatistics statistics;
    const auto first = AgcDriver::ComposeOverlay(statistics, true, true);
    const auto second = AgcDriver::ComposeOverlayDetails(AgcDriver::OverlayDetailSample{});
    for (const VkExtent2D extent : {VkExtent2D{640, 360}, VkExtent2D{1280, 720}, VkExtent2D{1366, 768}, VkExtent2D{1920, 1080}, VkExtent2D{3840, 2160}}) {
        for (const auto corner : {OverlayCorner::TopLeft, OverlayCorner::TopRight, OverlayCorner::BottomLeft, OverlayCorner::BottomRight}) {
            const auto panel = AgcDriver::LayOutPadStatus(four, extent, corner);
            for (const auto* page : {&first, &second}) {
                const auto overlay = AgcDriver::LayOutOverlay(*page, extent, corner);
                require(disjoint(panel.bounds, overlay.bounds), "the panel covers the performance overlay");
            }
        }
    }
    std::printf("pad status layout: never covers the performance overlay, in any corner or page\n");
}

void priorityTests() {
    require(AgcDriver::PadStatusDrawn(true, false), "a visible panel is not drawn without the keyboard");
    require(!AgcDriver::PadStatusDrawn(true, true), "the panel is drawn over the system keyboard");
    require(!AgcDriver::PadStatusDrawn(false, false), "a hidden panel is drawn");
    require(!AgcDriver::PadStatusDrawn(false, true), "a hidden panel is drawn with the keyboard");
    PadStatusDisplay display(PadStatusMode::Events);
    display.Observe(at(0), snapshot({pad(PadKind::Xbox, PadPower::Full)}));
    require(AgcDriver::PadStatusDrawn(display.Visible(at(1000)), false), "the panel is not drawn before the keyboard opens");
    require(!AgcDriver::PadStatusDrawn(display.Visible(at(2000)), true), "the panel is drawn while the keyboard is open");
    require(AgcDriver::PadStatusDrawn(display.Visible(at(3000)), false), "the panel does not come back when the keyboard closes within its 4 seconds");
    require(!AgcDriver::PadStatusDrawn(display.Visible(at(5000)), false), "the panel waited for the keyboard past its 4 seconds");
    std::printf("pad status priority: the system keyboard wins, the panel's clock keeps running\n");
}

void sharedDisplayTests(bool events) {
    require(!AgcDriver::PadStatusOverlayLines().has_value(), "the panel is shown before anything happened");
    std::size_t reads = 0;
    const auto connected = snapshot({pad(PadKind::DualSense, PadPower::Wired)});
    const AgcDriver::PadStatusProvider provider{[&] {
        ++reads;
        return connected;
    }};
    AgcDriverPadStatusPoll_nid_postfix(&provider);
    require((reads == 1) == events && (reads == 0) == !events, events ? "the events mode did not read the provider" : "the provider was read with ANYPS5_PAD_STATUS unset and no request");
    require(AgcDriver::PadStatusOverlayLines().has_value() == events, events ? "a connect does not show the panel with ANYPS5_PAD_STATUS=1" : "a connect shows the panel with ANYPS5_PAD_STATUS unset");
    AgcDriverPadStatusShow_nid_postfix();
    AgcDriverPadStatusPoll_nid_postfix(&provider);
    require(reads == (events ? 2u : 1u), "a request did not make the next poll read the provider once");
    const auto lines = AgcDriver::PadStatusOverlayLines();
    require(lines.has_value() && *lines == std::vector<std::string>{"P1 DUALSENSE  WIRED"}, "the shown panel does not list the controller");
    std::printf("pad status: the shared display %s\n", events ? "shows on a connect and on a request" : "reads and shows only after a request");
}

void invalidEnvironmentTests() {
    const auto connected = snapshot({pad(PadKind::DualSense, PadPower::Wired)});
    const AgcDriver::PadStatusProvider provider{[&] { return connected; }};
    try {
        AgcDriverPadStatusPoll_nid_postfix(&provider);
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("ANYPS5_PAD_STATUS") != std::string::npos, "the invalid value is refused without naming the variable");
        std::printf("pad status: an invalid ANYPS5_PAD_STATUS stops the first use: %s\n", error.what());
        return;
    }
    throw std::runtime_error("an invalid ANYPS5_PAD_STATUS was accepted at first use");
}

struct CountingProvider {
    std::vector<PadStatusSnapshot> script;
    std::size_t reads = 0;
    std::size_t at = 0;
    AgcDriver::PadStatusProvider provider() {
        return {[this] {
            ++reads;
            return script[std::min(at, script.size() - 1)];
        }};
    }
};

void pollTests() {
    CountingProvider off;
    off.script = {snapshot({pad(PadKind::DualSense, PadPower::Wired)})};
    PadStatusDisplay hidden(PadStatusMode::Off);
    const auto offProvider = off.provider();
    for (int step = 0; step < 500; ++step) require(!hidden.Poll(at(step * 16), offProvider) && !hidden.Visible(at(step * 16)), "the mode 0 display read a snapshot or showed the panel");
    require(off.reads == 0, "the provider was called with the mode off and no request");
    hidden.Request(at(10000));
    require(hidden.Poll(at(10000), offProvider) && off.reads == 1, "a request does not make the display read the provider");
    require(hidden.Visible(at(13999)) && !hidden.Visible(at(14000)), "the requested panel does not last 4 seconds");
    require((hidden.Lines() == std::vector<std::string>{"P1 DUALSENSE  WIRED"}), "the requested panel does not list the controller");
    off.script = {snapshot({pad(PadKind::DualSense, PadPower::Wired)}), snapshot({pad(PadKind::DualSense, PadPower::Low)})};
    off.at = 1;
    require(hidden.Poll(at(12000), offProvider) && off.reads == 2, "a shown panel stops following the controller");
    require((hidden.Lines() == std::vector<std::string>{"P1 DUALSENSE  BATTERY LOW"}), "a shown panel does not follow a battery change");
    require(!hidden.Poll(at(14000), offProvider) && off.reads == 2, "an expired panel still reads the provider");
    off.at = 0;
    hidden.Request(at(20000));
    require(hidden.Poll(at(20000), offProvider) && off.reads == 3 && hidden.Visible(at(23999)) && !hidden.Visible(at(24000)), "a second request does not behave like the first");
    require((hidden.Lines() == std::vector<std::string>{"P1 DUALSENSE  WIRED"}), "a second request shows a stale snapshot");

    CountingProvider on;
    on.script = {snapshot({}), snapshot({pad(PadKind::Xbox, PadPower::Full)}), snapshot({pad(PadKind::Xbox, PadPower::Full)}), snapshot({})};
    PadStatusDisplay events(PadStatusMode::Events);
    PadStatusDisplay direct(PadStatusMode::Events);
    const auto onProvider = on.provider();
    for (std::size_t step = 0; step < on.script.size(); ++step) {
        on.at = step;
        const int time = static_cast<int>(step) * 1000;
        require(events.Poll(at(time), onProvider), "the events mode did not read the provider");
        direct.Observe(at(time), on.script[step]);
        for (const int offset : {0, 1, 999, 3999, 4000, 4001})
            require(events.Visible(at(time + offset)) == direct.Visible(at(time + offset)), "polling shows the panel at other times than observing");
    }
    require(on.reads == 4, "the events mode did not read the provider on every poll");
    require(events.Lines() == direct.Lines(), "polling lists other lines than observing");
    std::printf("pad status: the snapshot is read only in the events mode or while a request keeps the panel shown\n");
}

}

int main(int argc, char** argv) try {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "invalid") {
        invalidEnvironmentTests();
        return 0;
    }
    if (mode == "events" || mode == "default") {
        sharedDisplayTests(mode == "events");
        std::printf("pad status shared display tests passed\n");
        return 0;
    }
    modeTests();
    hiddenByDefaultTests();
    expiryTests();
    initialConnectTests();
    extensionTests();
    batteryTests();
    replacementTests();
    requestTests();
    providerTests();
    lineTests();
    nameTests();
    listTests();
    layoutTests();
    paintTests();
    cornerTests();
    priorityTests();
    pollTests();
    std::printf("pad status overlay tests passed\n");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
