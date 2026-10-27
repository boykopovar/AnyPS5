#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP

#include "SDL_events.h"
#include "SDL_gamecontroller.h"
#include "prx/libScePad/include/KeyboardMouse.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libSceVideoOut/include/PadOutputPacing.hpp"
#include <array>
#include <chrono>
#include "prx/libSceVideoOut/include/Telemetry.hpp"

class DisplayWindow;

class PadInput {
public:
    PadInput();
    ~PadInput();
    void HandleEvent(const SDL_Event& event, DisplayWindow& window);
    void Update(const DisplayWindow& window);
    bool MouseModeEnabled() const { return keyboardMouse.MouseMode(); }
    bool SlotOccupied(int slot) const {
        return slot >= 0 && slot < PAD_MAX_CONTROLLERS && slots[slot].controller != nullptr;
    }

private:
    struct ControllerSlot {
        SDL_GameController* controller = nullptr;
        SDL_JoystickID instanceId = -1;
        PadInputState state{};
        std::uint32_t outputSequence = 0;
        PadOutputState outputState{};
        bool outputPending = false;
        std::chrono::steady_clock::time_point nextRumbleRefresh{};
        LightBarPacer lightBar;
        TriggerEffectCache triggerEffect;
    };

    explicit PadInput(Pad::InputConfiguration configuration);
    void publish(std::uint64_t frame);
    void publish(std::uint64_t frame, int slot);
    void setMouseMode(bool enabled);
    void openAvailableControllers();
    void openController(int deviceIndex);
    void closeController(int slot);
    PadInputState sampleController(int slot) const;
    void applyOutput();
    void enableSensors(int slot);

    Pad::KeyboardMouse keyboardMouse;
    std::array<ControllerSlot, PAD_MAX_CONTROLLERS> slots{};
    VideoOutTelemetry::PadChangeFilter telemetryFilter;
};

#endif
