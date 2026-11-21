#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_HOSTMESSAGEBOX_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "prx/libSceIme/include/ModalDialog.hpp"

namespace HostBox {

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

struct HostBoxAnswer {
    std::optional<std::size_t> chosen;
};

}

// Called from the other libraries, so it has C linkage and the _nid_postfix name that the NID patcher keeps.
// A C++ name is dropped from the patched libSceIme.prx, and the importing library then fails to load with error 127.
extern "C" bool ShowHostMessageBox_nid_postfix(std::string_view message, const ModalDialogRequest& request, HostBox::HostBoxAnswer& answer);

#endif
