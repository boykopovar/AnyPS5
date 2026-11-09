#include "prx/libSceMsgDialog/include/HostMessageBox.hpp"

#include <SDL.h>

#include <string>
#include <vector>

namespace MsgDialog {

std::optional<HostBoxAnswer> ShowHostMessageBox(std::string_view message, const ModalDialogRequest& request) {
    std::vector<std::string> labels;
    labels.reserve(request.buttons.size());
    for (const auto& label : request.buttons) labels.push_back(HostMessageLabel(label));

    std::vector<SDL_MessageBoxButtonData> buttons;
    buttons.reserve(labels.size());
    for (std::size_t index = 0; index < labels.size(); ++index) {
        Uint32 flags = 0;
        if (index == request.cancel) flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        if (index == request.focus) flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        buttons.push_back({flags, static_cast<int>(index), labels[index].c_str()});
    }

    (void)message;
    return std::nullopt;
}

}
