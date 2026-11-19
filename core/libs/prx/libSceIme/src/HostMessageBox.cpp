#include "prx/libSceIme/include/HostMessageBox.hpp"

#include <SDL.h>

#include <string>
#include <vector>

extern "C" bool ShowHostMessageBox_nid_postfix(std::string_view message, const ModalDialogRequest& request, HostBox::HostBoxAnswer& answer) {
    std::vector<std::string> labels;
    labels.reserve(request.buttons.size());
    for (const auto& label : request.buttons) labels.push_back(HostBox::HostMessageLabel(label));

    std::vector<SDL_MessageBoxButtonData> buttons;
    buttons.reserve(labels.size());
    for (std::size_t index = 0; index < labels.size(); ++index) {
        Uint32 flags = 0;
        if (index == request.cancel) flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        if (index == request.focus) flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        buttons.push_back({flags, static_cast<int>(index), labels[index].c_str()});
    }

    const std::string text(message);
    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.title = "AnyPS5";
    data.message = text.c_str();
    data.numbuttons = static_cast<int>(buttons.size());
    data.buttons = buttons.data();
    int pressed = -1;
    if (SDL_ShowMessageBox(&data, &pressed) != 0) return false;
    answer = HostBox::HostBoxAnswer{HostBox::IndexOfHostButton(pressed, buttons.size(), request.cancel)};
    return true;
}
