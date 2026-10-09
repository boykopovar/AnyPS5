#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP

#include "SDL_events.h"
#include "SDL_gamecontroller.h"
#include "prx/libScePad/include/InputMapping.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <chrono>
#include <vector>

class DisplayWindow;

class PadInput {
public:
    PadInput();
    ~PadInput();
    void HandleEvent(const SDL_Event& event, DisplayWindow& window);
    void Update();

private:
    explicit PadInput(Pad::InputConfiguration configuration);
    void publish();
    void setMouseMode(bool enabled);
    void openFirstAvailableController();
    void openController(int deviceIndex);
    void closeController();
    PadInputState sampleController() const;
    void applyOutput();
    void enableSensors();

    std::vector<Pad::InputBinding> bindings;
    Pad::InputSettings settings;
    std::vector<bool> pressed;
    std::vector<std::chrono::steady_clock::time_point> wheelReleaseTimes;
    std::array<std::uint8_t, 2> mouseStick{128, 128};
    std::chrono::steady_clock::time_point nextMousePoll{};
    bool mouseEnabled = false;
    SDL_GameController* controller = nullptr;
    PadInputState controllerState{};
    std::uint32_t outputSequence = 0;
    PadOutputState outputState{};
    bool outputPending = false;
    std::chrono::steady_clock::time_point nextRumbleRefresh{};
};

#endif
