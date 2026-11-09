#include "prx/libSceMsgDialog/include/HostMessageBox.hpp"

namespace MsgDialog {

std::optional<HostBoxAnswer> ShowHostMessageBox(std::string_view message, const ModalDialogRequest& request) {
    (void)message;
    (void)request;
    return std::nullopt;
}

}
