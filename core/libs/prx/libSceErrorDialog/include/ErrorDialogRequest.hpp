#ifndef CORE_LIBS_PRX_LIBSCEERRORDIALOG_INCLUDE_ERRORDIALOGREQUEST_HPP
#define CORE_LIBS_PRX_LIBSCEERRORDIALOG_INCLUDE_ERRORDIALOGREQUEST_HPP

#include <cstdint>
#include <cstdio>
#include <string>

#include "prx/libSceIme/include/ModalDialog.hpp"

namespace ErrorDialog {

inline ModalDialogRequest BuildRequest(std::int32_t errorCode) {
    char code[16] = {};
    std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(errorCode));
    ModalDialogRequest request;
    request.message = u"An error occurred.\nError code: ";
    for (const char* digit = code; *digit != '\0'; ++digit) request.message.push_back(static_cast<char16_t>(*digit));
    request.buttons = {u"OK"};
    request.cancel = 0;
    return request;
}

}

#endif
