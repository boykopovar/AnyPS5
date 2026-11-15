#include "SDL_config.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#undef main

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "pad_port0: %s\n", message);
    std::exit(1);
}

void SetPort0State(const char* value) {
#ifdef _WIN32
    _putenv_s("ANYPS5_PAD_PORT0_STATE", value == nullptr ? "" : value);
#else
    if (value == nullptr) unsetenv("ANYPS5_PAD_PORT0_STATE");
    else setenv("ANYPS5_PAD_PORT0_STATE", value, 1);
#endif
}

bool User0LoggedIn() {
    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK, "login list query failed");
    return list.user_id[0] == USER_SERVICE_INITIAL_USER_ID;
}

void Send(PadInput& pad, DisplayWindow& window, Uint32 type, int which) {
    SDL_Event event{};
    event.type = type;
    event.cdevice.which = which;
    pad.HandleEvent(event, window);
}

void SendKey(PadInput& pad, DisplayWindow& window, Uint32 type, SDL_Scancode key) {
    SDL_Event event{};
    event.type = type;
    event.key.keysym.scancode = key;
    pad.HandleEvent(event, window);
}

bool CrossPressed() {
    return (Pad::ReadState(0).buttons & static_cast<std::uint32_t>(Pad::PadButton::Cross)) != 0;
}

void Pump(PadInput& pad, DisplayWindow& window, std::chrono::milliseconds duration) {
    const auto end = std::chrono::steady_clock::now() + duration;
    do {
        pad.Update(window);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    } while (std::chrono::steady_clock::now() < end);
}

template <class Predicate>
std::chrono::milliseconds PumpUntil(PadInput& pad, DisplayWindow& window, std::chrono::milliseconds limit, Predicate done) {
    const auto start = std::chrono::steady_clock::now();
    while (!done()) {
        Require(std::chrono::steady_clock::now() - start < limit, "the pad state did not change in time");
        pad.Update(window);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
}

void CheckDefaultKeepsPort0(DisplayWindow& window, int index, SDL_JoystickID id) {
    SetPort0State(nullptr);
    PadInput pad;
    Require(pad.SlotOccupied(0), "the attached controller is bound to slot 0");
    Require(Pad::IsConnected(0), "default port 0 is connected with a controller");
    Send(pad, window, SDL_CONTROLLERDEVICEREMOVED, id);
    Require(!pad.SlotOccupied(0), "the removal unbinds slot 0");
    Pump(pad, window, std::chrono::milliseconds{800});
    Require(Pad::IsConnected(0), "default port 0 stays connected after its controller is removed");
    Require(User0LoggedIn(), "default user 0 stays logged in after its controller is removed");
    Require(Pad::ReadState(0).connected, "default port 0 reads as connected after its controller is removed");
    Send(pad, window, SDL_CONTROLLERDEVICEADDED, index);
    Require(pad.SlotOccupied(0), "the controller is bound to slot 0 again");
}

void CheckControllerModeFollowsTheController(DisplayWindow& window, int index, SDL_JoystickID id) {
    SetPort0State("controller");
    PadInput pad;
    Require(pad.SlotOccupied(0), "the attached controller is bound to slot 0 in controller mode");
    Require(Pad::IsConnected(0) && User0LoggedIn(), "controller mode starts with the controller present");

    Send(pad, window, SDL_CONTROLLERDEVICEREMOVED, id);
    Require(Pad::IsConnected(0), "a removal is not committed at once");
    const auto removed = PumpUntil(pad, window, std::chrono::seconds{5}, [] { return !Pad::IsConnected(0); });
    Require(removed >= std::chrono::milliseconds{450}, "the removal is committed only after the hold");
    Require(!User0LoggedIn(), "user 0 logs out with its controller");
    const PadData gone = Pad::ReadState(0);
    Require(!gone.connected && gone.buttons == 0 && gone.left_stick_x == 0 && gone.left_stick_y == 0, "a withdrawn port 0 reads zeroed");

    Send(pad, window, SDL_CONTROLLERDEVICEADDED, index);
    Require(pad.SlotOccupied(0), "the controller is bound to slot 0 again");
    PumpUntil(pad, window, std::chrono::seconds{5}, [] { return Pad::IsConnected(0); });
    Require(User0LoggedIn(), "user 0 logs in with its controller");

    Send(pad, window, SDL_CONTROLLERDEVICEREMOVED, id);
    Pump(pad, window, std::chrono::milliseconds{100});
    Send(pad, window, SDL_CONTROLLERDEVICEADDED, index);
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds{800};
    do {
        pad.Update(window);
        Require(Pad::IsConnected(0), "a controller that returns inside the hold does not flap port 0");
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    } while (std::chrono::steady_clock::now() < end);
    Require(User0LoggedIn(), "user 0 stays logged in across a brief removal");
}

void CheckKeyboardStaysOutOfWithdrawnPort0(DisplayWindow& window, int index, SDL_JoystickID id) {
    SetPort0State("controller");
    PadInput pad;
    Require(Pad::IsConnected(0), "controller mode starts with the controller present");
    SendKey(pad, window, SDL_KEYDOWN, SDL_SCANCODE_SPACE);
    Require(CrossPressed(), "a bound key reaches port 0 while the controller is present");
    SendKey(pad, window, SDL_KEYUP, SDL_SCANCODE_SPACE);
    Require(!CrossPressed(), "releasing the key clears it on port 0");

    Send(pad, window, SDL_CONTROLLERDEVICEREMOVED, id);
    PumpUntil(pad, window, std::chrono::seconds{5}, [] { return !Pad::IsConnected(0); });
    Send(pad, window, SDL_CONTROLLERDEVICEADDED, index);
    Pump(pad, window, std::chrono::milliseconds{460});
    SendKey(pad, window, SDL_KEYDOWN, SDL_SCANCODE_SPACE);
    SendKey(pad, window, SDL_KEYUP, SDL_SCANCODE_SPACE);
    PumpUntil(pad, window, std::chrono::seconds{5}, [] { return Pad::IsConnected(0); });
    Require(!CrossPressed(), "a key pressed while port 0 was withdrawn is not given to it when the controller returns");
}

}

int main() {
    Require(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0, "SDL init failed");
    const int hostJoysticks = SDL_NumJoysticks();
    if (hostJoysticks < 0) {
        std::fprintf(stderr, "pad_port0: SDL_NumJoysticks failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
        return 1;
    }
    if (hostJoysticks > 0) {
        std::fprintf(stderr, "pad_port0: skipped, SDL sees %d host joysticks, which take the pad slots this test checks\n", hostJoysticks);
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
        return 77;
    }

    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = 6;
    desc.nbuttons = 16;
    desc.name = "Virtual Controller 0";
    const int device = SDL_JoystickAttachVirtualEx(&desc);
    Require(device >= 0 && SDL_IsGameController(device), "virtual controller attach failed");
    const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK, "user service init failed");
    Pad::Initialize();
    DisplayWindow window;
    CheckDefaultKeepsPort0(window, device, id);
    CheckControllerModeFollowsTheController(window, device, id);
    CheckKeyboardStaysOutOfWithdrawnPort0(window, device, id);

    SetPort0State(nullptr);
    Require(SDL_JoystickDetachVirtual(device) == 0, "virtual controller detach failed");
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    return 0;
}
