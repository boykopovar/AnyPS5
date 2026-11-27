#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "prx/libSceIme/include/ModalDialog.hpp"

namespace MsgDialog {

inline bool HostMessageBoxEnabled(const char* value, bool apple) {
    return !apple && value != nullptr && std::string_view(value) == "1";
}

inline std::string HostMessageLabel(const std::u16string& label) {
    std::string text;
    text.reserve(label.size());
    for (const char16_t unit : label) text += unit < 0x80 ? static_cast<char>(unit) : '?';
    return text;
}

inline std::optional<std::size_t> IndexOfHostButton(int pressed, std::size_t count, std::size_t) {
    if (pressed < 0 || static_cast<std::size_t>(pressed) >= count) return std::nullopt;
    return static_cast<std::size_t>(pressed);
}

struct HostBoxButton {
    std::string label;
    bool isFocus = false;
    bool isCancel = false;
};

struct HostBoxSpec {
    std::string message;
    std::vector<HostBoxButton> buttons;
};

inline std::optional<HostBoxSpec> BuildHostMessageBoxSpec(std::string_view message, const ModalDialogRequest& request) {
    if (request.buttons.empty() || request.buttons.size() > ModalDialogMaxButtons) {
        return std::nullopt;
    }

    HostBoxSpec spec;
    spec.message.reserve(message.size());
    for (const char ch : message) {
        if (ch != '\0') spec.message.push_back(ch);
    }

    const std::size_t focus = request.focus < request.buttons.size() ? request.focus : 0;
    spec.buttons.reserve(request.buttons.size());
    for (std::size_t index = 0; index < request.buttons.size(); ++index) {
        spec.buttons.push_back({
            HostMessageLabel(request.buttons[index]),
            index == focus,
            index == request.cancel,
        });
    }

    return spec;
}

struct HostBoxAnswer {
    std::optional<std::size_t> chosen;
};

std::optional<HostBoxAnswer> ShowHostMessageBox(std::string_view message, const ModalDialogRequest& request);

}

#endif
