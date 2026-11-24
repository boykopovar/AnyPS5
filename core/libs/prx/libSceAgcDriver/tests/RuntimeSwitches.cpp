#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

using AgcDriver::Graphics::AbSchedule;
using AgcDriver::Graphics::RuntimeSwitch;
namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr) setenv(name, value, 1);
    else unsetenv(name);
#endif
}

std::string Refusal(const char* ab, const char* start = "", const char* settle = "") {
    try {
        AbSchedule::Parse(ab, start, settle);
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return {};
}

void registry() {
    for (std::size_t index = 0; index < static_cast<std::size_t>(RuntimeSwitch::Count); ++index) {
        const auto id = static_cast<RuntimeSwitch>(index);
        Require(RuntimeSwitches::Find(RuntimeSwitches::Name(id)) == id, std::string("the name of a switch does not find it: ") + RuntimeSwitches::Name(id));
        Require(RuntimeSwitches::Set(id) == (std::getenv(RuntimeSwitches::Name(id)) != nullptr), std::string("a switch does not start as the environment sets it: ") + RuntimeSwitches::Name(id));
    }
    Require(RuntimeSwitches::Find("APS5_NO_BARRIER_BATCH") == RuntimeSwitch::NoBarrierBatch, "APS5_NO_BARRIER_BATCH is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_DRAW_TABLE_SNAPSHOT") == RuntimeSwitch::TableSnapshot, "APS5_DRAW_TABLE_SNAPSHOT is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_WIDE_LIBRARY_KEYS") == RuntimeSwitch::WideLibraryKeys, "APS5_WIDE_LIBRARY_KEYS is not a runtime switch");
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "a switch set in the environment did not start set");
    Require(!RuntimeSwitches::Find("APS5_FULL_BARRIERS").has_value() && !RuntimeSwitches::Find("APS5_NO_BARRIER_BATCH ").has_value(), "a name that is not a runtime switch was found");
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, false);
    Require(!RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "an assigned switch did not read back unset");
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, true);
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "an assigned switch did not read back set");
}

void parse() {
    const auto config = AbSchedule::Parse(" 30 : APS5_NO_BARRIER_BATCH ", "815", "3");
    Require(config.phaseSeconds == 30 && config.settleSeconds == 3 && config.startPresent == 815 && !config.startSeconds.has_value(), "the phase settings were not parsed");
    Require(config.switches.size() == 1 && config.switches[0] == RuntimeSwitch::NoBarrierBatch, "the switch list was not parsed");
    Require(config.arms.size() == 2 && config.arms[0].name == "A" && config.arms[0].switches.empty() && config.arms[1].name == "B" && config.arms[1].switches.size() == 1, "the legacy form is not the two arms A and B");
    const auto bySeconds = AbSchedule::Parse("60:APS5_NO_BARRIER_BATCH", "90s", "");
    Require(bySeconds.startSeconds == 90.0 && bySeconds.settleSeconds == 2 && bySeconds.startPresent == 0, "a start in seconds was not parsed");
    Require(Refusal("60:APS5_FULL_BARRIERS").find("APS5_FULL_BARRIERS cannot change while the title runs; switches the A/B can change: APS5_NO_BARRIER_BATCH") != std::string::npos, "a startup-only variable was not refused with the list of switches");
    Require(Refusal("60:APS5_NO_BARRIER_BATCH+APS5_NO_BARRIER_BATCH").find("listed twice") != std::string::npos, "a duplicate switch was not refused");
    Require(Refusal("60:+APS5_NO_BARRIER_BATCH").find("empty switch name") != std::string::npos && Refusal("60:APS5_NO_BARRIER_BATCH+").find("empty switch name") != std::string::npos, "an empty switch name was not refused");
    Require(Refusal("60:").find("no switch listed") != std::string::npos, "an empty switch list was not refused");
    Require(!Refusal("APS5_NO_BARRIER_BATCH").empty(), "a missing phase length was not refused");
    Require(!Refusal("0.5:APS5_NO_BARRIER_BATCH").empty() && !Refusal("x:APS5_NO_BARRIER_BATCH").empty() && !Refusal("-5:APS5_NO_BARRIER_BATCH").empty() && !Refusal("inf:APS5_NO_BARRIER_BATCH").empty(), "a bad phase length was not refused");
    Require(!Refusal("10:APS5_NO_BARRIER_BATCH", "", "10").empty() && !Refusal("10:APS5_NO_BARRIER_BATCH", "", "x").empty(), "a bad settle was not refused");
    for (const char* start : {"-3", "abc", "s", "+3", "3x", "99999999999999999999999"}) {
        Require(Refusal("10:APS5_NO_BARRIER_BATCH", start).find("APS5_AB_START") != std::string::npos, std::string("the start '") + start + "' was not refused");
    }
}

void arms() {
    const auto plan = AbSchedule::Parse("60:A|X=APS5_NO_BARRIER_BATCH|A|Y=APS5_NO_GUARD_SPANS+APS5_NO_OWNED_DRAW_INPUTS|X", "", "");
    Require(plan.arms.size() == 5, "the five arms of A|X|A|Y|X were not parsed");
    Require(plan.arms[0].name == "A" && plan.arms[0].switches.empty() && plan.arms[2].name == "A" && plan.arms[2].switches.empty(), "the baseline arm A was not parsed as no switches");
    Require(plan.arms[1].name == "X" && plan.arms[1].switches.size() == 1 && plan.arms[1].switches[0] == RuntimeSwitch::NoBarrierBatch, "the group X was not parsed");
    Require(plan.arms[3].name == "Y" && plan.arms[3].switches.size() == 2 && plan.arms[3].switches[1] == RuntimeSwitch::NoOwnedDrawInputs, "the group Y with two switches was not parsed");
    Require(plan.arms[4].name == "X" && plan.arms[4].switches == plan.arms[1].switches, "a reused group name did not reuse its switches");
    Require(plan.switches.size() == 3 && plan.switches[0] == RuntimeSwitch::NoBarrierBatch && plan.switches[1] == RuntimeSwitch::NoGuardSpans && plan.switches[2] == RuntimeSwitch::NoOwnedDrawInputs, "the union of the arms' switches was not collected");
    const auto bare = AbSchedule::Parse("60:A|APS5_NO_OWNED_DRAW_INPUTS", "", "");
    Require(bare.arms.size() == 2 && bare.arms[1].name == "APS5_NO_OWNED_DRAW_INPUTS" && bare.arms[1].switches.size() == 1, "a bare switch name as an arm was not parsed");
    Require(Refusal("60:A|X").find("cannot change while the title runs") != std::string::npos, "an undefined group name was not refused");
    Require(Refusal("60:A|X=APS5_NO_BARRIER_BATCH|X=APS5_NO_OWNED_DRAW_INPUTS").find("defined twice") != std::string::npos, "a group defined twice was not refused");
    Require(Refusal("60:A|").find("empty arm") != std::string::npos, "an empty arm was not refused");
    Require(Refusal("60:A=APS5_NO_BARRIER_BATCH|A").find("needs a name other than A") != std::string::npos, "an arm without a name before = was not refused");
    Require(Refusal("60:A|X-Y=APS5_NO_BARRIER_BATCH").find("may only contain letters, digits and _") != std::string::npos, "an arm name with punctuation was not refused");
    Require(Refusal("60:A|X=").find("empty switch name") != std::string::npos, "a group without switches was not refused");
    Require(Refusal("60:A|X=APS5_NO_BARRIER_BATCH+APS5_NO_BARRIER_BATCH").find("listed twice") != std::string::npos, "a group listing a switch twice was not refused");
}

void schedule() {
    AbSchedule plan(AbSchedule::Parse("60:APS5_NO_BARRIER_BATCH", "10", "2"));
    using Event = AbSchedule::Event;
    const auto expect = [&](std::uint64_t present, double seconds, Event event, std::uint64_t phase, std::size_t arm) {
        const auto step = plan.AtPresent(present, seconds);
        Require(step.event == event && (event == Event::None || (step.phase == phase && step.arm == arm)), "unexpected step at present " + std::to_string(present));
    };
    expect(1, 0.0, Event::None, 0, 0);
    expect(9, 100.0, Event::None, 0, 0);
    expect(10, 102.0, Event::Begin, 0, 0);
    expect(11, 103.0, Event::None, 0, 0);
    expect(12, 104.5, Event::Settled, 0, 0);
    expect(13, 150.0, Event::None, 0, 0);
    expect(14, 161.9, Event::None, 0, 0);
    expect(15, 162.0, Event::Begin, 1, 1);
    expect(16, 163.0, Event::None, 1, 1);
    expect(17, 300.0, Event::Begin, 2, 0);
    expect(18, 301.0, Event::None, 2, 0);
    expect(19, 302.5, Event::Settled, 2, 0);
    expect(20, 362.5, Event::Begin, 3, 1);

    AbSchedule timed(AbSchedule::Parse("30:APS5_NO_BARRIER_BATCH", "90s", ""));
    Require(timed.AtPresent(500, 1000.0).event == Event::None, "a start in seconds began at the first present");
    Require(timed.AtPresent(501, 1089.0).event == Event::None, "a start in seconds began early");
    const auto begin = timed.AtPresent(502, 1090.0);
    Require(begin.event == Event::Begin && begin.phase == 0 && begin.arm == 0, "a start in seconds did not begin phase A");

    AbSchedule cycle(AbSchedule::Parse("60:A|X=APS5_NO_BARRIER_BATCH|A|Y=APS5_NO_OWNED_DRAW_INPUTS", "", "2"));
    const auto arm0 = cycle.AtPresent(1, 0.0);
    Require(arm0.event == Event::Begin && arm0.phase == 0 && arm0.arm == 0, "the multi-arm cycle did not begin at arm A");
    const auto arm1 = cycle.AtPresent(2, 60.0);
    Require(arm1.event == Event::Begin && arm1.phase == 1 && arm1.arm == 1, "the multi-arm cycle did not change to arm X");
    const auto arm2 = cycle.AtPresent(3, 120.0);
    Require(arm2.event == Event::Begin && arm2.phase == 2 && arm2.arm == 2, "the multi-arm cycle did not change back to arm A");
    const auto arm3 = cycle.AtPresent(4, 180.0);
    Require(arm3.event == Event::Begin && arm3.phase == 3 && arm3.arm == 3, "the multi-arm cycle did not change to arm Y");
    const auto arm4 = cycle.AtPresent(5, 240.0);
    Require(arm4.event == Event::Begin && arm4.phase == 4 && arm4.arm == 0, "the multi-arm cycle did not wrap to arm A");
}

void rotation() {
    using Event = AbSchedule::Event;
    const auto two = AbSchedule::Parse("60:A|X=APS5_NO_BARRIER_BATCH", "", "", true);
    Require(two.rotate, "the rotation was not kept in the configuration");
    AbSchedule pairs(two);
    const std::size_t twoArms[] = {0, 1, 1, 0, 0, 1, 1, 0};
    for (std::size_t phase = 0; phase < 8; ++phase) {
        const auto step = pairs.AtPresent(phase + 1, 60.0 * static_cast<double>(phase));
        Require(step.event == Event::Begin && step.phase == phase && step.arm == twoArms[phase], "the two-arm rotation put arm " + std::to_string(step.arm) + " at phase " + std::to_string(phase));
    }
    AbSchedule three(AbSchedule::Parse("60:A|X=APS5_NO_BARRIER_BATCH|Y=APS5_NO_GUARD_SPANS", "", "2", true));
    const std::size_t threeArms[] = {0, 1, 2, 1, 2, 0, 2, 0, 1, 0, 1, 2};
    for (std::size_t phase = 0; phase < 12; ++phase) {
        const auto begin = three.AtPresent(phase * 2 + 1, 60.0 * static_cast<double>(phase));
        Require(begin.event == Event::Begin && begin.phase == phase && begin.arm == threeArms[phase], "the three-arm rotation put arm " + std::to_string(begin.arm) + " at phase " + std::to_string(phase));
        Require(begin.arm == (phase % 3 + phase / 3) % 3, "cycle " + std::to_string(phase / 3) + " did not begin with arm " + std::to_string(phase / 3 % 3));
        const auto settled = three.AtPresent(phase * 2 + 2, 60.0 * static_cast<double>(phase) + 2.0);
        Require(settled.event == Event::Settled && settled.phase == phase && settled.arm == threeArms[phase], "the settle step of phase " + std::to_string(phase) + " named another arm");
    }
    AbSchedule plain(AbSchedule::Parse("60:A|X=APS5_NO_BARRIER_BATCH|Y=APS5_NO_GUARD_SPANS", "", ""));
    Require(!plain.Settings().rotate, "the rotation was on without being asked");
    for (std::size_t phase = 0; phase < 6; ++phase) Require(plain.AtPresent(phase + 1, 60.0 * static_cast<double>(phase)).arm == phase % 3, "the arms changed their order without the rotation");
}

void glue() {
    Require(AgcDriver::Graphics::AbConfigured(), "APS5_AB is not seen");
    Require(AgcDriver::Graphics::AbAtPresent(1, 0.0).empty(), "the A/B printed before its start");
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, false);
    auto lines = AgcDriver::Graphics::AbAtPresent(3, 1.0);
    Require(lines.size() == 2 && lines[0] == "[ab] in-run A/B: phases of 5.0 s from present 3, settle 2.0 s; A: APS5_NO_BARRIER_BATCH set; B: APS5_NO_BARRIER_BATCH unset\n" && lines[1] == "[ab] phase A 0 begins at present 3 (t 1.000 s): APS5_NO_BARRIER_BATCH set\n", "the start lines differ: " + (lines.empty() ? std::string() : lines.front()));
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "phase A does not have the environment's setting");
    lines = AgcDriver::Graphics::AbAtPresent(4, 3.5);
    Require(lines.size() == 1 && lines[0] == "[ab] phase A 0 settled at present 4 (t 3.500 s)\n", "the settle line differs");
    lines = AgcDriver::Graphics::AbAtPresent(5, 6.0);
    Require(lines.size() == 1 && lines[0] == "[ab] phase B 1 begins at present 5 (t 6.000 s): APS5_NO_BARRIER_BATCH unset\n", "the phase B line differs");
    Require(!RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "phase B did not toggle the listed switch");
    Require(AgcDriver::Graphics::AbAtPresent(6, 7.0).empty(), "a present inside the settle printed");
    lines = AgcDriver::Graphics::AbAtPresent(7, 11.5);
    Require(lines.size() == 1 && lines[0] == "[ab] phase A 2 begins at present 7 (t 11.500 s): APS5_NO_BARRIER_BATCH set\n", "phase A did not come back");
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "phase A did not restore the environment's setting");
}

int rotated() {
    try {
        Require(AgcDriver::Graphics::AbConfigured(), "APS5_AB is not seen");
        Require(AgcDriver::Graphics::AbAtPresent(1, 0.0).empty(), "the A/B printed before its start");
        RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, false);
        auto lines = AgcDriver::Graphics::AbAtPresent(3, 1.0);
        Require(lines.size() == 2 && lines[0] == "[ab] in-run A/B: phases of 5.0 s from present 3, settle 2.0 s, the arm order rotates by one at every cycle of 2 phases; A: APS5_NO_BARRIER_BATCH set; X: APS5_NO_BARRIER_BATCH unset\n" && lines[1] == "[ab] phase A 0 begins at present 3 (t 1.000 s): APS5_NO_BARRIER_BATCH set\n", "the rotated start lines differ: " + (lines.empty() ? std::string() : lines.front()));
        Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "phase A does not have the environment's setting");
        lines = AgcDriver::Graphics::AbAtPresent(4, 3.5);
        Require(lines.size() == 1 && lines[0] == "[ab] phase A 0 settled at present 4 (t 3.500 s)\n", "the settle line of phase A differs");
        lines = AgcDriver::Graphics::AbAtPresent(5, 6.0);
        Require(lines.size() == 1 && lines[0] == "[ab] phase X 1 begins at present 5 (t 6.000 s): APS5_NO_BARRIER_BATCH unset\n", "the line of phase X 1 differs");
        Require(!RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "phase X did not toggle the listed switch");
        lines = AgcDriver::Graphics::AbAtPresent(6, 8.5);
        Require(lines.size() == 1 && lines[0] == "[ab] phase X 1 settled at present 6 (t 8.500 s)\n", "the settle line of phase X 1 differs");
        lines = AgcDriver::Graphics::AbAtPresent(7, 11.5);
        Require(lines.size() == 1 && lines[0] == "[ab] phase X 2 begins at present 7 (t 11.500 s): APS5_NO_BARRIER_BATCH unset\n", "the second cycle did not begin with arm X");
        Require(!RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "arm X did not keep its setting into the next phase");
        lines = AgcDriver::Graphics::AbAtPresent(8, 17.0);
        Require(lines.size() == 1 && lines[0] == "[ab] phase A 3 begins at present 8 (t 17.000 s): APS5_NO_BARRIER_BATCH set\n", "phase 3 did not return to arm A");
        Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "arm A did not restore the environment's setting");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
    std::printf("runtime switch rotation test passed\n");
    return 0;
}

int refused() {
    SetEnvironment("APS5_AB", "5:APS5_FULL_BARRIERS");
    try {
        AgcDriver::Graphics::AbAtPresent(1, 0.0);
    } catch (const std::invalid_argument& error) {
        if (std::string_view(error.what()).find("APS5_FULL_BARRIERS cannot change while the title runs") != std::string_view::npos) {
            std::printf("runtime switch refusal test passed\n");
            return 0;
        }
        std::fprintf(stderr, "FAILED: unexpected refusal: %s\n", error.what());
        return 1;
    }
    std::fprintf(stderr, "FAILED: APS5_AB naming a startup-only variable was not refused at the first present\n");
    return 1;
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "refused") return refused();
    if (argc > 1 && std::string_view(argv[1]) == "rotated") {
        SetEnvironment("APS5_NO_BARRIER_BATCH", "1");
        SetEnvironment("APS5_AB", "5:A|X=APS5_NO_BARRIER_BATCH");
        SetEnvironment("APS5_AB_START", "3");
        SetEnvironment("APS5_AB_SETTLE", nullptr);
        SetEnvironment("APS5_AB_ROTATE", "1");
        return rotated();
    }
    try {
        SetEnvironment("APS5_NO_BARRIER_BATCH", "1");
        SetEnvironment("APS5_AB", "5:APS5_NO_BARRIER_BATCH");
        SetEnvironment("APS5_AB_START", "3");
        SetEnvironment("APS5_AB_SETTLE", nullptr);
        registry();
        parse();
        arms();
        schedule();
        rotation();
        glue();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
    std::printf("runtime switch tests passed\n");
    return 0;
}
