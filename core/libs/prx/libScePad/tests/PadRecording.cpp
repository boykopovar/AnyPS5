#include "prx/libScePad/include/PadRecording.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

void Check(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}

template <typename Action>
bool Refused(Action action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-pad-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Pad::RecordedState idle;
        Pad::RecordedState cross;
        cross.buttons = 0x4000;
        Pad::RecordedState walk;
        walk.sticks = {255, 128, 128, 128};
        walk.triggers = {0, 200};
        {
            Pad::RecordingWriter writer(path);
            Check(writer.Active(), "the recording file was not created");
            writer.Record(5000000, idle);
            writer.Record(5100000, idle);
            writer.Record(5250000, cross);
            writer.Record(5400000, walk);
            writer.Record(5400500, walk);
            writer.Record(6000000, idle);
        }
        std::stringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        Check(text.str() == "APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0\n250 4000 128 128 128 128 0 0\n400 0 255 128 128 128 0 200\n1000 0 128 128 128 128 0 0\n",
            "the recording does not list each state change once, in milliseconds from the first read");

        Pad::RecordingPlayer player(path);
        Check(player.Active(), "the recording was not loaded");
        Check(player.At(20000000) == idle, "the replay did not start with the first recorded state");
        Check(player.At(20249000) == idle, "a state was replayed before its time");
        Check(player.At(20250000) == cross, "a state was not replayed at its time from the first read");
        Check(player.At(20399999) == cross, "a state ended before the next change");
        Check(player.At(20400000) == walk, "the sticks and triggers were not replayed");
        Check(player.At(21000000) == idle, "the last change was not replayed");
        Check(player.At(90000000) == idle, "the last recorded state did not hold after the recording");

        Check(!Pad::ParseRecording("0 0 128 128 128 128 0 0\n"), "a recording without its header was accepted");
        Check(!Pad::ParseRecording("APS5_PAD_RECORDING 2\n0 0 128 128 128 128 0 0\n"), "a recording of another version was accepted");
        for (const char* row : {"0a 128 128 128 128 0 0", "12F 128 128 128 128 0 0", "0ff 128 128 128 128 0 0"}) {
            Check(!Pad::ParseRecording(std::string(Pad::RecordingHeader) + "\n" + row + "\n"), "a timestamp and buttons without a separator were accepted");
        }
        const auto spaced = Pad::ParseRecording("APS5_PAD_RECORDING 1\n  0   4000  128 128 128 128  0 0\n");
        Check(spaced && spaced->size() == 1 && spaced->front().ms == 0 && spaced->front().state == cross, "space-separated fields were not read");
        Check(!Pad::ParseRecording("APS5_PAD_RECORDING 1\n5 0 128 128 128 128 0\n"), "a line with a missing field was accepted");
        Check(!Pad::ParseRecording("APS5_PAD_RECORDING 1\n5 0 128 128 128 300 0 0\n"), "a stick position past 255 was accepted");
        Check(!Pad::ParseRecording("APS5_PAD_RECORDING 1\n5 0 128 128 128 128 0 0 7\n"), "a line with an extra field was accepted");
        Check(!Pad::ParseRecording("APS5_PAD_RECORDING 1\n9 0 128 128 128 128 0 0\n5 0 128 128 128 128 0 0\n"), "states out of time order were accepted");
        const auto crlf = Pad::ParseRecording("APS5_PAD_RECORDING 1\r\n0 4000 128 128 128 128 0 0\r\n");
        Check(crlf && crlf->size() == 1 && crlf->front().state == cross, "a recording with CRLF line ends was not read");
        Check(!Pad::RecordingPlayer(std::filesystem::path{}).Active() && !Pad::RecordingWriter(std::filesystem::path{}).Active(), "an empty path turned recording or replay on");
        Check(Refused([&] { Pad::RecordingPlayer missing(path.string() + ".missing"); }), "a missing recording did not stop the replay");
        Check(Refused([&] { Pad::RecordingWriter unwritable(path / "missing-directory" / "recording"); }), "an unwritable recording path did not stop the recording");
        std::ofstream(path, std::ios::binary | std::ios::trunc) << "0 4000 128 128 128 128 0 0\n";
        Check(Refused([&] { Pad::RecordingPlayer headerless(path); }), "a recording without its header was replayed");
        std::ofstream(path, std::ios::binary | std::ios::trunc) << "APS5_PAD_RECORDING 1\n0a 128 128 128 128 0 0\n";
        Check(Refused([&] { Pad::RecordingPlayer joined(path); }), "a recording with joined fields was replayed");
        std::ofstream(path, std::ios::binary | std::ios::trunc) << "APS5_PAD_RECORDING 1\n";
        Pad::RecordingPlayer empty(path);
        Check(empty.Active() && empty.At(1000) == idle && empty.At(9000000) == idle, "an empty recording did not replay the idle state");

        Pad::RecordingPlayer backwardPlayer(std::vector<Pad::RecordedSample>{{0, idle}, {250, cross}, {400, walk}});
        Check(backwardPlayer.At(20000000) == idle, "the backward-timestamp replay did not start at 0 ms");
        Check(backwardPlayer.At(19000000) == idle, "a timestamp before replay start did not clamp to 0 ms");
        Check(backwardPlayer.At(20000000) == idle, "replay at exact start timestamp did not return the 0 ms state");
        Check(backwardPlayer.At(20400000) == walk, "replay did not advance normally after clamping a backward timestamp");

        {
            Pad::RecordingWriter backwardWriter(path);
            Check(backwardWriter.Active(), "the backward-timestamp recording file was not created");
            backwardWriter.Record(5000000, idle);
            backwardWriter.Record(4000000, cross);
            backwardWriter.Record(5500000, walk);
            backwardWriter.Record(5200000, cross);
        }
        std::stringstream backwardText;
        backwardText << std::ifstream(path, std::ios::binary).rdbuf();
        const auto clamped = Pad::ParseRecording(backwardText.str());
        Check(clamped && clamped->size() == 4, "backward timestamps produced an unparseable or incomplete recording");
        Check((*clamped)[0].ms == 0 && (*clamped)[0].state == idle, "initial sample timestamp or state was incorrect");
        Check((*clamped)[1].ms == 0 && (*clamped)[1].state == cross, "timestamp before startUs did not clamp to 0 ms");
        Check((*clamped)[2].ms == 500 && (*clamped)[2].state == walk, "forward sample timestamp or state was incorrect");
        Check((*clamped)[3].ms == 500 && (*clamped)[3].state == cross, "regressing timestamp was not clamped to previous sample timestamp");

        std::filesystem::remove(path);
        std::cout << "pad recording tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
