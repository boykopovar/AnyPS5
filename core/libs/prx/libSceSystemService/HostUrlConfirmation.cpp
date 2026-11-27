#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include "prx/libSceSystemService/HostUrl.hpp"
#include <SDL.h>
#include <cstdlib>

namespace {

constexpr std::size_t WRAP_WIDTH = 96;
constexpr int BUTTON_CANCEL = 0;
constexpr int BUTTON_OPEN = 1;

#ifdef __linux__
bool IsSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}
#endif

std::string SanitizeConfirmationUrl(const std::string& url) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string sanitized;
    sanitized.reserve(url.size());
    for (const char ch : url) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte < 0x20 || byte == 0x7F) {
            sanitized += '%';
            sanitized += HEX[byte >> 4];
            sanitized += HEX[byte & 0x0F];
        } else {
            sanitized += ch;
        }
    }
    return sanitized;
}

}

std::string HostConfirmationText(const std::string& url) {
    const std::string sanitized = SanitizeConfirmationUrl(url);
    std::string text = "A game asks to open this address in your default web browser:\n\n";
    for (std::size_t offset = 0; offset < sanitized.size(); offset += WRAP_WIDTH) {
        if (offset != 0) text += '\n';
        text.append(sanitized, offset, WRAP_WIDTH);
    }
    text += "\n\nOpen it only if you trust it.";
    return text;
}

bool HostMessageBoxConfirmation::Confirm(const std::string& url) {
    if (ValidateHostUrl(url) != HostUrlVerdict::Ok) return false;
#ifdef __linux__
    if (!IsSet("DISPLAY") && !IsSet("WAYLAND_DISPLAY")) return false;
#endif
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
