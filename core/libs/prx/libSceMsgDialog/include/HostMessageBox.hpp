#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

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

}

#endif
