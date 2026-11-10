#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"

#include "SDL.h"

namespace AgcDriver::DriverDetail {

StallChoice ShowStallPrompt(const char* message) {
    SDL_MessageBoxButtonData buttons[]{
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Wait"},
        {0, 1, "Quit"},
    };
    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_WARNING;
    data.title = "AnyPS5";
    data.message = message;
    data.numbuttons = 2;
    data.buttons = buttons;
    int button = -1;
    if (SDL_ShowMessageBox(&data, &button) != 0) return StallChoice::Unavailable;
    return button == 1 ? StallChoice::Quit : StallChoice::Wait;
}

}
