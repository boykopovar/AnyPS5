#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGREQUEST_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGREQUEST_HPP

#include <cstddef>
#include <utility>

#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogText.hpp"

namespace MsgDialog {

inline bool BuildRequest(const ParsedParam& parsed, ModalDialogRequest& request) {
    if (parsed.mode != ModeUserMsg || !parsed.hasUserMessage || !parsed.hasText) return false;
    ModalDialogRequest built;
    switch (parsed.buttonType) {
        case ButtonTypeOk:
            built.buttons = {u"OK"};
            built.cancel = 0;
            break;
        case ButtonTypeYesNo:
            built.buttons = {u"Yes", u"No"};
            built.cancel = 1;
            break;
        case ButtonTypeOkCancel:
            built.buttons = {u"OK", u"Cancel"};
            built.cancel = 1;
            break;
        case ButtonTypeYesNoFocusNo:
            built.buttons = {u"Yes", u"No"};
            built.focus = 1;
            built.cancel = 1;
            break;
        default:
            return false;
    }
    built.message = DecodeUtf8(parsed.text);
    request = std::move(built);
    return true;
}

inline int ButtonIdOf(std::size_t index) {
    return static_cast<int>(index) + 1;
}

}

#endif
