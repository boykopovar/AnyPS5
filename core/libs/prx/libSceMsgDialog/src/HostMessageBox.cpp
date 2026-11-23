#include "prx/libSceMsgDialog/include/HostMessageBox.hpp"

#include <SDL.h>

#include <vector>

namespace MsgDialog {

std::optional<HostBoxAnswer> ShowHostMessageBox(std::string_view message, const ModalDialogRequest& request) {
    const auto spec = BuildHostMessageBoxSpec(message, request);
    if (!spec) return std::nullopt;

    std::vector<SDL_MessageBoxButtonData> buttons;
    buttons.reserve(spec->buttons.size());
    for (std::size_t index = 0; index < spec->buttons.size(); ++index) {
        Uint32 flags = 0;
        if (spec->buttons[index].isCancel) flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        if (spec->buttons[index].isFocus) flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        buttons.push_back({flags, static_cast<int>(index), spec->buttons[index].label.c_str()});
    }

    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.title = "AnyPS5";
    data.message = spec->message.c_str();
    data.numbuttons = static_cast<int>(buttons.size());
    data.buttons = buttons.data();
    int pressed = -1;
    if (SDL_ShowMessageBox(&data, &pressed) != 0) return std::nullopt;
    return HostBoxAnswer{IndexOfHostButton(pressed, buttons.size(), request.cancel)};
}

}
