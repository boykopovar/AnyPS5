#include "SDL_config.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#undef main

namespace {

using Clock = std::chrono::steady_clock;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "pad_identify_wiring: %s\n", message);
    std::exit(1);
}

void SetIdentifyMode(const char* value) {
#ifdef _WIN32
    _putenv_s("ANYPS5_PAD_IDENTIFY", value);
#else
    setenv("ANYPS5_PAD_IDENTIFY", value, 1);
#endif
}

std::vector<Clock::time_point> pulseEdges;
std::vector<Clock::time_point> titleEdges;
bool motorOn = false;

int RecordRumble(void*, Uint16 low, Uint16 high) {
    const bool on = low != 0 || high != 0;
    if (on && !motorOn) {
        if (low == 65535 && high == 65535) pulseEdges.push_back(Clock::now());
        else titleEdges.push_back(Clock::now());
    }
    motorOn = on;
    return 0;
}

void Pump(PadInput& padInput, const DisplayWindow& window, int milliseconds) {
    const auto end = Clock::now() + std::chrono::milliseconds(milliseconds);
    while (Clock::now() < end) {
        padInput.Update(window);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int Run(const char* mode) {
    Require(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0, "SDL initializes the pad subsystems");
    if (SDL_NumJoysticks() > 0) {
        std::fputs("pad_identify_wiring: skipped, host joysticks take the pad slots\n", stderr);
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
        return 77;
    }
    Pad::Initialize();

    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = 6;
    desc.nbuttons = 16;
    desc.name = "Identify Virtual Controller";
    desc.Rumble = RecordRumble;
    const int device = SDL_JoystickAttachVirtualEx(&desc);
    Require(device >= 0 && SDL_IsGameController(device), "a virtual controller with a rumble callback attaches");

    if (std::strcmp(mode, "invalid") == 0) {
        SetIdentifyMode("bogus");
        try {
            PadInput refused;
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            Require(message.find("ANYPS5_PAD_IDENTIFY") != std::string::npos && message.find("bogus") != std::string::npos,
                "an invalid mode stops the start with an error naming the variable and the value");
            SDL_JoystickDetachVirtual(device);
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
            return 0;
        }
        Require(false, "an invalid mode must not start the pad input");
    }

    if (std::strcmp(mode, "off") == 0) SetIdentifyMode("off");
    else if (std::strcmp(mode, "connect") == 0 || std::strcmp(mode, "rumble") == 0) SetIdentifyMode("connect");
    else return 2;

    if (std::strcmp(mode, "rumble") == 0) Pad::SetVibration(0, 200, 200);
    const auto bound = Clock::now();
    DisplayWindow window;
    PadInput padInput;
    Require(padInput.SlotOccupied(0), "the virtual controller takes slot 0");

    if (std::strcmp(mode, "rumble") == 0) {
        Pump(padInput, window, 1500);
        Require(pulseEdges.empty(), "no identify pulse is sent while the title is rumbling");
        Require(!titleEdges.empty(), "the title's own rumble reaches the controller");
        Pad::SetVibration(0, 0, 0);
        Pump(padInput, window, 1500);
        Require(pulseEdges.size() == 1, "after the title stops, one pulse for player 1 is sent");
        Require(pulseEdges[0] - bound >= std::chrono::milliseconds(1500), "the pulse waits for the title to stay quiet");
    } else {
        Pump(padInput, window, 3000);
        if (std::strcmp(mode, "off") == 0) {
            Require(pulseEdges.empty() && titleEdges.empty(), "off sends no pulse");
        } else {
            Require(pulseEdges.size() == 1, "connect sends exactly one pulse for player 1");
            Require(pulseEdges[0] - bound >= std::chrono::milliseconds(450) && pulseEdges[0] - bound <= std::chrono::milliseconds(1500),
                "the pulse follows the 500 ms quiet window");
        }
    }

    Require(SDL_JoystickDetachVirtual(device) == 0, "the virtual controller detaches");
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    return 0;
}

}

int main(int argc, char** argv) {
    return Run(argc > 1 ? argv[1] : "");
}
