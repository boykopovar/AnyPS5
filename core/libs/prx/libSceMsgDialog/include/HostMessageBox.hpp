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

}

#endif
