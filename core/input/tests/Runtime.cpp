#define SDL_MAIN_HANDLED
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static PadInputState published;
extern "C" void PadPublishInput_nid_postfix(const PadInputState& input) { published = input; }
extern "C" bool PadFetchOutput_nid_postfix(std::uint32_t*, PadOutputState*) { return false; }
SDL_Window* DisplayWindow::Handle() const { return nullptr; }
void DisplayWindow::ToggleFullscreen() {}
DisplayWindow::~DisplayWindow() = default;
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }

int main() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, SDL_GetError());
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-runtime-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    const auto path = directory / "anyps5-input.ini";
#ifdef _WIN32
    _putenv_s("ANYPS5_INPUT_CONFIG", path.string().c_str());
#else
    setenv("ANYPS5_INPUT_CONFIG", path.string().c_str(), 1);
#endif
    InputConfig::WriteAtomic(path, "Cross=KEY:F\n");
    try {
        const int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 3, 0);
        Require(device >= 0, SDL_GetError());
        auto* joystick = SDL_JoystickOpen(device);
        Require(joystick != nullptr, SDL_GetError());
        for (int axis = 4; axis < 6; ++axis) SDL_JoystickSetVirtualAxis(joystick, axis, -32768);
        const auto guid = InputConfig::Guid(joystick);
        InputConfig::WriteAtomic(directory / "anyps5-gamecontrollerdb.txt", guid + ",Runtime pad,a:b0,b:b1,back:b2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,\n");
        {
            PadInput pad;
            DisplayWindow window;
            SDL_JoystickSetVirtualButton(joystick, 2, 1); pad.Update();
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) != 0 && published.touch[0].active && published.touch[0].x == 960 && published.touch[0].y == 471, "View touch contact not published to the game");
            pad.SetSuspended(true); pad.Update();
            Require(published.buttons == 0 && !published.touch[0].active, "View contact leaked while editor active");
            SDL_JoystickSetVirtualButton(joystick, 2, 0); pad.SetSuspended(false); pad.Update();
            Require(published.buttons == 0 && !published.touch[0].active, "released View contact persisted");
            SDL_Event key{};
            key.type = SDL_KEYDOWN; key.key.keysym.scancode = SDL_SCANCODE_F;
            pad.HandleEvent(key, window);
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) != 0, "keyboard binding not active");
            pad.SetSuspended(true);
            Require(published.buttons == 0 && published.sticks[0] == 128, "editor did not clear held keyboard input");
            SDL_JoystickSetVirtualButton(joystick, 0, 1);
            SDL_JoystickSetVirtualAxis(joystick, 0, 32767);
            pad.Update();
            Require(published.buttons == 0 && published.sticks[0] == 128, "controller leaked into game while editor active");
            InputConfig::WriteAtomic(path, "Cross=KEY:G\n");
            InputConfig::ControllerProfile profile;
            profile.buttons[0] = SDL_CONTROLLER_BUTTON_B;
            profile.buttons[1] = SDL_CONTROLLER_BUTTON_A;
            InputConfig::WriteAtomic(directory / "anyps5-controller.ini", InputConfig::SerializeControllers({{guid, profile}}));
            pad.Reload(true, false);
            pad.Update();
            Require(published.buttons == 0, "reload escaped suspension");
            SDL_JoystickSetVirtualButton(joystick, 0, 0);
            pad.SetSuspended(false);
            pad.Update();
            Require(published.buttons == 0, "stuck keyboard input after closing editor");
            key.key.keysym.scancode = SDL_SCANCODE_F;
            pad.HandleEvent(key, window);
            Require(published.buttons == 0, "old keyboard mapping still active");
            key.key.keysym.scancode = SDL_SCANCODE_G;
            pad.HandleEvent(key, window);
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) != 0, "new keyboard mapping not active");
            key.type = SDL_KEYUP; pad.HandleEvent(key, window);
            SDL_JoystickSetVirtualButton(joystick, 0, 1); pad.Update();
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::Circle)) != 0, "new controller profile not active");
            InputConfig::WriteAtomic(directory / "anyps5-gamecontrollerdb.txt", guid + ",Runtime pad,a:b1,b:b0,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,\n");
            pad.Reload(false, true); pad.Update();
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) != 0, "physical mapping not applied live");
            SDL_JoystickClose(joystick);
            SDL_JoystickDetachVirtual(device);
            SDL_Event event;
            while (SDL_PollEvent(&event)) pad.HandleEvent(event, window);
            pad.Update();
            Require(published.buttons == 0, "disconnect left a button pressed");
            const int next = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 3, 0);
            while (SDL_PollEvent(&event)) pad.HandleEvent(event, window);
            joystick = SDL_JoystickOpen(next);
            for (int axis = 4; axis < 6; ++axis) SDL_JoystickSetVirtualAxis(joystick, axis, -32768);
            SDL_JoystickSetVirtualButton(joystick, 0, 1); pad.Update();
            Require((published.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) != 0, "reconnected controller lost its profile");
            SDL_JoystickClose(joystick); SDL_JoystickDetachVirtual(next);
        }
        SDL_Quit(); std::filesystem::remove_all(directory);
        std::cout << "input_runtime: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; SDL_Quit(); std::filesystem::remove_all(directory); return 1;
    }
}
