#include "SDL_config.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdio>
#include <cstdlib>
#undef main

namespace {

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
#ifdef SDL_JOYSTICK_HIDAPI
    std::puts("pad_sdl: SDL drives controllers through HIDAPI");
#else
    std::fputs("pad_sdl: SDL is built without HIDAPI, so a DualSense gets no light bar, touchpad or trigger effects\n", stderr);
    return 1;
#endif

    Require(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0);

    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = 6;
    desc.nbuttons = 16;
    desc.name = "Virtual Controller 0";
    const int dev0 = SDL_JoystickAttachVirtualEx(&desc);
    Require(dev0 >= 0);
    Require(SDL_IsGameController(dev0));

    DisplayWindow window;
    PadInput padInput;
    Require(padInput.SlotOccupied(0));
    Require(!padInput.SlotOccupied(1));

    SDL_Event add0{};
    add0.type = SDL_CONTROLLERDEVICEADDED;
    add0.cdevice.which = dev0;
    padInput.HandleEvent(add0, window);
    Require(padInput.SlotOccupied(0));
    Require(!padInput.SlotOccupied(1));

    desc.name = "Virtual Controller 1";
    const int dev1 = SDL_JoystickAttachVirtualEx(&desc);
    Require(dev1 >= 0);
    Require(SDL_IsGameController(dev1));

    SDL_Event add1{};
    add1.type = SDL_CONTROLLERDEVICEADDED;
    add1.cdevice.which = dev1;
    padInput.HandleEvent(add1, window);
    Require(padInput.SlotOccupied(0));
    Require(padInput.SlotOccupied(1));
    Require(!padInput.SlotOccupied(2));
    Require(Pad::IsConnected(1));

    const SDL_JoystickID id0 = SDL_JoystickGetDeviceInstanceID(dev0);
    SDL_Event rem0{};
    rem0.type = SDL_CONTROLLERDEVICEREMOVED;
    rem0.cdevice.which = id0;
    padInput.HandleEvent(rem0, window);
    Require(!padInput.SlotOccupied(0));
    Require(padInput.SlotOccupied(1));
    Require(Pad::IsConnected(1));

    padInput.HandleEvent(add0, window);
    Require(padInput.SlotOccupied(0));
    Require(padInput.SlotOccupied(1));

    const SDL_JoystickID id1 = SDL_JoystickGetDeviceInstanceID(dev1);
    SDL_Event rem1{};
    rem1.type = SDL_CONTROLLERDEVICEREMOVED;
    rem1.cdevice.which = id1;
    padInput.HandleEvent(rem1, window);
    Require(padInput.SlotOccupied(0));
    Require(!padInput.SlotOccupied(1));
    Require(!Pad::IsConnected(1));

    padInput.HandleEvent(rem0, window);
    Require(!padInput.SlotOccupied(0));
    Require(!padInput.SlotOccupied(1));

    Require(SDL_JoystickDetachVirtual(dev1) == 0);
    Require(SDL_JoystickDetachVirtual(dev0) == 0);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    return 0;
}
