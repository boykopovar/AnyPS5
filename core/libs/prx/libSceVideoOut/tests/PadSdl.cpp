#include "SDL_config.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>
#include <thread>
#include <vector>
#undef main

namespace {

static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "pad_sdl: check failed at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

std::vector<ImeHostPad> forwarded;

void Forward(const ImeHostPad& pad) {
    forwarded.push_back(pad);
}

void HandOff(PadInput& padInput, const DisplayWindow& window, int device) {
    constexpr std::uint32_t Cross = 0x4000;
    constexpr std::uint32_t R2 = 0x0200;
    constexpr std::uint32_t Intercepted = 0x80000000;
    Pad::Initialize();
    SDL_Joystick* joystick = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(device));
    Require(joystick != nullptr);
    const auto button = [&](SDL_GameControllerButton which, bool down) {
        Require(SDL_JoystickSetVirtualButton(joystick, which, down ? 1 : 0) == 0);
        padInput.Update(window);
    };
    const auto trigger = [&](bool down) {
        Require(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, down ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN) == 0);
        padInput.Update(window);
    };
    Require(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_JOYSTICK_AXIS_MIN) == 0);
    trigger(false);
    button(SDL_CONTROLLER_BUTTON_A, true);
    Require((Pad::ReadState(0).buttons & Cross) != 0);
    button(SDL_CONTROLLER_BUTTON_A, false);
    Require((Pad::ReadState(0).buttons & Cross) == 0);
    button(SDL_CONTROLLER_BUTTON_A, true);
    button(SDL_CONTROLLER_BUTTON_A, false);

    ImeSetHostPadSink_nid_no_patch(&Forward);
    padInput.Update(window);
    button(SDL_CONTROLLER_BUTTON_A, true);
    const PadData held = Pad::ReadState(0);
    Require(held.buttons == Intercepted);
    Require(held.left_stick_x == 128 && held.left_stick_y == 128 && held.right_stick_x == 128 && held.right_stick_y == 128);
    Require(held.analog_buttons_l2 == 0 && held.analog_buttons_r2 == 0);
    Require(held.connected);
    Require(!forwarded.empty() && (forwarded.back().buttons & Cross) != 0);
    PadData queued[64]{};
    const int count = Pad::Read(queued, 64, 0);
    Require(count >= 1);
    for (int index = 0; index < count; ++index) Require(queued[index].buttons == Intercepted);
    button(SDL_CONTROLLER_BUTTON_A, false);
    Require((forwarded.back().buttons & Cross) == 0);
    trigger(true);
    Require(Pad::ReadState(0).buttons == Intercepted);
    Require((forwarded.back().buttons & R2) != 0);

    ImeSetHostPadSink_nid_no_patch(nullptr);
    const auto received = forwarded.size();
    padInput.Update(window);
    Require(forwarded.size() == received);
    PadData after = Pad::ReadState(0);
    Require((after.buttons & (R2 | Intercepted)) == 0 && after.analog_buttons_r2 == 0);
    padInput.Update(window);
    Require((Pad::ReadState(0).buttons & R2) == 0);
    trigger(false);
    Require((Pad::ReadState(0).buttons & R2) == 0);
    trigger(true);
    Require((Pad::ReadState(0).buttons & R2) != 0);
    button(SDL_CONTROLLER_BUTTON_A, true);
    Require((Pad::ReadState(0).buttons & Cross) != 0);
    button(SDL_CONTROLLER_BUTTON_A, false);
    trigger(false);

    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest request;
    request.message = u"Continue?";
    request.buttons = {u"Yes", u"No"};
    request.cancel = 1;
    Require(ModalDialogOpen_nid_postfix(request));
    padInput.Update(window);
    Require((Pad::ReadState(0).buttons & Intercepted) != 0);
    button(SDL_CONTROLLER_BUTTON_A, true);
    const PadData answering = Pad::ReadState(0);
    Require(answering.buttons == Intercepted && answering.left_stick_x == 128 && answering.left_stick_y == 128);
    std::size_t chosen = 9;
    Require(ModalDialogPoll_nid_postfix(chosen) == ModalDialogState::Answered && chosen == 0);
    padInput.Update(window);
    Require((Pad::ReadState(0).buttons & (Cross | Intercepted)) == 0);
    button(SDL_CONTROLLER_BUTTON_A, false);
    button(SDL_CONTROLLER_BUTTON_A, true);
    Require((Pad::ReadState(0).buttons & Cross) != 0);
    button(SDL_CONTROLLER_BUTTON_A, false);

    Require(ModalDialogOpen_nid_postfix(request));
    Require(ModalDialogCapturesInput_nid_postfix());
    ModalDialogClose_nid_postfix();
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

void InjectedAnswer(PadInput& padInput, const DisplayWindow& window, const std::filesystem::path& commandPath) {
    constexpr std::uint32_t Cross = 0x4000;
    constexpr std::uint32_t Intercepted = 0x80000000;
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest request;
    request.message = u"Create new data?";
    request.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(request));
    padInput.Update(window);
    Require(Pad::ReadState(0).buttons == Intercepted);
    std::ofstream(commandPath) << "1 16384 400\n";
    std::size_t chosen = 9;
    ModalDialogState state = ModalDialogState::Showing;
    for (int frame = 0; frame < 200 && state == ModalDialogState::Showing; ++frame) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        padInput.Update(window);
        Require(Pad::ReadState(0).buttons == Intercepted);
        state = ModalDialogPoll_nid_postfix(chosen);
    }
    Require(state == ModalDialogState::Answered && chosen == 0);
    padInput.Update(window);
    Require(Pad::ReadState(0).buttons == 0);
    for (int frame = 0; frame < 50; ++frame) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        padInput.Update(window);
        Require(Pad::ReadState(0).buttons == 0);
    }
    std::ofstream(commandPath) << "2 16384 400\n";
    bool reached = false;
    for (int frame = 0; frame < 200 && !reached; ++frame) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        padInput.Update(window);
        reached = Pad::ReadState(0).buttons == Cross;
    }
    Require(reached);
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

std::uint32_t TitleButtons(bool useRead) {
    if (!useRead) return Pad::ReadState(0).buttons;
    std::array<PadData, 8> data{};
    const int count = Pad::Read(data.data(), static_cast<int>(data.size()), 0);
    Require(count > 0);
    return data[static_cast<std::size_t>(count - 1)].buttons;
}

void InjectedPressAfterUnreadRelease(PadInput& padInput, const DisplayWindow& window, const std::filesystem::path& commandPath, std::uint64_t sequence, bool useRead) {
    constexpr std::uint32_t Cross = 0x4000;
    constexpr std::uint32_t Intercepted = 0x80000000;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    padInput.Update(window);
    Require(TitleButtons(useRead) == 0);
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest request;
    request.message = u"Create new data?";
    request.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(request));
    padInput.Update(window);
    Require(TitleButtons(useRead) == Intercepted);
    std::ofstream(commandPath) << "APS5_PAD_TIMELINE 1 " << sequence << " 2\n0 400 16384\n1000 400 16384\n";
    const auto written = std::chrono::steady_clock::now();
    std::size_t chosen = 9;
    ModalDialogState state = ModalDialogState::Showing;
    for (int frame = 0; frame < 30 && state == ModalDialogState::Showing; ++frame) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        padInput.Update(window);
        state = ModalDialogPoll_nid_postfix(chosen);
    }
    Require(state == ModalDialogState::Answered && chosen == 0);
    padInput.Update(window);
    Require(TitleButtons(useRead) == 0);
    Require(std::chrono::steady_clock::now() - written < std::chrono::milliseconds(350));
    std::this_thread::sleep_until(written + std::chrono::milliseconds(1150));
    padInput.Update(window);
    Require(TitleButtons(useRead) == Cross);
    std::this_thread::sleep_until(written + std::chrono::milliseconds(1700));
    padInput.Update(window);
    Require(TitleButtons(useRead) == 0);
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

}

int main() {
#ifdef SDL_JOYSTICK_HIDAPI
    std::puts("pad_sdl: SDL drives controllers through HIDAPI");
#else
    std::fputs("pad_sdl: SDL is built without HIDAPI, so a DualSense gets no light bar, touchpad or trigger effects\n", stderr);
    return 1;
#endif

    const auto commandPath = std::filesystem::temp_directory_path() / ("anyps5-pad-sdl-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{commandPath};
#ifdef _WIN32
    Require(_wputenv_s(L"APS5_PAD_INPUT_FILE", commandPath.c_str()) == 0);
#else
    Require(setenv("APS5_PAD_INPUT_FILE", commandPath.string().c_str(), 1) == 0);
#endif

    Require(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0);

    const int hostJoysticks = SDL_NumJoysticks();
    if (hostJoysticks < 0) {
        std::fprintf(stderr, "pad_sdl: SDL_NumJoysticks failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
        return 1;
    }
    if (hostJoysticks > 0) {
        std::fprintf(stderr, "pad_sdl: skipped, SDL sees %d host joysticks, which take the pad slots this test checks\n", hostJoysticks);
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
        return 77;
    }

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
    HandOff(padInput, window, dev0);

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
    InjectedAnswer(padInput, window, commandPath);
    InjectedPressAfterUnreadRelease(padInput, window, commandPath, 3, false);
    InjectedPressAfterUnreadRelease(padInput, window, commandPath, 4, true);

    Require(SDL_JoystickDetachVirtual(dev1) == 0);
    Require(SDL_JoystickDetachVirtual(dev0) == 0);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    return 0;
}
