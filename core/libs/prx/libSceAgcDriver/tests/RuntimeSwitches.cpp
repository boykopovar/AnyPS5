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
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "a switch set in the environment did not start set");
    Require(!RuntimeSwitches::Find("APS5_FULL_BARRIERS").has_value() && !RuntimeSwitches::Find("APS5_NO_BARRIER_BATCH ").has_value(), "a name that is not a runtime switch was found");
    const auto generation = RuntimeSwitches::Generation();
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, false);
    Require(!RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "an assigned switch did not read back unset");
    Require(RuntimeSwitches::Generation() == generation + 1, "flipping a switch did not move the switch generation");
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, false);
    Require(RuntimeSwitches::Generation() == generation + 1, "assigning a switch its own value moved the switch generation");
    RuntimeSwitches::Assign(RuntimeSwitch::NoBarrierBatch, true);
    Require(RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch), "an assigned switch did not read back set");
    Require(RuntimeSwitches::Generation() == generation + 2, "flipping a switch back did not move the switch generation");
    Require(RuntimeSwitches::Find("APS5_LEAN_FRONT") == RuntimeSwitch::LeanFront, "APS5_LEAN_FRONT is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_LEAN_LIST_DIRECT") == RuntimeSwitch::LeanListDirect, "APS5_LEAN_LIST_DIRECT is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_DIRECT_DRAWS") == RuntimeSwitch::DirectDraws, "APS5_DIRECT_DRAWS is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_DIRECT_RETRY") == RuntimeSwitch::DirectRetry, "APS5_DIRECT_RETRY is not a runtime switch");
    Require(RuntimeSwitches::Find("APS5_DIRECT_PASS_TARGETS") == RuntimeSwitch::DirectPassTargets && !RuntimeSwitches::Set(RuntimeSwitch::DirectPassTargets), "APS5_DIRECT_PASS_TARGETS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_DIRECT_BUFFERS") == RuntimeSwitch::DirectBuffers && !RuntimeSwitches::Set(RuntimeSwitch::DirectBuffers), "APS5_DIRECT_BUFFERS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_DIRECT_INPUTS") == RuntimeSwitch::DirectInputs && !RuntimeSwitches::Set(RuntimeSwitch::DirectInputs), "APS5_DIRECT_INPUTS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_FAST_SETTLE") == RuntimeSwitch::FastSettle && !RuntimeSwitches::Set(RuntimeSwitch::FastSettle), "APS5_FAST_SETTLE is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_PRECISE_SETTLE") == RuntimeSwitch::PreciseSettle && !RuntimeSwitches::Set(RuntimeSwitch::PreciseSettle), "APS5_PRECISE_SETTLE is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_IMPORT_SNAPSHOTS") == RuntimeSwitch::ImportSnapshots && !RuntimeSwitches::Set(RuntimeSwitch::ImportSnapshots), "APS5_IMPORT_SNAPSHOTS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_MERGED_SETTLE") == RuntimeSwitch::MergedSettle && !RuntimeSwitches::Set(RuntimeSwitch::MergedSettle), "APS5_MERGED_SETTLE is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_UNLOCKED_STAMPS") == RuntimeSwitch::UnlockedStamps && !RuntimeSwitches::Set(RuntimeSwitch::UnlockedStamps), "APS5_UNLOCKED_STAMPS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_WRITE_SNAPSHOT_FRONTS") == RuntimeSwitch::WriteSnapshotFronts && !RuntimeSwitches::Set(RuntimeSwitch::WriteSnapshotFronts), "APS5_WRITE_SNAPSHOT_FRONTS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_PENDING_FRONTS") == RuntimeSwitch::PendingFronts && !RuntimeSwitches::Set(RuntimeSwitch::PendingFronts), "APS5_PENDING_FRONTS is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_LEAN_PREPARE") == RuntimeSwitch::LeanPrepare && !RuntimeSwitches::Set(RuntimeSwitch::LeanPrepare), "APS5_LEAN_PREPARE is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_BUFFER_BLOB_APPEND") == RuntimeSwitch::BufferBlobAppend && !RuntimeSwitches::Set(RuntimeSwitch::BufferBlobAppend), "APS5_BUFFER_BLOB_APPEND is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_PREPARED_CAPACITY") == RuntimeSwitch::PreparedCapacity && !RuntimeSwitches::Set(RuntimeSwitch::PreparedCapacity), "APS5_PREPARED_CAPACITY is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Find("APS5_INVOCATION_SLOTS_WIDE") == RuntimeSwitch::InvocationSlotsWide && !RuntimeSwitches::Set(RuntimeSwitch::InvocationSlotsWide), "APS5_INVOCATION_SLOTS_WIDE is not a runtime switch that starts unset");
    Require(RuntimeSwitches::Set(RuntimeSwitch::LeanFront),"APS5_LEAN_FRONT set in the environment did not start set");
}

void snapshot() {
    const auto taken = RuntimeSwitches::Snapshot();
    for (std::size_t index = 0; index < static_cast<std::size_t>(RuntimeSwitch::Count); ++index) {
        const auto id = static_cast<RuntimeSwitch>(index);
        Require(taken.Set(id) == RuntimeSwitches::Set(id), std::string("a switch copy differs from the switch: ") + RuntimeSwitches::Name(id));
    }
    const auto generation = RuntimeSwitches::Generation();
    RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, false);
    const auto flipped = RuntimeSwitches::Snapshot();
    Require(RuntimeSwitches::Generation() == generation + 1, "flipping APS5_LEAN_FRONT did not move the switch generation");
    Require(taken.Set(RuntimeSwitch::LeanFront) && !flipped.Set(RuntimeSwitch::LeanFront) && !(taken == flipped), "a switch copy taken after a flip does not show it");
    RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, true);
    Require(RuntimeSwitches::Snapshot() == taken, "a switch copy taken after flipping back differs from the first one");
    AgcDriver::Graphics::SwitchSnapshot built;
    built.Assign(RuntimeSwitch::LeanFront, true);
    built.Assign(RuntimeSwitch::NoBarrierBatch, true);
    built.Assign(RuntimeSwitch::NoBarrierBatch, false);
    for (std::size_t index = 0; index < static_cast<std::size_t>(RuntimeSwitch::Count); ++index) {
        const auto id = static_cast<RuntimeSwitch>(index);
        Require(built.Set(id) == (id == RuntimeSwitch::LeanFront), std::string("a built switch copy has the wrong bit for ") + RuntimeSwitches::Name(id));
    }
}

void parse() {
    const auto config = AbSchedule::Parse(" 30 : APS5_NO_BARRIER_BATCH ", "815", "3");
    Require(config.phaseSeconds == 30 && config.settleSeconds == 3 && config.startPresent == 815 && !config.startSeconds.has_value(), "the phase settings were not parsed");
    Require(config.switches.size() == 1 && config.switches[0] == RuntimeSwitch::NoBarrierBatch, "the switch list was not parsed");
    const auto bySeconds = AbSchedule::Parse("60:APS5_NO_BARRIER_BATCH", "90s", "");
    const auto lean = AbSchedule::Parse("30:APS5_LEAN_FRONT+APS5_LEAN_LIST_DIRECT", "", "");
    Require(lean.switches.size() == 2 && lean.switches[0] == RuntimeSwitch::LeanFront && lean.switches[1] == RuntimeSwitch::LeanListDirect, "APS5_AB does not accept the lean front switches");
    const auto direct = AbSchedule::Parse("30:APS5_DIRECT_PASS_TARGETS+APS5_DIRECT_BUFFERS+APS5_DIRECT_INPUTS", "", "");
    Require(direct.switches.size() == 3 && direct.switches[0] == RuntimeSwitch::DirectPassTargets && direct.switches[1] == RuntimeSwitch::DirectBuffers && direct.switches[2] == RuntimeSwitch::DirectInputs, "APS5_AB does not accept the direct draw layer switches");
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

void schedule() {
    AbSchedule plan(AbSchedule::Parse("60:APS5_NO_BARRIER_BATCH", "10", "2"));
    using Event = AbSchedule::Event;
    const auto expect = [&](std::uint64_t present, double seconds, Event event, std::uint64_t phase, bool b) {
        const auto step = plan.AtPresent(present, seconds);
        Require(step.event == event && (event == Event::None || (step.phase == phase && step.b == b)), "unexpected step at present " + std::to_string(present));
    };
    expect(1, 0.0, Event::None, 0, false);
    expect(9, 100.0, Event::None, 0, false);
    expect(10, 102.0, Event::Begin, 0, false);
    expect(11, 103.0, Event::None, 0, false);
    expect(12, 104.5, Event::Settled, 0, false);
    expect(13, 150.0, Event::None, 0, false);
    expect(14, 161.9, Event::None, 0, false);
    expect(15, 162.0, Event::Begin, 1, true);
    expect(16, 163.0, Event::None, 1, true);
    expect(17, 300.0, Event::Begin, 2, false);
    expect(18, 301.0, Event::None, 2, false);
    expect(19, 302.5, Event::Settled, 2, false);
    expect(20, 362.5, Event::Begin, 3, true);

    AbSchedule timed(AbSchedule::Parse("30:APS5_NO_BARRIER_BATCH", "90s", ""));
    Require(timed.AtPresent(500, 1000.0).event == Event::None, "a start in seconds began at the first present");
    Require(timed.AtPresent(501, 1089.0).event == Event::None, "a start in seconds began early");
    const auto begin = timed.AtPresent(502, 1090.0);
    Require(begin.event == Event::Begin && begin.phase == 0 && !begin.b, "a start in seconds did not begin phase A");
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
    try {
        SetEnvironment("APS5_NO_BARRIER_BATCH", "1");
        SetEnvironment("APS5_AB", "5:APS5_NO_BARRIER_BATCH");
        SetEnvironment("APS5_AB_START", "3");
        SetEnvironment("APS5_AB_SETTLE", nullptr);
        registry();
        snapshot();
        parse();
        schedule();
        glue();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
    std::printf("runtime switch tests passed\n");
    return 0;
}
