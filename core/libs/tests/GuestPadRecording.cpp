#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

namespace {

std::filesystem::path directory;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_recording: %s\n", what);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::exit(1);
}

void SetPath(const char* name, const std::filesystem::path& path) {
#ifdef _WIN32
    const std::wstring wide(name, name + std::strlen(name));
    _wputenv_s(wide.c_str(), path.wstring().c_str());
#else
    setenv(name, path.string().c_str(), 1);
#endif
}

bool ReadRefused() {
    try {
        Pad::ReadState(0);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool Replayed(const PadData& data) {
    return data.buttons == 0x4000 && data.left_stick_x == 255 && data.left_stick_y == 0 && data.right_stick_x == 128 && data.right_stick_y == 128 &&
        data.analog_buttons_l2 == 0 && data.analog_buttons_r2 == 200;
}

bool Idle(const PadData& data) {
    return data.buttons == 0 && data.left_stick_x == 128 && data.left_stick_y == 128 && data.right_stick_x == 128 && data.right_stick_y == 128 &&
        data.analog_buttons_l2 == 0 && data.analog_buttons_r2 == 0;
}

}

int main() {
    directory = std::filesystem::temp_directory_path() /
        ("anyps5-guest-pad-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto replay = directory / "replay.aps5pad";
    const auto record = directory / "record.aps5pad";
    std::ofstream(replay, std::ios::binary) << "APS5_PAD_RECORDING 1\n0 4000 255 0 128 128 0 200\n150 0 128 128 128 128 0 0\n";

    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit failed");
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0, "scePadOpen failed");
    PadInputState live;
    live.buttons = 0x1000;
    live.sticks = {10, 20, 30, 40};
    live.analogButtonsL2 = 90;
    PadPublishInput_nid_postfix(live);

    SetPath("APS5_PAD_REPLAY", directory / "missing.aps5pad");
    Require(ReadRefused(), "a missing APS5_PAD_REPLAY file did not stop the controller read");
    SetPath("APS5_PAD_REPLAY", replay);
    SetPath("APS5_PAD_RECORD", directory / "missing-directory" / "record.aps5pad");
    Require(ReadRefused(), "an unwritable APS5_PAD_RECORD path did not stop the controller read");
    SetPath("APS5_PAD_RECORD", record);

    PadInputState pressed = live;
    pressed.buttons = 0x2000;
    PadPublishInput_nid_postfix(pressed);
    PadData state{};
    Require(scePadReadState(handle, &state) == PAD_OK, "scePadReadState failed");
    Require(Replayed(state), "scePadReadState returned the live controller instead of the replayed state");
    PadPublishInput_nid_postfix(live);
    PadData queued[8]{};
    const int count = Pad::Read(queued, 8, 0);
    Require(count >= 1, "scePadRead returned no state");
    for (int i = 0; i < count; ++i) Require(Replayed(queued[i]), "a queued scePadRead state was not replaced by the replay");
    PadData empty[8]{};
    Require(Pad::Read(empty, 8, 0) == 1 && Replayed(empty[0]), "scePadRead without queued input returned the live controller");
    Require(Pad::ReadState(1).buttons != 0x4000, "the replay of port 0 reached port 1");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Require(Idle(Pad::ReadState(0)), "the replay did not move to the change recorded at 150 ms");
    Require(scePadClose_nid_postfix(handle) == PAD_OK, "scePadClose failed");

    std::stringstream text;
    text << std::ifstream(record, std::ios::binary).rdbuf();
    std::string header, first, second, extra;
    std::getline(text, header);
    std::getline(text, first);
    std::getline(text, second);
    Require(header == "APS5_PAD_RECORDING 1", "the recording has no header");
    Require(first == "0 4000 255 0 128 128 0 200", "the recording does not start with the state the title read first");
    const auto space = second.find(' ');
    Require(space != std::string::npos && std::stoull(second.substr(0, space)) >= 150 && second.substr(space) == " 0 128 128 128 128 0 0",
        "the recording does not hold the later change once, after its time");
    Require(!std::getline(text, extra), "the recording repeats a state the title read again");

    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("guest pad recording tests passed");
    return 0;
}
