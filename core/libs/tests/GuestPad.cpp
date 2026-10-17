#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/InputCommandFile.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

extern "C" {
int APS5_VABI scePadOpen_nid_postfix(int, int, int, const void*);
int APS5_VABI scePadOpenExt(int, int, int, const void*);
int APS5_VABI scePadClose_nid_postfix(int);
int APS5_VABI scePadGetHandle(int, int, int);
int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool);
int APS5_VABI scePadSetVibrationMode(int, int);
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadUnknown_n3kSX62fgNo();
int APS5_VABI scePadRead_nid_postfix(int, PadData*, int);
int APS5_VABI scePadReadState(int, PadData*);
int APS5_VABI scePadSetTiltCorrectionState(int, bool);
int APS5_VABI scePadResetOrientation(int);
int APS5_VABI scePadSetAngularVelocityDeadbandState(int, bool);
int APS5_VABI scePadSetAngularVelocityBiasCorrectionState(int, bool);
int APS5_VABI scePadIsRemoteController(int, bool*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void RequireUnknownPadNidNotImplemented() {
    try {
        scePadUnknown_n3kSX62fgNo();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "scePadUnknown_n3kSX62fgNo not implemented");
        return;
    }
    std::abort();
}

static void CheckInputCommands() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-pad-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    const auto write = [&](const char* command) { std::ofstream(path) << command << '\n'; };
    write("1 16384 200");
    Pad::InputCommandFile commands(path);
    Require(commands.Buttons(1000) == 0);
    write("2 16384 200");
    Require(commands.Buttons(101000) == 0x4000);
    Require(commands.Buttons(300999) == 0x4000);
    Require(commands.Buttons(301000) == 0);
    Require(commands.Buttons(401000) == 0);
    write("3 8192 5001");
    Require(commands.Buttons(501000) == 0);
    write("-3 8192 300");
    Require(commands.Buttons(601000) == 0);
    write("3 1 300");
    Require(commands.Buttons(701000) == 0);
    write("3 8192 300 trailing");
    Require(commands.Buttons(801000) == 0);
    write("3 8192 300");
    Require(commands.Buttons(901000) == 0x2000);
    write("4 0 1");
    Require(commands.Buttons(1001000) == 0);
    write("2 16384 200");
    Require(commands.Buttons(1101000) == 0);
    write("5 0 300 0 128 255 64");
    const auto tilted = commands.Poll(1201000);
    Require(tilted.buttons == 0 && tilted.hasSticks);
    Require(tilted.sticks[0] == 0 && tilted.sticks[1] == 128 && tilted.sticks[2] == 255 && tilted.sticks[3] == 64);
    Require(!commands.Poll(1501000).hasSticks);
    write("6 16384 300");
    const auto pressed = commands.Poll(1601000);
    Require(pressed.buttons == 0x4000 && !pressed.hasSticks);
    const Pad::InputCommandFile::Input none;
    write("7 16384 300 128 128");
    Require(commands.Poll(1901000) == none);
    write("7 16384 300 128 128 128 256");
    Require(commands.Poll(2001000) == none);
    write("7 16384 300 128 128 128 128 128");
    Require(commands.Poll(2101000) == none);
    write("7 16384 300 128 128 128 up");
    Require(commands.Poll(2201000) == none);
    write("7 16384 300 +");
    Require(commands.Poll(2301000) == none);
    write("7 16384 300 4294967296");
    Require(commands.Poll(2401000) == none);
    write("7 16384 300 128 128 128 128x");
    Require(commands.Poll(2501000) == none);
    write("7 8192 300 1 2 3 4");
    const auto accepted = commands.Poll(2601000);
    Require(accepted.buttons == 0x2000 && accepted.hasSticks && accepted.sticks[3] == 4);
    write("8 2097152 300");
    const auto left = commands.Poll(3001000);
    Require(left.buttons == 0 && left.touchLeft && !left.touchRight && !left.hasSticks);
    write("9 4210688 300");
    const auto right = commands.Poll(3401000);
    Require(right.buttons == 0x4000 && !right.touchLeft && right.touchRight);
    write("10 8388608 300");
    Require(commands.Poll(3801000) == none);
    std::filesystem::remove(path);
    Require(commands.Buttons(4201000) == 0);
    Pad::InputCommandFile disabled(std::filesystem::path{});
    Require(disabled.Buttons(0) == 0);
}

static float SettleOrientationW() {
    float w = 1.0f;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        w = Pad::ReadState().orientation_w;
    }
    return w;
}

static void CheckTiltCorrection(int handle) {
    RequireUnknownPadNidNotImplemented();
    PadInputState tilted;
    tilted.hasMotion = true;
    tilted.accel = {9.80665f, 0.0f, 0.0f};
    PadPublishInput_nid_postfix(tilted);
    Require(scePadSetTiltCorrectionState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);
    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(SettleOrientationW() == 1.0f);
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    Require(SettleOrientationW() < 0.999f);
    PadPublishInput_nid_postfix(PadInputState{});
}

static void CheckAngularVelocityDeadband(int handle) {
    PadInputState moving;
    moving.hasMotion = true;
    moving.accel = {0.0f, 9.80665f, 0.0f};
    moving.gyro = {0.01f, 0.0175f, -0.017f};
    PadPublishInput_nid_postfix(moving);
    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.01f && data.angular_velocity_y == 0.0175f && data.angular_velocity_z == -0.017f);
    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f && data.angular_velocity_y == 0.0175f && data.angular_velocity_z == 0.0f);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.01f && data.angular_velocity_z == -0.017f);
    PadPublishInput_nid_postfix(PadInputState{});
}

static void CheckTouchContact() {
    PadInputState touch;
    touch.buttons = static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
    touch.touch[0] = {true, 960, 471, 0};
    PadPublishInput_nid_postfix(touch);
    const auto data = Pad::ReadState();
    Require((data.buttons & static_cast<std::uint32_t>(Pad::PadButton::TouchPad)) != 0);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 960);
    Require(data.touch_data_touch0_y == 471);
    PadPublishInput_nid_postfix(PadInputState{});
    Pad::ReadState();
}

static void CheckPartialTrigger() {
    PadInputState partial;
    partial.buttons = static_cast<std::uint32_t>(Pad::PadButton::L2) | static_cast<std::uint32_t>(Pad::PadButton::R2);
    partial.analogButtonsL2 = 1;
    partial.analogButtonsR2 = 73;
    PadPublishInput_nid_postfix(partial);
    const auto data = Pad::ReadState();
    Require(data.buttons == partial.buttons);
    Require(data.analog_buttons_l2 == 1 && data.analog_buttons_r2 == 73);
    PadInputState digital;
    digital.buttons = static_cast<std::uint32_t>(Pad::PadButton::L2);
    PadPublishInput_nid_postfix(digital);
    Require(Pad::ReadState().analog_buttons_l2 == 255);
    PadPublishInput_nid_postfix(PadInputState{});
    Pad::ReadState();
}

static void CheckReadQueue(int handle) {
    PadData drained[64]{};
    scePadRead_nid_postfix(handle, drained, 64);
    PadInputState pressed;
    pressed.buttons = static_cast<std::uint32_t>(Pad::PadButton::Cross);
    PadPublishInput_nid_postfix(pressed);
    PadPublishInput_nid_postfix(PadInputState{});
    PadData data[4]{};
    Require(scePadRead_nid_postfix(handle, data, 4) == 2);
    Require((data[0].buttons & pressed.buttons) != 0);
    Require((data[1].buttons & pressed.buttons) == 0);
    Require(data[0].timestamp <= data[1].timestamp);
    Require(scePadRead_nid_postfix(handle, data, 4) == 1);
    Require(data[0].buttons == 0);
}

static void CheckReadStateHandle(int handle) {
    PadData data{};
    Require(scePadReadState(0, &data) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle + 1, &data) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(0, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle, &data) == PAD_OK);
}

static void CheckRemoteController(int handle) {
    bool remote = true;
    Require(scePadIsRemoteController(handle + 1, &remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsRemoteController(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(remote);
    Require(scePadIsRemoteController(handle, &remote) == PAD_OK);
    Require(!remote);
}

static void CheckInjectedState(const std::filesystem::path& path) {
    PadInputState physical;
    physical.buttons = 0x2000;
    physical.analogButtonsR2 = 73;
    physical.sticks[0] = 42;
    PadPublishInput_nid_postfix(physical);
    const auto before = Pad::ReadState();
    std::ofstream(path) << "1 16640 300\n";
    PadData pressed{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        pressed = Pad::ReadState();
        if ((pressed.buttons & 0x4000) != 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    Require(pressed.buttons == (0x2000 | 0x4000 | 0x100));
    Require(pressed.analog_buttons_l2 == 255 && pressed.analog_buttons_r2 == 73);
    Require(pressed.left_stick_x == 42 && pressed.timestamp > before.timestamp);
    std::this_thread::sleep_for(std::chrono::milliseconds(310));
    const auto released = Pad::ReadState();
    Require(released.buttons == 0x2000 && released.analog_buttons_l2 == 0);
    Require(released.timestamp > pressed.timestamp);
    physical.sticks[3] = 17;
    PadPublishInput_nid_postfix(physical);
    const auto still = Pad::ReadState();
    std::ofstream(path) << "2 0 300 200 0 128 128\n";
    PadData moved{};
    const auto stickDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        moved = Pad::ReadState();
        if (moved.left_stick_x == 200) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < stickDeadline);
    Require(moved.left_stick_x == 200 && moved.left_stick_y == 0);
    Require(moved.right_stick_x == 128 && moved.right_stick_y == 128);
    Require(moved.buttons == 0x2000 && moved.timestamp > still.timestamp);
    std::this_thread::sleep_for(std::chrono::milliseconds(310));
    const auto centred = Pad::ReadState();
    Require(centred.left_stick_x == 42 && centred.right_stick_y == 17);
    Require(centred.timestamp > moved.timestamp);
    for (const auto& [command, x] : {std::pair{"3 2097152 300\n", 480}, std::pair{"4 4194304 300\n", 1440}}) {
        std::ofstream(path) << command;
        PadData touched{};
        const auto touchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            touched = Pad::ReadState();
            if (touched.touch_data_touch_num != 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < touchDeadline);
        Require(touched.touch_data_touch_num == 1 && touched.touch_data_touch0_x == x && touched.touch_data_touch0_y == 471);
        Require(touched.buttons == (0x2000 | 0x100000));
        std::this_thread::sleep_for(std::chrono::milliseconds(310));
        const auto lifted = Pad::ReadState();
        Require(lifted.touch_data_touch_num == 0 && lifted.buttons == 0x2000);
    }
    PadPublishInput_nid_postfix(PadInputState{});
}

int main() {
    CheckInputCommands();
    Require(scePadUnknown_n3kSX62fgNo() == static_cast<int>(0x80920005));
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-pad-state-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    };
    auto commandPath = path;
    commandPath += std::filesystem::path(u8"-žluťoučký-漢字");
    Cleanup cleanup{commandPath};
#ifdef _WIN32
    Require(_wputenv_s(L"APS5_PAD_INPUT_FILE", commandPath.c_str()) == 0);
#else
    Require(setenv("APS5_PAD_INPUT_FILE", commandPath.string().c_str(), 1) == 0);
#endif
    {
        std::uint32_t sequence = 0;
        PadOutputState snapshot;
        Pad::SetVibration(12, 34);
        PadSubmitAudioHaptics_nid_postfix(100, 150);
        Require(PadFetchOutput_nid_postfix(&sequence, &snapshot));
        Require(snapshot.audioVibrationLarge == 100 && snapshot.audioVibrationSmall == 150);
        Require(snapshot.vibrationLarge == 12 && snapshot.vibrationSmall == 34);
        PadQueueLegacyAudioHaptics_nid_postfix(1, 0, ~std::uint64_t{0}, 200, 20);
        Require(PadFetchOutput_nid_postfix(&sequence, &snapshot));
        Require(snapshot.audioVibrationLarge == 200 && snapshot.audioVibrationSmall == 150);
        PadClearLegacyAudioHaptics_nid_postfix(1);
        Require(PadFetchOutput_nid_postfix(&sequence, &snapshot));
        Require(snapshot.audioVibrationLarge == 100 && snapshot.audioVibrationSmall == 150);
        Require(snapshot.vibrationLarge == 12 && snapshot.vibrationSmall == 34);
        PadSubmitAudioHaptics_nid_postfix(0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Require(PadFetchOutput_nid_postfix(&sequence, &snapshot));
        Require(snapshot.audioVibrationLarge == 0 && snapshot.audioVibrationSmall == 0);
        Require(snapshot.vibrationLarge == 12 && snapshot.vibrationSmall == 34);
        Pad::SetVibration(0, 0);
    }
    constexpr int noHandle = static_cast<int>(0x80920008);
    constexpr int user = 0x10000000;

    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadOpen_nid_postfix(user, 1, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(user, 0, 1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    const unsigned char wheel[16]{0xb7, 0x0e, 0x04, 0x0e, 0x08, 0x6e, 0x01};
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 0, wheel) == static_cast<int>(0x80920007));
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 1, wheel) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadOpen_nid_postfix(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == static_cast<int>(0x80920007));
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadInit_nid_postfix() == 0);
    Require(scePadUnknown_n3kSX62fgNo() == static_cast<int>(0x809200ff));
    const int handle = scePadOpen_nid_postfix(user, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(user, 0, 0) == handle);
    Require(scePadOpen_nid_postfix(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == static_cast<int>(0x80920007));
    Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == noHandle);
    Require(scePadGetHandle(user, 0, 0) == handle);
    CheckTiltCorrection(handle);
    CheckAngularVelocityDeadband(handle);
    CheckTouchContact();
    CheckPartialTrigger();
    CheckReadQueue(handle);
    CheckReadStateHandle(handle);
    CheckRemoteController(handle);
    CheckInjectedState(commandPath);
    Require(scePadSetVibrationMode(handle, 1) == 0);
    Require(scePadSetVibrationMode(handle, 2) == 0);
    Require(scePadSetVibrationMode(handle, 3) == 0);
    Require(scePadSetVibrationMode(handle, 4) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(handle, 5) == 0);
    Require(scePadSetVibrationMode(handle, 6) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(handle, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(0xff, 16, 0) == handle);
    Require(scePadGetHandle(user, 16, 0) == noHandle);
    Require(scePadGetHandle(user, 0, 1) == noHandle);
    Require(scePadClose_nid_postfix(handle) == 0);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(handle + 1, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_OK);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
}
