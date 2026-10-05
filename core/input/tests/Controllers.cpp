#define SDL_MAIN_HANDLED
#include "ControllerMapping.hpp"
#include "SDL.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

extern "C" void SDL_PrivateJoystickAddTouchpad(SDL_Joystick* joystick, int fingers);
extern "C" int SDL_PrivateJoystickTouchpad(SDL_Joystick* joystick, int touchpad, int finger, Uint8 state, float x, float y, float pressure);

void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class TOperation> void Reject(TOperation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "invalid configuration accepted");
}
int main() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Require(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0, SDL_GetError());
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-controller-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        const int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 16, 1);
        Require(device >= 0, SDL_GetError());
        SDL_Joystick* joystick = SDL_JoystickOpen(device);
        Require(joystick != nullptr, SDL_GetError());
        const auto guid = InputConfig::Guid(joystick);
        const std::string mapping = guid + ",Virtual pad,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,start:b6,leftstick:b7,rightstick:b8,touchpad:b9,back:b11,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
        InputConfig::ValidateMapping(mapping);
        Reject([&] { InputConfig::ValidateMapping(guid + ",Bad,a:b0,a:b1,"); });
        Reject([&] { InputConfig::ValidateMapping(guid + ",Bad,a:b-1,"); });
        Reject([&] { InputConfig::ValidateMapping(guid + ",Bad,dpup:h0.3,"); });
        const auto databasePath = directory / "anyps5-gamecontrollerdb.txt";
        InputConfig::WriteAtomic(databasePath, InputConfig::SerializeDatabase({mapping}));
        InputConfig::ApplyDatabase(InputConfig::LoadDatabase(databasePath));
        Require(SDL_IsGameController(device), "custom mapping did not recognize joystick");
        SDL_GameController* controller = SDL_GameControllerOpen(device);
        Require(controller != nullptr, SDL_GetError());
        for (int axis = 0; axis < 6; ++axis) SDL_JoystickSetVirtualAxis(joystick, axis, axis < 4 ? 0 : -32768);
        SDL_JoystickSetVirtualButton(joystick, 0, 1);
        SDL_JoystickSetVirtualHat(joystick, 0, SDL_HAT_UP);
        SDL_GameControllerUpdate();
        auto profile = InputConfig::ControllerProfile{};
        auto state = InputConfig::SampleController(controller, profile);
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) != 0, "button not forwarded");
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::Up)) != 0, "hat not forwarded");
        Require(state.sticks[0] == 128 && state.analogButtonsL2 == 0, "neutral analog state incorrect");
        SDL_JoystickSetVirtualButton(joystick, 11, 1); SDL_GameControllerUpdate();
        state = InputConfig::SampleController(controller, profile);
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) != 0 && state.touch[0].active && state.touch[0].x == 960 && state.touch[0].y == 471, "View fallback lost its center touch contact");
        SDL_LockJoysticks();
        SDL_PrivateJoystickAddTouchpad(joystick, 2);
        SDL_PrivateJoystickTouchpad(joystick, 0, 1, SDL_PRESSED, 0.25f, 0.75f, 1.0f);
        SDL_UnlockJoysticks();
        state = InputConfig::SampleController(controller, profile);
        Require(!state.touch[0].active && state.touch[1].active && state.touch[1].x == 479 && state.touch[1].y == 706, "View fallback overwrote a real finger contact");
        SDL_LockJoysticks();
        SDL_PrivateJoystickTouchpad(joystick, 0, 1, SDL_RELEASED, 0.25f, 0.75f, 0.0f);
        SDL_UnlockJoysticks();
        profile.buttons.back() = SDL_CONTROLLER_BUTTON_INVALID;
        state = InputConfig::SampleController(controller, profile);
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) == 0 && !state.touch[0].active, "disabled touchpad retained View fallback");
        profile.buttons.back() = SDL_CONTROLLER_BUTTON_B;
        Require((InputConfig::SampleController(controller, profile).buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) == 0, "reassigned touchpad retained View fallback");
        SDL_JoystickSetVirtualButton(joystick, 1, 1); SDL_GameControllerUpdate();
        Require(InputConfig::SampleController(controller, profile).touch[0].active, "reassigned touchpad has no synthetic contact");
        SDL_JoystickSetVirtualButton(joystick, 1, 0);
        profile = InputConfig::ControllerProfile{};
        for (const auto* type : {"PS4", "PS5"}) {
            Require(SDL_GameControllerAddMapping((mapping + "type:" + type + ",").c_str()) >= 0, SDL_GetError());
            SDL_GameControllerUpdate();
            Require(SDL_GameControllerGetType(controller) == (std::string_view(type) == "PS4" ? SDL_CONTROLLER_TYPE_PS4 : SDL_CONTROLLER_TYPE_PS5), "virtual PlayStation type not applied");
            state = InputConfig::SampleController(controller, profile);
            Require((state.buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) == 0 && !state.touch[0].active, "Share/Create incorrectly became a touchpad click");
            SDL_JoystickSetVirtualButton(joystick, 9, 1); SDL_GameControllerUpdate();
            Require((InputConfig::SampleController(controller, profile).buttons & static_cast<unsigned>(Pad::PadButton::TouchPad)) != 0, "physical PlayStation touchpad click lost");
            SDL_JoystickSetVirtualButton(joystick, 9, 0);
        }
        InputConfig::ApplyDatabase({mapping});
        SDL_JoystickSetVirtualButton(joystick, 11, 0); SDL_GameControllerUpdate();
        Require(!InputConfig::SampleController(controller, profile).touch[0].active, "View release retained synthetic contact");
        SDL_JoystickSetVirtualAxis(joystick, 0, 32767);
        SDL_JoystickSetVirtualAxis(joystick, 4, 0);
        SDL_GameControllerUpdate();
        state = InputConfig::SampleController(controller, profile);
        Require(state.sticks[0] == 255 && state.analogButtonsL2 >= 127 && state.analogButtonsL2 <= 128, "analog precision lost");
        profile.axes[0].inverted = true;
        profile.buttons[0] = SDL_CONTROLLER_BUTTON_B;
        profile.buttons[1] = SDL_CONTROLLER_BUTTON_A;
        profile.buttons[9] = SDL_CONTROLLER_BUTTON_INVALID;
        state = InputConfig::SampleController(controller, profile);
        Require(state.sticks[0] == 0, "axis inversion incorrect");
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::Cross)) == 0 && (state.buttons & static_cast<unsigned>(Pad::PadButton::Circle)) != 0, "button reassignment incorrect");
        Require((state.buttons & static_cast<unsigned>(Pad::PadButton::Up)) == 0, "disabled button active");
        SDL_JoystickSetVirtualAxis(joystick, 4, 32767);
        SDL_GameControllerUpdate();
        Require(InputConfig::SampleController(controller, profile).analogButtonsL2 == 255, "full trigger incorrect");
        auto digitalMapping = mapping;
        digitalMapping.replace(digitalMapping.find("lefttrigger:a4"), std::string("lefttrigger:a4").size(), "lefttrigger:b10");
        InputConfig::ApplyDatabase({digitalMapping});
        SDL_JoystickSetVirtualButton(joystick, 10, 1); SDL_GameControllerUpdate();
        Require(InputConfig::SampleController(controller, InputConfig::ControllerProfile{}).analogButtonsL2 == 255, "digital trigger press not converted to analog");
        SDL_JoystickSetVirtualButton(joystick, 10, 0); SDL_GameControllerUpdate();
        Require(InputConfig::SampleController(controller, InputConfig::ControllerProfile{}).analogButtonsL2 == 0, "digital trigger release not converted to analog");
        profile.axes[4] = {SDL_CONTROLLER_AXIS_INVALID, true};
        Require(InputConfig::SampleController(controller, profile).analogButtonsL2 == 0, "disabled trigger active");
        const auto configPath = directory / "anyps5-controller.ini";
        const auto serialized = InputConfig::SerializeControllers({{guid, profile}});
        InputConfig::WriteAtomic(configPath, serialized);
        Require(InputConfig::SerializeControllers(InputConfig::LoadControllers(configPath)) == serialized, "controller profile round trip changed settings");
        InputConfig::WriteAtomic(configPath, "[" + guid + "]\nCross=BUTTON:not-a-button\n");
        Reject([&] { InputConfig::LoadControllers(configPath); });
        SDL_GameControllerClose(controller);
        SDL_JoystickClose(joystick);
        Require(SDL_JoystickDetachVirtual(device) == 0, SDL_GetError());
        const int reconnected = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 16, 1);
        Require(reconnected >= 0 && SDL_IsGameController(reconnected), "mapping was not retained across reconnection");
        SDL_JoystickDetachVirtual(reconnected);
        std::filesystem::remove_all(directory);
        SDL_Quit();
        std::cout << "input_controllers: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        std::filesystem::remove_all(directory);
        return 1;
    }
}
