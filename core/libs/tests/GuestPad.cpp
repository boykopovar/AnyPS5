#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/InputCommandFile.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

extern "C" {
int APS5_VABI scePadOpen_nid_postfix(int, int, int, const void*);
int APS5_VABI scePadOpenExt(int, int, int, const void*);
int APS5_VABI scePadClose_nid_postfix(int);
int APS5_VABI scePadGetHandle(int, int, int);
int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool);
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadRead_nid_postfix(int, PadData*, int);
int APS5_VABI scePadReadState(int, PadData*);
int APS5_VABI scePadSetTiltCorrectionState(int, bool);
int APS5_VABI scePadResetOrientation(int);
int APS5_VABI scePadSetAngularVelocityDeadbandState(int, bool);
int APS5_VABI scePadIsRemoteController(int, bool*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void CheckInputCommands() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-pad-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    const auto write = [&](const char* command) { std::ofstream(path) << command << '\n'; };
    write("1 16384 200");
    Pad::InputCommandFile commands(path.string().c_str());
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
    std::filesystem::remove(path);
    Require(commands.Buttons(1201000) == 0);
    Pad::InputCommandFile disabled(nullptr);
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
    Require(scePadInit_nid_postfix() == 0);
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
    PadPublishInput_nid_postfix(PadInputState{});
}

int main() {
    CheckInputCommands();
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-pad-state-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
#ifdef _WIN32
    Require(_putenv_s("APS5_PAD_INPUT_FILE", path.string().c_str()) == 0);
#else
    Require(setenv("APS5_PAD_INPUT_FILE", path.string().c_str(), 1) == 0);
#endif
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
    const int handle = scePadOpen_nid_postfix(user, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(user, 0, 0) == handle);
    Require(scePadGetHandle(user, 2, 0) == handle);
    CheckTiltCorrection(handle);
    CheckTouchContact();
    CheckReadStateHandle(handle);
    CheckRemoteController(handle);
    CheckInjectedState(path);
    Require(scePadGetHandle(0xff, 16, 0) == handle);
    Require(scePadGetHandle(user, 16, 0) == noHandle);
    Require(scePadGetHandle(user, 0, 1) == noHandle);
    Require(scePadClose_nid_postfix(handle) == 0);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
}
