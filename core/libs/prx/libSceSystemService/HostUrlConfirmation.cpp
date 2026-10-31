#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include <SDL.h>

namespace {

constexpr std::size_t WRAP_WIDTH = 96;
constexpr int BUTTON_CANCEL = 0;
constexpr int BUTTON_OPEN = 1;

}

std::string HostConfirmationText(const std::string& url) {
    std::string text = "A game asks to open this address in your default web browser:\n\n";
    for (std::size_t offset = 0; offset < url.size(); offset += WRAP_WIDTH) {
        if (offset != 0) text += '\n';
        text.append(url, offset, WRAP_WIDTH);
    }
    text += "\n\nOpen it only if you trust it.";
    return text;
}

bool HostMessageBoxConfirmation::Confirm(const std::string& url) {
    const std::string text = HostConfirmationText(url);
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, BUTTON_CANCEL, "Cancel"},
        {0, BUTTON_OPEN, "Open"},
    };
    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_WARNING;
    data.title = "AnyPS5: open a link?";
    data.message = text.c_str();
    data.numbuttons = 2;
    data.buttons = buttons;
    int pressed = BUTTON_CANCEL;
    if (SDL_ShowMessageBox(&data, &pressed) != 0) return false;
    return pressed == BUTTON_OPEN;
}
