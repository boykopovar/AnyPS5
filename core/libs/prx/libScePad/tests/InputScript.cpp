#include "prx/libScePad/include/InputScript.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Check(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(std::string("input script assertion failed: ") + what);
}

bool Fails(const std::string& text, const std::string& expected) {
    try {
        Pad::InputScript::ParseText(text);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

constexpr std::uint32_t Cross = 0x4000, Circle = 0x2000, L2 = 0x100, R1 = 0x800, Up = 0x10, Left = 0x80, Touchpad = 0x100000;
}

int main() {
    try {
        const auto entries = Pad::InputScript::ParseText(
            "# comment\n\n1000 cross\n2000 circle+r1 300 # trailing\n3000 l2+left 50\r\nstick left 255 0 4000 200\n5000 repeat 100 3 up 20\nrepeat 10 2 touchpad\n");
        Check(entries.size() == 1 + 1 + 1 + 1 + 3 + 2, "entry count");
        Check(entries[0].startMs == 1000 && entries[0].holdMs == 150 && entries[0].buttons == Cross, "default hold");
        Check(entries[1].buttons == (Circle | R1) && entries[1].holdMs == 300, "combo and hold");
        Check(entries[2].buttons == (L2 | Left) && entries[2].label == "l2+left", "crlf line");
        Check(entries[3].stick == 0 && entries[3].axes[0] == 255 && entries[3].axes[1] == 0 && entries[3].startMs == 4000, "stick");
        Check(entries[4].startMs == 5000 && entries[5].startMs == 5100 && entries[6].startMs == 5200 && entries[6].buttons == Up && entries[6].holdMs == 20, "repeat with start");
        Check(entries[7].startMs == 0 && entries[8].startMs == 10 && entries[8].buttons == Touchpad, "repeat without start");

        Check(Fails("1000 cross\n2000 bogus\n", "line 2"), "unknown button names the line");
        Check(Fails("1000 cross\n2000 bogus\n", "bogus"), "unknown button names the token");
        Check(Fails("abc cross\n", "line 1"), "bad start");
        Check(Fails("100 cross 0\n", "hold_ms"), "zero hold");
        Check(Fails("100\n", "expected"), "missing button");
        Check(Fails("100 cross 5 6\n", "expected"), "too many fields");
        Check(Fails("stick middle 1 1 0 10\n", "left or right"), "stick side");
        Check(Fails("stick left 256 1 0 10\n", "0..255"), "stick range");
        Check(Fails("stick left 1 1 0\n", "expected"), "stick fields");
        Check(Fails("0 repeat 10 0 cross\n", "count"), "repeat count");

        Pad::InputScript script;
        script.SetScript(Pad::InputScript::ParseText("100 cross 100\n150 l2 100\nstick right 10 20 400 100\n"));
        Check(script.Sample(1000000).buttons == 0, "nothing before start");
        script.Start(5000000);
        script.Start(9000000);
        Pad::InputScript::Report report;
        Check(script.Sample(5050000, &report).buttons == 0 && report.fired.empty(), "before first event");
        auto frame = script.Sample(5100000, &report);
        Check(frame.buttons == Cross && report.fired.size() == 1, "first event");
        Check(report.fired[0].elapsedMs == 100 && report.fired[0].label == "cross" && report.fired[0].holdMs == 100, "fired record");
        report = {};
        frame = script.Sample(5160000, &report);
        Check(frame.buttons == (Cross | L2) && frame.analogL2 == 255 && frame.analogR2 == 0, "overlap merges and sets trigger");
        Check(report.fired.size() == 1 && report.fired[0].label == "l2", "second event logs once");
        report = {};
        Check(script.Sample(5170000, &report).buttons == (Cross | L2) && report.fired.empty(), "no repeat log");
        frame = script.Sample(5200000, &report);
        Check(frame.buttons == L2, "first released at its end");
        Check(script.Sample(5250000).buttons == 0, "all released");
        frame = script.Sample(5400000, &report);
        Check(frame.hasSticks && frame.sticks[2] == 10 && frame.sticks[3] == 20 && frame.sticks[0] == 128, "stick injected");
        Check(frame.stickMask == 2, "only the right stick is marked as scripted");
        Check(!script.Sample(5500000).hasSticks, "stick released");

        Pad::InputScript looping;
        looping.SetScript(Pad::InputScript::ParseText("0 cross 100\n"));
        looping.SetLoopMs(900);
        looping.Start(0);
        Pad::InputScript::Report first;
        Check(looping.Sample(50000, &first).buttons == Cross && first.fired.size() == 1, "loop first cycle");
        Check(looping.Sample(500000).buttons == 0, "loop idle");
        Pad::InputScript::Report second;
        Check(looping.Sample(1010000, &second).buttons == Cross && second.fired.size() == 1, "loop second cycle");

        const auto path = std::filesystem::temp_directory_path() /
            ("anyps5-pad-script-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::remove(path);
        Pad::InputScript live;
        live.SetLive(path);
        Check(live.Active(), "live is active");
        live.Start(1000000);
        Check(live.Sample(1000000).buttons == 0, "missing live file");
        const auto append = [&](const std::string& text) { std::ofstream(path, std::ios::app | std::ios::binary) << text; };
        append("0 cross\n200 circle 100\n0 nonsense\n");
        Pad::InputScript::Report liveReport;
        Check(live.Sample(1100000, &liveReport).buttons == 0, "poll throttled");
        auto liveFrame = live.Sample(2000000, &liveReport);
        Check(liveReport.errors.size() == 1 && liveReport.errors[0].find("nonsense") != std::string::npos, "live error reported");
        Check(liveReport.errors[0].find("line 3:") != std::string::npos, "live error names its line in the file");
        Check(liveFrame.buttons == Cross && liveReport.fired.size() == 1, "live line scheduled from read time");
        Check(live.Sample(2100000).buttons == Cross, "live hold");
        Check(live.Sample(2160000).buttons == 0, "live hold ends");
        Check(live.Sample(2250000).buttons == Circle, "delayed live event");
        Check(live.Sample(2350000).buttons == 0, "delayed live event ends");
        append("0 r");
        Check(live.Sample(3000000).buttons == 0, "partial line waits");
        append("1 40\n");
        Check(live.Sample(3300000).buttons == R1, "completed line scheduled");
        append("0 bogus\n");
        Pad::InputScript::Report laterReport;
        static_cast<void>(live.Sample(4000000, &laterReport));
        Check(laterReport.errors.size() == 1 && laterReport.errors[0].find("line 5:") != std::string::npos, "a later live error keeps counting lines");
        std::filesystem::remove(path);

        Pad::InputScript capped;
        capped.SetLive(path);
        capped.Start(0);
        append("0 repeat 1 20000 cross 100000\n0 circle\n");
        Pad::InputScript::Report capReport;
        Check(capped.Sample(1000000, &capReport).buttons == Cross, "live events up to the cap are scheduled");
        Check(capReport.errors.size() == 1 && capReport.errors[0].find("live events pending") != std::string::npos && capReport.errors[0].find("line 2:") != std::string::npos, "live events past the cap are refused");
        std::filesystem::remove(path);

        std::filesystem::remove(path);
        {
            std::ofstream(path) << "0 up\n";
        }
        Pad::InputScript preexisting;
        preexisting.SetLive(path);
        preexisting.Start(0);
        Check(preexisting.Sample(1000000).buttons == 0, "lines present at startup are ignored");
        append("0 down 5000\n");
        Check(preexisting.Sample(2000000).buttons == 0x40, "line appended after startup runs");
        append("0 nope\n");
        Pad::InputScript::Report preexistingReport;
        static_cast<void>(preexisting.Sample(3000000, &preexistingReport));
        Check(preexistingReport.errors.size() == 1 && preexistingReport.errors[0].find("line 3:") != std::string::npos, "line numbers count the lines present at startup");
        std::filesystem::remove(path);
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
    return 0;
}
